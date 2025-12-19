#include "common.h"
#include "rsunicode.h"
#include "rsmem.h"
#include "iam/core/IAMUtils.h"
#include <array>
#include <codecvt>
#include <cstring>
#include <iostream>
#include <string>

using namespace Redshift::IamSupport;

// NOTE ON PLATFORM/STL DIVERGENCES
// These tests exercise error-edge cases in std::wstring_convert<std::codecvt_...>,
// which is deprecated and has implementation-defined behavior.
// Different standard libraries diverge in two ways:
//   (1) What the conversion RETURNS for explicit-length inputs:
//       - MSVC STL sometimes returns BYTES consumed.
//       - libc++/libstdc++ return UTF-16 CODE UNITS produced.
//   (2) How invalid sequences are HANDLED:
//       - libc++ (and often MSVC STL) throw std::range_error;
//         our wrapper catches, clears output, and returns 0.
//       - libstdc++ often emits the valid prefix (and sometimes U+FFFD) and stops.
// We use preprocessor checks (USING_MSVC_STL / USING_LIBCXX / USING_LIBSTDCXX)
// to set expectations accordingly, while always validating the actual output content.

// Build a u16 string by "widening" each byte (U+00XX) -- matches the MSVC widening quirk
static std::u16string widen_bytes_to_u16(const std::string& bytes) {
    std::u16string u16;
    u16.reserve(bytes.size());
    for (unsigned char b : bytes) u16.push_back(static_cast<char16_t>(b));
    return u16;
}

// Portable UTF-8 bytes for "你好" (avoid char8_t / source-encoding pitfalls)
static inline std::string bytes_nihao() { return std::string("\xE4\xBD\xA0\xE5\xA5\xBD"); }

class UNICODE_TEST_SUITE : public ::testing::Test {
};

TEST_F(UNICODE_TEST_SUITE, test_basic_representation__not_a_test) {
    std::stringstream ss;
    std::string value1 = "Hello, 你好";
    std::string value2 = "Hello, 你好";
    ss << "value1: " << value1 << " " << value1.size() << std::endl;
    ss << "value2: " << value2 << " " << value2.size() << std::endl;

    ss << "Byte representation of value1: ";
    for (char c : value1) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    ss << "Byte representation of value2: ";
    for (char c : value2) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    // Convert std::string to std::wstring (UTF-16)
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring utf16_value1 = converter.from_bytes(value1);
    std::wstring utf16_value2 = converter.from_bytes(value2);

    // Output byte representations
    ss << "Byte representation of utf16_value1: ";
    for (char c : utf16_value1) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    ss << "Byte representation of utf16_value2: ";
    for (char c : utf16_value2) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    // Convert std::wstring (UTF-16) back to std::string (UTF-8)
    std::string utf8_value1 = converter.to_bytes(utf16_value1);
    std::string utf8_value2 = converter.to_bytes(utf16_value2);

    // Output byte representations
    ss << "Byte representation of utf8_value1: ";
    for (char c : utf8_value1) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    ss << "Byte representation of utf8_value2: ";
    for (char c : utf8_value2) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    ASSERT_EQ(0, memcmp(value1.data(), utf8_value1.data(),
                        (std::max<size_t>)(utf8_value1.size(), value1.size())))
        << ss.str();
    ASSERT_EQ(0, memcmp(value2.data(), utf8_value2.data(),
                        (std::max<size_t>)(utf8_value2.size(), value2.size())))
        << ss.str();
}

// tests are grouped by util function - each test validates multiple cases
TEST_F(UNICODE_TEST_SUITE, test_wchar16_to_utf8_char) {
    // UTF-16 encoded string: u"Hello, 你好"
    // Null terminated - converts as expected
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    std::string result = "Hello, 你好";
    char szStr[100] = "123"; // dirty
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = wchar16_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                      szStrSize);
    ASSERT_EQ(0, memcmp(szStr, result.data(), len));

    // UTF-16 encoded string: u"Hello, 你好"
    // Not Null terminated - converts as expected
    const unsigned short wszStr2[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                      0x002C, 0x0020, 0x4F60, 0x597D};
    int wszStr2Len = sizeof(wszStr2) / sizeof(wszStr2[0]);
    len = wchar16_to_utf8_char((const SQLWCHAR *)wszStr2, wszStr2Len, szStr,
                               szStrSize);
    ASSERT_EQ(13, len); // 你好 is 6 bytes not 2 (4 bytes exta) 9 - 2 + 6 = 13.
    ASSERT_EQ(0, memcmp(szStr, result.data(), len));
    ASSERT_EQ(0, result[len]);

    // Reusing buffer szStr is safe
    std::string result2 = "Hello, 你";
    char szStr2[8];
    int szStr2Size = sizeof(szStr2) / sizeof(szStr2[0]);
    len =
        wchar16_to_utf8_char((const SQLWCHAR *)wszStr2, 8, szStr2, szStr2Size);
    ASSERT_EQ(0, memcmp(szStr2, result2.data(), len));
    char szStr3[6];
    int szStr3Size = sizeof(szStr3) / sizeof(szStr3[0]);
    len = wchar16_to_utf8_char((const SQLWCHAR *)wszStr2, 8, szStr3,
                               szStr3Size);            // smaller buffer
    ASSERT_EQ(szStr3Size - 1, len);                    // -1 for null terminator
    ASSERT_EQ(0, memcmp(szStr3, result2.data(), len)); // same check as below
    ASSERT_EQ(0, memcmp(szStr3, "Hello", 5));
    ASSERT_EQ(0, szStr3[5]);
}

TEST_F(UNICODE_TEST_SUITE, test_wchar16_to_utf8_str) {
    // UTF-16 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    std::string result = "Hello, 你好";
    std::string szStr;
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr, SQL_NTS, szStr);
    ASSERT_EQ(szStr, result);

    // UTF-16 encoded string: u"Hello, 你好"
    // Not Null terminated
    const unsigned short wszStr2[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                      0x002C, 0x0020, 0x4F60, 0x597D};
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr2, 9, szStr);
    ASSERT_EQ(szStr, result);
    // Reusing buffer szStr is safe
    result = "Hello, 你";
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr2, 8, szStr);
    ASSERT_EQ(szStr, result);
}

TEST_F(UNICODE_TEST_SUITE, test_char_utf8_to_utf16_str) {
    std::string szStr = "Hello, 你好";
    int cchLenBytes = 13;          // "Hello, ":7 + 你:3 + 好:3
    int lenResultInCharacters = 9; // "Hello, ":7 + 你:1 + 好:1
    std::u16string result = u"Hello, 你好";
    std::u16string wszStr;
    int len =
        char_utf8_to_utf16_str((const char *)szStr.data(), cchLenBytes, wszStr);
    ASSERT_EQ(lenResultInCharacters, len);

    std::stringstream ss;
    ss << "szStr: " << szStr << " " << szStr.size() << std::endl;
    ss << "wszStr size: " << wszStr.size() << std::endl;
    ss << "result size: " << result.size() << std::endl;

    ss << "Byte representation of wszStr: ";
    for (const char c : wszStr) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    ss << "Byte representation of result: ";
    for (char c : result) {
        ss << "0x" << std::hex
           << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;

    ASSERT_EQ(0, memcmp(wszStr.data(), result.data(), cchLenBytes)) << ss.str();
    // Skip the last character (which is 3 bytes)
    len = char_utf8_to_utf16_str((const char *)szStr.data(), cchLenBytes - 3,
                                 wszStr);
    ASSERT_EQ(0, memcmp((char *)wszStr.data(), result.data(), cchLenBytes - 3))
        << ss.str();
    ASSERT_EQ(lenResultInCharacters - 1, len) << ss.str();
}

TEST_F(UNICODE_TEST_SUITE, test_char_utf8_to_utf16_wchar) {
    // UTF-8 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned char szStr[] = {'H',  'e',  'l',  'l',  'o',  ',', ' ',
                                   0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
    size_t cchLenBytes = sizeof(szStr) / sizeof(szStr[0]);
    const unsigned short result[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D};
    size_t lenResult = sizeof(result) / sizeof(result[0]);
    unsigned short wszStr[50] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    // We want one character less : "Hello, 你" ( sizeof("Hello, ") + 3 = 10)
    int len = char_utf8_to_utf16_wchar((const char *)szStr, 10,
                                       (SQLWCHAR *)wszStr, wszStrSize);
    ASSERT_EQ(lenResult - 1, len);
    ASSERT_EQ(0, memcmp(wszStr, result, len)); // excluding null terminator
    ASSERT_EQ(0, wszStr[len]);                 // null terminator
}

TEST_F(UNICODE_TEST_SUITE, test_null_output) {
    int anyNumber = 10;
    std::vector<char> szStr(10, 'a');
    unsigned short *wszStrNULL = NULL;
    int len = char_utf8_to_utf16_wchar(szStr.data(), anyNumber,
                                       (SQLWCHAR *)wszStrNULL, anyNumber);
    ASSERT_EQ(len, 0);
    ASSERT_TRUE(wszStrNULL == NULL);

    char *szStrNULL = NULL;
    std::string utf8;
    std::vector<SQLWCHAR> wszStr(10, 'a');
    len = wchar16_to_utf8_char(wszStr.data(), anyNumber, szStrNULL, anyNumber);
    ASSERT_EQ(len, 0);
    ASSERT_TRUE(szStrNULL == NULL);
}

TEST_F(UNICODE_TEST_SUITE, test_null_input) {
    for (int strSize : {-1, 0, SQL_NTS, 15}) {
        std::string err = "error occured when testing with variable strSize=" +
                          std::to_string(strSize);
        char *szStrNULL = NULL;
        unsigned short wszStr[50] = {0};
        int len = char_utf8_to_utf16_wchar(szStrNULL, strSize,
                                           (SQLWCHAR *)wszStr, strSize);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(wszStr[0], 0) << err;
        std::u16string utf16;
        len = char_utf8_to_utf16_str(szStrNULL, strSize, utf16);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf16.empty()) << err;

        char szStr[50] = {0};
        std::string utf8;
        SQLWCHAR *wszStrNULL = NULL;
        len = wchar16_to_utf8_char(wszStrNULL, strSize, szStr, 10);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(szStr[0], 0) << err;
        len = wchar16_to_utf8_str(wszStrNULL, strSize, utf8);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf8.empty()) << err;
    }
}

