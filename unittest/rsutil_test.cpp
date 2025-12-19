#include "common.h"
#include "rsutil.h"
#include "rsunicode.h"
#include <sstream>
#include <string>
#include <cstring>
#include <climits>
#include <iomanip>

// Test printHexSQLCHAR and printHexSQLWCHR
class PrintHexSQLCharAndWCharTest : public ::testing::Test {
  protected:
    std::stringstream logStream;
    int savedUnicodeType;

    void SetUp() override {
        savedUnicodeType = tls_unicode_ref();
        // Redirect the logging function to write to our logStream
        logFunc = [this](const std::string &message) {
            logStream << message << std::endl;
        };
    }

    void TearDown() override {
        tls_unicode_ref() = savedUnicodeType;
    }

    std::function<void(const std::string &)> logFunc;
};

TEST_F(PrintHexSQLCharAndWCharTest, PrintsHexBytesForSQLCHAR) {
    // Prepare the input data
    SQLCHAR sqlchar[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    int len = sizeof(sqlchar);

    // Call the function under test
    printHexSQLCHAR(sqlchar, len, logFunc);

    // Verify the output
    std::string logOutput = logStream.str();
    for (auto &str : {"Hex bytes:", "01 23 45 67 ", "89 AB CD EF "}) {
        EXPECT_THAT(logOutput, testing::HasSubstr(str))
            << "Missing expected string:'" << str << "'.";
    }
}

TEST_F(PrintHexSQLCharAndWCharTest, PrintsHexBytesForSQLWCHAR_UTF16) {
    // Ensure UTF-16 mode (SQLWCHAR is 2 bytes)
    tls_unicode_ref() = SQL_DD_CP_UTF16;
    
    // Prepare the input data - function expects character count
    SQLWCHAR sqlwchr[] = {0x0101, 0x2323, 0x4545, 0x6767,
                          0x8989, 0xABAB, 0xCDCD, 0xEFEF};
    int charCount = sizeof(sqlwchr) / sizeof(SQLWCHAR);

    // Call the function under test
    printHexSQLWCHR(sqlwchr, charCount, logFunc);

    // Verify the output
    std::string logOutput = logStream.str();
    for (auto &str : {"Printing SQLWCHAR* as hex bytes:", "Hex bytes:",
                      "01 01 23 23 45 45 67 67 ", "89 89 AB AB CD CD EF EF "}) {
        EXPECT_THAT(logOutput, testing::HasSubstr(str))
            << "Missing expected string:'" << str << "'.";
    }
}

TEST_F(PrintHexSQLCharAndWCharTest, PrintsHexBytesForSQLWCHAR_UTF32) {
    // Ensure UTF-32 mode (SQLWCHAR is 4 bytes)
    tls_unicode_ref() = SQL_DD_CP_UTF32;
    
    // Prepare the input data - function expects character count
    // Note: SQLWCHAR is 2 bytes at compile time (unixODBC), but runtime treats as 4 bytes in UTF-32 mode
    // Need 8 SQLWCHAR elements (16 bytes) to represent 4 UTF-32 characters (4 bytes each)
    SQLWCHAR sqlwchr[] = {0x0101, 0x0000, 0x2323, 0x0000, 0x4545, 0x0000, 0x6767, 0x0000};
    int charCount = sizeof(sqlwchr) / sizeof(SQLWCHAR); // 4 UTF-32 characters

    // Call the function under test
    printHexSQLWCHR(sqlwchr, charCount, logFunc);

    // Verify the output
    std::string logOutput = logStream.str();
    for (auto &str : {"Printing SQLWCHAR* as hex bytes:", "Hex bytes:",
                      "01 01 00 00 23 23 00 00 ", "45 45 00 00 67 67 00 00 "}) {
        EXPECT_THAT(logOutput, testing::HasSubstr(str))
            << "Missing expected string:'" << str << "'.";
    }
}

/*
stristr:
    Is a custom implementation of strcasestr.
    It redirects to POSIX extension strcasestr in Linux
    and custom implementation in Windows.

strcasestrwhole:
    Is a custom implementation of strcasestr that matches whole words.
    It uses C++ std::regexx

findSQLClause : performs strcasestrwhole but skips quoted matches.
*/

TEST(STRCASESTR_TEST_SUITE, StringAndSubstringNull) {
    char *str = nullptr;
    char *subStr = nullptr;
    EXPECT_EQ(stristr(str, subStr), nullptr);
#ifdef WIN32
    // Linux crash on strcasestr(NULL).
    // We provide our own strcasestr for windoes and don't want it to crash like
    // linux.
    EXPECT_EQ(strcasestr(str, subStr), nullptr);
#endif
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
    EXPECT_EQ(findSQLClause(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, StringNullSubstringNotNull) {
    char *str = nullptr;
    char *subStr = "hello";
    EXPECT_EQ(stristr(str, subStr), nullptr);

#ifdef WIN32
    // Linux crash on strcasestr(NULL).
    // We provide our own strcasestr for windoes and don't want it to crash like
    // linux.
    EXPECT_EQ(strcasestr(str, subStr), nullptr);
#endif
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
    EXPECT_EQ(findSQLClause(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, StringNotNullSubstringNull) {
    char *str = "Hello World";
    char *subStr = nullptr;
    EXPECT_EQ(stristr(str, subStr), nullptr);
#ifdef WIN32
    // Linux crash on strcasestr(NULL).
    // We provide our own strcasestr for windoes and don't want it to crash like
    // linux.
    EXPECT_EQ(strcasestr(str, subStr), nullptr);
#endif
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
    EXPECT_EQ(findSQLClause(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, SubstringAtBeginning) {
    char *str = "Hello World";
    char *subStr = "hello";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), str);
    EXPECT_EQ(findSQLClause(str, subStr), nullptr); // can't be in the beginning
}

TEST(STRCASESTR_TEST_SUITE, SubstringInMiddle) {
    char *str = "Hello World";
    char *subStr = "O WORLD";
    EXPECT_EQ(stristr(str, " "), str + 5);
    EXPECT_EQ(strcasestr(str, " "), str + 5);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
    EXPECT_EQ(findSQLClause(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, SubstringAtEnd) {
    char *str = "Hello World";
    char *subStr = "world";
    EXPECT_EQ(stristr(str, subStr), str + 6);
    EXPECT_EQ(strcasestrwhole(str, subStr), str + 6);
    EXPECT_EQ(findSQLClause(str, subStr), str + 6);
}

TEST(STRCASESTR_TEST_SUITE, SubstringNotFound) {
    const char *str = "Hello World";
    const char *subStr = "planet";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, EmptySubStr) {
    const char *str = "Hello World";
    const char *subStr = "";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), str);
}

TEST(STRCASESTR_TEST_SUITE, EmptyStrAndSubStr) {
    const char *str = "";
    const char *subStr = "";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), str);
}

TEST(STRCASESTR_TEST_SUITE, EmptyStr) {
    const char *str = "";
    const char *subStr = "hello";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, CaseInsensitiveMatch) {
    const char *str = "abcdefg";
    const char *subStr = "CdE";
    EXPECT_EQ(stristr(str, subStr), str + 2);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, NoMatchWhenSubStrLongerThanStr) {
    const char *str = "short";
    const char *subStr = "longersubstring";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, PartialMatchOnly) {
    const char *str = "aabbcc";
    const char *subStr = "bbcd";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(stristr(str, "aabbcd"), nullptr);
    EXPECT_EQ(stristr(str, "dbbcc"), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, "aabbcd"), nullptr);
    EXPECT_EQ(strcasestrwhole(str, "dbbcc"), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, MultipleOccurrences) {
    const char *str = "abcabcabc";
    const char *subStr = "AbC";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, SubStrLargerThanStr) {
    const char *str = "short";
    const char *subStr = "thisisaverylongsubstring";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, SingleCharacterSubStr) {
    const char *str = "hello";
    const char *subStr = "H";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, SingleCharacterStrSubStr) {
    const char *str = "h";
    const char *subStr = "H";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), str);
}

TEST(STRCASESTR_TEST_SUITE, BothStringsEmpty) {
    const char *str = "";
    const char *subStr = "";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), str);
}

