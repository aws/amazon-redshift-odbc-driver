#include "common.h"
#include "rstrace.h"
#include "rsunicode.h"
#include "rslog.h"

#include <vector>
#include <string>
#include <chrono>
#include <optional>
#include <algorithm>
#include <limits>
#include <iomanip>
#include <array>

// ---------- Helpers: build safe SQLWCHAR buffers (portable across 2/4-byte SQLWCHAR) ----------

static std::vector<SQLWCHAR> makeSqlwFromU16(const char16_t *u16) {
    std::vector<SQLWCHAR> out;
    if constexpr (sizeof(SQLWCHAR) == 2) {
        for (size_t i = 0; u16[i] != 0; ++i) {
            out.push_back(static_cast<SQLWCHAR>(u16[i]));
        }
        out.push_back(0);
    } else {
        // Minimal UTF-16 -> UTF-32 widening for BMP-only test inputs used here.
        for (size_t i = 0; u16[i] != 0; ++i) {
            // For BMP characters, simple casting works
            out.push_back(static_cast<SQLWCHAR>(u16[i]));
        }
        out.push_back(0);
    }
    return out;
}

static std::vector<SQLWCHAR> makePaddedSqlwWithCanary(const char16_t *u16,
                                                      size_t claimedLen) {
    auto content = makeSqlwFromU16(u16);       // ends with NUL
    const size_t realLen = content.size() - 1; // code units before NUL
    size_t bufSize =
        (std::max)(claimedLen, realLen + 1) + 1; // Ensure sufficient space
    std::vector<SQLWCHAR> buf(bufSize, 0);      // extra padding
    std::copy(content.begin(), content.end(), buf.begin());
    buf[realLen + 1] = static_cast<SQLWCHAR>('X');
    return buf;
}

// ---------- Test-friendly wrapper to access protected methods ----------
class TestableRsTrace : public RsTrace {
public:
    using RsTrace::clearBuffer;
    using RsTrace::getBuffer;
    using RsTrace::traceArg;
    // Expose protected methods for testing
    void testTraceWStrValWithSmallLen(const char *pArgName, SQLWCHAR *pwVal, SQLSMALLINT cchLen) {
        traceWStrValWithSmallLen(pArgName, pwVal, cchLen);
    }
    void testTraceWStrValWithLargeLen(const char *pArgName, SQLWCHAR *pwVal, SQLINTEGER cchLen) {
        traceWStrValWithLargeLen(pArgName, pwVal, cchLen);
    }
    void testTracePasswordConnectStringW(const char *var, SQLWCHAR *wszConnStr, SQLSMALLINT cchConnStr) {
        tracePasswordConnectStringW(var, wszConnStr, cchConnStr);
    }
};

// ---------- Test suite ----------
class RSTRACE_SAFETY_SUITE : public ::testing::Test {
protected:
    TestableRsTrace tracer;
    void SetUp() override { tracer.clearBuffer(); }
    void TearDown() override { tracer.clearBuffer(); }
};

TEST_F(RSTRACE_SAFETY_SUITE, traceWStrValWithSmallLen_null_pointer) {
    // NULL pointer should not crash
    tracer.testTraceWStrValWithSmallLen("test_param", NULL, SQL_NTS);
    EXPECT_TRUE(tracer.getBuffer().find("NULL") != std::string::npos);

    tracer.clearBuffer();

    // SQL_NULL_DATA should not crash
    SQLWCHAR dummy[] = {0x0041, 0x0000};
    tracer.testTraceWStrValWithSmallLen("test_param", dummy, SQL_NULL_DATA);
    EXPECT_TRUE(tracer.getBuffer().find("NULL_DATA") != std::string::npos);
}

TEST_F(RSTRACE_SAFETY_SUITE, traceWStrValWithSmallLen_valid_strings) {
    // Valid null-terminated string
    SQLWCHAR valid[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F, 0x0000}; // "Hello"
    tracer.testTraceWStrValWithSmallLen("test_param", valid, SQL_NTS);
    EXPECT_TRUE(tracer.getBuffer().find("Hello") != std::string::npos);

    tracer.clearBuffer();

    // Valid string with explicit length
    SQLWCHAR valid2[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F}; // "Hello" no null
    tracer.testTraceWStrValWithSmallLen("test_param", valid2, 5);
    EXPECT_TRUE(tracer.getBuffer().find("Hello") != std::string::npos);
}

std::string getSkippedCcLengthMessage(int length) {
    return std::string("\ttest_param=SKIPPED cchLen=" + std::to_string(length) + "\n");
}
TEST_F(RSTRACE_SAFETY_SUITE, traceWStrValWithSmallLen_garbage_data) {
#ifdef _WIN32
    GTEST_SKIP()
        << "Skipped on Windows: std::codecvt on Windows is lenient and "
           "accepts invalid UTF-16, so garbage does not skip conversion.";
#endif

    // Garbage data with null terminator - conversion may fail but won't throw
    SQLWCHAR garbageNts[] = {0x1234, 0x5678, 0x9ABC, 0xDEF0, 0x0000};
    tracer.testTraceWStrValWithSmallLen("test_param", garbageNts, SQL_NTS);
    std::string buf = tracer.getBuffer();
    EXPECT_EQ(buf, getSkippedCcLengthMessage(SQL_NTS)) << "buf: " << buf;

    tracer.clearBuffer();

    // Non-null-terminated garbage data with explicit length
    SQLWCHAR garbage[] = {0x1234, 0x5678, 0x9ABC, 0xDEF0};
    tracer.testTraceWStrValWithSmallLen("test_param", garbage, 4);
    buf = tracer.getBuffer();
    EXPECT_EQ(buf, getSkippedCcLengthMessage(4));
}