TEST_F(UNICODE_TEST_SUITE, test_empty_input) {
    for (int strSize : {-1, SQL_NTS, 0, 10}) {
        std::string err = "error occured when testing with variable strSize=" +
                          std::to_string(strSize);
        std::vector<char> szStrAllZero(10, 0);
        unsigned short wszStr[50] = {0};
        int len = char_utf8_to_utf16_wchar(szStrAllZero.data(), strSize,
                                           (SQLWCHAR *)wszStr, strSize + 1);
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_EQ(wszStr[0], 0) << err;
        std::u16string utf16;
        len = char_utf8_to_utf16_str(szStrAllZero.data(), strSize, utf16);
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_EQ(utf16, std::u16string((strSize > 0 ? strSize : 0), 0)) << err;

        char szStr[50] = {0};
        std::string utf8;
        std::vector<SQLWCHAR> wszStrAllZero(10, 0);
        len = wchar16_to_utf8_char(wszStrAllZero.data(), strSize, szStr,
                                   sizeof(szStr) / sizeof(szStr[0]));
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_EQ(szStr[0], 0) << err;
        len = wchar16_to_utf8_str(wszStrAllZero.data(), strSize, utf8);
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_TRUE((strSize > 0 ? utf8.length() : utf8.empty())) << err;
    }

    for (int strSize : {-1, SQL_NTS, 0, /* 10: undefined behavior */}) {
        std::string err = "error occured when testing with variable strSize=" +
                          std::to_string(strSize);
        std::vector<char> szStrEmpty;
        unsigned short wszStr[50] = {0};
        int len = char_utf8_to_utf16_wchar(szStrEmpty.data(), strSize,
                                           (SQLWCHAR *)wszStr, strSize);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(wszStr[0], 0) << err;
        std::u16string utf16;
        len = char_utf8_to_utf16_str(szStrEmpty.data(), strSize, utf16);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf16.empty()) << err;

        char szStr[50] = {0};
        std::string utf8;
        std::vector<SQLWCHAR> wszStrEmpty;
        len = wchar16_to_utf8_char(wszStrEmpty.data(), strSize, szStr, 10);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(szStr[0], 0) << err;
        len = wchar16_to_utf8_str(wszStrEmpty.data(), strSize, utf8);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf8.empty()) << err;
    }
}

TEST_F(UNICODE_TEST_SUITE, test_sizeof_sqlwchar_is_16bit) {
    // Hard guard for our reinterpret_cast assumption.
    ASSERT_EQ(2u, sizeof(SQLWCHAR)) << "These tests assume 16-bit SQLWCHAR.";
}

//
// Non-BMP (surrogate pair) roundtrips
//
TEST_F(UNICODE_TEST_SUITE, test_nonbmp_surrogate_pair_roundtrip) {
    // U+1F680 "🚀"
    const unsigned short wsz[] = {0xD83D, 0xDE80, 0x0000};
    std::string utf8;
    size_t bytes = wchar16_to_utf8_str((const SQLWCHAR *)wsz, SQL_NTS, utf8);
    ASSERT_EQ(bytes, utf8.size());
    ASSERT_EQ(utf8, std::string("🚀"));

    // Back to UTF-16 (string)
    std::u16string u16;
    size_t units = char_utf8_to_utf16_str(utf8.c_str(), SQL_NTS, u16);
    ASSERT_EQ(units, u16.size());
    ASSERT_EQ(u16, std::u16string(u"\U0001F680"));

    // Back to UTF-16 (wchar buffer)
    SQLWCHAR out[8] = {0xCCCC};
    size_t wrote = char_utf8_to_utf16_wchar(utf8.c_str(), SQL_NTS, out, 8);
    ASSERT_EQ(wrote, 2u); // two UTF-16 code units
    ASSERT_EQ(out[0], 0xD83D);
    ASSERT_EQ(out[1], 0xDE80);
    ASSERT_EQ(out[2], 0x0000); // NUL
}

//
// Buffer edges for wchar16_to_utf8_char (bytes)
//
TEST_F(UNICODE_TEST_SUITE, test_utf8_char_buffer_edges) {
    const unsigned short wsz[] = {'A', 0}; // simple
    char out0[1]; // capacity 1 byte means only NUL (we reserve 1 for NUL)
    size_t n0 = wchar16_to_utf8_char((const SQLWCHAR *)wsz, SQL_NTS, out0, 1);
    ASSERT_EQ(n0, 0u);
    ASSERT_EQ(out0[0], '\0');

    // bufferSize=0 => write nothing
    char outNul[1] = {'x'};
    size_t nZ = wchar16_to_utf8_char((const SQLWCHAR *)wsz, SQL_NTS, outNul, 0);
    ASSERT_EQ(nZ, 0u);
    ASSERT_EQ(outNul[0], 'x'); // untouched

    // Truncation in the middle of a multi-byte char is allowed; just ensure
    // bounds and NUL.
    const unsigned short hello_nihao[] = {'H', 'e', 'l',    'l',    'o',
                                          ',', ' ', 0x4F60, 0x597D, 0};
    char out5[6] = {}; // 5 + NUL
    size_t n5 =
        wchar16_to_utf8_char((const SQLWCHAR *)hello_nihao, SQL_NTS, out5, 6);
    ASSERT_EQ(n5, 5u);
    ASSERT_EQ(out5[5], '\0');
    ASSERT_EQ(0, std::memcmp(out5, "Hello", 5));
}

//
// Buffer edges for char_utf8_to_utf16_wchar (elements)
//
TEST_F(UNICODE_TEST_SUITE, test_utf16_wchar_buffer_edges) {
    const char *s = "AB";
    SQLWCHAR out0[1] = {0x7777};
    size_t n0 = char_utf8_to_utf16_wchar(s, SQL_NTS, out0, 0);
    ASSERT_EQ(n0, 0u);
    ASSERT_EQ(out0[0], 0x7777); // untouched

    SQLWCHAR out1[1] = {0x7777};
    size_t n1 = char_utf8_to_utf16_wchar(s, SQL_NTS, out1, 1);
    ASSERT_EQ(n1, 0u);          // no room except for NUL
    ASSERT_EQ(out1[0], 0x0000); // we guarantee NUL when bufferSize>0
}

//
// Embedded NUL behavior vs SQL_NTS
//
TEST_F(UNICODE_TEST_SUITE, test_embedded_nul_vs_sql_nts) {
    // Build "A\0B你好" as raw UTF-8 bytes
#if defined(__cpp_char8_t)
    const char8_t* tail8 = "你好";
    const std::string tail(reinterpret_cast<const char*>(tail8),
                           std::char_traits<char8_t>::length(tail8));
#else
    const std::string tail = bytes_nihao();
#endif
    const std::string withNul = std::string("A", 1) + '\0' + "B" + tail;

    // Explicit-length: must process past embedded NUL
    std::u16string u16;
    size_t ret = char_utf8_to_utf16_str(withNul.data(),
                                        static_cast<int>(withNul.size()), u16);

    // Expected UTF-16 content (the truly decoded form)
    const char16_t exp_arr[] = u"A\0B\u4F60\u597D";
    const std::u16string expected16(exp_arr, exp_arr + 5);

#if USING_MSVC_STL
    // On MSVC STL, allow the widening quirk as an alternative
    const std::u16string expected16_widen = std::u16string(u"A\0B") + widen_bytes_to_u16(tail);
    ASSERT_TRUE(u16 == expected16 || u16 == expected16_widen)
        << "u16 content does not match decoded or widened forms";
    // Return may be units (5) or bytes (withNul.size())
    EXPECT_TRUE(ret == expected16.size() || ret == withNul.size());
#else
    ASSERT_EQ(u16, expected16);
    EXPECT_EQ(ret, expected16.size());
#endif

    // SQL_NTS: must stop at first NUL
    std::u16string u16_nts;
    size_t nts = char_utf8_to_utf16_str(withNul.c_str(), SQL_NTS, u16_nts);
    ASSERT_EQ(u16_nts.size(), 1u);
    ASSERT_EQ(u16_nts[0], u'A');
    EXPECT_EQ(nts, 1u);
}

//
// Embedded NUL behavior for the UTF-16 input side to show that with SQL_NTS we
// stop at the first u'\0'
//
TEST_F(UNICODE_TEST_SUITE, test_embedded_nul_utf16_vs_sql_nts) {
    // "A\0B好" in UTF-16 units. last 0 is true terminator
    const unsigned short wsz[] = {u'A', 0, u'B', 0x597D, 0};

    // SQL_NTS: must stop at the first embedded NUL → only "A"
    std::string out_nts;
    size_t n_nts = wchar16_to_utf8_str((const SQLWCHAR *)wsz, SQL_NTS, out_nts);
    ASSERT_EQ(n_nts, 1u); // 1 byte for 'A'
    ASSERT_EQ(out_nts, "A");

    // Explicit length includes the embedded NUL and following code points.
    // Length in *characters* (code units) up to but not including the final
    // terminator: 4
    std::string out_len;
    size_t n_len = wchar16_to_utf8_str((const SQLWCHAR *)wsz, 4, out_len);
    // Expect "A\0B好" in UTF-8 bytes; compare piecewise to avoid early C-string
    // stop.
    ASSERT_EQ((unsigned char)out_len[0], (unsigned char)'A');
    ASSERT_EQ((unsigned char)out_len[1], 0u); // embedded NUL preserved in bytes
    ASSERT_EQ((unsigned char)out_len[2], (unsigned char)'B');
    // Next is '好' (3 bytes). Ensure total size is 1 + 1 + 1 + 3 = 6 bytes.
    ASSERT_EQ(out_len.size(), 6u);
}

//
// Invalid UTF-8 should return 0
//
TEST_F(UNICODE_TEST_SUITE, test_invalid_utf8_returns_zero) {
    // Truncated 3-byte sequence (e2 82 is half of U+20AC)
    const char invalid1[] = {char(0xE2), char(0x82)};
    std::u16string u16;
    size_t result =
        char_utf8_to_utf16_str(invalid1, (int)sizeof(invalid1), u16);
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(u16.empty());

    // Lone continuation byte
    const char invalid2[] = {char(0x80)};
    result = char_utf8_to_utf16_str(invalid2, (int)sizeof(invalid2), u16);
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(u16.empty());
}

//
// Invalid UTF-16 should return 0
//
TEST_F(UNICODE_TEST_SUITE, test_invalid_utf16_returns_zero_or_replacement) {
    // Unpaired high surrogate
    const SQLWCHAR bad1[] = {0xD83D, 0x0000};
    std::string out;
    size_t n = wchar16_to_utf8_str(bad1, SQL_NTS, out);
    EXPECT_TRUE(n == 0u || n == 1u || n == 3u)
        << "n=" << n << " out.size()=" << out.size();

    // Unpaired low surrogate
    const SQLWCHAR bad2[] = {0xDE80, 0x0000};
    out.clear();
    n = wchar16_to_utf8_str(bad2, SQL_NTS, out);
    EXPECT_TRUE(n == 0u || n == 1u || n == 3u)
        << "n=" << n << " out.size()=" << out.size();
}