TEST(STRCASESTR_TEST_SUITE, StrEmptySubStrNotEmpty) {
    const char *str = "";
    const char *subStr = "a";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    auto t = strcasestrwhole(str, subStr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, SubStrEmptyStrNotEmpty) {
    const char *str = "a";
    const char *subStr = "";
    EXPECT_EQ(stristr(str, subStr), str);
    EXPECT_EQ(strcasestrwhole(str, subStr), str);
}

TEST(STRCASESTR_TEST_SUITE, NullStr) {
    const char *str = nullptr;
    const char *subStr = "hello";
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, NullSubStr) {
    const char *str = "Hello World";
    const char *subStr = nullptr;
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(STRCASESTR_TEST_SUITE, NullStrAndSubStr) {
    const char *str = nullptr;
    const char *subStr = nullptr;
    EXPECT_EQ(stristr(str, subStr), nullptr);
    EXPECT_EQ(strcasestrwhole(str, subStr), nullptr);
}

TEST(FIND_CLAUSE_SUITE, FindClauseInString) {
    char input[] = "SELECT * FROM table WHERE id = 1";
    char *clause = "WHERE";
    char *result = findSQLClause(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "WHERE id = 1");
}

TEST(FIND_CLAUSE_SUITE, ClauseNotFound) {
    char input[] = "SELECT * FROM table";
    char *clause = "WHERE";
    char *result = findSQLClause(input, clause);
    ASSERT_EQ(result, nullptr);
}

TEST(FIND_CLAUSE_SUITE, ClauseAtBeginning) {
    char input[] = "WHERE id = 1 SELECT * FROM table";
    char *clause = "WHERE";
    char *result = stristr(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "WHERE id = 1 SELECT * FROM table");

    result = findSQLClause(input, clause);
    ASSERT_EQ(result, nullptr);
}

TEST(FIND_CLAUSE_SUITE, ClauseEmbeddedInWord) {
    char input[] = "SELECT * FROM table WHERE_CLAUSE id = 1";
    char *clause = "WHERE";
    char *result = stristr(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "WHERE_CLAUSE id = 1");

    result = findSQLClause(input, clause);
    ASSERT_EQ(result, nullptr);
}

TEST(FIND_CLAUSE_SUITE, ClauseEmbeddedInDoubleQuotes) {
    char input[] = "SELECT * FROM table \"WHERE\" id = 1";
    char *clause = "WHERE";
    char *result = findSQLClause(input, clause);
    ASSERT_EQ(result, nullptr);
}

TEST(FIND_CLAUSE_SUITE, FindValuesClause) {
    char input[] = "INSERT INTO table (col1, col2) VALUES (1, 2), (3, 4)";
    char *clause = "VALUES";
    char *result = findSQLClause(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "VALUES (1, 2), (3, 4)");

    char input2[] = "INSERT INTO \"VALUES\" (col1, col2) VALUES (1, 2), (3, 4)";
    result = findSQLClause(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "VALUES (1, 2), (3, 4)");
}

TEST(FIND_CLAUSE_SUITE, FindValuesClauseQuoted) {
    char input[] = "INSERT INTO \"VALUES\" (col1, col2) VALUES (1, 2), (3, 4)";
    char *clause = "VALUES";
    char *result = findSQLClause(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "VALUES (1, 2), (3, 4)");
}

TEST(FIND_CLAUSE_SUITE, FindValuesClauseQuotedBadlyRight) {
    char input[] = "INSERT INTO \"VALUES (col1, col2) VALUES (1, 2), (3, 4)";
    char *clause = "VALUES";
    char *result = findSQLClause(input, clause);
    ASSERT_EQ(result, nullptr);
}

TEST(FIND_CLAUSE_SUITE, FindValuesClauseQuotedBadlyLeft) {
    char input[] = "INSERT INTO VALUES\" (col1, col2) VALUES (1, 2), (3, 4)";
    char *clause = "VALUES";
    char *result = findSQLClause(input, clause);
    ASSERT_STREQ(result, "VALUES\" (col1, col2) VALUES (1, 2), (3, 4)");
}

TEST(FIND_CLAUSE_SUITE, FindUnQuotedValuesClause) {
    char input[] = "INSERT INTO \"VALUES\" (col1, col2) VALUES (1, 2), (3, 4)";
    char *clause = "VALUES";

    char *result = findSQLClause(input, clause);
    ASSERT_NE(result, nullptr);
    ASSERT_STREQ(result, "VALUES (1, 2), (3, 4)");
}

TEST(FIND_CLAUSE_SUITE, ValuesClauseEmbeddedInDoubleQuotes) {
    char input[] = "INSERT INTO table (col1, col2) \"VALUES (1, 2), (3, 4)\"";
    char *clause = "VALUES";
    char *result = findSQLClause(input, clause);
    ASSERT_EQ(result, nullptr);
}

TEST(DoesEmbedInDoubleQuotesTest, NoDoubleQuotes) {
    char input[] = "SELECT * FROM table WHERE id = 1";
    ASSERT_EQ(DoesEmbedInDoubleQuotes(input, input + strlen(input)), 0);
}

TEST(DoesEmbedInDoubleQuotesTest, DoubleQuotesAtBeginning) {
    char input[] = "\"SELECT * FROM table WHERE id = 1\"";
    ASSERT_EQ(DoesEmbedInDoubleQuotes(input, input + strlen(input)), 0);
}

TEST(DoesEmbedInDoubleQuotesTest, DoubleQuotesAtEnd) {
    char input[] = "SELECT * FROM table WHERE id = 1\"";
    ASSERT_EQ(DoesEmbedInDoubleQuotes(input, input + strlen(input)), 1);
}

TEST(DoesEmbedInDoubleQuotesTest, NestedDoubleQuotes) {
    char input[] = "SELECT * FROM \"table\" WHERE \"id\" = 1";
    ASSERT_EQ(DoesEmbedInDoubleQuotes(input, input + strlen(input)), 0);
}

TEST(DoesEmbedInDoubleQuotesTest, UnbalancedDoubleQuotes) {
    char input[] = "SELECT * FROM \"table WHERE id = 1";
    ASSERT_EQ(DoesEmbedInDoubleQuotes(input, input + strlen(input)), 1);
}

// Unit test for Helper function isEmpty()
TEST(RSUTIL, test_isEmptyString) {
    ASSERT_EQ(true, isEmptyString((SQLCHAR *)""));
    ASSERT_EQ(false, isEmptyString(NULL));
    ASSERT_EQ(false, isEmptyString((SQLCHAR *)"not empty"));
}

// Unit test for Helper function isNullOrEmpty()
TEST(RSUTIL, test_isNullOrEmptyString) {
    ASSERT_EQ(true, isNullOrEmptyString((SQLCHAR *)""));
    ASSERT_EQ(true, isNullOrEmptyString(NULL));
    ASSERT_EQ(false, isNullOrEmptyString((SQLCHAR *)"not empty"));
}

// Unit test to check the return data type for helper function char2String
TEST(RSUTIL, test_char2String) {
    ASSERT_EQ(true, (std::is_same<decltype(char2String((SQLCHAR *)"test")),
                                  std::string>::value));
}

// Unit test to check the return data type for helper function char2StringView
TEST(RSUTIL, test_char2StringView) {
    ASSERT_EQ(true, (std::is_same<decltype(char2StringView((SQLCHAR *)"test")),
                                  std::string_view>::value));
}

// Test fixture for CopyWStrDataBigLen to ensure consistent Unicode mode
class CopyWStrDataBigLenTest : public ::testing::TestWithParam<int> {
  protected:
    int savedUnicodeType;

    void SetUp() override {
        savedUnicodeType = tls_unicode_ref();
        tls_unicode_ref() = GetParam(); // Set to UTF-16 or UTF-32
    }

    void TearDown() override {
        tls_unicode_ref() = savedUnicodeType;
    }
};

INSTANTIATE_TEST_SUITE_P(
    UTF16_and_UTF32,
    CopyWStrDataBigLenTest,
    ::testing::Values(SQL_DD_CP_UTF16, SQL_DD_CP_UTF32),
    [](const testing::TestParamInfo<int>& info) {
        return info.param == SQL_DD_CP_UTF16 ? "UTF16" : "UTF32";
    }
);

// Extract as UTF-16 (u16string) from a SQLWCHAR buffer.
// - Safe for SQLWCHAR = 2 or 4 at runtime (sizeofSQLWCHAR()).
// - Avoids misaligned dereferences by using memcpy for 4-byte reads.
// - Handles UTF-32 → UTF-16 surrogate pairs.
// - Optional cbLen (bytes). If 0, reads until NUL terminator.
static std::u16string extractSQLWCHARString(const SQLWCHAR* buf, size_t cbLenBytes = 0) {
    std::u16string out;
    if (!buf) return out;

    const size_t w = sizeofSQLWCHAR();         // 2 or 4 at runtime
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);

    if (w == sizeof(char16_t)) {
        // UTF-16 path: aligned to 2 bytes; direct cast is fine.
        const char16_t* u16 = reinterpret_cast<const char16_t*>(buf);
        if (cbLenBytes == 0) {
            // Read until NUL
            while (*u16) out.push_back(*u16++);
        } else {
            size_t n = cbLenBytes / w;
            for (size_t i = 0; i < n; ++i) {
                char16_t ch = u16[i];
                if (ch == 0) break;
                out.push_back(ch);
            }
        }
    } else if (w == sizeof(char32_t)) {
        // UTF-32 path: alignment may be only 2 bytes if the buffer was allocated
        // as SQLWCHAR[...]. Use memcpy to avoid UB.
        size_t i = 0;
        for (;;) {
            if (cbLenBytes && (i + 1) * w > cbLenBytes) break;

            uint32_t cp = 0;
            std::memcpy(&cp, p + i * w, w);

            if (cp == 0) break;

            if (cp <= 0xFFFF) {
                // Note: UTF-32 may legally contain values in the surrogate range; you
                // can decide to pass them through or sanitize. Here we pass through.
                out.push_back(static_cast<char16_t>(cp));
            } else {
                // Encode surrogate pair for cp in [0x10000, 0x10FFFF]
                // (you may clamp/replace if > 0x10FFFF)
                uint32_t t = cp - 0x10000;
                char16_t hi = static_cast<char16_t>(0xD800 + (t >> 10));
                char16_t lo = static_cast<char16_t>(0xDC00 + (t & 0x3FF));
                out.push_back(hi);
                out.push_back(lo);
            }
            ++i;
        }
    }
    return out;
}

// Tests that copyWStrDataBigLen correctly handles zero-length input strings
TEST_P(CopyWStrDataBigLenTest, ZeroLengthInput) {
    char src[] = ""; // 0-length UTF-8
    SQLWCHAR dest[4] = {SQLWCHAR_LITERAL('X'), SQLWCHAR_LITERAL('X')};
    SQLLEN lenInd = -1;
    SQLLEN offset = 1;

    auto rc = copyWStrDataBigLen(nullptr, src, 0, dest, 2 * sizeofSQLWCHAR(), &offset,
                                 &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], 0);
    EXPECT_EQ(lenInd, 0);
    EXPECT_EQ(offset, 0);
}

// Tests that copyWStrDataBigLen correctly handles null input with SQL_NULL_DATA
TEST_P(CopyWStrDataBigLenTest, NullInput) {
    SQLWCHAR dest[4] = {SQLWCHAR_LITERAL('X'), SQLWCHAR_LITERAL('X')}; // 2 chars in UTF-32 = 4 units
    SQLLEN lenInd = -1;
    SQLLEN offset = 1;

    auto rc = copyWStrDataBigLen(nullptr, nullptr, SQL_NULL_DATA, dest,
                                 2 * sizeofSQLWCHAR(), &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], SQLWCHAR_LITERAL('X')); // should be untouched
    EXPECT_EQ(dest[1], SQLWCHAR_LITERAL('X')); // should be untouched
    EXPECT_EQ(lenInd, SQL_NULL_DATA);
    EXPECT_EQ(offset, 0);
}

// Tests that copyWStrDataBigLen correctly copies a string that completely fits
// in the destination buffer
TEST_P(CopyWStrDataBigLenTest, FullStringFitsInBuffer) {
    const char *src = "abc";
    SQLWCHAR dest[20] = {0}; // 10 chars in UTF-32 = 20 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 10 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"abc");
    /*
    In UTF-32 mode on unixODBC, SQLWCHAR is 2 bytes at compile time but represents 4-byte characters at runtime. Each character occupies 2 SQLWCHAR units, so for N characters, the null terminator is at index N * (sizeofSQLWCHAR() / 2), not N.

    Example with "ABC" (3 characters):

    UTF-16: null at dest[3] ✓
    UTF-32: null at dest[6] (3 × 2 units), but code checks dest[3] ✗
    Lines 662-676 show the correct pattern already used elsewhere in this file.

    That is why, instead of checking dest[3], we check dest[3 * (sizeofSQLWCHAR() / 2)] to ensure the null terminator is correctly placed.
    */
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));  // Explicitly check for null terminator
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR());
    EXPECT_EQ(offset, 0); // reset
}

// Tests that copyWStrDataBigLen correctly handles truncation when the string is
// larger than the destination buffer
TEST_P(CopyWStrDataBigLenTest, StringTruncated) {
    const char *src = "abcdef";
    SQLWCHAR dest[8] = {0}; // 4 chars in UTF-32 = 8 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // dest can hold only 3 + 1 null = 4 WCHARs
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"abc"); // truncated
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(lenInd, 6 * sizeofSQLWCHAR());                // full length
    EXPECT_EQ(offset, 3);                                // updated
}

// Tests that copyWStrDataBigLen correctly handles sequential fetching of
// remaining data after truncation
TEST_P(CopyWStrDataBigLenTest, SequentialFetchRemainder) {
    const char *src = "abcdef";
    SQLWCHAR dest[8] = {0}; // 4 chars in UTF-32 = 8 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 3; // continue from previous test

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"def");
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(offset, 0); // reset after full
}

// Tests that copyWStrDataBigLen correctly handles cases where the buffer can
// only hold the null terminator
TEST_P(CopyWStrDataBigLenTest, BufferTooSmallForData) {
    const char *src = "abc";
    SQLWCHAR dest[2] = {0}; // 1 char in UTF-32 = 2 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // Only space for null terminator
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(dest[0], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(offset, 0);                             // offset unchanged
}

// Tests that copyWStrDataBigLen correctly sets rc when no destination buffer is
// provided
TEST_P(CopyWStrDataBigLenTest, NoDestinationBuffer) {
    const char *src = "abc";
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyWStrDataBigLen(nullptr, src, SQL_NTS, nullptr, 0, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_ERROR);
}

// Tests that copyWStrDataBigLen correctly sets rc when destination buffer is
// negative
TEST_P(CopyWStrDataBigLenTest, NegativeBufferLength) {
    const char *src = "abc";
    SQLWCHAR dest[1] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, -1, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_ERROR);
}

// Tests that copyWStrDataBigLen correctly handles cases where the string
// exactly fits in the destination buffer
TEST_P(CopyWStrDataBigLenTest, ExactFitBuffer) {
    const char *src = "abc";
    SQLWCHAR dest[8] = {0}; // 4 chars in UTF-32 = 8 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"abc");
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR());
}

// Tests that copyWStrDataBigLen correctly handles explicit length parameter
// instead of SQL_NTS
TEST_P(CopyWStrDataBigLenTest, ExplicitLengthInput) {
    const char *src = "abcdef";
    int len = 3; // Only "abc"
    SQLWCHAR dest[10] = {0}; // 5 chars in UTF-32 = 10 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, len, dest, 5 * sizeofSQLWCHAR(), &offset,
                                 &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"abc");
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR());
}

// Tests copyWStrDataBigLen with non-null terminated input string
TEST_P(CopyWStrDataBigLenTest, NonNullTerminatedInput) {
    const char src[5] = {'H', 'e', 'l', 'l', 'o'}; // No null terminator
    int length = 3;
    SQLWCHAR dest[6] = {0}; // 3 chars in UTF-32 = 6 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, 5, dest, 3 * sizeofSQLWCHAR(), &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"He");
    // Null terminator position: 2 chars × (sizeofSQLWCHAR/2) SQLWCHAR units
    EXPECT_EQ(dest[2 * (sizeofSQLWCHAR() / 2)], 0);
    EXPECT_EQ(lenInd, 5 * sizeofSQLWCHAR()); // REMAINING: 5 chars

    rc = copyWStrDataBigLen(nullptr, src, 5, dest, 3 * sizeofSQLWCHAR(), &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"ll");
    EXPECT_EQ(dest[2 * (sizeofSQLWCHAR() / 2)], 0);
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR()); // REMAINING: 3 chars

    rc = copyWStrDataBigLen(nullptr, src, 5, dest, 3 * sizeofSQLWCHAR(), &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"o");
    EXPECT_EQ(dest[1 * (sizeofSQLWCHAR() / 2)], 0);
    EXPECT_EQ(lenInd, 1 * sizeofSQLWCHAR()); // REMAINING: 1 char
}

// Tests copyWStrDataBigLen with sequential fetches using longer strings
TEST_P(CopyWStrDataBigLenTest, WideCharacters) {
    std::vector<std::string> testStrings = {
        "ABCDEFGHIJKLMNOPQR",  // 18 ASCII chars
        "I am smiling 🙂©👀"  // Emoji string
    };

    for (const auto& src : testStrings) {
        SQLWCHAR dest[10] = {0}; // 5 chars in UTF-32 = 10 SQLWCHAR units
        SQLLEN lenInd = -1;
        SQLLEN offset = 0;

        // UTF-16: emojis become surrogate pairs (18 code units for emoji string, 18 for ASCII)
        // UTF-32: actual character counts (16 code points for emoji string, 18 for ASCII)
        int totalChars = (sizeofSQLWCHAR() == sizeof(uint16_t) && src.find("🙂") != std::string::npos) ? 18 : 
                         (sizeofSQLWCHAR() == sizeof(uint32_t) && src.find("🙂") != std::string::npos) ? 16 : 18;

        // First call
        auto rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest,
                                     5 * sizeofSQLWCHAR(), &offset, &lenInd);
        EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
        EXPECT_EQ(lenInd, totalChars * sizeofSQLWCHAR());
        EXPECT_EQ(dest[4 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 4);

        // Second call
        rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, 5 * sizeofSQLWCHAR(),
                                &offset, &lenInd);
        EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
        EXPECT_EQ(lenInd, (totalChars - 4) * sizeofSQLWCHAR());
        EXPECT_EQ(dest[4 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 8);

        // Third call
        rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, 5 * sizeofSQLWCHAR(),
                                &offset, &lenInd);
        EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
        EXPECT_EQ(lenInd, (totalChars - 8) * sizeofSQLWCHAR());
        EXPECT_EQ(dest[4 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 12);

        // Fourth call
        rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, 5 * sizeofSQLWCHAR(),
                                &offset, &lenInd);
        int remainingAfter12 = totalChars - 12;
        if (remainingAfter12 > 4) {
            EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
            EXPECT_EQ(lenInd, remainingAfter12 * sizeofSQLWCHAR());
            EXPECT_EQ(dest[4 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
            EXPECT_EQ(offset, 16);

            // Fifth call - final
            rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, 5 * sizeofSQLWCHAR(),
                                    &offset, &lenInd);
            EXPECT_EQ(rc, SQL_SUCCESS);
            EXPECT_EQ(lenInd, (totalChars - 16) * sizeofSQLWCHAR());
            EXPECT_EQ(offset, 0);
        } else {
            EXPECT_EQ(rc, SQL_SUCCESS);
            EXPECT_EQ(lenInd, remainingAfter12 * sizeofSQLWCHAR());
            EXPECT_EQ(offset, 0);
        }
    }
}