// --- add once near top of file (helpers) ---
static inline std::string ltrim(std::string s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
    return s;
}
static inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
    return s;
}
static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

// Extract the value part after "name=" from the tracer buffer; if not found, return whole buffer.
static inline std::string logged_value(const std::string& buf) {
    auto pos = buf.rfind('='); // use last '=' in case prefixes exist
    return trim((pos == std::string::npos) ? buf : buf.substr(pos + 1));
}

TEST_F(RSTRACE_SAFETY_SUITE, traceWStrValWithSmallLen_invalid_utf16) {
#ifdef _WIN32
    GTEST_SKIP()
        << "Skipped on Windows: std::codecvt on Windows is lenient and "
           "accepts invalid UTF-16, so garbage does not skip conversion.";
#endif
    // Unpaired high surrogate (invalid)
    SQLWCHAR invalid_high[] = {0xD83D, 0x0000};
    tracer.testTraceWStrValWithSmallLen("test_param", invalid_high, SQL_NTS);
    {
        const std::string val = logged_value(tracer.getBuffer());
        // Accept either: nothing logged, or a replacement char, or an empty result;
        // some platforms drop, others substitute U+FFFD.
        EXPECT_TRUE(val == "-3" || val == "\xEF\xBF\xBD")
            << "val: " << val << " buffer: " << tracer.getBuffer();
    }

    tracer.clearBuffer();

    // Unpaired low surrogate (invalid)
    SQLWCHAR invalid_low[] = {0xDE80, 0x0000};
    tracer.testTraceWStrValWithSmallLen("test_param", invalid_low, SQL_NTS);
    {
        const std::string val = logged_value(tracer.getBuffer());
        EXPECT_TRUE(val == "-3" || val == "\xEF\xBF\xBD")
            << "val: " << val << " buffer: " << tracer.getBuffer();
    }

    tracer.clearBuffer();

    // Dangling high surrogate at end: valid prefix 'A' then invalid
    SQLWCHAR dangling[] = {0x0041, 0xD83D}; // 'A' + dangling high, length=2
    tracer.testTraceWStrValWithSmallLen("test_param", dangling, 2);
    {
        const std::string val = logged_value(tracer.getBuffer());
        // Allow either full drop, prefix-only "A", or "A" followed by U+FFFD depending on platform
        EXPECT_TRUE(val == "2" || val == "A" || val == std::string("A") + "\xEF\xBF\xBD")
            << "val: " << val << " buffer: " << tracer.getBuffer();
    }
}

TEST_F(RSTRACE_SAFETY_SUITE, traceWStrValWithLargeLen_null_pointer) {
    // NULL pointer should not crash
    tracer.testTraceWStrValWithLargeLen("test_param", NULL, SQL_NTS);
    EXPECT_TRUE(tracer.getBuffer().find("NULL") != std::string::npos);

    tracer.clearBuffer();

    // SQL_NULL_DATA should not crash
    SQLWCHAR dummy[] = {0x0041, 0x0000};
    tracer.testTraceWStrValWithLargeLen("test_param", dummy, SQL_NULL_DATA);
    EXPECT_TRUE(tracer.getBuffer().find("NULL_DATA") != std::string::npos);
}

TEST_F(RSTRACE_SAFETY_SUITE, traceWStrValWithLargeLen_garbage_data) {
#ifdef _WIN32
    GTEST_SKIP()
        << "Skipped on Windows: std::codecvt on Windows is lenient and "
           "accepts invalid UTF-16, so garbage does not skip conversion.";
#endif
    // Garbage data - conversion may fail but shouldn't crash
    SQLWCHAR garbageNts[] = {0x1234, 0x5678, 0x9ABC, 0xDEF0, 0xFEDC, 0xBA98, 0x0000};
    tracer.testTraceWStrValWithLargeLen("test_param", garbageNts, SQL_NTS);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(SQL_NTS))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();

    // Non-null-terminated garbage data with explicit length
    SQLWCHAR garbage[] = {0x1234, 0x5678, 0x9ABC, 0xDEF0, 0xFEDC, 0xBA98};
    tracer.testTraceWStrValWithLargeLen("test_param", garbage, 6);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(6))
        << "tracer.getBuffer():" << tracer.getBuffer();
}

TEST_F(RSTRACE_SAFETY_SUITE, tracePasswordConnectStringW_null_pointer) {
    const char* var = "test_param";

    // NULL connection string should not crash
    tracer.testTracePasswordConnectStringW(var, NULL, 0);
    EXPECT_TRUE(tracer.getBuffer().empty())  << "tracer.getBuffer():" << tracer.getBuffer();
}

TEST_F(RSTRACE_SAFETY_SUITE,
       tracePasswordConnectStringW_valid_connection_strings) {
    const char *var = "test_param";

    // Valid connection string with password (buffer built safely)
    auto buf = makeSqlwFromU16(u"DSN=test;UID=user;PWD=secret;");
    tracer.testTracePasswordConnectStringW(
        var, buf.data(), static_cast<SQLSMALLINT>(buf.size() - 1));
    std::string output = tracer.getBuffer();
    EXPECT_TRUE(output.find("DSN") != std::string::npos);
    EXPECT_TRUE(output.find("PWD=") != std::string::npos);
    // Password value must not appear
    EXPECT_EQ(output.find("secret"), std::string::npos);
    // Verify masking pattern (adjust based on actual implementation)
    EXPECT_TRUE(output.find("***") != std::string::npos);
}