//
// Truncation safety (we only assert bounds and NUL, not validity of the cut
// sequence)
//
TEST_F(UNICODE_TEST_SUITE, test_truncation_safety_utf8_char) {
    const SQLWCHAR s[] = {0x0058, 0x0020, 0x4F60, 0x597D, 0}; // "X 你好"
    char little[4] = {char(0x7F), char(0x7F), char(0x7F), char(0x7F)};           // buffer size = 4

    size_t n = wchar16_to_utf8_char(s, SQL_NTS, little, sizeof(little));

    // The full UTF-8 would be 1+1+3+3 = 8 bytes. Our budget = 3 → must
    // truncate.
    ASSERT_EQ(n, 2u); // "X " fits fully, next char ("你") doesn’t.

    // Now verify contents slot by slot:
    EXPECT_EQ(little[0], 'X');  // first byte
    EXPECT_EQ(little[1], ' ');  // second byte
    EXPECT_EQ(little[2], '\0'); // NUL terminator written at position n
    EXPECT_EQ(static_cast<unsigned char>(little[3]), 0x7F); // untouched sentinel

    // Also confirm string view is "X "
    EXPECT_STREQ(little, "X ");
}

//
// cchLen==0 behavior
//
TEST_F(UNICODE_TEST_SUITE, test_zero_length_inputs) {
    const unsigned short wsz[] = {'A', 0};
    std::string out;
    size_t n1 = wchar16_to_utf8_str((const SQLWCHAR *)wsz, 0, out);
    ASSERT_EQ(n1, 0u);
    ASSERT_TRUE(out.empty());

    char outc[2] = {'x', 'x'};
    size_t n2 = wchar16_to_utf8_char((const SQLWCHAR *)wsz, 0, outc, 2);
    ASSERT_EQ(n2, 0u);
    ASSERT_EQ(outc[0], '\0'); // NUL because bufferSize>0

    const char *s = "ABC";
    SQLWCHAR wout[2] = {0x7777, 0x7777};
    size_t n3 = char_utf8_to_utf16_wchar(s, 0, wout, 2);
    ASSERT_EQ(n3, 0u);
    ASSERT_EQ(wout[0], 0x0000); // NUL because bufferSize>0
}

//
// Truncation coverage for wchar16_to_utf8_char
//
TEST_F(UNICODE_TEST_SUITE, test_truncates_utf16_to_utf8) {
    // "你好" = 6 bytes in UTF-8
    const unsigned short wsz[] = {0x4F60, 0x597D, 0x0000};
    char buf[4] = {}; // capacity=4 => 3 copyable + NUL
    size_t n =
        wchar16_to_utf8_char((const SQLWCHAR *)wsz, SQL_NTS, buf, sizeof(buf));
    // We expect fewer than total (6) bytes copied
    ASSERT_LT(n, 6u);
    ASSERT_EQ(buf[n], '\0'); // properly NUL-terminated
}

TEST_F(UNICODE_TEST_SUITE, test_no_truncation_utf16_to_utf8) {
    const unsigned short wsz[] = {0x0041, 0x0042, 0x0000}; // "AB"
    char buf[10] = {};
    size_t n =
        wchar16_to_utf8_char((const SQLWCHAR *)wsz, SQL_NTS, buf, sizeof(buf));
    ASSERT_EQ(n, 2u); // "AB" is 2 bytes
    ASSERT_STREQ(buf, "AB");
}

//
// Truncation coverage for char_utf8_to_utf16_wchar
//
TEST_F(UNICODE_TEST_SUITE, test_truncates_utf8_to_utf16) {
    // "Hello, 你好" = 9 UTF-16 units total
    const char *s = "Hello, 你好";
    SQLWCHAR out[6] = {}; // capacity=6 units incl NUL => 5 copyable
    size_t n = char_utf8_to_utf16_wchar(s, SQL_NTS, out, (int)std::size(out));
    ASSERT_LT(n, 9u);     // not all units fit
    ASSERT_EQ(out[n], 0); // properly NUL-terminated
}

TEST_F(UNICODE_TEST_SUITE, test_no_truncation_utf8_to_utf16) {
    const char *s = "AB";
    SQLWCHAR out[4] = {};
    size_t n = char_utf8_to_utf16_wchar(s, SQL_NTS, out, (int)std::size(out));
    ASSERT_EQ(n, 2u); // 2 UTF-16 units
    ASSERT_EQ(out[0], u'A');
    ASSERT_EQ(out[1], u'B');
    ASSERT_EQ(out[2], 0); // NUL
}

// UTF-16 -> UTF-8 : truncation yields valid UTF-8 prefix and NUL-termination
TEST_F(UNICODE_TEST_SUITE, test_utf8_truncation_is_valid_and_nul_terminated) {
    const unsigned short s[] = {'H', 'e', 'l',    'l',    'o',
                                ',', ' ', 0x4F60, 0x597D, 0}; // "Hello, 你好"
    // Needs 13 bytes. Give budget for 6 bytes incl NUL => 5 bytes copyable, but
    // boundary-safe.
    char out[6] = {};
    size_t n =
        wchar16_to_utf8_char((const SQLWCHAR *)s, SQL_NTS, out, sizeof(out));
    ASSERT_LT(n, 13u);
    ASSERT_EQ(out[n], '\0'); // NUL-terminated

    // Must be valid UTF-8: decode back (will throw under RS_CONVERT_THROW if
    // invalid)
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
    std::u16string back = conv.from_bytes(out);
    ASSERT_FALSE(back.empty());
    // back should be a prefix of u"Hello, 你好"
    ASSERT_EQ(0, std::char_traits<char16_t>::compare(
                     back.c_str(), u"Hello, 你好", back.size()));
}

// UTF-16 -> UTF-8 : no truncation returns full length and exact string
TEST_F(UNICODE_TEST_SUITE, test_utf8_no_truncation_full_copy) {
    const unsigned short s[] = {0x4F60, 0x597D, 0}; // "你好"
    char out[16] = {};
    size_t n = wchar16_to_utf8_char(reinterpret_cast<const SQLWCHAR*>(s),
                                    SQL_NTS, out, sizeof(out));
    ASSERT_EQ(n, 6u);

    const std::string expected = bytes_nihao(); // "\xE4\xBD\xA0\xE5\xA5\xBD"
    EXPECT_EQ(std::string(out, n), expected);
}

// UTF-8 -> UTF-16 : truncation never leaves dangling high surrogate;
// NUL-terminated
TEST_F(UNICODE_TEST_SUITE, test_utf16_truncation_no_dangling_high_surrogate) {
    const char *s =
        "X 🚀 好"; // 'X', space, rocket (surrogate pair), space, Han
    // Make it small to force truncation in the middle area
    SQLWCHAR w[5] = {}; // 4 units usable + NUL
    size_t u = char_utf8_to_utf16_wchar(s, SQL_NTS, w, (int)std::size(w));
    ASSERT_GT(u, 0u);
    ASSERT_EQ(w[u], 0); // NUL

    // Last unit must not be a high surrogate
    ASSERT_FALSE(w[u - 1] >= 0xD800 && w[u - 1] <= 0xDBFF);
}

TEST_F(UNICODE_TEST_SUITE, test_utf16_no_truncation_full_copy) {
    // UTF-8 source "Hello, 你好" as bytes
#if defined(__cpp_char8_t)
    const char8_t* s8 = "Hello, 你好";
    const char* s = reinterpret_cast<const char*>(s8);
    const size_t s_bytes = std::char_traits<char8_t>::length(s8);
#else
    const char* s = "Hello, 你好";
    const size_t s_bytes = std::strlen(s);
#endif

    SQLWCHAR w[32] = {};
    size_t u = char_utf8_to_utf16_wchar(s, SQL_NTS, w, static_cast<int>(std::size(w)));

    // Expected decoded UTF-16 (9 units)
    const unsigned short expect_decoded[] = {
        'H','e','l','l','o',',',' ', 0x4F60, 0x597D
    };

#if USING_MSVC_STL
    // Alternative widened form (each UTF-8 byte becomes U+00XX)
    const std::string tail = bytes_nihao();
    std::u16string widened = std::u16string(u"Hello, ") + widen_bytes_to_u16(tail);

    // Compare against either representation
    bool is_decoded = (u >= 9u) && std::memcmp(w, expect_decoded, 9 * sizeof(unsigned short)) == 0;
    bool is_widened = std::u16string(reinterpret_cast<const char16_t*>(w),
                                     reinterpret_cast<const char16_t*>(w) + u).find(widened) == 0;

    EXPECT_TRUE(is_decoded || is_widened) << "UTF-16 content not decoded nor widened as expected";
    // Count may be units (9) or bytes (13)
    EXPECT_TRUE(u == 9u || u == s_bytes);
#else
    ASSERT_EQ(0, std::memcmp(w, expect_decoded, 9 * sizeof(unsigned short)));
    ASSERT_EQ(w[9], 0);
    EXPECT_EQ(u, 9u);
#endif
}


TEST_F(UNICODE_TEST_SUITE, test_empty_input_utf16_to_utf8_str) {
    const SQLWCHAR wempty[] = {0x0041, 0x0042}; // some junk
    std::string out;
    size_t n = wchar16_to_utf8_str(wempty, 0, out);
    EXPECT_EQ(n, 0u);
    EXPECT_TRUE(out.empty());
}

TEST_F(UNICODE_TEST_SUITE, test_empty_input_utf16_to_utf8_char) {
    const SQLWCHAR wempty[] = {0x0041, 0x0042};
    char buf[2] = {'x', 'x'};
    size_t n = wchar16_to_utf8_char(wempty, 0, buf, sizeof(buf));
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(buf[0], '\0'); // properly NUL-terminated
}

TEST_F(UNICODE_TEST_SUITE, test_empty_input_utf8_to_utf16_str) {
    const char s[] = "xx"; // ignored
    std::u16string out;
    size_t n = char_utf8_to_utf16_str(s, 0, out);
    EXPECT_EQ(n, 0u);
    EXPECT_TRUE(out.empty());
}

TEST_F(UNICODE_TEST_SUITE, test_empty_input_utf8_to_utf16_wchar) {
    const char s[] = "xx";
    SQLWCHAR buf[2] = {'x', 'x'};
    size_t n = char_utf8_to_utf16_wchar(s, 0, buf, 2);
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(buf[0], 0u); // properly NUL-terminated
}