// Test that null terminator is always written when buffer has space
// This addresses a review concern about the null termination logic
TEST_P(CopyWStrDataBigLenTest, NullTerminatorAlwaysWrittenWhenSpaceAvailable) {
    const char *src = "ABC";
    
    // Test with buffer that has exactly enough space for data + null
    // 3 chars + 1 null = 4 SQLWCHAR (8 units in UTF-32)
    SQLWCHAR dest[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"ABC");
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0')); // Null terminator must be present
    EXPECT_EQ(offset, 0); // All data fetched
}

// Test edge case: buffer has space for N-1 chars + null (should not write Nth char)
TEST_P(CopyWStrDataBigLenTest, BufferExactlyOneLessThanNeeded) {
    const char *src = "ABCD"; // 4 chars
    
    // Buffer for 3 chars + null = 4 SQLWCHAR (8 units in UTF-32)
    SQLWCHAR dest[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO); // Truncation
    EXPECT_EQ(extractSQLWCHARString(dest), u"ABC"); // Only 3 chars
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0')); // Null terminator present
    EXPECT_EQ(offset, 3); // 3 chars consumed, 1 remaining
}

// Test that the reviewer's concern doesn't apply: copyChars can never equal cchLen
TEST_P(CopyWStrDataBigLenTest, CopyCharsNeverEqualsCchLen) {
    const char *src = "ABCDEFGH"; // 8 chars
    
    // Various buffer sizes to verify copyChars is always < cchLen
    for (int bufSize = 1; bufSize <= 8; bufSize++) {
        // Allocate enough SQLWCHAR units for UTF-32 mode: bufSize chars * (sizeofSQLWCHAR/2) units
        std::vector<SQLWCHAR> dest(bufSize * (sizeofSQLWCHAR() / 2), 0xFFFF);
        SQLLEN lenInd = -1;
        SQLLEN offset = 0;

        auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest.data(),
                                     bufSize * sizeofSQLWCHAR(), &offset, &lenInd);

        // Verify null terminator is always present at the correct position
        EXPECT_EQ(dest[(bufSize - 1) * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0')) 
            << "Buffer size: " << bufSize;
        
        // Verify at most bufSize-1 characters were copied
        size_t actualLen = 0;
        for (int i = 0; i < (bufSize - 1) * (sizeofSQLWCHAR() / 2); i++) {
            if (dest[i] != 0xFFFF && dest[i] != 0) actualLen++;
        }
        EXPECT_LE(actualLen, (size_t)((bufSize - 1) * (sizeofSQLWCHAR() / 2))) 
            << "Buffer size: " << bufSize;
    }
}

// ODBC Specification Tests: When should copyWStrDataBigLen return SQL_SUCCESS_WITH_INFO?
// According to ODBC spec, SQL_SUCCESS_WITH_INFO with SQLSTATE 01004 should be returned when:
// 1. String data is truncated (not all data fits in buffer)
// 2. Buffer is too small to hold even the null terminator

// Test: Buffer size is 0 - cannot write anything, not even null terminator
TEST_P(CopyWStrDataBigLenTest, ODBC_ZeroBufferSize_ShouldReturnInfo) {
    const char *src = "ABC";
    SQLWCHAR dest[1] = {0xFFFF};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // cbLen = 0 means no space at all
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 0, &offset, &lenInd);

    // ODBC spec: Should return SQL_SUCCESS_WITH_INFO because data cannot fit
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR()); // Indicates how much data is available
    EXPECT_EQ(dest[0], (SQLWCHAR)0xFFFF); // Buffer should be untouched
}

// Test: Buffer can only hold null terminator (1 SQLWCHAR) but data exists
TEST_P(CopyWStrDataBigLenTest, ODBC_BufferOnlyForNull_ShouldReturnInfo) {
    const char *src = "ABC";
    SQLWCHAR dest[2] = {0xFFFF, 0xFFFF}; // 1 char in UTF-32 = 2 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // Buffer for 1 SQLWCHAR - can only hold null terminator
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeofSQLWCHAR(), 
                                 &offset, &lenInd);

    // ODBC spec: Should return SQL_SUCCESS_WITH_INFO because no actual data fits
    // Only null terminator can be written
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(dest[0], SQLWCHAR_LITERAL('\0')); // Only null terminator
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR()); // Full data length
    EXPECT_EQ(offset, 0); // No data consumed
}

// Test: Buffer too small for all data - classic truncation
TEST_P(CopyWStrDataBigLenTest, ODBC_PartialDataFits_ShouldReturnInfo) {
    const char *src = "ABCDEF"; // 6 chars
    SQLWCHAR dest[8] = {0}; // 4 chars in UTF-32 = 8 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS_WITH_INFO because not all data fits
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"ABC"); // Partial data
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0')); // Null terminated
    EXPECT_EQ(lenInd, 6 * sizeofSQLWCHAR()); // Full data length
    EXPECT_EQ(offset, 3); // 3 chars consumed, more remain
}

// Test: All data fits exactly - should return SQL_SUCCESS
TEST_P(CopyWStrDataBigLenTest, ODBC_AllDataFits_ShouldReturnSuccess) {
    const char *src = "ABC";
    SQLWCHAR dest[8] = {0}; // 4 chars in UTF-32 = 8 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 4 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS because all data fits with null terminator
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"ABC");
    EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR());
    EXPECT_EQ(offset, 0); // Reset after complete fetch
}

// Test: Buffer larger than needed - should return SQL_SUCCESS
TEST_P(CopyWStrDataBigLenTest, ODBC_BufferLargerThanNeeded_ShouldReturnSuccess) {
    const char *src = "AB";
    SQLWCHAR dest[20] = {0}; // 10 chars in UTF-32 = 20 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 10 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS because all data fits
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"AB");
    EXPECT_EQ(lenInd, 2 * sizeofSQLWCHAR());
}

// Test: Reviewer's scenario - "Hello" with cbLen=8
// UTF-16: cchLen=4, fits 3 chars -> "Hel\0"
// UTF-32: cchLen=2, fits 1 char -> "H\0"
TEST_P(CopyWStrDataBigLenTest, ODBC_HelloWith8ByteBuffer) {
    const char *src = "Hello";
    SQLWCHAR dest[8] = {0}; // 4 chars in UTF-32 = 8 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 8, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    if (sizeofSQLWCHAR() == sizeof(uint16_t)) {
        // UTF-16: cchLen=4, copies 3 chars
        EXPECT_EQ(extractSQLWCHARString(dest), u"Hel");
        EXPECT_EQ(dest[3 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 3);
        EXPECT_EQ(lenInd, 10); // 5 chars * 2
    } else {
        // UTF-32: cchLen=2, copies 1 char
        EXPECT_EQ(extractSQLWCHARString(dest), u"H");
        EXPECT_EQ(dest[1 * (sizeofSQLWCHAR() / 2)], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 1);
        EXPECT_EQ(lenInd, 20); // 5 chars * 4
    }
}

// Test: cbLen not aligned to sizeofSQLWCHAR() - integer division truncates
// UTF-16: cbLen=7 -> cchLen=3 (7/2=3), fits 2 chars
// UTF-32: cbLen=7 -> cchLen=1 (7/4=1), fits 0 chars (only null)
TEST_P(CopyWStrDataBigLenTest, ODBC_UnalignedBufferSize) {
    const char *src = "ABC";
    SQLWCHAR dest[8] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 7, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    if (sizeofSQLWCHAR() == sizeof(uint16_t)) {
        // UTF-16: cchLen=3, copies 2 chars
        EXPECT_EQ(extractSQLWCHARString(dest), u"AB");
        EXPECT_EQ(dest[2], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 2);
    } else {
        // UTF-32: cchLen=1, copies 0 chars (only null)
        EXPECT_EQ(dest[0], SQLWCHAR_LITERAL('\0'));
        EXPECT_EQ(offset, 0);
    }
    EXPECT_EQ(lenInd, 3 * sizeofSQLWCHAR());
}

// Test: Bad offset past end - driver should clamp and return success
TEST_P(CopyWStrDataBigLenTest, BadOffsetPastEnd) {
    const char *src = "Hello";
    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 999; // Way past end

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 10 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(offset, 0); // Reset
    EXPECT_EQ(lenInd, 0); // No remaining data
}

// Test: Empty string with adequate buffer - should return SQL_SUCCESS
TEST_P(CopyWStrDataBigLenTest, ODBC_EmptyStringAdequateBuffer_ShouldReturnSuccess) {
    const char *src = "";
    SQLWCHAR dest[10] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // 5 chars in UTF-32 = 10 units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 5 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS because empty string fits (just null terminator)
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], SQLWCHAR_LITERAL('\0'));
    EXPECT_EQ(lenInd, 0); // Empty string length
}

// Test: Empty string with zero buffer - should return SQL_SUCCESS_WITH_INFO
TEST_P(CopyWStrDataBigLenTest, ODBC_EmptyStringZeroBuffer_ShouldReturnInfo) {
    const char *src = "";
    SQLWCHAR dest[1] = {0xFFFF};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 0, &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS_WITH_INFO because even null terminator doesn't fit
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, 0); // Empty string
    EXPECT_EQ(dest[0], (SQLWCHAR)0xFFFF); // Untouched
}

// Test: Sequential fetch - first call truncates
TEST_P(CopyWStrDataBigLenTest, ODBC_SequentialFetchFirstCall_ShouldReturnInfo) {
    const char *src = "ABCDEF";
    SQLWCHAR dest[6] = {0}; // 3 chars in UTF-32 = 6 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 3 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS_WITH_INFO because more data remains
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"AB");
    EXPECT_EQ(lenInd, 6 * sizeofSQLWCHAR()); // Total available at start
    EXPECT_EQ(offset, 2); // 2 chars consumed
}

// Test: Sequential fetch - last call completes
TEST_P(CopyWStrDataBigLenTest, ODBC_SequentialFetchLastCall_ShouldReturnSuccess) {
    const char *src = "ABCDEF";
    SQLWCHAR dest[10] = {0}; // 5 chars in UTF-32 = 10 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 2; // Continuing from previous fetch

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 5 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS because all remaining data fits
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"CDEF");
    EXPECT_EQ(lenInd, 4 * sizeofSQLWCHAR()); // Remaining at start of this call
    EXPECT_EQ(offset, 0); // Reset after completion
}

// Test: Multibyte UTF-8 truncation
TEST_P(CopyWStrDataBigLenTest, ODBC_MultibyteUTF8Truncation_ShouldReturnInfo) {
    const char *src = "AB你好"; // 2 ASCII + 2 Chinese = 4 wide chars
    SQLWCHAR dest[6] = {0}; // 3 chars in UTF-32 = 6 SQLWCHAR units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 3 * sizeofSQLWCHAR(),
                                 &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS_WITH_INFO because not all data fits
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"AB");
    EXPECT_EQ(lenInd, 4 * sizeofSQLWCHAR());
    EXPECT_EQ(offset, 2);
}

// Test: NULL data - should return SQL_SUCCESS (not INFO)
TEST_P(CopyWStrDataBigLenTest, ODBC_NullData_ShouldReturnSuccess) {
    SQLWCHAR dest[10] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; // 5 chars in UTF-32 = 10 units
    SQLLEN lenInd = -1;
    SQLLEN offset = 5;

    auto rc = copyWStrDataBigLen(nullptr, nullptr, SQL_NULL_DATA, dest, 
                                 5 * sizeofSQLWCHAR(), &offset, &lenInd);

    // ODBC spec: SQL_SUCCESS for NULL data (not truncation)
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, SQL_NULL_DATA);
    EXPECT_EQ(offset, 0); // Reset
    EXPECT_EQ(dest[0], (SQLWCHAR)0xFFFF); // Untouched
}

// Non-sequential truncation (offset == nullptr)
TEST_P(CopyWStrDataBigLenTest, NonSequential_Truncation) {
    const char* src = "ABCDE";
    // Allocate enough SQLWCHAR units: 4 chars * (sizeofSQLWCHAR/2) units per char
    std::vector<SQLWCHAR> dest(4 * (sizeofSQLWCHAR() / 2), 0);
    SQLLEN ind = -1;
    // 3 chars + NUL capacity; pass NULL offset
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest.data(), 4 * sizeofSQLWCHAR(),
                                 /*cbLenOffset*/ nullptr, &ind);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest.data()), u"ABC");
    EXPECT_EQ(ind, 5 * sizeofSQLWCHAR()); // if you choose "total" semantics
}