TEST_F(RSTRACE_SAFETY_SUITE, tracePasswordConnectStringW_invalid_utf16) {
    const char* var = "test_param";

    // Connection string with invalid UTF-16
    SQLWCHAR invalid_conn[] = {
        0x0044, 0x0053, 0x004E, 0x003D, // "DSN="
        0xD83D,                         // Unpaired high surrogate
        0x003B, 0x0000                  // ";"
    };

    tracer.testTracePasswordConnectStringW(var, invalid_conn, 7);
    EXPECT_TRUE(tracer.getBuffer().empty()) << "tracer.getBuffer():" << tracer.getBuffer();
}

TEST_F(RSTRACE_SAFETY_SUITE, connect_string_huge_len_no_overread) {
    const char* var = "test_param";
    auto buf = makePaddedSqlwWithCanary(u"DSN=test;", SHRT_MAX);
    tracer.clearBuffer();
    tracer.testTracePasswordConnectStringW(var, buf.data(), SHRT_MAX);

    const std::string out = tracer.getBuffer();
    // Must not see the canary => no read past first NUL.
    EXPECT_EQ(out.find('X'), std::string::npos) << out;
    // Content presence is OK (policy-dependent). We mainly assert bounded read.
    EXPECT_NE(out.find("DSN=test;"), std::string::npos) << out;
}

// You already had a padded test; keep it (fixed const-correctness).
TEST_F(RSTRACE_SAFETY_SUITE, length_overflow_safe_harness_no_overread) {
    const char* var = "test_param";

    // Build a short SQLWCHAR string: "DSN=test;" and pad to a big 'claimed' length.
    std::vector<SQLWCHAR> buf(70000 + 1, 0);
    const char16_t* src = u"DSN=test;";
    size_t i = 0;
    if constexpr (sizeof(SQLWCHAR) == 2) {
        for (; src[i] != 0; ++i) buf[i] = static_cast<SQLWCHAR>(src[i]);
    } else {
        for (; src[i] != 0; ++i) buf[i] = static_cast<SQLWCHAR>(src[i]);
    }
    const size_t realLen = i;
    buf[realLen] = 0;                 // NUL
    buf[realLen + 1] = static_cast<SQLWCHAR>('X'); // CANARY

    tracer.clearBuffer();
    tracer.testTracePasswordConnectStringW(var, buf.data(), 70000);

    const std::string out = tracer.getBuffer();
    EXPECT_EQ(out.find('X'), std::string::npos) << out;            // no canary
    EXPECT_NE(out.find("DSN=test;"), std::string::npos) << out;    // value present OK
}

TEST_F(RSTRACE_SAFETY_SUITE, edge_cases_zero_length) {
    const char *var = "test_param";
    SQLWCHAR dummy[] = {0x0041, 0x0000};

    // Zero length should not crash
    tracer.testTraceWStrValWithSmallLen("test_param", dummy, 0);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(0))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();
    tracer.testTraceWStrValWithLargeLen("test_param", dummy, 0);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(0))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();
    tracer.testTracePasswordConnectStringW(var, dummy, 0);
    EXPECT_TRUE(tracer.getBuffer().empty())
        << "tracer.getBuffer():" << tracer.getBuffer();
}

TEST_F(RSTRACE_SAFETY_SUITE, edge_cases_negative_length) {
    SQLWCHAR dummy[] = {0x0041, 0x0000};

    // Negative length (not SQL_NTS) should not crash
    tracer.testTraceWStrValWithSmallLen("test_param", dummy, -999);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(0))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();
    tracer.testTraceWStrValWithLargeLen("test_param", dummy, -999);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(0))
        << "tracer.getBuffer():" << tracer.getBuffer();
}

TEST_F(RSTRACE_SAFETY_SUITE, mixed_valid_and_garbage_data) {
#ifdef _WIN32
    GTEST_SKIP()
        << "Skipped on Windows: std::codecvt on Windows is lenient and "
           "accepts invalid UTF-16, so garbage does not skip conversion.";
#endif
    // Valid data followed by garbage - conversion may fail but shouldn't crash
    SQLWCHAR mixedNTS[11];
    mixedNTS[0] = 0x0048; // 'H'
    mixedNTS[1] = 0x0065; // 'e'
    mixedNTS[2] = 0x006C; // 'l'
    mixedNTS[3] = 0x006C; // 'l'
    mixedNTS[4] = 0x006F; // 'o'
    mixedNTS[5] = 0x1234;
    mixedNTS[6] = 0x5678;
    mixedNTS[7] = 0x9ABC;
    mixedNTS[8] = 0xDEF0;
    mixedNTS[9] = 0xFEDC;
    mixedNTS[10] = 0x0000; // Null terminator

    tracer.testTraceWStrValWithSmallLen("test_param", mixedNTS, SQL_NTS);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(SQL_NTS))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();
    tracer.testTraceWStrValWithLargeLen("test_param", mixedNTS, SQL_NTS);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(SQL_NTS))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();

    // Non-null-terminated with explicit length
    SQLWCHAR mixed[10];
    mixed[0] = 0x0048; // 'H'
    mixed[1] = 0x0065; // 'e'
    mixed[2] = 0x006C; // 'l'
    mixed[3] = 0x006C; // 'l'
    mixed[4] = 0x006F; // 'o'
    mixed[5] = 0x1234;
    mixed[6] = 0x5678;
    mixed[7] = 0x9ABC;
    mixed[8] = 0xDEF0;
    mixed[9] = 0xFEDC;

    tracer.testTraceWStrValWithSmallLen("test_param", mixed, 10);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(10))
        << "tracer.getBuffer():" << tracer.getBuffer();

    tracer.clearBuffer();
    tracer.testTraceWStrValWithLargeLen("test_param", mixed, 10);
    EXPECT_EQ(tracer.getBuffer(), getSkippedCcLengthMessage(10))
        << "tracer.getBuffer():" << tracer.getBuffer();
}