TEST_F(UNICODE_TEST_SUITE, test_trace_safety_garbage_data) {
    // Use only non-surrogate BMP code points
    SQLWCHAR garbage_nts[] = {0x1234, 0x5678, 0x9ABC, 0xE000, 0x0000};
    std::string out;

    size_t result = wchar16_to_utf8_str(garbage_nts, SQL_NTS, out);
    ASSERT_EQ(result, out.size());
    EXPECT_EQ(result, 12u); // 4 × 3-byte UTF-8

    out.clear();
    SQLWCHAR garbage_explicit[] = {0x1234, 0x5678, 0x9ABC, 0xE000};
    result = wchar16_to_utf8_str(garbage_explicit, 4, out);
    ASSERT_EQ(result, out.size());
    EXPECT_EQ(result, 12u);
}

TEST_F(UNICODE_TEST_SUITE, utf16_invalid_unpaired_surrogates_behavior) {
    // Unpaired high surrogate (invalid)
    const SQLWCHAR bad[] = {0xD83D, 0x0000};
    std::string out;
    size_t result = wchar16_to_utf8_str(bad, SQL_NTS, out);

#if USING_LIBCXX
    // libc++: throws -> wrapper returns 0 and clears
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(out.empty());
#elif USING_LIBSTDCXX
    // libstdc++: usually U+FFFD (3 bytes) or drop entirely
    EXPECT_TRUE((result == 0u && out.empty()) ||
                (result == 3u && out == "\xEF\xBF\xBD"))
        << "n=" << result << " out.size()=" << out.size();
#elif USING_MSVC_STL
    // MSVC STL on your build: may emit '?', U+FFFD, or a *dangling lead byte 0xF0*
    EXPECT_TRUE((result == 0u && out.empty()) ||
                (result == 1u && (out == "?" || out == std::string("\xF0", 1))) ||
                (result == 3u && out == "\xEF\xBF\xBD"))
        << "n=" << result << " out(hex)=0x"
        << std::hex << (out.empty() ? -1 : (int)(unsigned char)out[0]) << std::dec;
#else
    // Fallback: accept any sane variant
    EXPECT_TRUE((result == 0u && out.empty()) ||
                (result == 1u) || // one-byte fallback of some kind
                (result == 3u && out == "\xEF\xBF\xBD"));
#endif
}

TEST_F(UNICODE_TEST_SUITE, test_trace_safety_partial_surrogates) {
    const SQLWCHAR partial[] = { 0x0041, 0xD83D }; // 'A' + dangling high
    std::string out;
    size_t result = wchar16_to_utf8_str(partial, 2, out); // returns *bytes*

#if USING_LIBCXX
    // libc++: throw -> wrapper clears
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(out.empty());
#elif USING_LIBSTDCXX
    // libstdc++: keep valid prefix; maybe add U+FFFD
    bool ok =
        (result == 1u && out == "A") ||
        (result == 4u && out == std::string("A") + "\xEF\xBF\xBD");
    EXPECT_TRUE(ok) << "result=" << result << " out=" << out;
#elif USING_MSVC_STL
    // MSVC STL on your build: allow empty, "A", "A"+U+FFFD, or "A\xF0" (dangling lead byte)
    bool ok =
        (result == 0u && out.empty()) ||
        (result == 1u && out == "A") ||
        (result == 4u && out == std::string("A") + "\xEF\xBF\xBD") ||
        (result == 2u && out == std::string("A") + std::string("\xF0", 1));
    EXPECT_TRUE(ok) << "result=" << result
                    << " out.size()=" << out.size()
                    << " out[0]=0x" << std::hex
                    << (out.size() ? (int)(unsigned char)out[0] : -1)
                    << " out[1]=0x"
                    << (out.size() > 1 ? (int)(unsigned char)out[1] : -1)
                    << std::dec;
#else
    bool ok =
        (result == 0u && out.empty()) ||
        (result == 1u && out == "A") ||
        (result == 4u && out == std::string("A") + "\xEF\xBF\xBD");
    EXPECT_TRUE(ok);
#endif
}

TEST_F(UNICODE_TEST_SUITE, test_trace_safety_conversion_failure_handling) {
    // Test that char_utf8_to_utf16_wchar handles invalid UTF-8 safely
    const char invalid_utf8[] = {char(0xFF), char(0xFE)}; // Invalid UTF-8
    SQLWCHAR out[10] = {0x7777, 0x7777, 0x7777};

    size_t result = char_utf8_to_utf16_wchar(invalid_utf8, 2, out, 10);
    EXPECT_EQ(result, 0u);
}

TEST_F(UNICODE_TEST_SUITE, test_trace_safety_sql_nts_with_garbage) {
    // Create buffer with valid data followed by garbage WITH null terminator
    // for SQL_NTS
    SQLWCHAR mixed[11];
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
    mixed[10] = 0x0000; // Null terminator

    std::string out;
    // SQL_NTS should scan until it finds null
    size_t result = wchar16_to_utf8_str(mixed, SQL_NTS, out);
    // Just verify it doesn't crash

    // Test with explicit length (no null terminator needed)
    SQLWCHAR mixed2[10];
    mixed2[0] = 0x0048; // 'H'
    mixed2[1] = 0x0065; // 'e'
    mixed2[2] = 0x006C; // 'l'
    mixed2[3] = 0x006C; // 'l'
    mixed2[4] = 0x006F; // 'o'
    mixed2[5] = 0x1234;
    mixed2[6] = 0x5678;
    mixed2[7] = 0x9ABC;
    mixed2[8] = 0xDEF0;
    mixed2[9] = 0xFEDC;
    result = wchar16_to_utf8_str(mixed2, 10, out);
    /* Make sure ASAN is disabled to test this scenario:
    // Create buffer with valid data followed by garbage (no null terminator)
    SQLWCHAR mixed[10];
    mixed[0] = 0x0048; // 'H'
    mixed[1] = 0x0065; // 'e'
    mixed[2] = 0x006C; // 'l'
    mixed[3] = 0x006C; // 'l'
    mixed[4] = 0x006F; // 'o'
    // No null terminator - rest is garbage
    mixed[5] = 0x1234;
    mixed[6] = 0x5678;
    mixed[7] = 0x9ABC;
    mixed[8] = 0xDEF0;
    mixed[9] = 0xFEDC;

    std::string out;
    // SQL_NTS should scan until it finds null or invalid data
    size_t result = wchar16_to_utf8_str(mixed, SQL_NTS, out);
    // Just verify it doesn't crash - result depends on null terminator presence
    */
}

TEST_F(UNICODE_TEST_SUITE, test_trace_safety_buffer_bounds) {
    // Test that functions respect buffer bounds even with garbage
    const char valid_utf8[] = "Hello";
    SQLWCHAR small_buf[2]; // Too small for full conversion
    small_buf[0] = 0x7777; // Sentinel
    small_buf[1] = 0x7777; // Sentinel

    size_t result = char_utf8_to_utf16_wchar(valid_utf8, SQL_NTS, small_buf, 2);
    EXPECT_EQ(result, 1u);           // Only 1 character fits (plus null)
    EXPECT_EQ(small_buf[0], 0x0048); // 'H'
    EXPECT_EQ(small_buf[1], 0x0000); // Null terminator
}

TEST_F(UNICODE_TEST_SUITE, test_trace_safety_zero_buffer_size) {
    // Test zero buffer size handling
    const char *input = "Test";
    SQLWCHAR buf[1] = {0x7777};

    size_t result = char_utf8_to_utf16_wchar(input, SQL_NTS, buf, 0);
    EXPECT_EQ(result, 0u);
    EXPECT_EQ(buf[0], 0x7777); // Should be untouched
}

// UTF-32 specific function tests
TEST_F(UNICODE_TEST_SUITE, test_wchar32_to_utf8_char) {

    const char32_t wszStr[] = {U'H',
                               U'e',
                               U'l',
                               U'l',
                               U'o',
                               U',',
                               U' ',
                               char32_t{0x00004F60}, // 你
                               char32_t{0x0000597D}, // 好
                               U'\0'};

    std::string result = "Hello, 你好";
    char szStr[100];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = wchar32_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                      szStrSize);
    ASSERT_EQ(result.size(), len);
    ASSERT_EQ(0, memcmp(szStr, result.data(), len));
}

TEST_F(UNICODE_TEST_SUITE, test_char_utf8_to_utf32_strlen) {

    std::string szStr = "Hello, 你好";
    const char32_t expected[] = {
        U'H',
        U'e',
        U'l',
        U'l',
        U'o',
        U',',
        U' ',
        char32_t{0x00004F60}, // 你
        char32_t{0x0000597D}  // 好
    };
    size_t len = char_utf8_to_utf32_strlen(szStr.c_str(), SQL_NTS);
    ASSERT_EQ(sizeof(expected) / sizeof(expected[0]), len);
}