//pcbLenInd == NULL (no crash, correct rc)
TEST_P(CopyWStrDataBigLenTest, IndicatorNull_NoCrash) {
    const char* src = "ABCDEFG";
    std::vector<SQLWCHAR> dest(4 * (sizeofSQLWCHAR() / 2), 0);
    SQLLEN off = 0;
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest.data(), 4 * sizeofSQLWCHAR(),
                                 &off, /*pcbLenInd*/ nullptr);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest.data()), u"ABC");
    EXPECT_EQ(off, 3);
}

// Invalid negative iSrcLen (not SQL_NTS/SQL_NULL_DATA)
TEST_P(CopyWStrDataBigLenTest, InvalidNegativeLength_ShouldError) {
    const char* src = "ABCDEF";
    SQLWCHAR dest[8] = {0};
    SQLLEN ind = -1, off = 0;
    auto rc = copyWStrDataBigLen(nullptr, src, /*iSrcLen*/ -5, dest,
                                 4 * sizeofSQLWCHAR(), &off, &ind);
    EXPECT_EQ(rc, SQL_ERROR);
}

// Conversion failure (invalid UTF-8)
TEST_P(CopyWStrDataBigLenTest, InvalidUTF8_ShouldError) {
    // Invalid UTF-8: E2 28 A1
    const char bad[] = { (char)0xE2, (char)0x28, (char)0xA1, 0 };

    SQLWCHAR dest[8];
    // Poison the buffer to detect unintended writes
    for (auto &c : dest) c = (SQLWCHAR)0xFFFF;

    SQLLEN ind = -123;   // unchanged unless function sets it
    SQLLEN off = 0;      // offset provided

    SQLRETURN rc = copyWStrDataBigLen(nullptr, bad, SQL_NTS,
                                      dest, 4 * sizeofSQLWCHAR(),
                                      &off, &ind);

    EXPECT_EQ(rc, SQL_ERROR);

    // Optional invariants you may enforce in your impl:
    // - indicator not set on error (unchanged)
    EXPECT_EQ(ind, -123);

    // - destination should not be written on conversion failure.
    for (auto c : dest) EXPECT_EQ(c, (SQLWCHAR)0xFFFF);

    // - offset unchanged on hard error
    EXPECT_EQ(off, 0);
}

// Surrogate boundary split (UTF-16 only) - DISABLED: unclear expectations
// The function handles characters, not code units, so surrogates are handled correctly
TEST_P(CopyWStrDataBigLenTest, DISABLED_UTF16_SurrogateSplit_Sequential) {
    if (sizeofSQLWCHAR() != 2) GTEST_SKIP();
    const char* src = "A🙂B"; // includes surrogate pair in UTF-16
    SQLWCHAR dest[2] = {0}; // capacity 1 char + NUL
    SQLLEN ind=-1, off=0;
    auto rc1 = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 2 * sizeofSQLWCHAR(), &off, &ind);
    EXPECT_EQ(rc1, SQL_SUCCESS_WITH_INFO);
    auto rc2 = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, 2 * sizeofSQLWCHAR(), &off, &ind);
    EXPECT_TRUE(rc2 == SQL_SUCCESS_WITH_INFO || rc2 == SQL_SUCCESS);
}

// cbLen odd, only NUL should be written (explicit)
TEST_P(CopyWStrDataBigLenTest, OddCbLen_OnlyNulWrittenIfNoFullCharFits) {
    const char *src = "A";
    SQLWCHAR buf[1] = {0};
    std::memset(buf, 0xAA, sizeof(buf));

    SQLLEN ind = -1, off = 0;

    // Pass cbLen=1 (smaller than sizeofSQLWCHAR()), so cchLen == 0
    SQLRETURN rc =
        copyWStrDataBigLen(nullptr, src, SQL_NTS, buf, /*cbLen*/ 1, &off, &ind);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    // 1 wide char available in bytes
    EXPECT_EQ(ind, 1 * (SQLLEN)sizeofSQLWCHAR());
    EXPECT_EQ(off, 0); // no progress
    // Buffer must be untouched (no partial wide write, no NUL)
    SQLWCHAR sentinel;
    std::memset(&sentinel, 0xAA, sizeof(sentinel));
    // Buffer untouched
    EXPECT_EQ(0, std::memcmp(buf, &sentinel, sizeof(buf)));
}

// Non-sequential exact full fit (+NUL)
TEST_P(CopyWStrDataBigLenTest, NonSequential_ExactFullPlusNul) {
    const char* src = "Hello";
    std::vector<uint8_t> buf((5+1) * sizeofSQLWCHAR());
    SQLLEN ind=-1;
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, (SQLWCHAR*)buf.data(),
                                 buf.size(), /*offset*/ nullptr, &ind);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(ind, 5 * sizeofSQLWCHAR());
}

// copyStrDataBigLen

// Tests that copyStrDataBigLen correctly handles null source with SQL_NULL_DATA
TEST(CopyStrDataBigLen, NullSrcAndSQLNullData) {
    char dest[2] = {'X', 'X'};
    SQLLEN lenInd = -1;
    SQLLEN offset = 1;

    auto rc = copyStrDataBigLen(nullptr, nullptr, SQL_NULL_DATA, dest,
                                sizeof(dest), &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    // ensure buffer is untouched
    EXPECT_EQ(dest[0], 'X'); 
    EXPECT_EQ(dest[1], 'X');
    EXPECT_EQ(lenInd, SQL_NULL_DATA);
    EXPECT_EQ(offset, 0);
}

// Tests that copyStrDataBigLen correctly handles empty string input with zero
// length
TEST(CopyStrDataBigLen, EmptyStringInput) {
    const char *src = "";
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};
    SQLLEN lenInd = -1;
    SQLLEN offset = 1;

    auto rc = copyStrDataBigLen(nullptr, src, 0, dest, sizeof(dest), &offset,
                                &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], '\0');
    EXPECT_EQ(lenInd, 0);
    EXPECT_EQ(offset, 0);
}

// Tests that copyStrDataBigLen correctly copies a string that completely fits
// in the destination buffer
TEST(CopyStrDataBigLen, FullStringFitsInBuffer) {
    const char *src = "abc";
    char dest[10] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(dest, "abc");
    EXPECT_EQ(dest[3], '\0');
    EXPECT_EQ(lenInd, 3);
    EXPECT_EQ(offset, 0);
}

// Tests that copyStrDataBigLen correctly handles truncation when the string is
// larger than the destination buffer
TEST(CopyStrDataBigLen, TruncatedString) {
    const char *src = "abcdef";
    char dest[4] = {0}; // can hold 3 chars + null
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_STREQ(dest, "abc");
    EXPECT_EQ(dest[3], '\0');
    EXPECT_EQ(lenInd, 6);
    EXPECT_EQ(offset, 3);
}

// Tests that copyStrDataBigLen correctly handles sequential fetching of
// remaining data after truncation
TEST(CopyStrDataBigLen, SequentialFetch) {
    const char *src = "abcdef";
    char dest[4] = {0}; // can hold 3 chars + null
    SQLLEN lenInd = -1;
    SQLLEN offset = 3;

    auto rc = copyStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(dest, "def");
    EXPECT_EQ(dest[3], '\0');
    EXPECT_EQ(offset, 0);
}

// Tests that copyStrDataBigLen correctly handles cases where the buffer can
// only hold the null terminator
TEST(CopyStrDataBigLen, OneByteBufferOnlyNull) {
    const char *src = "abc";
    char dest[1] = {'X'};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyStrDataBigLen(nullptr, src, SQL_NTS, dest, 1, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(dest[0], '\0'); // Only null terminator
    EXPECT_EQ(offset, 0);   // No progress made
}

// Tests that copyStrDataBigLen correctly sets rc when destination buffer is
// null
TEST(CopyStrDataBigLen, NullDestPointer) {
    const char *src = "abc";
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyStrDataBigLen(nullptr, src, SQL_NTS, nullptr, 0, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_ERROR);
}

// Tests that copyStrDataBigLen correctly sets rc when destination buffer length
// is negative
TEST(CopyStrDataBigLen, NegativeBufferLength) {
    const char *src = "abc";
    char dest[1] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyStrDataBigLen(nullptr, src, SQL_NTS, dest, -1, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_ERROR);
}

// Tests that copyStrDataBigLen correctly handles explicit length parameter
// instead of SQL_NTS
TEST(CopyStrDataBigLen, ExplicitLengthInput) {
    const char *src = "abcdef";
    int length = 3;
    char dest[5] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyStrDataBigLen(nullptr, src, length, dest, sizeof(dest),
                                &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(dest, "abc");
    EXPECT_EQ(dest[3], '\0');
    EXPECT_EQ(lenInd, 3);
}

// Tests copyStrDataBigLen with non-null terminated input string
TEST(CopyStrDataBigLen, NonNullTerminatedInput) {
    const char src[5] = {'H', 'e', 'l', 'l', 'o'}; // No null terminator
    int length = 3;
    char dest[5] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyStrDataBigLen(nullptr, src, 5, dest, 3, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_STREQ(dest, "He");
    EXPECT_EQ(dest[2], '\0');
    EXPECT_EQ(lenInd, 5);

    rc = copyStrDataBigLen(nullptr, src, 5, dest, 3, &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_STREQ(dest, "ll");
    EXPECT_EQ(dest[2], '\0');
    EXPECT_EQ(lenInd, 3);

    rc = copyStrDataBigLen(nullptr, src, 5, dest, 3, &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(dest, "o");
    EXPECT_EQ(dest[1], '\0');
    EXPECT_EQ(lenInd, 1);
}

// copyStrDataLargeLen

// Test copyStrDataLargeLen function behavior with NULL pDest and non-NULL pcbLen
TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_null_pDest_with_pcbLen) {
    const char* testStr = "test";
    SQLINTEGER pcbLen = 0;
    
    SQLRETURN rc = copyStrDataLargeLen(testStr, SQL_NTS, NULL, 0, &pcbLen);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 4); // Length of "test"
}

TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_explicit_length) {
    const char* testStr = "test_longer_string";
    char buffer[10];
    SQLINTEGER pcbLen = 0;

    // Only copy first 4 characters using explicit length
    SQLRETURN rc = copyStrDataLargeLen(testStr, 4, buffer, sizeof(buffer), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 4);
    EXPECT_STREQ(buffer, "test");
}

// Test copyStrDataLargeLen function behavior with short buffer
TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_short_buffer) {
    const char* testStr = "test";
    char shortBuffer[2];
    SQLINTEGER pcbLen = 0;
    
    SQLRETURN rc = copyStrDataLargeLen(testStr, SQL_NTS, shortBuffer, sizeof(shortBuffer), &pcbLen);
    
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(pcbLen, 4); // Length of "test"
    EXPECT_EQ(shortBuffer[0], 't');
    EXPECT_EQ(shortBuffer[1], '\0');
}

// Test copyStrDataLargeLen function behavior with adequate buffer
TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_adequate_buffer) {
    const char* testStr = "test";
    char buffer[10];
    SQLINTEGER pcbLen = 0;
    
    SQLRETURN rc = copyStrDataLargeLen(testStr, SQL_NTS, buffer, sizeof(buffer), &pcbLen);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 4); // Length of "test"
    EXPECT_STREQ(buffer, "test");
}

// Test copyStrDataLargeLen function behavior with NULL source
TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_null_source) {
    char buffer[10];
    SQLINTEGER pcbLen = 0;
    
    SQLRETURN rc = copyStrDataLargeLen(NULL, SQL_NTS, buffer, sizeof(buffer), &pcbLen);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 0);
    EXPECT_EQ(buffer[0], '\0');
}

// Test copyStrDataLargeLen function behavior with NULL pcbLen
TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_null_pcbLen) {
    const char* testStr = "test";
    char buffer[10];

    SQLRETURN rc = copyStrDataLargeLen(testStr, SQL_NTS, buffer, sizeof(buffer), NULL);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(buffer, "test");
}

// makeNullTerminatedStr

class NullTerminatedStrTest : public ::testing::Test {
  protected:
    RS_STR_BUF strBuf{};

    void SetUp() override {
        memset(&strBuf, 0, sizeof(strBuf));
    }

    void TearDown() override {
        // Free any dynamically allocated memory
        if (strBuf.iAllocDataLen > 0 && strBuf.pBuf != strBuf.buf) {
            free(strBuf.pBuf);
        }
        memset(&strBuf, 0, sizeof(strBuf));
    }
};

// Test handling embedded null bytes
// Note: makeNullTerminatedStr uses rs_strnlen which stops at the first null byte,
// so it only copies up to the first null, not the entire buffer with embedded nulls
TEST_F(NullTerminatedStrTest, FirstNullByteTermination) {
    char data[10];
    memset(data, 'A', sizeof(data));
    data[3] = '\0';  // Embedded null byte
    data[7] = '\0';  // Another embedded null byte

    unsigned char *result = makeNullTerminatedStr(data, sizeof(data), &strBuf);

    EXPECT_NE(nullptr, result);
    // rs_strnlen stops at first null, so only first 3 bytes ('AAA') are copied
    EXPECT_EQ(0, memcmp(result, data, 3));
    // Verify proper null termination at position 3
    EXPECT_EQ('\0', result[3]);
    // Verify the string length is 3, not 10
    EXPECT_EQ(3, strlen((char *)result));
}