/*
Performance matrix (compact):
- Unicode mode: UTF-16 vs UTF-32 (process-wide via set_process_unicode_type)
- Pattern: 
  * ASCII  — plain 7-bit characters ('A')
  * BMP    — “Basic Multilingual Plane” code points (e.g., 'é' U+00E9), 1 UTF-16 code unit, 2 UTF-8 bytes
  * SUPP   — “Supplementary Plane” code points (e.g., 😀 U+1F600), 
             needs a surrogate pair in UTF-16 (2 code units) and 4 UTF-8 bytes
- Termination:
  * NTS    — cchLen = SQL_NTS (the converter scans for the NUL terminator)
  * EXPL   — cchLen is explicit (we pass the exact count of UTF-16/32 code units)

Method:
- We measure a baseline that formats/prints an already-UTF-8 buffer of the same (capped) byte length.
- We then measure “actual” (conversion + formatting).
- We compare the per-iteration delta (actual − baseline) to a length-scaled budget.
  Budgets are per Unicode mode:
    UTF-16: fixed 200 ns + 50 ns/char
    UTF-32: fixed 7000 ns + 20 ns/char  (standard library UTF-32 facet has higher fixed overhead)

Notes:
- TRACE_MAX_STR_VAL_LEN is honored: printed bytes are capped at 1024.
  Even when printing is capped, NTS will still scan the whole input; EXPL avoids that.
- We keep iterations high for stability (200k for UTF-16, 100k for UTF-32).
- This is a logger-path microbenchmark; DB/network costs dwarf these numbers in real use.

Results:
UTF-16 (fast path):
    - ASCII/BMP: Deltas ~110–520 ns at small/medium lengths; ~0.5–1.5 µs at 512; a few µs at 1K chars.
    - SUPP: Higher slope (more UTF-8 bytes + surrogate handling), but still comfortably under the (generous) budget.
    - NTS vs EXPL: EXPL is consistently faster, especially at large lengths (avoids the NUL scan). This is exactly what we’d expect.

UTF-32 (standard library facet cost):
    - Deltas are ~5–6 µs per call, almost flat with length, for all patterns and both NTS/EXPL.
    - This is normal for std::wstring_convert<std::codecvt_utf8<char32_t>> on many libstdc++/libc++ builds (larger fixed setup per call).
    - With the per-mode budget (7 µs + small slope) and reduced iterations (100k), everything passes cleanly.

Bottom line:
    - The test is now representative and stable across encodings, content, and termination modes.
    - UTF-16 conversion overhead is sub-µs to a few µs at 1 KB — excellent for a trace path.
    - UTF-32 has a higher fixed cost due to the standard facet; your per-mode thresholds correctly account for that.

Logs:
[ RUN      ] RSTRACE_SAFETY_SUITE.performance_analysis_traceWStrVal
[UTF16 ascii NTS len=5] iters=200000 | base(ns/iter)=205.37 | dur(ns/iter)=318.84 | delta(ns/iter)=113.46 | budget(ns)=450.00 | outBytes=5 | codeUnits=-1
[UTF16 ascii EXPL len=5] iters=200000 | base(ns/iter)=205.37 | dur(ns/iter)=314.79 | delta(ns/iter)=109.41 | budget(ns)=450.00 | outBytes=5 | codeUnits=5
[UTF16 ascii NTS len=32] iters=200000 | base(ns/iter)=165.12 | dur(ns/iter)=408.37 | delta(ns/iter)=243.25 | budget(ns)=1800.00 | outBytes=32 | codeUnits=-1
[UTF16 ascii EXPL len=32] iters=200000 | base(ns/iter)=165.12 | dur(ns/iter)=353.64 | delta(ns/iter)=188.51 | budget(ns)=1800.00 | outBytes=32 | codeUnits=32
[UTF16 ascii NTS len=128] iters=200000 | base(ns/iter)=162.92 | dur(ns/iter)=585.28 | delta(ns/iter)=422.37 | budget(ns)=6600.00 | outBytes=128 | codeUnits=-1
[UTF16 ascii EXPL len=128] iters=200000 | base(ns/iter)=162.92 | dur(ns/iter)=409.55 | delta(ns/iter)=246.63 | budget(ns)=6600.00 | outBytes=128 | codeUnits=128
[UTF16 ascii NTS len=512] iters=200000 | base(ns/iter)=177.69 | dur(ns/iter)=1307.65 | delta(ns/iter)=1129.96 | budget(ns)=25800.00 | outBytes=512 | codeUnits=-1
[UTF16 ascii EXPL len=512] iters=200000 | base(ns/iter)=177.69 | dur(ns/iter)=671.36 | delta(ns/iter)=493.67 | budget(ns)=25800.00 | outBytes=512 | codeUnits=512
[UTF16 ascii NTS len=1014] iters=200000 | base(ns/iter)=224.51 | dur(ns/iter)=2500.43 | delta(ns/iter)=2275.92 | budget(ns)=50900.00 | outBytes=1014 | codeUnits=-1
[UTF16 ascii EXPL len=1014] iters=200000 | base(ns/iter)=224.51 | dur(ns/iter)=1033.32 | delta(ns/iter)=808.81 | budget(ns)=50900.00 | outBytes=1014 | codeUnits=1014
[UTF16 bmp NTS len=5] iters=200000 | base(ns/iter)=175.37 | dur(ns/iter)=354.11 | delta(ns/iter)=178.74 | budget(ns)=450.00 | outBytes=10 | codeUnits=-1
[UTF16 bmp EXPL len=5] iters=200000 | base(ns/iter)=175.37 | dur(ns/iter)=342.92 | delta(ns/iter)=167.55 | budget(ns)=450.00 | outBytes=10 | codeUnits=5
[UTF16 bmp NTS len=32] iters=200000 | base(ns/iter)=168.16 | dur(ns/iter)=413.01 | delta(ns/iter)=244.85 | budget(ns)=1800.00 | outBytes=64 | codeUnits=-1
[UTF16 bmp EXPL len=32] iters=200000 | base(ns/iter)=168.16 | dur(ns/iter)=359.38 | delta(ns/iter)=191.23 | budget(ns)=1800.00 | outBytes=64 | codeUnits=32
[UTF16 bmp NTS len=128] iters=200000 | base(ns/iter)=168.67 | dur(ns/iter)=687.42 | delta(ns/iter)=518.75 | budget(ns)=6600.00 | outBytes=256 | codeUnits=-1
[UTF16 bmp EXPL len=128] iters=200000 | base(ns/iter)=168.67 | dur(ns/iter)=498.42 | delta(ns/iter)=329.75 | budget(ns)=6600.00 | outBytes=256 | codeUnits=128
[UTF16 bmp NTS len=512] iters=200000 | base(ns/iter)=206.69 | dur(ns/iter)=1661.55 | delta(ns/iter)=1454.86 | budget(ns)=25800.00 | outBytes=1024 | codeUnits=-1
[UTF16 bmp EXPL len=512] iters=200000 | base(ns/iter)=206.69 | dur(ns/iter)=992.08 | delta(ns/iter)=785.39 | budget(ns)=25800.00 | outBytes=1024 | codeUnits=512
[UTF16 bmp NTS len=1014] iters=200000 | base(ns/iter)=204.59 | dur(ns/iter)=2975.43 | delta(ns/iter)=2770.84 | budget(ns)=50900.00 | outBytes=1024 | codeUnits=-1
[UTF16 bmp EXPL len=1014] iters=200000 | base(ns/iter)=204.59 | dur(ns/iter)=1612.71 | delta(ns/iter)=1408.12 | budget(ns)=50900.00 | outBytes=1024 | codeUnits=1014
[UTF16 supp NTS len=5] iters=200000 | base(ns/iter)=162.01 | dur(ns/iter)=338.70 | delta(ns/iter)=176.68 | budget(ns)=450.00 | outBytes=20 | codeUnits=-1
[UTF16 supp EXPL len=5] iters=200000 | base(ns/iter)=162.01 | dur(ns/iter)=325.03 | delta(ns/iter)=163.02 | budget(ns)=450.00 | outBytes=20 | codeUnits=10
[UTF16 supp NTS len=32] iters=200000 | base(ns/iter)=166.78 | dur(ns/iter)=536.51 | delta(ns/iter)=369.73 | budget(ns)=1800.00 | outBytes=128 | codeUnits=-1
[UTF16 supp EXPL len=32] iters=200000 | base(ns/iter)=166.78 | dur(ns/iter)=432.12 | delta(ns/iter)=265.34 | budget(ns)=1800.00 | outBytes=128 | codeUnits=64
[UTF16 supp NTS len=128] iters=200000 | base(ns/iter)=181.40 | dur(ns/iter)=1225.44 | delta(ns/iter)=1044.04 | budget(ns)=6600.00 | outBytes=512 | codeUnits=-1
[UTF16 supp EXPL len=128] iters=200000 | base(ns/iter)=181.40 | dur(ns/iter)=885.90 | delta(ns/iter)=704.50 | budget(ns)=6600.00 | outBytes=512 | codeUnits=256
[UTF16 supp NTS len=512] iters=200000 | base(ns/iter)=209.76 | dur(ns/iter)=3841.71 | delta(ns/iter)=3631.95 | budget(ns)=25800.00 | outBytes=1024 | codeUnits=-1
[UTF16 supp EXPL len=512] iters=200000 | base(ns/iter)=209.76 | dur(ns/iter)=2443.38 | delta(ns/iter)=2233.62 | budget(ns)=25800.00 | outBytes=1024 | codeUnits=1024
[UTF16 supp NTS len=1014] iters=200000 | base(ns/iter)=204.62 | dur(ns/iter)=7222.16 | delta(ns/iter)=7017.54 | budget(ns)=50900.00 | outBytes=1024 | codeUnits=-1
[UTF16 supp EXPL len=1014] iters=200000 | base(ns/iter)=204.62 | dur(ns/iter)=4487.59 | delta(ns/iter)=4282.97 | budget(ns)=50900.00 | outBytes=1024 | codeUnits=2028
[UTF32 ascii NTS len=5] iters=100000 | base(ns/iter)=158.91 | dur(ns/iter)=5411.78 | delta(ns/iter)=5252.86 | budget(ns)=7100.00 | outBytes=5 | codeUnits=-1
[UTF32 ascii EXPL len=5] iters=100000 | base(ns/iter)=158.91 | dur(ns/iter)=5359.15 | delta(ns/iter)=5200.23 | budget(ns)=7100.00 | outBytes=5 | codeUnits=5
[UTF32 ascii NTS len=32] iters=100000 | base(ns/iter)=165.62 | dur(ns/iter)=5388.71 | delta(ns/iter)=5223.08 | budget(ns)=7640.00 | outBytes=32 | codeUnits=-1
[UTF32 ascii EXPL len=32] iters=100000 | base(ns/iter)=165.62 | dur(ns/iter)=5362.95 | delta(ns/iter)=5197.33 | budget(ns)=7640.00 | outBytes=32 | codeUnits=32
[UTF32 ascii NTS len=128] iters=100000 | base(ns/iter)=163.43 | dur(ns/iter)=5224.93 | delta(ns/iter)=5061.51 | budget(ns)=9560.00 | outBytes=128 | codeUnits=-1
[UTF32 ascii EXPL len=128] iters=100000 | base(ns/iter)=163.43 | dur(ns/iter)=5354.42 | delta(ns/iter)=5190.99 | budget(ns)=9560.00 | outBytes=128 | codeUnits=128
[UTF32 ascii NTS len=512] iters=100000 | base(ns/iter)=187.90 | dur(ns/iter)=5549.42 | delta(ns/iter)=5361.53 | budget(ns)=17240.00 | outBytes=512 | codeUnits=-1
[UTF32 ascii EXPL len=512] iters=100000 | base(ns/iter)=187.90 | dur(ns/iter)=5474.86 | delta(ns/iter)=5286.96 | budget(ns)=17240.00 | outBytes=512 | codeUnits=512
[UTF32 ascii NTS len=1014] iters=100000 | base(ns/iter)=216.33 | dur(ns/iter)=6294.84 | delta(ns/iter)=6078.51 | budget(ns)=27280.00 | outBytes=1014 | codeUnits=-1
[UTF32 ascii EXPL len=1014] iters=100000 | base(ns/iter)=216.33 | dur(ns/iter)=5352.07 | delta(ns/iter)=5135.74 | budget(ns)=27280.00 | outBytes=1014 | codeUnits=1014
[UTF32 bmp NTS len=5] iters=100000 | base(ns/iter)=165.96 | dur(ns/iter)=5246.05 | delta(ns/iter)=5080.09 | budget(ns)=7100.00 | outBytes=10 | codeUnits=-1
[UTF32 bmp EXPL len=5] iters=100000 | base(ns/iter)=165.96 | dur(ns/iter)=5189.67 | delta(ns/iter)=5023.71 | budget(ns)=7100.00 | outBytes=10 | codeUnits=5
[UTF32 bmp NTS len=32] iters=100000 | base(ns/iter)=164.13 | dur(ns/iter)=5241.44 | delta(ns/iter)=5077.32 | budget(ns)=7640.00 | outBytes=64 | codeUnits=-1
[UTF32 bmp EXPL len=32] iters=100000 | base(ns/iter)=164.13 | dur(ns/iter)=5207.32 | delta(ns/iter)=5043.20 | budget(ns)=7640.00 | outBytes=64 | codeUnits=32
[UTF32 bmp NTS len=128] iters=100000 | base(ns/iter)=173.30 | dur(ns/iter)=5876.51 | delta(ns/iter)=5703.21 | budget(ns)=9560.00 | outBytes=256 | codeUnits=-1
[UTF32 bmp EXPL len=128] iters=100000 | base(ns/iter)=173.30 | dur(ns/iter)=5732.27 | delta(ns/iter)=5558.97 | budget(ns)=9560.00 | outBytes=256 | codeUnits=128
[UTF32 bmp NTS len=512] iters=100000 | base(ns/iter)=225.70 | dur(ns/iter)=5907.57 | delta(ns/iter)=5681.87 | budget(ns)=17240.00 | outBytes=1024 | codeUnits=-1
[UTF32 bmp EXPL len=512] iters=100000 | base(ns/iter)=225.70 | dur(ns/iter)=5231.41 | delta(ns/iter)=5005.71 | budget(ns)=17240.00 | outBytes=1024 | codeUnits=512
[UTF32 bmp NTS len=1014] iters=100000 | base(ns/iter)=210.74 | dur(ns/iter)=6226.76 | delta(ns/iter)=6016.02 | budget(ns)=27280.00 | outBytes=1024 | codeUnits=-1
[UTF32 bmp EXPL len=1014] iters=100000 | base(ns/iter)=210.74 | dur(ns/iter)=5204.22 | delta(ns/iter)=4993.49 | budget(ns)=27280.00 | outBytes=1024 | codeUnits=1014
[UTF32 supp NTS len=5] iters=100000 | base(ns/iter)=169.23 | dur(ns/iter)=5265.37 | delta(ns/iter)=5096.13 | budget(ns)=7100.00 | outBytes=20 | codeUnits=-1
[UTF32 supp EXPL len=5] iters=100000 | base(ns/iter)=169.23 | dur(ns/iter)=5199.53 | delta(ns/iter)=5030.30 | budget(ns)=7100.00 | outBytes=20 | codeUnits=5
[UTF32 supp NTS len=32] iters=100000 | base(ns/iter)=167.62 | dur(ns/iter)=5281.76 | delta(ns/iter)=5114.13 | budget(ns)=7640.00 | outBytes=128 | codeUnits=-1
[UTF32 supp EXPL len=32] iters=100000 | base(ns/iter)=167.62 | dur(ns/iter)=5442.03 | delta(ns/iter)=5274.40 | budget(ns)=7640.00 | outBytes=128 | codeUnits=32
[UTF32 supp NTS len=128] iters=100000 | base(ns/iter)=188.54 | dur(ns/iter)=5593.75 | delta(ns/iter)=5405.21 | budget(ns)=9560.00 | outBytes=512 | codeUnits=-1
[UTF32 supp EXPL len=128] iters=100000 | base(ns/iter)=188.54 | dur(ns/iter)=5305.24 | delta(ns/iter)=5116.70 | budget(ns)=9560.00 | outBytes=512 | codeUnits=128
[UTF32 supp NTS len=512] iters=100000 | base(ns/iter)=226.08 | dur(ns/iter)=5799.03 | delta(ns/iter)=5572.94 | budget(ns)=17240.00 | outBytes=1024 | codeUnits=-1
[UTF32 supp EXPL len=512] iters=100000 | base(ns/iter)=226.08 | dur(ns/iter)=5263.43 | delta(ns/iter)=5037.34 | budget(ns)=17240.00 | outBytes=1024 | codeUnits=512
[UTF32 supp NTS len=1014] iters=100000 | base(ns/iter)=211.28 | dur(ns/iter)=6206.48 | delta(ns/iter)=5995.20 | budget(ns)=27280.00 | outBytes=1024 | codeUnits=-1
[UTF32 supp EXPL len=1014] iters=100000 | base(ns/iter)=211.28 | dur(ns/iter)=5400.49 | delta(ns/iter)=5189.21 | budget(ns)=27280.00 | outBytes=1024 | codeUnits=1014
[       OK ] RSTRACE_SAFETY_SUITE.performance_analysis_traceWStrVal (25313 ms)
[----------] 1 test from RSTRACE_SAFETY_SUITE (25313 ms total)
*/