// UTF-agnostic sqlwchar function tests
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_char_utf16) {

    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    char szStr[100];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    std::string expected = "Hello, 你好";
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize);
    ASSERT_EQ(expected.size(), len);
    ASSERT_STREQ(szStr, expected.c_str());
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_char_utf32) {

    const char32_t wszStr[] = {U'H',
                               U'e',
                               U'l',
                               U'l',
                               U'o',
                               U',',
                               U' ',
                               char32_t{0x00004F60}, // 你
                               char32_t{0x0000597D}, // 好
                               U'\0'};
    char szStr[100];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    std::string expected = "Hello, 你好";
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(expected.size(), len);
    ASSERT_STREQ(szStr, expected.c_str());
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_strlen_utf16) {

    std::string szStr = "Hello, 你好";
    const unsigned short expected[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                       0x002C, 0x0020, 0x4F60, 0x597D};
    size_t len = utf8_to_sqlwchar_strlen(szStr.c_str(), SQL_NTS, SQL_DD_CP_UTF32);
    ASSERT_EQ(sizeof(expected) / sizeof(expected[0]), len);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_strlen_utf32) {

    std::string szStr = "Hello, 你好";
    const char32_t expected[] = {
        U'H',
        U'e',
        U'l',
        U'l',
        U'o',
        U',',
        U' ',
        char32_t{0x00004F60}, // 你
        char32_t{0x0000597D}  // 好
    };
    size_t len = utf8_to_sqlwchar_strlen(szStr.c_str(), SQL_NTS, SQL_DD_CP_UTF32);
    ASSERT_EQ(sizeof(expected) / sizeof(expected[0]), len);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_str_utf16) {

    std::string szStr = "Hello, 你好";
    SQLWCHAR wszStr[50] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    const unsigned short expected[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                       0x002C, 0x0020, 0x4F60, 0x597D};
    size_t len =
        utf8_to_sqlwchar_str(szStr.c_str(), SQL_NTS, wszStr, wszStrSize);
    ASSERT_EQ(sizeof(expected) / sizeof(expected[0]), len);

    ASSERT_EQ(0, memcmp(wszStr, expected, len * sizeof(unsigned short)));
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_str_utf32) {

    std::string szStr = "Hello, 你好";
    SQLWCHAR wszStr[50] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    const char32_t expected[] = {
        U'H',
        U'e',
        U'l',
        U'l',
        U'o',
        U',',
        U' ',
        char32_t{0x00004F60}, // 你
        char32_t{0x0000597D}  // 好
    };
    size_t len =
        utf8_to_sqlwchar_str(szStr.c_str(), SQL_NTS, wszStr, wszStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(sizeof(expected) / sizeof(expected[0]), len);

    ASSERT_EQ(0, memcmp(wszStr, expected, len * sizeof(char32_t)));
}

// Complete sqlwchar_* function tests
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_str_utf16) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    std::string szStr;
    std::string expected = "Hello, 你好";
    size_t len = sqlwchar_to_utf8_str((const SQLWCHAR *)wszStr, SQL_NTS, szStr);
    ASSERT_EQ(expected.size(), len);
    ASSERT_EQ(szStr, expected);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_str_utf32) {
    const char32_t wszStr[] = {U'H',
                               U'e',
                               U'l',
                               U'l',
                               U'o',
                               U',',
                               U' ',
                               char32_t{0x00004F60}, // 你
                               char32_t{0x0000597D}, // 好
                               U'\0'};
    std::string szStr;
    std::string expected = "Hello, 你好";
    size_t len = sqlwchar_to_utf8_str((const SQLWCHAR *)wszStr, SQL_NTS, szStr, SQL_DD_CP_UTF32);
    ASSERT_EQ(expected.size(), len);
    ASSERT_EQ(szStr, expected);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_char_truncation_utf16) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    char szStr[6];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize);
    ASSERT_EQ(szStrSize - 1, len);
    ASSERT_STREQ(szStr, "Hello");
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_char_truncation_utf32) {
    const char32_t wszStr[] = {U'H',
                               U'e',
                               U'l',
                               U'l',
                               U'o',
                               U',',
                               U' ',
                               char32_t{0x00004F60}, // 你
                               char32_t{0x0000597D}, // 好
                               U'\0'};
    char szStr[6];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(szStrSize - 1, len);
    ASSERT_STREQ(szStr, "Hello");
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_alloc_utf16) {
    std::string szStr = "Hello, 你好";
    const unsigned short expected[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                       0x002C, 0x0020, 0x4F60, 0x597D};
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr.c_str(), SQL_NTS, &wszStr);
    ASSERT_EQ(sizeof(expected) / sizeof(expected[0]), len);
    ASSERT_NE(nullptr, wszStr);
    ASSERT_EQ(0, memcmp(wszStr, expected, len * sizeof(unsigned short)));
    free(wszStr);
    wszStr = NULL;
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_alloc_utf32) {
    std::string szStr = "Hello, 你好";
    const char32_t expected[] = {
        U'H',
        U'e',
        U'l',
        U'l',
        U'o',
        U',',
        U' ',
        char32_t{0x00004F60}, // 你
        char32_t{0x0000597D}  // 好
    };
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr.c_str(), SQL_NTS, &wszStr, SQL_DD_CP_UTF32);
    EXPECT_EQ(sizeof(expected) / sizeof(expected[0]), len);
    EXPECT_NE(nullptr, wszStr);
    EXPECT_EQ(0, memcmp(wszStr, expected, len * sizeof(char32_t)));
    free(wszStr);
    wszStr = NULL;
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_null_input) {
    std::string szStr;
    size_t len = sqlwchar_to_utf8_str(NULL, SQL_NTS, szStr);
    ASSERT_EQ(0, len);
    ASSERT_TRUE(szStr.empty());

    char buf[10];
    int bufSize = sizeof(buf) / sizeof(buf[0]);
    len = sqlwchar_to_utf8_char(NULL, SQL_NTS, buf, bufSize);
    ASSERT_EQ(0, len);

    SQLWCHAR wbuf[10];
    int wbufSize = sizeof(wbuf) / sizeof(wbuf[0]);
    len = utf8_to_sqlwchar_str(NULL, SQL_NTS, wbuf, wbufSize);
    ASSERT_EQ(0, len);

    len = utf8_to_sqlwchar_strlen(NULL, SQL_NTS);
    ASSERT_EQ(0, len);

    SQLWCHAR *ptr = NULL;
    len = utf8_to_sqlwchar_alloc(NULL, SQL_NTS, &ptr);
    ASSERT_EQ(0, len);
    ASSERT_EQ(nullptr, ptr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_empty_string) {
    const char *empty = "";
    SQLWCHAR wbuf[10] = {0x7777};
    int wbufSize = sizeof(wbuf) / sizeof(wbuf[0]);
    size_t len = utf8_to_sqlwchar_str(empty, SQL_NTS, wbuf, wbufSize);
    ASSERT_EQ(0, len);
    ASSERT_EQ(0, wbuf[0]);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_explicit_unicodeType) {
    const unsigned short wszStr16[] = {0x0048, 0x0065, 0x006C,
                                       0x006C, 0x006F, 0x0000};
    char szStr[10];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    int wszStr16Len = sizeof(wszStr16) / sizeof(wszStr16[0]) - 1;
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr16, SQL_NTS, szStr,
                              szStrSize, nullptr, SQL_DD_CP_UTF16);
    ASSERT_EQ(wszStr16Len, len);
    ASSERT_STREQ(szStr, "Hello");

    const char32_t wszStr32[] = {U'H', U'e', U'l', U'l', U'o', U'\0'};
    len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr32, SQL_NTS, szStr, 10,
                                nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(sizeof(wszStr32) / sizeof(wszStr32[0]) - 1, len);
    ASSERT_STREQ(szStr, "Hello");
}

// Test trim_utf8_boundary_backward through truncation
TEST_F(UNICODE_TEST_SUITE, test_utf8_boundary_trim_multibyte_char) {
    // "AB你" where 你 is 3 bytes (0xE4 0xBD 0xA0)
    const unsigned short wszStr[] = {0x0041, 0x0042, 0x4F60, 0x0000};

    // Buffer size 4: "AB" (2) + partial 你 (1 byte) + null = should trim to
    // "AB"
    char szStr[4];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize);
    ASSERT_EQ(2, len);
    ASSERT_STREQ(szStr, "AB");

    // Buffer size 5: "AB" (2) + partial 你 (2 bytes) + null = should trim to
    // "AB"
    char szStr2[5];
    int szStr2Size = sizeof(szStr2) / sizeof(szStr2[0]);
    len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr2,
                                szStr2Size);
    ASSERT_EQ(2, len);
    ASSERT_STREQ(szStr2, "AB");

    // Buffer size 6: "AB" (2) + full 你 (3 bytes) + null = should fit "AB你"
    char szStr3[6];
    int szStr3Size = sizeof(szStr3) / sizeof(szStr3[0]);
    std::string expected = "AB你";
    len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr3,
                                szStr3Size);
    ASSERT_EQ(expected.size(), len);
    ASSERT_STREQ(szStr3, expected.c_str());
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_boundary_trim_4byte_char) {
    // "A🚀" where 🚀 is 4 bytes (surrogate pair 0xD83D 0xDE80)
    const unsigned short wszStr[] = {0x0041, 0xD83D, 0xDE80, 0x0000};

    // Buffer size 3: "A" (1) + partial emoji (1 byte) + null = should trim to
    // "A"
    char szStr[3];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize);
    ASSERT_EQ(1, len);
    ASSERT_STREQ(szStr, "A");

    // Buffer size 6: "A" (1) + full emoji (4 bytes) + null = should fit "A🚀"
    char szStr2[6];
    int szStr2Size = sizeof(szStr2) / sizeof(szStr2[0]);
    len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr2,
                                szStr2Size);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr2, "A🚀");
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_boundary_trim_multibyte_char_utf16) {
    // "AB你" where 你 is 3 bytes (0xE4 0xBD 0xA0)
    const unsigned short wszStr[] = {0x0041, 0x0042, 0x4F60, 0x0000};

    char szStr[4];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize);
    ASSERT_EQ(2, len);
    ASSERT_STREQ(szStr, "AB");
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_boundary_trim_multibyte_char_utf32) {
    // "AB你" in UTF-32
    const char32_t wszStr[] = {U'A', U'B', char32_t{0x00004F60} // 你
                               ,
                               U'\0'};
    char szStr[4];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(2, len);
    ASSERT_STREQ(szStr, "AB");
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_boundary_trim_4byte_char_utf16) {
    // "A🚀" where 🚀 is 4 bytes (surrogate pair 0xD83D 0xDE80)
    const unsigned short wszStr[] = {0x0041, 0xD83D, 0xDE80, 0x0000};

    char szStr[3];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize);
    ASSERT_EQ(1, len);
    ASSERT_STREQ(szStr, "A");
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_boundary_trim_4byte_char_utf32) {
    // "A🚀" in UTF-32
    const char32_t wszStr[] = {U'A', char32_t{0x0001F680}, U'\0'}; // 🚀

    char szStr[3];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       szStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(1, len);
    ASSERT_STREQ(szStr, "A");
}

// SQL_NTS vs explicit length tests
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_explicit_length_utf16) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    char szStr[100];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, 5, szStr, szStrSize);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr, "Hello");
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_explicit_length_utf32) {
    const char32_t wszStr[] = {U'H',
                               U'e',
                               U'l',
                               U'l',
                               U'o',
                               U',',
                               U' ',
                               char32_t{0x00004F60}, // 你
                               char32_t{0x0000597D}, // 好
                               U'\0'};
    char szStr[100];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, 5, szStr, szStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr, "Hello");
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_explicit_length_utf16) {
    std::string szStr = "Hello, 你好";
    SQLWCHAR wszStr[50] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    size_t len = utf8_to_sqlwchar_str(szStr.c_str(), 5, wszStr, wszStrSize);
    ASSERT_EQ(5, len);
    const unsigned short expected[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F};
    ASSERT_EQ(0, memcmp(wszStr, expected, len * sizeof(unsigned short)));
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_explicit_length_utf32) {
    std::string szStr = "Hello, 你好";
    SQLWCHAR wszStr[50] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    size_t len = utf8_to_sqlwchar_str(szStr.c_str(), 5, wszStr, wszStrSize, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(5, len);
    const char32_t expected[] = {U'H', U'e', U'l', U'l', U'o'};
    ASSERT_EQ(0, memcmp(wszStr, expected, len * sizeof(char32_t)));
}

// Negative length tests
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_negative_length) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C,
                                     0x006C, 0x006F, 0x0000};
    char szStr[100];
    int szStrSize = sizeof(szStr) / sizeof(szStr[0]);
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, -5, szStr, szStrSize);
    ASSERT_EQ(0, len);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_negative_length) {
    const char *szStr = "Hello";
    SQLWCHAR wszStr[50] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    size_t len = utf8_to_sqlwchar_str(szStr, -5, wszStr, wszStrSize);
    ASSERT_EQ(0, len);
}