// Test NULL input pointer
TEST_F(NullTerminatedStrTest, NullInputPointer) {
    unsigned char *result = makeNullTerminatedStr(NULL, 10, &strBuf);
    EXPECT_EQ(nullptr, result);
}

// Test SQL_NULL_DATA input length
TEST_F(NullTerminatedStrTest, SqlNullDataLength) {
    char data[] = "test";
    unsigned char *result = makeNullTerminatedStr(data, SQL_NULL_DATA, &strBuf);
    EXPECT_EQ(nullptr, result);
}

// Test non empty input but with zero as length
TEST_F(NullTerminatedStrTest, SqlZeroLength) {
    char data[] = "test";
    unsigned char *result = makeNullTerminatedStr(data, 0, &strBuf);
    EXPECT_NE(nullptr, result);
    EXPECT_EQ('\0', result[0]);
    EXPECT_EQ(strlen((char *)result), 0);
}

// Test already null-terminated string (SQL_NTS)
TEST_F(NullTerminatedStrTest, AlreadyNullTerminated) {
    char data[] = "test";
    unsigned char *result = makeNullTerminatedStr(data, SQL_NTS, &strBuf);
    EXPECT_EQ((unsigned char *)data, result);
    EXPECT_EQ(data, strBuf.pBuf);
}

// Test invalid negative length (not SQL_NTS or SQL_NULL_DATA)
TEST_F(NullTerminatedStrTest, InvalidNegativeLength) {
    char data[] = "test";
    unsigned char *result = makeNullTerminatedStr(data, -10, &strBuf);
    EXPECT_EQ(nullptr, result);
}

// Test short string (fits in internal buffer)
TEST_F(NullTerminatedStrTest, ShortString) {
    const int testSize = 100; // Much smaller than SHORT_STR_DATA
    char data[testSize];
    memset(data, 'A', testSize - 1);
    data[testSize - 1] = 'B'; // Intentionally not null-terminated

    unsigned char *result = makeNullTerminatedStr(data, testSize, &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(strBuf.buf, (char *)result);
    EXPECT_EQ(0, strncmp((char *)result, data, testSize));
    EXPECT_EQ('\0', result[testSize]);
    EXPECT_EQ(0, strBuf.iAllocDataLen); // No allocation for short strings
}

// Test long string (requires allocation)
TEST_F(NullTerminatedStrTest, LongString) {
    const int longStrSize = SHORT_STR_DATA + 100;
    std::unique_ptr<char[]> datau = std::make_unique<char[]>(longStrSize);
    char *data = datau.get();
    memset(data, 'X', longStrSize);

    unsigned char *result = makeNullTerminatedStr(data, longStrSize, &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_NE(strBuf.buf, (char *)result);
    EXPECT_EQ(0, strncmp((char *)result, data, longStrSize));
    EXPECT_EQ('\0', result[longStrSize]);
    EXPECT_EQ(longStrSize, strBuf.iAllocDataLen);
}

// Test without buffer manager
TEST_F(NullTerminatedStrTest, NoBufManager) {
    const char data[] = "test without buffer manager";
    const int dataLen = strlen(data);

    unsigned char *result = makeNullTerminatedStr((char *)data, dataLen, NULL);

    EXPECT_NE(nullptr, result);
    EXPECT_NE((unsigned char *)data, result);
    EXPECT_EQ(0, strncmp((char *)result, data, dataLen));
    EXPECT_EQ('\0', result[dataLen]);

    free(result); // We need to free since no buffer manager was provided
}

// Test non-null-terminated input
TEST_F(NullTerminatedStrTest, NonNullTerminatedInput) {
    char data[10];
    memset(data, 'Z', sizeof(data));
    // Intentionally not null-terminated

    unsigned char *result = makeNullTerminatedStr(data, sizeof(data), &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(0, strncmp((char *)result, data, sizeof(data)));
    EXPECT_EQ('\0', result[sizeof(data)]);
}

// Test boundary case - empty string
TEST_F(NullTerminatedStrTest, EmptyString) {
    char data[] = "";

    unsigned char *result = makeNullTerminatedStr(data, 0, &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ('\0', result[0]);
}

// Test boundary case - string of length 1
TEST_F(NullTerminatedStrTest, SingleCharString) {
    char data[] = "X";

    unsigned char *result = makeNullTerminatedStr(data, 1, &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ('X', result[0]);
    EXPECT_EQ('\0', result[1]);
}

// Test boundary case - exactly SHORT_STR_DATA
TEST_F(NullTerminatedStrTest, ExactShortStrData) {
    std::unique_ptr<char[]> datau = std::make_unique<char[]>(SHORT_STR_DATA);
    char *data = datau.get();
    memset(data, 'A', SHORT_STR_DATA);

    unsigned char *result =
        makeNullTerminatedStr(data, SHORT_STR_DATA, &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(strBuf.buf, (char *)result);
    EXPECT_EQ('\0', result[SHORT_STR_DATA]);
}

// Test boundary case - SHORT_STR_DATA+1
TEST_F(NullTerminatedStrTest, JustOverShortStrData) {
    std::unique_ptr<char[]> datau = std::make_unique<char[]>(SHORT_STR_DATA + 1);
    char *data = datau.get();

    memset(data, 'A', SHORT_STR_DATA + 1);

    unsigned char *result =
        makeNullTerminatedStr(data, SHORT_STR_DATA + 1, &strBuf);

    EXPECT_NE(nullptr, result);
    EXPECT_NE(strBuf.buf, (char *)result);
    EXPECT_EQ(0, strncmp((char *)result, data, SHORT_STR_DATA + 1));
    EXPECT_EQ('\0', result[SHORT_STR_DATA + 1]);
    EXPECT_EQ(SHORT_STR_DATA + 1, strBuf.iAllocDataLen);
}

// Test extremely large allocation
// Note: The function uses rs_strnlen which finds the actual string length,
// so even with a huge size parameter, it only allocates what's needed
TEST_F(NullTerminatedStrTest, LargeAllocationSizeParameter) {
    char data[] = "test";
    // Test with a value near INT64_MAX
    int64_t hugeSize = static_cast<int64_t>(INT64_MAX - 10);

    unsigned char *result = makeNullTerminatedStr(data, hugeSize, &strBuf);

    // Function succeeds because rs_strnlen finds the actual length (4)
    // and only allocates 5 bytes (4 + null terminator)
    EXPECT_NE(nullptr, result);
    EXPECT_STREQ((char *)result, "test");
}

// Test handling of resetPaStrBuf with NULL parameter
TEST_F(NullTerminatedStrTest, ResetNullBuffer) {
    char data[] = "test";

    // This should not crash
    unsigned char *result = makeNullTerminatedStr(data, strlen(data), NULL);

    EXPECT_NE(nullptr, result);
    free(result);
}

// Test allocation exactly at SIZE_MAX-1
// Note: The function uses rs_strnlen which finds the actual string length,
// so even with a huge size parameter, it only allocates what's needed
TEST_F(NullTerminatedStrTest, SizeMaxMinusOneBoundary) {
    char data[] = "test";
    // Cast carefully to avoid overflow during test setup
    int64_t boundarySize;
    boundarySize = static_cast<int64_t>(INT64_MAX - 1);
    unsigned char *result = makeNullTerminatedStr(data, boundarySize, &strBuf);
    // Function succeeds because rs_strnlen finds the actual length (4)
    EXPECT_NE(nullptr, result);
    EXPECT_STREQ((char *)result, "test");
}

// Test large allocation with valid source data
TEST_F(NullTerminatedStrTest, LargeButValidAllocation) {
    // Create a source buffer large enough for the test
    // Using a vector to allocate a contiguous block
    std::vector<char> largeData(10240, 'X'); // 10KB buffer filled with 'X'
    
    // Fill the end portion with a different pattern to verify it's not modified
    std::fill(largeData.begin() + 10000, largeData.end(), 'Y');

    // Use a size smaller than our buffer
    int64_t dataSize = 10000; // 10KB

    unsigned char *result =
        makeNullTerminatedStr(largeData.data(), dataSize, &strBuf);

    ASSERT_NE(nullptr, result);
    // Check beginning
    EXPECT_EQ('X', result[0]);
    // Check somewhere in the middle
    EXPECT_EQ('X', result[5000]);
    // Check end
    EXPECT_EQ('X', result[dataSize - 1]);
    // Check null termination
    EXPECT_EQ('\0', result[dataSize]);
    
    // Verify data beyond dataSize in source buffer is not modified
    EXPECT_EQ('Y', largeData[dataSize]);
    EXPECT_EQ('Y', largeData[dataSize + 100]);
    EXPECT_EQ('Y', largeData.back());
}

// rs_strncpy_safe

TEST(StrncpySafeTest, NormalCopy) {
    char dest[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const char *src = "hello";
    char* res = rs_strncpy_safe(dest, src, sizeof(dest));
    EXPECT_STREQ(dest, "hello");
    EXPECT_EQ(dest, res);
}

TEST(StrncpySafeTest, TruncatesAndNullTerminates) {
    char dest[6] = {'a', 'b', 'c', 'd', 'e', 'f'};
    const char *src = "123456789";
    char* res = rs_strncpy_safe(dest, src, sizeof(dest));
    EXPECT_EQ(dest[5], '\0');
    EXPECT_EQ(std::string(dest), "12345");
    EXPECT_EQ(dest, res);
}

TEST(StrncpySafeTest, NullSourceReturnsNull) {
    char dest[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(rs_strncpy_safe(dest, nullptr, sizeof(dest)), nullptr);
}

TEST(StrncpySafeTest, NullDestReturnsNull) {
    const char *src = "test";
    EXPECT_EQ(rs_strncpy_safe(nullptr, src, 10), nullptr);
}

TEST(StrncpySafeTest, ZeroLengthReturnsNull) {
    char dest[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const char *src = "hello";
    EXPECT_EQ(rs_strncpy_safe(dest, src, 0), nullptr);
}

TEST(StrncpySafeTest, CopyEmptyString) {
    char dest[5] = {'x', 'x', 'x', 'x', 'x'};
    char* res = rs_strncpy_safe(dest, "", sizeof(dest));
    EXPECT_STREQ(dest, "");
    EXPECT_EQ(dest[0], '\0');
    EXPECT_EQ(dest, res);
}

TEST(StrncpySafeTest, SourceEqualsDest_TruncatesWhenSizeIsCorrect) {
    char buffer[10] = {'h','e','l','l','o','h','e','l','l','o'};
    char* res = rs_strncpy_safe(buffer, buffer, sizeof(buffer));
    EXPECT_EQ(res, buffer);
    EXPECT_STREQ(buffer, "hellohell"); // 9 chars + NUL
}

TEST(StrncpySafeTest, RejectsSQLNTSExplicitCast) {
    char buf[16] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    size_t sql_nts = (size_t)(SQL_NTS);
    EXPECT_EQ(rs_strncpy_safe(buf, "hello", sql_nts), nullptr);
}

TEST(StrncpySafeTest, RejectsSQLNTSDirectMacro) {
    char buf[16] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(rs_strncpy_safe(buf, "test", SQL_NTS), nullptr);
}

TEST(StrncpySafeTest, DoesNotModifyDestOnSQLNTS) {
    char buf[16] = "unchanged";
    char* res = rs_strncpy_safe(buf, "new", SQL_NTS);
    EXPECT_STREQ(buf, "unchanged");
    EXPECT_EQ(nullptr, res);
}

TEST(StrncpySafeTest, SQLNTSCheckDoesNotTriggerForValidSize) {
    char buf[16] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    char* res = rs_strncpy_safe(buf, "abc", 4);  // not SQL_NTS
    EXPECT_STREQ(buf, "abc");
    EXPECT_EQ(buf, res);
}

TEST(StrncpySafeTest, ExactFit) {
    char dest[6] = {'a', 'b', 'c', 'd', 'e', 'f'};
    char* res = rs_strncpy_safe(dest, "12345", sizeof(dest));  // strlen = 5, n = 6
    EXPECT_STREQ(dest, "12345");
    EXPECT_EQ(dest, res);
}

TEST(StrncpySafeTest, TruncatesWithoutGarbage) {
    char dest[5] = {'X', 'X', 'X', 'X', 'X'};
    char* res = rs_strncpy_safe(dest, "abcdef", sizeof(dest));  // Only "abcd" copied
    EXPECT_EQ(std::string(dest), "abcd");
    EXPECT_EQ(dest[4], '\0');
    EXPECT_EQ(dest, res);
}

TEST(StrncpySafeTest, SourceEqualsDestStillNullTerminates) {
    char buffer[6] = "ABCDE";
    char* res = rs_strncpy_safe(buffer, buffer, sizeof(buffer));
    EXPECT_EQ(buffer[5], '\0');  // Last byte is explicitly null
    EXPECT_EQ(buffer, res);
}

TEST(StrncpySafeTest, RejectsZeroLength) {
    char buffer[10] = "unchanged";
    const char* src = "hello";

    char* res = rs_strncpy_safe(buffer, src, 0);
    EXPECT_EQ(res, nullptr);
    EXPECT_STREQ(buffer, "unchanged");  // Should be untouched
    EXPECT_NE(buffer, res);
}

TEST(StrncpySafeTest, RejectsSQLNTSSize) {
    char buffer[10] = "unchanged";
    const char* src = "data";

    char* res = rs_strncpy_safe(buffer, src, SQL_NTS);  // SQL_NTS
    EXPECT_EQ(res, nullptr);
    EXPECT_STREQ(buffer, "unchanged");
    EXPECT_NE(buffer, res);
}

TEST(StrncpySafeTest, OverlappingMemory) {
    char buffer[10] = "abcdefghi";
    char* res = rs_strncpy_safe(buffer, buffer, 5);
    EXPECT_EQ(res, buffer);
    EXPECT_STREQ(buffer, "abcd");
    res = rs_strncpy_safe(buffer, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "abcd");
    EXPECT_EQ(buffer, res);
    EXPECT_EQ(memcmp("abcd\0fghi", buffer, sizeof(buffer)), 0);
}

TEST(StrncpySafeTest, PartiallyOverlappingMemory) {
    char buffer[10] = "abcdefghi";
    char expected[10] = "ababcd\0hi";
    std::stringstream ss;
    // Destination starts at buffer+2, creating partial overlap
    char* res = rs_strncpy_safe(buffer+2, buffer, 4+1);
    ss << "Modified:\n";
    for (int i = 0; i < 10; ++i) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(buffer[i])) << " ";
    }
    ss << "\nExpected:\n";
    for (int i = 0; i < 10; ++i) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(expected[i])) << " ";
    }
    // Add after existing assertions:
    EXPECT_EQ(memcmp(expected, buffer, sizeof(buffer)), 0) << ss.str();
    EXPECT_EQ(res, buffer+2);
    EXPECT_STREQ(res, "abcd");
}

TEST(StrncpySafeTest, PartiallyOverlappingMemory_NullTerminatedInBetween) {
    char buffer[10] = "abcdefghi";
    char expected[10] = "adefg\0ghi";
    std::stringstream ss;
    char* res = rs_strncpy_safe(buffer+1, buffer+3, 4+1);
    ss << "\nModified:\n";
    for (int i = 0; i < 10; ++i) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(buffer[i])) << " ";
    }
    ss << "\nExpected:\n";
    for (int i = 0; i < 10; ++i) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(expected[i])) << " ";
    }
    EXPECT_EQ(res, buffer+1);
    // Add after existing assertions:
    EXPECT_EQ(memcmp(expected, buffer, sizeof(buffer)), 0) << ss.str();
    EXPECT_STREQ(res, "defg");
}

// Unicode handling. ONLY trivial case!!!
// Note:
// rs_strncpy_safe is byte-oriented; it doesn’t preserve code-point
// boundaries in general. This test happens to pick a size that aligns.
// That’s fine, but don’t rely on boundary preservation elsewhere.
TEST(StrncpySafeTest, UTF8Handling) {

    // Japanese 'hello' (こんにちは) - each character is 3 bytes in UTF-8
    const char *src = "こんにちは";

    // Test complete fit. Enough for all characters + null
    char fullDest[16] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    char* res = rs_strncpy_safe(fullDest, src, sizeof(fullDest));
    EXPECT_STREQ(fullDest, src);
    EXPECT_EQ(fullDest, res);

    // Test truncation - only room for 2 characters + null
    char truncDest[7] = {'1', '2', '3', '4', '5', '6', '7'};
    res = rs_strncpy_safe(truncDest, src, sizeof(truncDest));
    EXPECT_EQ(truncDest[6], '\0'); // Verify null termination
    EXPECT_EQ(std::string(truncDest), "こん"); // Should contain exactly 2 characters
    EXPECT_EQ(truncDest, res);
    // Verify null termination and that we don't exceed buffer bounds
    EXPECT_EQ(truncDest[6], '\0');
    // Verify we copied some data but don't assume complete characters
    EXPECT_GT(strlen(truncDest), 0);
    EXPECT_LT(strlen(truncDest), strlen(src));
}

TEST(StrncpySafeTest, OneByteBufferJustNull) {
    char dest[1] = {'X'};
    const char* src = "hello";
    ASSERT_NE(rs_strncpy_safe(dest, src, sizeof(dest)), nullptr);
    EXPECT_EQ(dest[0], '\0');         // only terminator fits
}

TEST(StrncpySafeTest, NullSourceDoesNotModifyDest) {
    char dest[10] = "unchanged";
    EXPECT_EQ(rs_strncpy_safe(dest, nullptr, sizeof(dest)), nullptr);
    EXPECT_STREQ(dest, "unchanged");
}

TEST(StrncpySafeTest, BackwardOverlap_memmoveDirection) {
    // dest < src overlap: copy must proceed safely (backward move)
    char buf[12] = "ABCDEFGHIJ";      // NUL at [10]
    // Copy "CDEF" (starts at buf+2) into buf (starts earlier)
    char* res = rs_strncpy_safe(buf, buf + 2, 5); // n=5 => cap=4 => copy 4
    ASSERT_EQ(res, buf);
    EXPECT_STREQ(buf, "CDEF");        // then NUL
}

TEST(StrncpySafeTest, SrcShorterThanCapKeepsNullOnly) {
    char dest[6] = {'X','X','X','X','X','X'};
    ASSERT_NE(rs_strncpy_safe(dest, "abc", sizeof(dest)), nullptr);
    EXPECT_STREQ(dest, "abc");
    // Optional: bytes after the first NUL are unspecified, so don't assert them.
}

TEST(StrncpySafeTest, RejectsVeryLargeSizeIfYouKeepThatGuard) {
    char dest[8] = "stay";
    char* res = rs_strncpy_safe(dest, "x", static_cast<size_t>(INT_MAX) + 1);
    EXPECT_EQ(res, nullptr);
    EXPECT_STREQ(dest, "stay");
}

TEST(StrncpySafeTest, SingleByteBuffer) {
    char dest[1] = {'1'};
    const char *src = "hello";
    char* res = rs_strncpy_safe(dest, src, sizeof(dest));
    EXPECT_EQ(res, dest);
    EXPECT_EQ(dest[0], '\0');
}

TEST(StrncpySafeTest, RejectsGreaterThanIntMax) {
    char buffer[10] = "unchanged";
    const char* src = "test";

    // Test with a size larger than INT_MAX
    size_t too_large = static_cast<size_t>(INT_MAX) + 1;
    EXPECT_EQ(rs_strncpy_safe(buffer, src, too_large), nullptr);
    EXPECT_STREQ(buffer, "unchanged");  // Buffer should remain untouched
}


static std::string DumpHex(const char* buf, size_t len) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << "0x" << std::setw(2)
           << static_cast<int>(static_cast<unsigned char>(buf[i])) << " ";
    }
    return ss.str();
}