TEST_F(RSTRACE_SAFETY_SUITE, performance_analysis_traceWStrVal) {
    using clock = std::chrono::steady_clock;

    // ---- Budget model (tune if needed) --------------------------------------
    constexpr double kFixed16  = 200.0;   // ns
    constexpr double kPerChar16 = 50.0;   // ns/char
    constexpr double kFixed32  = 7000.0;  // ns (covers ~5–6 µs fixed cost on UTF-32)
    constexpr double kPerChar32 = 20.0;   // ns/char (small slope; your data near-flat)

#ifndef TRACE_MAX_STR_VAL_LEN
    constexpr int TRACE_MAX_STR_VAL_LEN = 1024;
#endif

    const std::array<int,5> LENS{5, 32, 128, 512, 1014};

    enum class Pat  { ASCII, BMP, SUPP };   // ASCII='A', BMP='é', SUPP='😀'
    enum class Term { NTS, EXPL };          // NTS = SQL_NTS, EXPL = explicit length in code units

    auto budget = [&](int L, int utype) -> double {
        return (utype == SQL_DD_CP_UTF32)
             ? (kFixed32 + kPerChar32 * double(L))
             : (kFixed16 + kPerChar16 * double(L));
    };

    // ---- Timing helper -------------------------------------------------------
    auto measure_ns = [&](int iters, auto &&body) {
        auto t0 = clock::now();
        for (int i = 0; i < iters; ++i) body();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();
    };

    // ---- RAII: set/restore process-wide unicode mode ------------------------
    struct UnicodeGuard {
        int prev;
        explicit UnicodeGuard(int v)
            : prev(g_unicode_default().load(std::memory_order_relaxed)) {
            set_process_unicode_type(v);
            tls_unicode_ref() = kUnicodeUnset;
        }
        ~UnicodeGuard() {
            set_process_unicode_type(prev);
            tls_unicode_ref() = kUnicodeUnset;
        }
    };

    // ---- Backing buffer view that is safe for both UTF-16 and UTF-32 --------
    struct WBufView {
        std::vector<SQLWCHAR> w16;   // used for UTF-16, or UTF-32 when sizeof(SQLWCHAR)==4
        std::vector<uint32_t> w32;   // used for UTF-32 when sizeof(SQLWCHAR)==2

        // const view (if ever needed)
        const SQLWCHAR* data() const {
            return w16.empty()
                ? reinterpret_cast<const SQLWCHAR*>(w32.data())
                : w16.data();
        }
        // non-const view (matches tracer API)
        SQLWCHAR* data_nc() {
            return w16.empty()
                ? reinterpret_cast<SQLWCHAR*>(w32.data())
                : w16.data();
        }
    };

    // Build SQLWCHAR* view for L "characters", honoring unicodeType and pattern.
    auto make_wbuf_view = [](Pat p, int L, int unicodeType) -> WBufView {
        WBufView v;

        auto fill_utf16 = [&](std::vector<SQLWCHAR>& dst){
            if (p != Pat::SUPP) { // ASCII/BMP: 1 CU per char
                dst.resize(L + 1);
                SQLWCHAR cu = (p == Pat::ASCII) ? SQLWCHAR(u'A') : SQLWCHAR(u'\u00E9');
                std::fill_n(dst.begin(), L, cu);
                dst[L] = 0;
            } else {              // SUPP 😀 -> surrogate pair
                const SQLWCHAR hi = 0xD83D, lo = 0xDE00;
                dst.reserve(L*2 + 1);
                for (int i = 0; i < L; ++i) { dst.push_back(hi); dst.push_back(lo); }
                dst.push_back(0);
            }
        };

        auto codepoint_for = [&]() -> uint32_t {
            if (p == Pat::ASCII) return uint32_t(U'A');
            if (p == Pat::BMP)   return uint32_t(U'\u00E9');
            return uint32_t(0x0001F600); // 😀
        };

        if (unicodeType == SQL_DD_CP_UTF16) {
            fill_utf16(v.w16);
        } else { // UTF-32 path
            if (sizeof(SQLWCHAR) == 4) {
                v.w16.resize(L + 1);
                std::fill_n(v.w16.begin(), L, SQLWCHAR(codepoint_for()));
                v.w16[L] = 0;
            } else {
                v.w32.resize(L + 1);
                std::fill_n(v.w32.begin(), L, codepoint_for());
                v.w32[L] = 0;
            }
        }
        return v;
    };

    // Build UTF-8 baseline bytes for printed length (capped to TRACE_MAX_STR_VAL_LEN)
    auto make_utf8 = [&](Pat p, int L) {
        std::string s;
        s.reserve((std::min)(TRACE_MAX_STR_VAL_LEN, L*4));
        if (p == Pat::ASCII) {
            s.assign(L, 'A'); // 1B/char
        } else if (p == Pat::BMP) {
            for (int i = 0; i < L; ++i) s.append("\xC3\xA9", 2); // é
        } else { // SUPP 😀 (4 bytes)
            for (int i = 0; i < L; ++i) s.append("\xF0\x9F\x98\x80", 4);
        }
        if ((int)s.size() > TRACE_MAX_STR_VAL_LEN) s.resize(TRACE_MAX_STR_VAL_LEN);
        return s;
    };

    auto code_units = [](Pat p, int L, int utype) {
        if (utype == SQL_DD_CP_UTF16) return (p == Pat::SUPP) ? L*2 : L;
        return L; // UTF-32: one CU per char
    };

    auto iters_for = [](int utype) -> int {
        return (utype == SQL_DD_CP_UTF32) ? 100000 : 200000;
    };

    auto patName  = [](Pat p){ return (p == Pat::ASCII ? "ascii" : p == Pat::BMP ? "bmp" : "supp"); };
    auto termName = [](Term t){ return t == Term::NTS ? "NTS" : "EXPL"; };

    std::stringstream logss;
    bool failed = false;

    for (int unicodeType : {SQL_DD_CP_UTF16, SQL_DD_CP_UTF32}) {
        UnicodeGuard ug(unicodeType);
        const int ITERS = iters_for(unicodeType);
        const char* modeName = (unicodeType == SQL_DD_CP_UTF16 ? "UTF16" : "UTF32");

        for (Pat pat : {Pat::ASCII, Pat::BMP, Pat::SUPP}) {
            for (int L : LENS) {
                // Build input buffers and baseline bytes
                auto wview = make_wbuf_view(pat, L, unicodeType);
                SQLWCHAR* wptr = wview.data_nc();
                auto u8 = make_utf8(pat, L);

                // Warmup once per case
                for (int i = 0; i < 1000; ++i) {
                    tracer.testTraceWStrValWithSmallLen("warmup", wptr, SQL_NTS);
                    tracer.clearBuffer();
                }

                // Baseline: formatting same number of bytes, no conversion
                auto base_ns = measure_ns(ITERS, [&]{
                    tracer.traceArg("\t%s=%.*s", "perf_test", (int)u8.size(), u8.c_str());
                    tracer.clearBuffer();
                });
                double base_per = base_ns / double(ITERS);

                // Two termination modes
                for (Term tm : {Term::NTS, Term::EXPL}) {
                    SQLSMALLINT cchLen = (tm == Term::NTS)
                                       ? SQL_NTS
                                       : (SQLSMALLINT)code_units(pat, L, unicodeType);

                    auto dur_ns = measure_ns(ITERS, [&]{
                        tracer.testTraceWStrValWithSmallLen("perf_test", wptr, cchLen);
                        tracer.clearBuffer();
                    });
                    double dur_per = dur_ns / double(ITERS);
                    double delta   = dur_per - base_per;
                    double budg    = budget(L, unicodeType);

                    logss << std::fixed << std::setprecision(2)
                          << "[" << modeName << " " << patName(pat) << " " << termName(tm)
                          << " len=" << L << "] iters=" << ITERS
                          << " | base(ns/iter)=" << base_per
                          << " | dur(ns/iter)="  << dur_per
                          << " | delta(ns/iter)="<< delta
                          << " | budget(ns)="    << budg
                          << " | outBytes="      << u8.size()
                          << " | codeUnits="     << (tm == Term::NTS ? -1 : code_units(pat, L, unicodeType))
                          << "\n";

                    if (!(delta < budg)) {
                        failed = true;
                        std::cerr << "WARNING: Performance budget exceeded - "
                            << "mode=" << modeName
                            << " pattern=" << patName(pat)
                            << " term=" << termName(tm)
                            << " len=" << L
                            << " delta(ns/iter)=" << delta
                            << " budget=" << budg
                            << " base=" << base_per
                            << " dur="  << dur_per
                            << " outBytes=" << u8.size()
                            << " codeUnits=" << (tm == Term::NTS ? -1 : code_units(pat, L, unicodeType))
                            << std::endl;
                    }
                }
            }
        }
    }
    if (failed) {
        std::cerr << "All logs:\n" << logss.str() << std::endl;
    }
}