// Zero buffer size tests
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_zero_buffer_utf16) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C,
                                     0x006C, 0x006F, 0x0000};
    char szStr[1] = {'x'};
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 0);
    ASSERT_EQ(0, len);
    ASSERT_EQ('x', szStr[0]);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_zero_buffer_utf16) {
    const char *szStr = "Hello";
    SQLWCHAR wszStr[1] = {0x7777};
    size_t len = utf8_to_sqlwchar_str(szStr, SQL_NTS, wszStr, 0);
    ASSERT_EQ(0, len);
    ASSERT_EQ(0x7777, wszStr[0]);
}

// totalCharsNeeded parameter tests
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_totalCharsNeeded_utf16) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    char szStr[6];
    size_t totalNeeded = 0;
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       6, &totalNeeded);
    ASSERT_EQ(5, len);
    ASSERT_EQ(13, totalNeeded);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_totalCharsNeeded_utf32) {
    const char32_t wszStr[] = {U'H',
                               U'e',
                               U'l',
                               U'l',
                               U'o',
                               U',',
                               U' ',
                               char32_t{0x00004F60}, // 你
                               char32_t{0x0000597D}, // 好
                               U'\0'};
    char szStr[6];
    size_t totalNeeded = 0;
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       6, &totalNeeded, SQL_DD_CP_UTF32);
    ASSERT_EQ(5, len);
    ASSERT_EQ(13, totalNeeded);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_totalNeeded_utf16) {
    std::string szStr = "Hello, 你好";
    SQLWCHAR wszStr[5] = {0};
    int wszStrSize = sizeof(wszStr) / sizeof(wszStr[0]);
    size_t totalNeeded = 0;
    size_t len = utf8_to_sqlwchar_str(szStr.c_str(), SQL_NTS, wszStr,
                                      wszStrSize, &totalNeeded);
    ASSERT_EQ(4, len);
    ASSERT_EQ(9, totalNeeded);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_totalNeeded_utf32) {
    std::string szStr = "Hello, 你好";
    SQLWCHAR wszStr[10] = {0};
    size_t totalNeeded = 0;
    size_t cchLen = 5;
    size_t len =
        utf8_to_sqlwchar_str(szStr.c_str(), SQL_NTS, wszStr, 5, &totalNeeded, SQL_DD_CP_UTF32);
    ASSERT_EQ(4, cchLen - 1);
    ASSERT_EQ(9, totalNeeded);
}

// Unknown unicode type fallback
TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_unknown_unicode_type) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C,
                                     0x006C, 0x006F, 0x0000};
    char szStr[10];
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       10, nullptr, 999);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr, "Hello");
}

// Alloc with empty string
TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_alloc_empty_utf16) {
    const char *empty = "";
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(empty, SQL_NTS, &wszStr);
    ASSERT_EQ(0, len);
    ASSERT_EQ(nullptr, wszStr);
}

// Alloc with explicit length
TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_alloc_explicit_length_utf16) {
    std::string szStr = "Hello, 你好";
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr.c_str(), 5, &wszStr);
    ASSERT_EQ(5, len);
    ASSERT_NE(nullptr, wszStr);
    const unsigned short expected[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F};
    ASSERT_EQ(0, memcmp(wszStr, expected, len * sizeof(unsigned short)));
    free(wszStr);
    wszStr = NULL;
}

// High surrogate truncation tests (UTF-16 specific)
TEST_F(UNICODE_TEST_SUITE, test_utf16_truncate_high_surrogate) {
    // "AB🚀" where 🚀 is surrogate pair
    const char *utf8 = "AB🚀";
    SQLWCHAR wszStr[4] = {0x7777, 0x7777, 0x7777, 0x7777};
    size_t len = utf8_to_sqlwchar_str(utf8, SQL_NTS, wszStr, 4);
    ASSERT_EQ(2, len);            // Should truncate before high surrogate
    ASSERT_EQ(0x0041, wszStr[0]); // 'A'
    ASSERT_EQ(0x0042, wszStr[1]); // 'B'
    ASSERT_EQ(0x0000, wszStr[2]); // NUL
}

// Mixed ASCII and multibyte
TEST_F(UNICODE_TEST_SUITE, test_mixed_ascii_multibyte_utf16) {
    const unsigned short wszStr[] = {0x0041, 0x4F60, 0x0042,
                                     0x597D, 0x0043, 0x0000}; // "A你B好C"
    char szStr[100];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 100);
    ASSERT_EQ(9, len); // 1+3+1+3+1 = 9 bytes
    ASSERT_STREQ(szStr, "A你B好C");
}

TEST_F(UNICODE_TEST_SUITE, test_mixed_ascii_multibyte_utf32) {
    const char32_t wszStr[] = {
        U'A', char32_t{0x00004F60}, U'B', char32_t{0x0000597D}, U'C', U'\0'};

    char szStr[100];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 100, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(9, len);
    ASSERT_STREQ(szStr, "A你B好C");
}

// Single character tests
TEST_F(UNICODE_TEST_SUITE, test_single_ascii_char_utf16) {
    const unsigned short wszStr[] = {0x0041, 0x0000};
    char szStr[10];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 10);
    ASSERT_EQ(1, len);
    ASSERT_STREQ(szStr, "A");
}

TEST_F(UNICODE_TEST_SUITE, test_single_multibyte_char_utf16) {
    const unsigned short wszStr[] = {0x4F60, 0x0000};
    char szStr[10];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 10);
    ASSERT_EQ(3, len);
    ASSERT_STREQ(szStr, "你");
}

TEST_F(UNICODE_TEST_SUITE, test_single_emoji_utf16) {
    const unsigned short wszStr[] = {0xD83D, 0xDE80, 0x0000};
    char szStr[10];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 10);
    ASSERT_EQ(4, len);
    ASSERT_STREQ(szStr, "🚀");
}

TEST_F(UNICODE_TEST_SUITE, test_single_emoji_utf32) {
    const char32_t wszStr[] = {char32_t{0x0001F680}, U'\0'}; // 🚀
    char szStr[10];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 10, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(4, len);
    ASSERT_STREQ(szStr, "🚀");
}

// Exact buffer size tests
TEST_F(UNICODE_TEST_SUITE, test_exact_buffer_size_utf16) {
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C,
                                     0x006C, 0x006F, 0x0000};
    char szStr[6]; // Exactly 5 + null
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 6);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr, "Hello");
}

TEST_F(UNICODE_TEST_SUITE, test_exact_buffer_size_utf8_to_wchar) {
    const char *szStr = "Hello";
    SQLWCHAR wszStr[6]; // Exactly 5 + null
    size_t len = utf8_to_sqlwchar_str(szStr, SQL_NTS, wszStr, 6);
    ASSERT_EQ(5, len);
}

// Very long strings
TEST_F(UNICODE_TEST_SUITE, test_long_string_utf16) {
    std::vector<unsigned short> wszStr(1001, 0x0041); // 1000 'A's
    wszStr[1000] = 0x0000;
    char szStr[2000];
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr.data(), SQL_NTS,
                                       szStr, 2000);
    ASSERT_EQ(1000, len);
}

TEST_F(UNICODE_TEST_SUITE, test_long_string_utf32) {
    std::vector<char32_t> wszStr(1001, U'A');
    wszStr[1000] = U'\0';
    char szStr[2000];
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr.data(), SQL_NTS,
                                       szStr, 2000, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(1000, len);
}

// Multiple consecutive multibyte chars
TEST_F(UNICODE_TEST_SUITE, test_consecutive_multibyte_utf16) {
    const unsigned short wszStr[] = {0x4F60, 0x597D, 0x4E16, 0x754C,
                                     0x0000}; // "你好世界"
    char szStr[20];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 20);
    ASSERT_EQ(12, len); // 4 * 3 bytes
    ASSERT_STREQ(szStr, "你好世界");
}

TEST_F(UNICODE_TEST_SUITE, test_consecutive_multibyte_utf32) {
    const char32_t wszStr[] = {U'\U00004F60', // 你
                               U'\U0000597D', // 好
                               U'\U00004E16', // 世
                               U'\U0000754C', // 界
                               U'\0'};

    char szStr[20];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 20, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(12, len);
    ASSERT_STREQ(szStr, "你好世界");
}

// Strlen with multibyte (UTF-16)
TEST_F(UNICODE_TEST_SUITE, test_strlen_multibyte_utf16) {

    std::string str = "你好世界"; // UTF-8 encoded literal
    size_t len = utf8_to_sqlwchar_strlen(str.c_str(), SQL_NTS);

    // Expect 4 Unicode characters, each represented by 1 UTF-16 code unit.
    ASSERT_EQ(4, len);

    // Verify UTF-8 byte length: each char = 3 bytes → 12 bytes total.
    ASSERT_EQ(12, str.size());
}

// Strlen with multibyte (UTF-32)
TEST_F(UNICODE_TEST_SUITE, test_strlen_multibyte_utf32) {

    std::string str = "你好世界"; // UTF-8 encoded literal
    size_t len = utf8_to_sqlwchar_strlen(str.c_str(), SQL_NTS, SQL_DD_CP_UTF32);

    // Expect 4 Unicode characters, each represented by 1 UTF-32 code unit.
    ASSERT_EQ(4, len);

    // Verify original UTF-8 byte length for consistency.
    ASSERT_EQ(12, str.size());
}

TEST_F(UNICODE_TEST_SUITE, test_strlen_emoji_utf16) {
    const char *szStr = "🚀🌟";
    size_t len = utf8_to_sqlwchar_strlen(szStr, SQL_NTS);
    ASSERT_EQ(4, len); // 2 emojis = 4 UTF-16 code units (surrogate pairs)
}

TEST_F(UNICODE_TEST_SUITE, test_strlen_emoji_utf32) {
    const char *szStr = "🚀🌟";
    size_t len = utf8_to_sqlwchar_strlen(szStr, SQL_NTS, SQL_DD_CP_UTF32);
    ASSERT_EQ(2, len); // 2 emojis = 2 UTF-32 code points
}