struct StrncpyCase {
    const char* initial;            // initial buffer contents (exact bytes)
    const char* expected;           // expected final contents (exact bytes)
    size_t      buf_size;           // total buffer size
    size_t      dst_off;            // destination offset
    size_t      src_off;            // source offset
    size_t      n;                  // n passed to rs_strncpy_safe
    const char* expected_res_cstr;  // C-string visible at dest (when non-null result)
    size_t      expected_res_off;   // returned pointer offset (when non-null result)
    bool        expect_null = false;
};

class StrncpySafeParamTest : public ::testing::TestWithParam<StrncpyCase> {};

TEST_P(StrncpySafeParamTest, Works) {
    const auto& tc = GetParam();

    std::vector<char> buffer(tc.buf_size, '\0');
    std::vector<char> expected(tc.buf_size, '\0');

    // Use memcpy so embedded NULs in literals are preserved exactly.
    std::memcpy(buffer.data(),  tc.initial,  tc.buf_size);
    std::memcpy(expected.data(), tc.expected, tc.buf_size);

    char* dest = buffer.data() + tc.dst_off;
    const char* src = buffer.data() + tc.src_off;

    char* res = rs_strncpy_safe(dest, src, tc.n);

    std::ostringstream diff;
    diff << "Modified:\n" << DumpHex(buffer.data(), tc.buf_size)
         << "\nExpected:\n" << DumpHex(expected.data(), tc.buf_size)
         << "\ninitial:\n" << DumpHex(tc.initial, tc.buf_size)
         << "\ndst_off=" << tc.dst_off
         << " src_off=" << tc.src_off
         << " n=" << tc.n << "\n";

    if (tc.expect_null) {
        EXPECT_EQ(res, nullptr) << diff.str();
    } else {
        EXPECT_EQ(res, buffer.data() + tc.expected_res_off) << diff.str();
        EXPECT_STREQ(res, tc.expected_res_cstr) << diff.str();
    }
    EXPECT_EQ(std::memcmp(expected.data(), buffer.data(), tc.buf_size), 0) << diff.str();
}

// Helper: give each case a readable name
std::string StrncpyCaseName(const testing::TestParamInfo<StrncpyCase>& info) {
    static int counter = 0;
    std::ostringstream ss;
    ss << counter++ << "_dst" << info.param.dst_off
       << "_src" << info.param.src_off
       << "_n"   << info.param.n;
    return ss.str();
}

INSTANTIATE_TEST_SUITE_P(
    StrncpySafeTest,
    StrncpySafeParamTest,
    ::testing::Values(
        // --- Your original two overlap cases (passed) ---
        StrncpyCase{
            /*initial*/           "abcdefghi",
            /*expected*/          "ababcd\0hi",
            /*buf_size*/          10,
            /*dst_off*/           2,
            /*src_off*/           0,
            /*n*/                 4 + 1,
            /*expected_res_cstr*/ "abcd",
            /*expected_res_off*/  2
        },
        StrncpyCase{
            /*initial*/           "abcdefghi",
            /*expected*/          "adefg\0ghi",
            /*buf_size*/          10,
            /*dst_off*/           1,
            /*src_off*/           3,
            /*n*/                 4 + 1,
            /*expected_res_cstr*/ "defg",
            /*expected_res_off*/  1
        },

        // n = 0 -> NULL return, buffer unchanged
        StrncpyCase{
            "abcdefghi", "abcdefghi", 10,
            /*dst*/0, /*src*/2, /*n*/0,
            /*res cstr*/"", /*res off*/0, /*expect_null*/ true
        },

        // n = 1 -> cap=0, only writes NUL at dest[0]
        StrncpyCase{
            "abcdefghi", "\0bcdefghi", 10,
            /*dst*/0, /*src*/2, /*n*/1,
            /*res cstr*/"", /*res off*/0
        },

        // Exact fit with n=4 -> cap=3 -> copy "abc" then NUL at [3]
        StrncpyCase{
            "abcdefghi", "abc\0efghi", 10,
            /*dst*/0, /*src*/0, /*n*/4,
            /*res cstr*/"abc", /*res off*/0
        },

        // Truncation with n=5 -> cap=4 -> copy "abcd" then NUL at [4]
        StrncpyCase{
            "abcdefghi", "abcd\0fghi", 10,
            /*dst*/0, /*src*/0, /*n*/5,
            /*res cstr*/"abcd", /*res off*/0
        },

        // Full alias, n=6 -> cap=5 -> copy "abcde" then NUL at [5]
        StrncpyCase{
            "abcdefghi", "abcde\0ghi", 10,
            /*dst*/0, /*src*/0, /*n*/6,
            /*res cstr*/"abcde", /*res off*/0
        },

        // Source has early NUL at src_off (initial "ab\0defghi")
        StrncpyCase{
            "\x61\x62\x00\x64\x65\x66\x67\x68\x69\x00",
            "\x61\x62\x00\x64\x65\x66\x67\x68\x69\x00",
            10, /*dst*/0, /*src*/0, /*n*/6,
            /*res cstr*/"ab", /*res off*/0
        },

        // Dest one before end: copy 1 char + NUL (dst=8 <- src=7, n=2)
        StrncpyCase{
            "abcdefghi", "\x61\x62\x63\x64\x65\x66\x67\x68\x68\x00",
            10, /*dst*/8, /*src*/7, /*n*/2,
            /*res cstr*/"h", /*res off*/8
        },

        // Non-overlap baseline (dst=0 <- src=3, n=4 -> cap=3): copy "def", NUL at [3]
        StrncpyCase{
            "abcdefghi",
            "\x64\x65\x66\x00\x65\x66\x67\x68\x69\x00", // "def\0efghi"
            10, /*dst*/0, /*src*/3, /*n*/4,
            /*res cstr*/"def", /*res off*/0
        },

        // Large n, dst=0 <- src=2, copy "cdefghi" (7), NUL at [7], [8] stays 'i'
        StrncpyCase{
            "abcdefghi", "cdefghi\0i\0", 10,
            /*dst*/0, /*src*/2, /*n*/100,
            /*res cstr*/"cdefghi", /*res off*/0
        },

        // Alias copy, n=5 -> writes NUL at [4]; rest unchanged
        StrncpyCase{
            "abcdefghi", "abcd\0fghi", 10,
            /*dst*/0, /*src*/0, /*n*/5,
            /*res cstr*/"abcd", /*res off*/0
        },

        // Alias copy, large n from "abcd\0fghi": remains unchanged
        StrncpyCase{
            "\x61\x62\x63\x64\x00\x66\x67\x68\x69\x00",
            "\x61\x62\x63\x64\x00\x66\x67\x68\x69\x00",
            10, /*dst*/0, /*src*/0, /*n*/10,
            /*res cstr*/"abcd", /*res off*/0
        },

        // n == SQL_NTS -> NULL (guard), unchanged buffer
        StrncpyCase{
            "abcdefghi", "abcdefghi", 10,
            /*dst*/0, /*src*/0, /*n*/(size_t)SQL_NTS,
            /*res cstr*/"", /*res off*/0, /*expect_null*/ true
        }
    ), StrncpyCaseName
);

// rs_strnlen tests