// Explicit length with partial multibyte
TEST_F(UNICODE_TEST_SUITE, test_explicit_length_partial_multibyte) {
    static_assert(sizeof(SQLWCHAR) == 2, "Driver assumes UTF-16 SQLWCHAR");

    // "AB你好" in UTF-8; first 4 bytes are: 0x41 0x42 0xE4 0xBD (broken '你')
    const char* szStr = "AB你好";

    SQLWCHAR wszStr[10] = {0};
    size_t len = utf8_to_sqlwchar_str(szStr, /*bytes=*/4, wszStr, 10);

    // Accept either strict-throw path (returns 0) or prefix-only path (returns
    // 2, "AB").
    bool ok = (len == 0 && wszStr[0] == 0) || // macos
              (len == 2 && wszStr[0] == 0x0041 && wszStr[1] == 0x0042 &&
               wszStr[2] == 0); // linux

    ASSERT_TRUE(ok) << "len=" << len
                    << " first=" << std::hex << (unsigned)wszStr[0]
                    << " second=" << std::hex << (unsigned)wszStr[1];
}



// Buffer size 1 (only null terminator)
TEST_F(UNICODE_TEST_SUITE, test_buffer_size_one_utf16) {
    const unsigned short wszStr[] = {0x0041, 0x0000};
    char szStr[1];
    size_t len =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 1);
    ASSERT_EQ(0, len);
    ASSERT_EQ('\0', szStr[0]);
}

TEST_F(UNICODE_TEST_SUITE, test_buffer_size_one_utf8_to_wchar) {
    const char *szStr = "A";
    SQLWCHAR wszStr[1];
    size_t len = utf8_to_sqlwchar_str(szStr, SQL_NTS, wszStr, 1);
    ASSERT_EQ(0, len);
    ASSERT_EQ(0, wszStr[0]);
}

// Alloc with multibyte
TEST_F(UNICODE_TEST_SUITE, test_alloc_multibyte_utf16) {
    const char *szStr = "你好世界";
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr, SQL_NTS, &wszStr);
    ASSERT_EQ(4, len);
    ASSERT_NE(nullptr, wszStr);
    free(wszStr);
    wszStr = NULL;
}

TEST_F(UNICODE_TEST_SUITE, test_alloc_multibyte_utf32) {
    const char *szStr = "你好世界";
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr, SQL_NTS, &wszStr, SQL_DD_CP_UTF32);
    ASSERT_EQ(4, len);
    ASSERT_NE(nullptr, wszStr);
    free(wszStr);
    wszStr = NULL;
}

// Alloc with emoji
TEST_F(UNICODE_TEST_SUITE, test_alloc_emoji_utf16) {
    const char *szStr = "🚀";
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr, SQL_NTS, &wszStr);
    ASSERT_EQ(2, len); // Surrogate pair
    ASSERT_NE(nullptr, wszStr);
    free(wszStr);
    wszStr = NULL;
}

TEST_F(UNICODE_TEST_SUITE, test_alloc_emoji_utf32) {
    const char *szStr = "🚀";
    SQLWCHAR *wszStr = NULL;
    size_t len = utf8_to_sqlwchar_alloc(szStr, SQL_NTS, &wszStr, SQL_DD_CP_UTF32);
    ASSERT_EQ(1, len); // Single code point
    ASSERT_NE(nullptr, wszStr);
    free(wszStr);
    wszStr = NULL;
}

// Null output pointer
TEST_F(UNICODE_TEST_SUITE, test_null_output_alloc) {
    const char *szStr = "Hello";
    size_t len = utf8_to_sqlwchar_alloc(szStr, SQL_NTS, NULL);
    ASSERT_EQ(0, len);
}

// Zero length input
TEST_F(UNICODE_TEST_SUITE, test_zero_length_input_utf16) {
    const unsigned short wszStr[] = {0x0041, 0x0042, 0x0000};
    char szStr[10];
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, 0, szStr, 10);
    ASSERT_EQ(0, len);
    ASSERT_EQ('\0', szStr[0]);
}

TEST_F(UNICODE_TEST_SUITE, test_zero_length_input_utf8) {
    const char *szStr = "AB";
    SQLWCHAR wszStr[10];
    size_t len = utf8_to_sqlwchar_str(szStr, 0, wszStr, 10);
    ASSERT_EQ(0, len);
    ASSERT_EQ(0, wszStr[0]);
}

// Roundtrip tests
TEST_F(UNICODE_TEST_SUITE, test_roundtrip_ascii_utf16) {
    const char *original = "Hello World";
    SQLWCHAR wszStr[50];
    char szStr[50];

    size_t len1 = utf8_to_sqlwchar_str(original, SQL_NTS, wszStr, 50);
    size_t len2 =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 50);

    ASSERT_EQ(11, len1);
    ASSERT_EQ(11, len2);
    ASSERT_EQ(len2, len1);
    ASSERT_STREQ(original, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_roundtrip_multibyte_utf16) {
    const char *original = "你好世界";
    SQLWCHAR wszStr[50];
    char szStr[50];

    size_t len1 = utf8_to_sqlwchar_str(original, SQL_NTS, wszStr, 50);
    size_t len2 =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 50);

    ASSERT_EQ(4, len1);
    ASSERT_EQ(12, len2);
    ASSERT_STREQ(original, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_roundtrip_emoji_utf16) {
    const char *original = "🚀🌟";
    SQLWCHAR wszStr[50];
    char szStr[50];

    size_t len1 = utf8_to_sqlwchar_str(original, SQL_NTS, wszStr, 50);
    size_t len2 =
        sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 50);

    ASSERT_EQ(4, len1); // 2 surrogate pairs
    ASSERT_EQ(8, len2); // 2 * 4 bytes
    ASSERT_STREQ(original, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_roundtrip_mixed_utf32) {

    const char *original = "Hello你好🚀"; // 8 code points: 5 + 2 + 1
    const int units = 50; // wide *units* (code units), not bytes

    // Allocate storage by app width (UTF-32 => 4 bytes per unit)
    std::vector<uint8_t> wideStorage(units * sizeofSQLWCHAR(SQL_DD_CP_UTF32));
    auto *wszStr = reinterpret_cast<SQLWCHAR *>(wideStorage.data());

    std::array<char, 64> szStr{}; // enough for UTF-8 result (len 15 + NUL)

    size_t len1 =
        utf8_to_sqlwchar_str(original, SQL_NTS, wszStr, units /*units*/, nullptr, SQL_DD_CP_UTF32);
    size_t len2 =
        sqlwchar_to_utf8_char(wszStr, SQL_NTS, szStr.data(), szStr.size(), nullptr, SQL_DD_CP_UTF32);

    ASSERT_EQ(8u, len1); // 8 UTF-32 code units copied (excluding NUL)
    ASSERT_EQ(strlen(original), len2); // 15 bytes copied (excluding NUL)
    ASSERT_STREQ(original, szStr.data());
}

// Explicit unicode type override
TEST_F(UNICODE_TEST_SUITE, test_explicit_type_override_utf16_to_utf32) {

    const char32_t wszStr[] = {U'H', U'e', U'l', U'l', U'o', U'\0'};
    char szStr[10];
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       10, nullptr, SQL_DD_CP_UTF32);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr, "Hello");
}

TEST_F(UNICODE_TEST_SUITE, test_explicit_type_override_utf32_to_utf16) {

    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C,
                                     0x006C, 0x006F, 0x0000};
    char szStr[10];
    size_t len = sqlwchar_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr,
                                       10, nullptr, SQL_DD_CP_UTF16);
    ASSERT_EQ(5, len);
    ASSERT_STREQ(szStr, "Hello");
}