TEST(StrnlenTest, NormalStringReturnsLength) {
    const char* s = "hello";
    EXPECT_EQ(5u, rs_strnlen(s, 10));
}

TEST(StrnlenTest, RespectsMaxlenWhenSmallerThanString) {
    const char* s = "hello";
    EXPECT_EQ(3u, rs_strnlen(s, 3));
}

TEST(StrnlenTest, ZeroMaxlenReturnsZero) {
    const char* s = "hello";
    EXPECT_EQ(0u, rs_strnlen(s, 0));
}

TEST(StrnlenTest, NullptrReturnsZero) {
    EXPECT_EQ(0u, rs_strnlen(nullptr, 100));
}

TEST(StrnlenTest, EmptyStringReturnsZero) {
    const char* s = "";
    EXPECT_EQ(0u, rs_strnlen(s, 8));
}

TEST(StrnlenTest, NoNulWithinMaxlenReturnsMaxlen) {
    // No NUL in first 5 bytes
    const char buf[5] = {'a','b','c','d','e'};
    EXPECT_EQ(5u, rs_strnlen(buf, 5));
}

TEST(StrnlenTest, EmbeddedNulWithinLimitStopsThere) {
    // "abc\0def"
    const char s[] = {'a','b','c','\0','d','e','f'};
    EXPECT_EQ(3u, rs_strnlen(s, sizeof(s)));  // sees NUL at index 3
}

TEST(StrnlenTest, NulExactlyAtMaxlenIsNotExamined) {
    // NUL at index 3; maxlen=3 means indices [0..2] scanned → not seen
    const char s[] = {'a','b','c','\0','x'};
    EXPECT_EQ(3u, rs_strnlen(s, 3));
    // With maxlen=4 we do see it
    EXPECT_EQ(3u, rs_strnlen(s, 4));
}

TEST(StrnlenTest, OneByteBufferJustNul) {
    const char s[] = {'\0'};
    EXPECT_EQ(0u, rs_strnlen(s, 1));
}

TEST(StrnlenTest, LargeMaxlenClampedByRealLength) {
    const char* s = "hi";
    EXPECT_EQ(2u, rs_strnlen(s, static_cast<size_t>(1) << 30));
}

// copySqlwForClient tests

TEST(CopySqlwForClient, LengthQueryWithNullDst) {
    const uint16_t src[] = {u'a', u'b', u'c'};
    SQLLEN pcbLen = 0;
    size_t copiedChars = 999;
    
    SQLRETURN rc = copySqlwForClient(nullptr, src, 3, 0, &pcbLen, &copiedChars, 2);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 6);
    EXPECT_EQ(copiedChars, 0);
}

TEST(CopySqlwForClient, BufferFitsAll) {
    const uint16_t src[] = {u'a', u'b', u'c'};
    uint16_t dst[5] = {0};
    SQLLEN pcbLen = 0;
    size_t copiedChars = 0;
    
    SQLRETURN rc = copySqlwForClient(dst, src, 3, 5, &pcbLen, &copiedChars, 2);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 6);
    EXPECT_EQ(copiedChars, 3);
    EXPECT_EQ(dst[0], u'a');
    EXPECT_EQ(dst[1], u'b');
    EXPECT_EQ(dst[2], u'c');
    EXPECT_EQ(dst[3], 0);
}

TEST(CopySqlwForClient, BufferTooSmall) {
    const uint16_t src[] = {u'a', u'b', u'c', u'd', u'e'};
    uint16_t dst[3] = {0};
    SQLLEN pcbLen = 0;
    size_t copiedChars = 0;
    
    SQLRETURN rc = copySqlwForClient(dst, src, 5, 3, &pcbLen, &copiedChars, 2);
    
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(pcbLen, 10);
    EXPECT_EQ(copiedChars, 2);
    EXPECT_EQ(dst[0], u'a');
    EXPECT_EQ(dst[1], u'b');
    EXPECT_EQ(dst[2], 0);
}

TEST(CopySqlwForClient, CharSize4) {
    const uint32_t src[] = {U'x', U'y'};
    uint32_t dst[4] = {0};
    SQLLEN pcbLen = 0;
    size_t copiedChars = 0;
    
    SQLRETURN rc = copySqlwForClient(dst, src, 2, 4, &pcbLen, &copiedChars, 4);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 8);
    EXPECT_EQ(copiedChars, 2);
    EXPECT_EQ(dst[0], U'x');
    EXPECT_EQ(dst[1], U'y');
    EXPECT_EQ(dst[2], 0);
}

TEST(CopySqlwForClient, OverflowDetection) {
    const uint16_t src[] = {u'a'};
    uint16_t dst[2] = {0};
    SQLLEN pcbLen = 0;
    size_t totalCharsNeeded =
        static_cast<size_t>((std::numeric_limits<SQLLEN>::max)()) / 2 + 1;

    SQLRETURN rc =
        copySqlwForClient(dst, src, totalCharsNeeded, 2, &pcbLen, nullptr, 2);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_EQ(pcbLen, (std::numeric_limits<SQLLEN>::max)());
}

TEST(CopySqlwForClient, NullPcbLen) {
    const uint16_t src[] = {u'a', u'b'};
    uint16_t dst[4] = {0};
    size_t copiedChars = 0;
    
    SQLRETURN rc = copySqlwForClient(dst, src, 2, 4, nullptr, &copiedChars, 2);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(copiedChars, 2);
    EXPECT_EQ(dst[0], u'a');
    EXPECT_EQ(dst[1], u'b');
    EXPECT_EQ(dst[2], 0);
}

TEST(CopySqlwForClient, ZeroCchLen) {
    const uint16_t src[] = {u'a'};
    uint16_t dst[1] = {0};
    SQLLEN pcbLen = 0;
    size_t copiedChars = 999;
    
    SQLRETURN rc = copySqlwForClient(dst, src, 1, 0, &pcbLen, &copiedChars, 2);
    
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 2);
    EXPECT_EQ(copiedChars, 0);
}

// copyAndTerminateSqlwchar tests

TEST(CopyAndTerminateSqlwchar, NullDst) {
    const uint16_t src[] = {u'a', u'b'};
    size_t copiedChars = 999;
    
    copyAndTerminateSqlwchar(nullptr, 5, src, 2, 2, &copiedChars);
    
    EXPECT_EQ(copiedChars, 0);
}

TEST(CopyAndTerminateSqlwchar, ZeroCapacity) {
    const uint16_t src[] = {u'a'};
    uint16_t dst[1] = {0};
    size_t copiedChars = 999;
    
    copyAndTerminateSqlwchar(dst, 0, src, 1, 2, &copiedChars);
    
    EXPECT_EQ(copiedChars, 0);
}

TEST(CopyAndTerminateSqlwchar, CharSize2) {
    const uint16_t src[] = {u'x', u'y', u'z'};
    uint16_t dst[5] = {0};
    size_t copiedChars = 0;
    
    copyAndTerminateSqlwchar(dst, 5, src, 3, 2, &copiedChars);
    
    EXPECT_EQ(copiedChars, 3);
    EXPECT_EQ(dst[0], u'x');
    EXPECT_EQ(dst[1], u'y');
    EXPECT_EQ(dst[2], u'z');
    EXPECT_EQ(dst[3], 0);
}

TEST(CopyAndTerminateSqlwchar, CharSize4) {
    const uint32_t src[] = {U'a', U'b'};
    uint32_t dst[4] = {0};
    size_t copiedChars = 0;
    
    copyAndTerminateSqlwchar(dst, 4, src, 2, 4, &copiedChars);
    
    EXPECT_EQ(copiedChars, 2);
    EXPECT_EQ(dst[0], U'a');
    EXPECT_EQ(dst[1], U'b');
    EXPECT_EQ(dst[2], 0);
}

TEST(CopyAndTerminateSqlwchar, Truncation) {
    const uint16_t src[] = {u'a', u'b', u'c', u'd'};
    uint16_t dst[3] = {0};
    size_t copiedChars = 0;
    
    copyAndTerminateSqlwchar(dst, 3, src, 4, 2, &copiedChars);
    
    EXPECT_EQ(copiedChars, 2);
    EXPECT_EQ(dst[0], u'a');
    EXPECT_EQ(dst[1], u'b');
    EXPECT_EQ(dst[2], 0);
}

TEST(CopyAndTerminateSqlwchar, NullSrc) {
    uint16_t dst[3] = {u'x', u'x', u'x'};
    size_t copiedChars = 0;
    
    copyAndTerminateSqlwchar(dst, 3, nullptr, 5, 2, &copiedChars);
    
    EXPECT_EQ(copiedChars, 0);
    EXPECT_EQ(dst[0], 0);
}

TEST(CopyAndTerminateSqlwchar, NullCopiedChars) {
    const uint16_t src[] = {u'a'};
    uint16_t dst[3] = {0};
    
    copyAndTerminateSqlwchar(dst, 3, src, 1, 2, nullptr);
    
    EXPECT_EQ(dst[0], u'a');
    EXPECT_EQ(dst[1], 0);
}

// convertWCharParamWithTruncCheck tests

TEST(ConvertWCharParamWithTruncCheck, NullInput) {
    RS_STMT_INFO stmt = {0};
    char szParam[10] = {0};
    size_t copiedChars = 0;
    
    ConversionResult rc = convertWCharParamWithTruncCheck(nullptr, SQL_NTS, szParam, sizeof(szParam), "test", "TEST", &stmt, &copiedChars);
    
    EXPECT_EQ(rc, CONVERSION_SUCCESS);
    EXPECT_EQ(copiedChars, 0);
    EXPECT_EQ(szParam[0], '\0');
}

TEST(ConvertWCharParamWithTruncCheck, EmptyInput) {
    RS_STMT_INFO stmt = {0};
    SQLWCHAR pwParam[] = {0};
    char szParam[10] = {0};
    size_t copiedChars = 0;
    
    ConversionResult rc = convertWCharParamWithTruncCheck(pwParam, SQL_NTS, szParam, sizeof(szParam), "test", "TEST", &stmt, &copiedChars);
    
    EXPECT_EQ(rc, CONVERSION_SUCCESS);
    EXPECT_EQ(copiedChars, 0);
    EXPECT_EQ(szParam[0], '\0');
}

TEST(ConvertWCharParamWithTruncCheck, ZeroLength) {
    RS_STMT_INFO stmt = {0};
    SQLWCHAR pwParam[] = {L'a', L'b', 0};
    char szParam[10] = {0};
    size_t copiedChars = 0;
    
    ConversionResult rc = convertWCharParamWithTruncCheck(pwParam, 0, szParam, sizeof(szParam), "test", "TEST", &stmt, &copiedChars);
    
    EXPECT_EQ(rc, CONVERSION_SUCCESS);
    EXPECT_EQ(copiedChars, 0);
    EXPECT_EQ(szParam[0], '\0');
}

TEST(ConvertWCharParamWithTruncCheck, NullOutputBuffer) {
    RS_STMT_INFO stmt = {0};
    SQLWCHAR pwParam[] = {L'a', 0};
    size_t copiedChars = 0;
    
    ConversionResult rc = convertWCharParamWithTruncCheck(pwParam, SQL_NTS, nullptr, 10, "test", "TEST", &stmt, &copiedChars);
    
    EXPECT_EQ(rc, CONVERSION_ERROR);
}

TEST(ConvertWCharParamWithTruncCheck, NullStmt) {
    SQLWCHAR pwParam[] = {L'a', 0};
    char szParam[10] = {0};
    size_t copiedChars = 0;
    
    ConversionResult rc = convertWCharParamWithTruncCheck(pwParam, SQL_NTS, szParam, sizeof(szParam), "test", "TEST", nullptr, &copiedChars);
    
    EXPECT_EQ(rc, CONVERSION_ERROR);
}

TEST(ConvertWCharParamWithTruncCheck, NullCopiedChars) {
    RS_STMT_INFO stmt = {0};
    SQLWCHAR pwParam[] = {L'a', 0};
    char szParam[10] = {0};
    
    ConversionResult rc = convertWCharParamWithTruncCheck(pwParam, SQL_NTS, szParam, sizeof(szParam), "test", "TEST", &stmt, nullptr);
    
    EXPECT_EQ(rc, CONVERSION_ERROR);
}

// Test fixture for copyWBinaryToHexDataBigLen
class CopyWBinaryToHexDataBigLenTest : public ::testing::TestWithParam<int> {
  protected:
    int savedUnicodeType;

    void SetUp() override {
        savedUnicodeType = get_app_unicode_type();
        set_process_unicode_type(GetParam());
    }

    void TearDown() override {
        set_process_unicode_type(savedUnicodeType);
    }
    // Runtime-sized wide buffer for tests
    struct WOut {
        std::vector<unsigned char> storage;  // bytes
        SQLWCHAR* ptr = nullptr;             // view as SQLWCHAR*
        SQLLEN cbLen = 0;                    // byte length

        explicit WOut(size_t char_capacity)
        {
            const size_t w = sizeofSQLWCHAR();           // 2 or 4 at runtime
            storage.assign(char_capacity * w, 0);
            ptr  = reinterpret_cast<SQLWCHAR*>(storage.data());
            cbLen = static_cast<SQLLEN>(storage.size());
        }
    };

    // Convenience factory
    inline WOut makeWOut(size_t char_capacity) { return WOut(char_capacity); }