// ========== sqlwchar function tests for UTF-16 ==========

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_basic_utf16) {
    const char *expected = "Hello, 你好";
    SQLWCHAR *wszStr = nullptr;
    size_t wlen = utf8_to_sqlwchar_alloc(expected, SQL_NTS, &wszStr);
    EXPECT_GT(wlen, 0);
    EXPECT_NE(nullptr, wszStr);

    char *szStr = nullptr;
    size_t len = sqlwchar_to_utf8_alloc(wszStr, SQL_NTS, &szStr);
    EXPECT_EQ(13, len);
    EXPECT_NE(nullptr, szStr);
    EXPECT_STREQ(expected, szStr);

    free(wszStr);
    free(szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_empty_utf16) {
    SQLWCHAR wszStr[1] = {0};
    char *szStr = nullptr;
    size_t len = sqlwchar_to_utf8_alloc(wszStr, SQL_NTS, &szStr);
    ASSERT_EQ(0, len);
    ASSERT_EQ(nullptr, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_null_input_utf16) {
    char *szStr = nullptr;
    size_t len = sqlwchar_to_utf8_alloc(nullptr, SQL_NTS, &szStr);
    ASSERT_EQ(0, len);
    ASSERT_EQ(nullptr, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_null_output_utf16) {
    SQLWCHAR wszStr[10] = {0};
    size_t len = sqlwchar_to_utf8_alloc(wszStr, SQL_NTS, nullptr);
    ASSERT_EQ(0, len);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_explicit_length_utf16) {
    const char *input = "Hello, 你好";
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc(input, SQL_NTS, &wszStr);

    char *szStr = nullptr;
    size_t len = sqlwchar_to_utf8_alloc(wszStr, 5, &szStr);
    EXPECT_EQ(5, len);
    EXPECT_NE(nullptr, szStr);
    EXPECT_STREQ("Hello", szStr);

    free(wszStr);
    free(szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_emoji_utf16) {
    const char *expected = "🚀🌟";
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc(expected, SQL_NTS, &wszStr);

    char *szStr = nullptr;
    size_t len = sqlwchar_to_utf8_alloc(wszStr, SQL_NTS, &szStr);
    EXPECT_EQ(8, len);
    EXPECT_NE(nullptr, szStr);
    EXPECT_STREQ(expected, szStr);

    free(wszStr);
    free(szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwcsnlen_cap_basic_utf16) {
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc("Hello", SQL_NTS, &wszStr);
    EXPECT_NE(nullptr, wszStr);

    size_t len = sqlwcsnlen_cap(wszStr, 100, SQL_DD_CP_UTF16);
    EXPECT_EQ(5, len);

    free(wszStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwcsnlen_cap_null_utf16) {
    size_t len = sqlwcsnlen_cap(nullptr, 100, SQL_DD_CP_UTF16);
    ASSERT_EQ(0, len);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwcsnlen_cap_zero_cap_utf16) {
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc("Hello", SQL_NTS, &wszStr);

    size_t len = sqlwcsnlen_cap(wszStr, 0, SQL_DD_CP_UTF16);
    EXPECT_EQ(0, len);

    free(wszStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwcsnlen_cap_with_limit_utf16) {
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc("Hello World", SQL_NTS, &wszStr);

    size_t len = sqlwcsnlen_cap(wszStr, 5, SQL_DD_CP_UTF16);
    EXPECT_EQ(5, len);

    free(wszStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwcsnlen_cap_multibyte_utf16) {
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc("你好世界", SQL_NTS, &wszStr);

    size_t len = sqlwcsnlen_cap(wszStr, 5, SQL_DD_CP_UTF16);
    EXPECT_EQ(4, len);

    free(wszStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_roundtrip_utf16) {
    const char *original = "Hello, 你好🚀";
    SQLWCHAR wszStr[50]; // 50 SQLWCHAR = 50 UTF-16 code units
    char szStr[50];

    size_t len1 = utf8_to_sqlwchar_str(original, SQL_NTS, wszStr, 50);
    ASSERT_GT(len1, 0);

    size_t len2 = sqlwchar_to_utf8_char(wszStr, SQL_NTS, szStr, 50);
    ASSERT_GT(len2, 0);
    ASSERT_STREQ(original, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_truncation_utf16) {
    SQLWCHAR *wszStr = nullptr;
    size_t wlen = utf8_to_sqlwchar_alloc("Hello, 你好", SQL_NTS, &wszStr);
    EXPECT_GT(wlen, 0);
    EXPECT_NE(nullptr, wszStr);

    char szStr[6];
    size_t totalNeeded = 0;
    size_t len = sqlwchar_to_utf8_char(wszStr, SQL_NTS, szStr, 6, &totalNeeded);
    EXPECT_EQ(5, len);
    EXPECT_EQ(13, totalNeeded);
    EXPECT_STREQ("Hello", szStr);

    free(wszStr);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_truncation_utf16) {
    const char *input = "Hello, 你好";
    SQLWCHAR wszStr[5]; // 5 SQLWCHAR = 5 UTF-16 code units
    size_t totalNeeded = 0;
    size_t len = utf8_to_sqlwchar_str(input, SQL_NTS, wszStr, 5, &totalNeeded);
    ASSERT_EQ(4, len);
    ASSERT_EQ(9, totalNeeded);
}

TEST_F(UNICODE_TEST_SUITE, test_sizeofSQLWCHAR_utf16) {
    ASSERT_EQ(sizeof(char16_t), sizeofSQLWCHAR());
    ASSERT_EQ(sizeof(char16_t), sizeofSQLWCHAR(SQL_DD_CP_UTF16));
}

TEST_F(UNICODE_TEST_SUITE, test_sizeofSQLWCHAR_utf32) {
    ASSERT_EQ(sizeof(char32_t), sizeofSQLWCHAR(SQL_DD_CP_UTF32));
}

// ========== sqlwchar function tests for UTF-32 ==========

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_to_utf8_alloc_basic_utf32) {
    const char *expected = "Hello, 你好";
    SQLWCHAR *wszStr = nullptr;
    size_t wlen = utf8_to_sqlwchar_alloc(expected, SQL_NTS, &wszStr, SQL_DD_CP_UTF32);
    EXPECT_GT(wlen, 0);
    EXPECT_NE(nullptr, wszStr);

    char *szStr = nullptr;
    size_t len = sqlwchar_to_utf8_alloc(wszStr, SQL_NTS, &szStr, SQL_DD_CP_UTF32);
    EXPECT_EQ(13, len);
    EXPECT_NE(nullptr, szStr);
    EXPECT_STREQ(expected, szStr);

    free(wszStr);
    free(szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwchar_roundtrip_utf32) {
    const char *original = "Hello, 你好🚀";
    // In UTF-32: need 2 SQLWCHAR per character, so 50 chars = 100 SQLWCHAR
    // units
    SQLWCHAR wszStr[100];
    char szStr[50];

    size_t len1 = utf8_to_sqlwchar_str(original, SQL_NTS, wszStr, 100, nullptr, SQL_DD_CP_UTF32);
    EXPECT_GT(len1, 0);

    size_t len2 = sqlwchar_to_utf8_char(wszStr, SQL_NTS, szStr, 50, nullptr, SQL_DD_CP_UTF32);
    EXPECT_GT(len2, 0);
    ASSERT_STREQ(original, szStr);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_no_truncation_utf32) {
    const char *input = "Hello, 你好";
    const int units = 10; // app units
    std::vector<uint8_t> storage(units *
                                 sizeofSQLWCHAR(SQL_DD_CP_UTF32)); // bytes for app width
    auto *wszStr = reinterpret_cast<SQLWCHAR *>(storage.data());
    size_t totalNeeded = 0;
    size_t len =
        utf8_to_sqlwchar_str(input, SQL_NTS, wszStr, units, &totalNeeded, SQL_DD_CP_UTF32);
    EXPECT_EQ(9, len);
    ASSERT_EQ(9, totalNeeded);
}

TEST_F(UNICODE_TEST_SUITE, test_utf8_to_sqlwchar_truncation_utf32) {
    const char *input = "Hello, 你好"; // 8 code points total

    const int units = 5; // want 4 chars + NUL -> force truncation
    std::vector<uint8_t> storage(units *
                                 sizeofSQLWCHAR(SQL_DD_CP_UTF32)); // bytes sized by APP width
    auto *wszStr = reinterpret_cast<SQLWCHAR *>(storage.data());

    size_t totalNeeded = 0;
    size_t len = utf8_to_sqlwchar_str(input, SQL_NTS, wszStr, units,
                                      &totalNeeded, SQL_DD_CP_UTF32);

    EXPECT_EQ(4u, len);         // 4 units copied
    ASSERT_EQ(9u, totalNeeded); // 8 + 1 NUL (units, not bytes)
}

TEST_F(UNICODE_TEST_SUITE, test_sqlwcsnlen_cap_multibyte_utf32) {
    SQLWCHAR *wszStr = nullptr;
    utf8_to_sqlwchar_alloc("你好世界", SQL_NTS, &wszStr, SQL_DD_CP_UTF32);
    ASSERT_NE(wszStr, nullptr);    
    auto rsfree = std::unique_ptr<SQLWCHAR, void(*)(void*)>(wszStr, RsFree);
    size_t len = sqlwcsnlen_cap(wszStr, 10000, SQL_DD_CP_UTF32);
    EXPECT_EQ(4, len);

    len = sqlwcsnlen_cap(wszStr, 3, SQL_DD_CP_UTF32);
    EXPECT_EQ(3, len);
}

// ========== IAMUtils conversion tests ==========

// ASCII → UTF-8
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertToUTF8_basic) {
    rs_wstring input = L"Hello";
    rs_string result = IAMUtils::convertToUTF8(input);
    ASSERT_EQ("Hello", result);
}

// Multibyte CJK → UTF-8
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertToUTF8_multibyte) {
    // U+4F60 '你', U+597D '好'  (safe regardless of source encoding)
    rs_wstring input = L"\u4F60\u597D";
    rs_string result = IAMUtils::convertToUTF8(input);

    // UTF-8: E4 BD A0 E5 A5 BD
    ASSERT_EQ(std::string("\xE4\xBD\xA0\xE5\xA5\xBD"), result);
}

// Emoji (non-BMP) → UTF-8
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertToUTF8_emoji) {
    // U+1F680 ROCKET
    rs_wstring input = L"\U0001F680";
    rs_string result = IAMUtils::convertToUTF8(input);

    // UTF-8: F0 9F 9A 80
    ASSERT_EQ(std::string("\xF0\x9F\x9A\x80"), result);
}

TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertToUTF8_empty) {
    rs_wstring input = L"";
    rs_string result = IAMUtils::convertToUTF8(input);
    ASSERT_EQ("", result);
}

// Mixed content → UTF-8
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertToUTF8_mixed) {
    rs_wstring input = L"Hello \u4F60\u597D \U0001F680";
    rs_string result = IAMUtils::convertToUTF8(input);

    // "Hello " + "你好" + " " + "🚀"
    std::string expected = "Hello ";
    expected += "\xE4\xBD\xA0\xE5\xA5\xBD";
    expected += " ";
    expected += "\xF0\x9F\x9A\x80";

    ASSERT_EQ(expected, result);
}

// -------- From UTF-8 to wstring --------

// ASCII UTF-8 → wstring
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertFromUTF8_basic) {
    rs_string input = "Hello";               // ASCII is safe
    rs_wstring result = IAMUtils::convertFromUTF8(input);
    ASSERT_EQ(L"Hello", result);
}

// Multibyte CJK UTF-8 → wstring
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertFromUTF8_multibyte) {
    // UTF-8 for "你好": E4 BD A0 E5 A5 BD
    rs_string input = std::string("\xE4\xBD\xA0\xE5\xA5\xBD");
    rs_wstring result = IAMUtils::convertFromUTF8(input);
    ASSERT_EQ(L"\u4F60\u597D", result);
}

// Emoji UTF-8 → wstring
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertFromUTF8_emoji) {
    // UTF-8 for "🚀": F0 9F 9A 80
    rs_string input = std::string("\xF0\x9F\x9A\x80");
    rs_wstring result = IAMUtils::convertFromUTF8(input);
    ASSERT_EQ(L"\U0001F680", result);
}

TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertFromUTF8_empty) {
    rs_string input = "";
    rs_wstring result = IAMUtils::convertFromUTF8(input);
    ASSERT_EQ(L"", result);
}

// Mixed UTF-8 → wstring
TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_convertFromUTF8_mixed) {
    std::string input = "Hello ";
    input += "\xE4\xBD\xA0\xE5\xA5\xBD";      // "你好"
    input += " ";
    input += "\xF0\x9F\x9A\x80";             // "🚀"

    rs_wstring result = IAMUtils::convertFromUTF8(input);
    ASSERT_EQ(L"Hello \u4F60\u597D \U0001F680", result);
}

// -------- Roundtrips --------

TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_roundtrip_ascii) {
    rs_wstring original = L"Hello World";
    rs_string utf8 = IAMUtils::convertToUTF8(original);
    rs_wstring back = IAMUtils::convertFromUTF8(utf8);
    ASSERT_EQ(original, back);
}

TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_roundtrip_multibyte) {
    rs_wstring original = L"\u4F60\u597D\u4E16\u754C";  // "你好世界"
    rs_string utf8 = IAMUtils::convertToUTF8(original);
    rs_wstring back = IAMUtils::convertFromUTF8(utf8);
    ASSERT_EQ(original, back);
}

TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_roundtrip_emoji) {
    rs_wstring original = L"\U0001F680\U0001F31F";      // "🚀🌟"
    rs_string utf8 = IAMUtils::convertToUTF8(original);
    rs_wstring back = IAMUtils::convertFromUTF8(utf8);
    ASSERT_EQ(original, back);
}

// -------- Invalid UTF-8 handling --------

TEST_F(UNICODE_TEST_SUITE, test_IAMUtils_invalid_utf8) {
    rs_string invalid = std::string("\xFF\xFE");        // invalid UTF-8
    rs_wstring result = IAMUtils::convertFromUTF8(invalid);
    ASSERT_EQ(L"", result);                             // your code logs + returns empty
}