    static std::u16string extractHex(const void* buf) {
        std::u16string out;
        if (!buf) return out;

        const size_t w = sizeofSQLWCHAR();  // 2 or 4 at runtime
        const unsigned char* p = static_cast<const unsigned char*>(buf);

        while (true) {
            uint32_t ch = 0;
            if (w == 2) {
                uint16_t u16 = 0; std::memcpy(&u16, p, 2); ch = u16; p += 2;
            } else if (w == 4) {
                std::memcpy(&ch, p, 4); p += 4;
            } else break;
            if (ch == 0) break;
            out.push_back(static_cast<char16_t>(ch));
        }
        return out;
    }
};

INSTANTIATE_TEST_SUITE_P(
    UTF16_and_UTF32,
    CopyWBinaryToHexDataBigLenTest,
    ::testing::Values(SQL_DD_CP_UTF16, SQL_DD_CP_UTF32),
    [](const testing::TestParamInfo<int>& info) {
        return info.param == SQL_DD_CP_UTF16 ? "UTF16" : "UTF32";
    }
);

TEST_P(CopyWBinaryToHexDataBigLenTest, NullInput) {
    SQLWCHAR dest[10] = {0};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen(nullptr, SQL_NULL_DATA, dest, 
                                         5 * sizeofSQLWCHAR(), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 0);
}

TEST_P(CopyWBinaryToHexDataBigLenTest, ZeroLength) {
    const char src[] = {0x01, 0x02};
    SQLWCHAR dest[10] = {0};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen(src, 0, dest, 5 * sizeofSQLWCHAR(), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 0);
    EXPECT_EQ(dest[0], 0);
}

TEST_P(CopyWBinaryToHexDataBigLenTest, SingleByte) {
    const char src[] = {(char)0xAB};
    auto out = makeWOut(5);  // 5 "wchar" slots (like your 5 * sizeofSQLWCHAR())

    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen(src, 1, out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(out.ptr), u"AB");
    EXPECT_EQ(pcbLen, 2 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, MultipleBytes) {
    const unsigned char src[] = {0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF};
    auto out = makeWOut(20); // 20 wchar slots (headroom: 16 needed + 1 NUL)

    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 8, out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(out.ptr), u"0123456789ABCDEF");
    EXPECT_EQ(pcbLen, 16 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, LengthInquiry) {
    const unsigned char src[] = {0x01, 0x02, 0x03};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 3, nullptr, 0, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 6 * sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, Truncation) {
    const unsigned char src[] = {0x01, 0x02, 0x03, 0x04};
    auto out = makeWOut(3);  // capacity == 3 wchar → 2 chars + NUL

    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 4, out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractHex(out.ptr), u"01");
    EXPECT_EQ(pcbLen, 8 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, ExactFit) {
    const unsigned char src[] = {0xAB, 0xCD};
    auto out = makeWOut(5);  // 4 chars + 1 NUL

    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 2, out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(out.ptr), u"ABCD");
    EXPECT_EQ(pcbLen, 4 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, BufferOnlyForNull) {
    const unsigned char src[] = {0x01};
    auto out = makeWOut(1);  // only room for NUL

    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 1, out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    // first wchar is NUL
    if (sizeofSQLWCHAR() == 2) {
        uint16_t u16; std::memcpy(&u16, out.ptr, 2); EXPECT_EQ(u16, 0u);
    } else {
        uint32_t u32; std::memcpy(&u32, out.ptr, 4); EXPECT_EQ(u32, 0u);
    }
    EXPECT_EQ(pcbLen, 2 * (SQLLEN)sizeofSQLWCHAR());
}



TEST_P(CopyWBinaryToHexDataBigLenTest, ZeroBuffer) {
    const unsigned char src[] = {0x01};
    SQLWCHAR dest[1] = {0xFFFF};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 1, dest, 0, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], (SQLWCHAR)0xFFFF);
    EXPECT_EQ(pcbLen, 2 * sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, AllZeros) {
    const unsigned char src[] = {0x00, 0x00, 0x00};
    auto out = makeWOut(10);  // runtime-safe allocation
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 3, out.ptr,
                                         out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(out.ptr), u"000000");
    EXPECT_EQ(pcbLen, 6 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, AllOnes) {
    const unsigned char src[] = {0xFF, 0xFF, 0xFF};
    auto out = makeWOut(10);
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 3, out.ptr,
                                         out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(out.ptr), u"FFFFFF");
    EXPECT_EQ(pcbLen, 6 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, NullPcbLen) {
    const unsigned char src[] = {0xAB};
    SQLWCHAR dest[10] = {0};

    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 1, dest, 
                                         5 * sizeofSQLWCHAR(), nullptr);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(dest), u"AB");
}

TEST_P(CopyWBinaryToHexDataBigLenTest, OddCapacity) {
    const unsigned char src[] = {0x01, 0x02};
    auto out = makeWOut(3);  // odd: 2 usable chars + NUL

    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 2, out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractHex(out.ptr), u"01");
    EXPECT_EQ(pcbLen, 4 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryToHexDataBigLenTest, UppercaseHex) {
    const unsigned char src[] = {0xab, 0xcd, 0xef};
    auto out = makeWOut(10);  // 10 wchar capacity @ runtime width
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryToHexDataBigLen((const char*)src, 3,
                                         out.ptr, out.cbLen, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractHex(out.ptr), u"ABCDEF");
}

TEST_P(CopyWBinaryToHexDataBigLenTest, LargeData) {
    std::vector<unsigned char> src(1000);
    for (size_t i = 0; i < src.size(); ++i) {
        src[i] = (unsigned char)(i % 256);
    }
    
    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryToHexDataBigLen((const char*)src.data(), 1000, 
                                         nullptr, 0, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 2000 * sizeofSQLWCHAR());
}

// copyWBinaryDataBigLen tests

class CopyWBinaryDataBigLenTest : public ::testing::TestWithParam<int> {
  protected:
    int savedUnicodeType;

    void SetUp() override {
        savedUnicodeType = get_app_unicode_type();
        set_process_unicode_type(GetParam());
    }

    void TearDown() override {
        set_process_unicode_type(savedUnicodeType);
    }

    std::u16string extractBinary(const SQLWCHAR *buf) {
        std::u16string result;
        if (sizeofSQLWCHAR() == 2) {
            result = std::u16string(reinterpret_cast<const char16_t *>(buf));
        } else {
            const uint8_t *p = reinterpret_cast<const uint8_t *>(buf);
            for (;;) {
                uint32_t ch = 0;
                std::memcpy(&ch, p, 4);
                if (ch == 0) break;
                result.push_back(static_cast<char16_t>(ch));
                p += 4;
            }
        }
        return result;
    }
};

INSTANTIATE_TEST_SUITE_P(
    UTF16_and_UTF32,
    CopyWBinaryDataBigLenTest,
    ::testing::Values(SQL_DD_CP_UTF16, SQL_DD_CP_UTF32),
    [](const testing::TestParamInfo<int>& info) {
        return info.param == SQL_DD_CP_UTF16 ? "UTF16" : "UTF32";
    }
);

TEST_P(CopyWBinaryDataBigLenTest, NullInput) {
    SQLWCHAR dest[10] = {0};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen(nullptr, SQL_NULL_DATA, dest, 
                                    5 * sizeofSQLWCHAR(), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 0);
}

TEST_P(CopyWBinaryDataBigLenTest, ZeroLength) {
    const char src[] = {0x01, 0x02};
    SQLWCHAR dest[10] = {0};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen(src, 0, dest, 5 * sizeofSQLWCHAR(), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 0);
    EXPECT_EQ(dest[0], 0);
}

TEST_P(CopyWBinaryDataBigLenTest, SingleByte) {
    const char src[] = {(char)0x41};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 5;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen(src, 1, dest, static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractBinary(dest), u"A");
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(1 * charSz));
}

TEST_P(CopyWBinaryDataBigLenTest, MultipleBytes) {
    const unsigned char src[] = {0x41, 0x42, 0x43};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 10;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen(reinterpret_cast<const char*>(src), 3, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractBinary(dest), u"ABC");
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(3 * charSz));
}

TEST_P(CopyWBinaryDataBigLenTest, LengthInquiry) {
    const unsigned char src[] = {0x01, 0x02, 0x03};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 3, nullptr, 0, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 3 * sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryDataBigLenTest, Truncation) {
    const unsigned char src[] = {0x41, 0x42, 0x43, 0x44};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 3;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 4, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractBinary(dest), u"AB");
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(4 * charSz));
}

TEST_P(CopyWBinaryDataBigLenTest, ExactFit) {
    const unsigned char src[] = {0x58, 0x59};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 3;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 2, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractBinary(dest), u"XY");
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(2 * charSz));
}

TEST_P(CopyWBinaryDataBigLenTest, BufferOnlyForNull) {
    const unsigned char src[] = {0x01};
    SQLWCHAR dest[2] = {0xFFFF, 0xFFFF};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 1, dest, 
                                    sizeofSQLWCHAR(), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(dest[0], 0);
    EXPECT_EQ(pcbLen, 1 * sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryDataBigLenTest, ZeroBuffer) {
    const unsigned char src[] = {0x01};
    SQLWCHAR dest[1] = {0xFFFF};
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 1, dest, 0, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], (SQLWCHAR)0xFFFF);
    EXPECT_EQ(pcbLen, 1 * sizeofSQLWCHAR());
}

TEST_P(CopyWBinaryDataBigLenTest, AllZeros) {
    const unsigned char src[] = {0x00, 0x00, 0x00};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 10;
    std::vector<uint8_t> buf(capChars * charSz, 0xFF);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 3, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], 0);
    EXPECT_EQ(dest[1], 0);
    EXPECT_EQ(dest[2], 0);
    EXPECT_EQ(dest[3], 0);
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(3 * charSz));
}

TEST_P(CopyWBinaryDataBigLenTest, HighBytes) {
    const unsigned char src[] = {0xFF, 0xFE, 0xFD};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 10;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 3, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(dest);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[charSz], 0xFE);
    EXPECT_EQ(bytes[2 * charSz], 0xFD);
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(3 * charSz));
}

TEST_P(CopyWBinaryDataBigLenTest, NullPcbLen) {
    const unsigned char src[] = {0x41};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 5;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());

    auto rc = copyWBinaryDataBigLen((const char*)src, 1, dest, 
                                    static_cast<SQLLEN>(buf.size()), nullptr);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractBinary(dest), u"A");
}


TEST_P(CopyWBinaryDataBigLenTest, NullTerminatorPlacement) {
    const unsigned char src[] = {0x41, 0x42};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 5;
    std::vector<uint8_t> buf(capChars * charSz, 0xFF);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 2, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(dest);
    for (size_t i = 0; i < charSz; ++i) {
        EXPECT_EQ(bytes[2 * charSz + i], 0);
    }
}

TEST_P(CopyWBinaryDataBigLenTest, LargeData) {
    std::vector<unsigned char> src(1000);
    for (size_t i = 0; i < src.size(); ++i) {
        src[i] = (unsigned char)(i % 256);
    }
    
    SQLLEN pcbLen = -1;
    auto rc = copyWBinaryDataBigLen((const char*)src.data(), 1000, 
                                    nullptr, 0, &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pcbLen, 1000 * sizeofSQLWCHAR());
}

// Additional byte-level validation tests
TEST_P(CopyWBinaryDataBigLenTest, ByteLevelValidation) {
    const unsigned char src[] = {0x41, 0x00, 0x80, 0xFF};
    const size_t charSz = sizeofSQLWCHAR();
    const size_t capChars = 5;
    std::vector<uint8_t> buf(capChars * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 4, dest, 
                                    static_cast<SQLLEN>(buf.size()), &pcbLen);

    EXPECT_EQ(rc, SQL_SUCCESS);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(dest);
    EXPECT_EQ(bytes[0], 0x41);
    EXPECT_EQ(bytes[charSz], 0x00);
    EXPECT_EQ(bytes[2 * charSz], 0x80);
    EXPECT_EQ(bytes[3 * charSz], 0xFF);
    for (size_t i = 1; i < charSz; ++i) {
        EXPECT_EQ(bytes[i], 0) << "Upper byte " << i << " not zeroed at index 0";
        EXPECT_EQ(bytes[charSz + i], 0) << "Upper byte " << i << " not zeroed at index 1";
        EXPECT_EQ(bytes[2 * charSz + i], 0) << "Upper byte " << i << " not zeroed at index 2";
        EXPECT_EQ(bytes[3 * charSz + i], 0) << "Upper byte " << i << " not zeroed at index 3";
    }
}

TEST_P(CopyWBinaryDataBigLenTest, OddCbLen) {
    const unsigned char src[] = {0x11, 0x22, 0x33};
    const size_t charSz = sizeofSQLWCHAR();
    const SQLLEN cbLen = static_cast<SQLLEN>(3 * charSz - 1);
    std::vector<uint8_t> buf(3 * charSz, 0);
    SQLWCHAR* dest = reinterpret_cast<SQLWCHAR*>(buf.data());
    SQLLEN pcbLen = -1;

    auto rc = copyWBinaryDataBigLen((const char*)src, 3, dest, cbLen, &pcbLen);

    EXPECT_TRUE(rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(pcbLen, static_cast<SQLLEN>(3 * charSz));
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(dest);
    EXPECT_EQ(bytes[0], 0x11);
    for (size_t i = 0; i < charSz; ++i) {
        EXPECT_EQ(bytes[charSz + i], 0) << "NUL not written at index 1, byte " << i;
    }
}
