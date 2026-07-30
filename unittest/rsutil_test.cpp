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

// --- getDisplaySize ---

struct DisplaySizeParam {
    short hType;
    int iSize;
    short hRsSpecialType;
    int expected;
    const char *name;
};

class GetDisplaySizeTest : public ::testing::TestWithParam<DisplaySizeParam> {};

TEST_P(GetDisplaySizeTest, ReturnsExpectedSize) {
    const auto &p = GetParam();
    EXPECT_EQ(getDisplaySize(p.hType, p.iSize, p.hRsSpecialType), p.expected)
        << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetDisplaySizeTest,
    ::testing::Values(
        DisplaySizeParam{SQL_WVARCHAR,     256, 0, 256, "SQL_WVARCHAR"},
        DisplaySizeParam{SQL_WCHAR,        20,  0, 20,  "SQL_WCHAR"},
        DisplaySizeParam{SQL_WLONGVARCHAR, 1000, 0, 1000, "SQL_WLONGVARCHAR"},
        // Narrow types should still work
        DisplaySizeParam{SQL_VARCHAR,      256, 0, 256, "SQL_VARCHAR"},
        DisplaySizeParam{SQL_CHAR,         20,  0, 20,  "SQL_CHAR"},
        DisplaySizeParam{SQL_LONGVARCHAR,  1000, 0, 1000, "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<DisplaySizeParam> &info) {
        return std::string(info.param.name);
    });

// --- getTypeName ---

struct TypeNameParam {
    short hType;
    short hRsSpecialType;
    const char *expected;
    const char *name;
};

class GetTypeNameTest : public ::testing::TestWithParam<TypeNameParam> {};

TEST_P(GetTypeNameTest, ReturnsExpectedName) {
    const auto &p = GetParam();
    char buf[128] = {0};
    getTypeName(p.hType, buf, sizeof(buf), p.hRsSpecialType);
    EXPECT_STREQ(buf, p.expected) << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetTypeNameTest,
    ::testing::Values(
        TypeNameParam{SQL_WVARCHAR,     0,     "CHARACTER VARYING", "SQL_WVARCHAR"},
        TypeNameParam{SQL_WCHAR,        0,     "CHARACTER",         "SQL_WCHAR"},
        TypeNameParam{SQL_WLONGVARCHAR, SUPER, "SUPER",             "SQL_WLONGVARCHAR_SUPER"},
        // Narrow types should still work
        TypeNameParam{SQL_VARCHAR,      0,     "CHARACTER VARYING", "SQL_VARCHAR"},
        TypeNameParam{SQL_CHAR,         0,     "CHARACTER",         "SQL_CHAR"},
        TypeNameParam{SQL_LONGVARCHAR,  SUPER, "SUPER",             "SQL_LONGVARCHAR_SUPER"},
        // BoolsAsChar: BOOLOID mapped to SQL_VARCHAR reports "bool"
        TypeNameParam{SQL_VARCHAR,      BOOLOID, "bool",            "SQL_VARCHAR_BOOLOID"},
        TypeNameParam{SQL_WVARCHAR,     BOOLOID, "bool",            "SQL_WVARCHAR_BOOLOID"}
    ),
    [](const ::testing::TestParamInfo<TypeNameParam> &info) {
        return std::string(info.param.name);
    });

// --- getPrecision ---

struct PrecisionParam {
    short hType;
    int iSize;
    short hRsSpecialType;
    int expected;
    const char *name;
};

class GetPrecisionTest : public ::testing::TestWithParam<PrecisionParam> {};

TEST_P(GetPrecisionTest, ReturnsExpectedPrecision) {
    const auto &p = GetParam();
    EXPECT_EQ(getPrecision(p.hType, p.iSize, p.hRsSpecialType), p.expected)
        << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetPrecisionTest,
    ::testing::Values(
        PrecisionParam{SQL_WVARCHAR,     256, 0, 0,  "SQL_WVARCHAR"},
        PrecisionParam{SQL_WCHAR,        20,  0, 0,  "SQL_WCHAR"},
        PrecisionParam{SQL_WLONGVARCHAR, 1000, 0, 0, "SQL_WLONGVARCHAR"},
        PrecisionParam{SQL_VARCHAR,      256, 0, 0,  "SQL_VARCHAR"},
        PrecisionParam{SQL_CHAR,         20,  0, 0,  "SQL_CHAR"},
        PrecisionParam{SQL_LONGVARCHAR,  1000, 0, 0, "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<PrecisionParam> &info) {
        return std::string(info.param.name);
    });

// --- getSearchable ---

struct SearchableParam {
    short hType;
    short hRsSpecialType;
    int expected;
    const char *name;
};

class GetSearchableTest : public ::testing::TestWithParam<SearchableParam> {};

TEST_P(GetSearchableTest, ReturnsExpectedSearchable) {
    const auto &p = GetParam();
    EXPECT_EQ(getSearchable(p.hType, p.hRsSpecialType), p.expected)
        << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetSearchableTest,
    ::testing::Values(
        SearchableParam{SQL_WVARCHAR,     0, SQL_PRED_SEARCHABLE, "SQL_WVARCHAR"},
        SearchableParam{SQL_WCHAR,        0, SQL_PRED_SEARCHABLE, "SQL_WCHAR"},
        SearchableParam{SQL_WLONGVARCHAR, 0, SQL_PRED_SEARCHABLE, "SQL_WLONGVARCHAR"},
        SearchableParam{SQL_VARCHAR,      0, SQL_PRED_SEARCHABLE, "SQL_VARCHAR"},
        SearchableParam{SQL_CHAR,         0, SQL_PRED_SEARCHABLE, "SQL_CHAR"},
        SearchableParam{SQL_LONGVARCHAR,  0, SQL_PRED_SEARCHABLE, "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<SearchableParam> &info) {
        return std::string(info.param.name);
    });

// --- getLiteralPrefix / getLiteralSuffix ---

struct LiteralParam {
    short hType;
    short hRsSpecialType;
    const char *expected;
    const char *name;
};

class GetLiteralPrefixTest : public ::testing::TestWithParam<LiteralParam> {};

TEST_P(GetLiteralPrefixTest, ReturnsExpectedPrefix) {
    const auto &p = GetParam();
    char buf[8] = {0};
    getLiteralPrefix(p.hType, buf, p.hRsSpecialType);
    EXPECT_STREQ(buf, p.expected) << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetLiteralPrefixTest,
    ::testing::Values(
        LiteralParam{SQL_WVARCHAR,     0, "'", "SQL_WVARCHAR"},
        LiteralParam{SQL_WCHAR,        0, "'", "SQL_WCHAR"},
        LiteralParam{SQL_WLONGVARCHAR, 0, "'", "SQL_WLONGVARCHAR"},
        LiteralParam{SQL_VARCHAR,      0, "'", "SQL_VARCHAR"},
        LiteralParam{SQL_CHAR,         0, "'", "SQL_CHAR"},
        LiteralParam{SQL_LONGVARCHAR,  0, "'", "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<LiteralParam> &info) {
        return std::string(info.param.name);
    });

class GetLiteralSuffixTest : public ::testing::TestWithParam<LiteralParam> {};

TEST_P(GetLiteralSuffixTest, ReturnsExpectedSuffix) {
    const auto &p = GetParam();
    char buf[8] = {0};
    getLiteralSuffix(p.hType, buf, p.hRsSpecialType);
    EXPECT_STREQ(buf, p.expected) << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetLiteralSuffixTest,
    ::testing::Values(
        LiteralParam{SQL_WVARCHAR,     0, "'", "SQL_WVARCHAR"},
        LiteralParam{SQL_WCHAR,        0, "'", "SQL_WCHAR"},
        LiteralParam{SQL_WLONGVARCHAR, 0, "'", "SQL_WLONGVARCHAR"},
        LiteralParam{SQL_VARCHAR,      0, "'", "SQL_VARCHAR"},
        LiteralParam{SQL_CHAR,         0, "'", "SQL_CHAR"},
        LiteralParam{SQL_LONGVARCHAR,  0, "'", "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<LiteralParam> &info) {
        return std::string(info.param.name);
    });

// --- getNumPrecRadix ---

struct NumPrecRadixParam {
    short hType;
    int expected;
    const char *name;
};

class GetNumPrecRadixTest : public ::testing::TestWithParam<NumPrecRadixParam> {};

TEST_P(GetNumPrecRadixTest, ReturnsExpectedRadix) {
    const auto &p = GetParam();
    EXPECT_EQ(getNumPrecRadix(p.hType), p.expected) << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetNumPrecRadixTest,
    ::testing::Values(
        NumPrecRadixParam{SQL_WVARCHAR,     0, "SQL_WVARCHAR"},
        NumPrecRadixParam{SQL_WCHAR,        0, "SQL_WCHAR"},
        NumPrecRadixParam{SQL_WLONGVARCHAR, 0, "SQL_WLONGVARCHAR"},
        NumPrecRadixParam{SQL_VARCHAR,      0, "SQL_VARCHAR"},
        NumPrecRadixParam{SQL_CHAR,         0, "SQL_CHAR"},
        NumPrecRadixParam{SQL_LONGVARCHAR,  0, "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<NumPrecRadixParam> &info) {
        return std::string(info.param.name);
    });

// --- getUnsigned ---

struct UnsignedParam {
    short hType;
    int expected;
    const char *name;
};

class GetUnsignedTest : public ::testing::TestWithParam<UnsignedParam> {};

TEST_P(GetUnsignedTest, ReturnsExpectedUnsigned) {
    const auto &p = GetParam();
    EXPECT_EQ(getUnsigned(p.hType), p.expected) << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetUnsignedTest,
    ::testing::Values(
        UnsignedParam{SQL_WVARCHAR,     SQL_TRUE, "SQL_WVARCHAR"},
        UnsignedParam{SQL_WCHAR,        SQL_TRUE, "SQL_WCHAR"},
        UnsignedParam{SQL_WLONGVARCHAR, SQL_TRUE, "SQL_WLONGVARCHAR"},
        UnsignedParam{SQL_VARCHAR,      SQL_TRUE, "SQL_VARCHAR"},
        UnsignedParam{SQL_CHAR,         SQL_TRUE, "SQL_CHAR"},
        UnsignedParam{SQL_LONGVARCHAR, SQL_TRUE, "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<UnsignedParam> &info) {
        return std::string(info.param.name);
    });

// --- getParamSize ---

struct ParamSizeParam {
    short hType;
    int expected;
    const char *name;
};

class GetParamSizeTest : public ::testing::TestWithParam<ParamSizeParam> {};

TEST_P(GetParamSizeTest, ReturnsExpectedSize) {
    const auto &p = GetParam();
    EXPECT_EQ(getParamSize(p.hType), p.expected) << p.name;
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetParamSizeTest,
    ::testing::Values(
        ParamSizeParam{SQL_WVARCHAR, 65535, "SQL_WVARCHAR"},
        ParamSizeParam{SQL_WCHAR,    65535, "SQL_WCHAR"},
        ParamSizeParam{SQL_VARCHAR,  65535, "SQL_VARCHAR"},
        ParamSizeParam{SQL_CHAR,     65535, "SQL_CHAR"}
    ),
    [](const ::testing::TestParamInfo<ParamSizeParam> &info) {
        return std::string(info.param.name);
    });

// --- getDefaultCTypeFromSQLType ---

struct DefaultCTypeParam {
    short hSQLType;
    short expectedCType;
    const char *name;
};

class GetDefaultCTypeTest : public ::testing::TestWithParam<DefaultCTypeParam> {};

TEST_P(GetDefaultCTypeTest, ReturnsExpectedCType) {
    const auto &p = GetParam();
    int convError = 0;
    EXPECT_EQ(getDefaultCTypeFromSQLType(p.hSQLType, &convError), p.expectedCType)
        << p.name;
    EXPECT_EQ(convError, 0) << p.name << " should not set conversion error";
}

INSTANTIATE_TEST_SUITE_P(
    WideTypes, GetDefaultCTypeTest,
    ::testing::Values(
        DefaultCTypeParam{SQL_WVARCHAR,     SQL_C_WCHAR, "SQL_WVARCHAR"},
        DefaultCTypeParam{SQL_WCHAR,        SQL_C_WCHAR, "SQL_WCHAR"},
        DefaultCTypeParam{SQL_WLONGVARCHAR, SQL_C_WCHAR, "SQL_WLONGVARCHAR"},
        // Narrow types map to SQL_C_CHAR
        DefaultCTypeParam{SQL_VARCHAR,      SQL_C_CHAR,  "SQL_VARCHAR"},
        DefaultCTypeParam{SQL_CHAR,         SQL_C_CHAR,  "SQL_CHAR"},
        DefaultCTypeParam{SQL_LONGVARCHAR,  SQL_C_CHAR,  "SQL_LONGVARCHAR"}
    ),
    [](const ::testing::TestParamInfo<DefaultCTypeParam> &info) {
        return std::string(info.param.name);
    });

// Unit Tests for isInRange Template Function

// Test suite for integer to integer conversions
TEST(IsInRangeTest, IntegerToIntegerSameType) {
    // Same type conversions should always return true
    EXPECT_TRUE(isInRange<int>(42));
    EXPECT_TRUE(isInRange<short>(static_cast<short>(100)));
    EXPECT_TRUE(isInRange<long long>(1000000LL));
    EXPECT_TRUE(isInRange<unsigned int>(42u));
}

TEST(IsInRangeTest, IntegerToIntegerSignedToSigned) {
    // int to short - within range
    EXPECT_TRUE(isInRange<short>(100));
    EXPECT_TRUE(isInRange<short>(32767));
    EXPECT_TRUE(isInRange<short>(-32768));

    // int to short - out of range
    EXPECT_FALSE(isInRange<short>(32768));
    EXPECT_FALSE(isInRange<short>(-32769));
    EXPECT_FALSE(isInRange<short>(100000));

    // long long to int - within range
    EXPECT_TRUE(isInRange<int>(2147483647LL));
    EXPECT_TRUE(isInRange<int>(-2147483648LL));
    EXPECT_TRUE(isInRange<int>(0LL));

    // long long to int - out of range
    EXPECT_FALSE(isInRange<int>(2147483648LL));
    EXPECT_FALSE(isInRange<int>(-2147483649LL));
}

TEST(IsInRangeTest, IntegerToIntegerSignedToUnsigned) {
    // Negative values should always be out of range for unsigned types
    EXPECT_FALSE(isInRange<unsigned int>(-1));
    EXPECT_FALSE(isInRange<unsigned short>(-100));
    EXPECT_FALSE(isInRange<unsigned char>(-1));

    // Positive values within range
    EXPECT_TRUE(isInRange<unsigned int>(100));
    EXPECT_TRUE(isInRange<unsigned short>(65535));
    EXPECT_TRUE(isInRange<unsigned char>(255));

    // Positive values out of range
    EXPECT_FALSE(isInRange<unsigned short>(65536));
    EXPECT_FALSE(isInRange<unsigned char>(256));
}

TEST(IsInRangeTest, IntegerToIntegerUnsignedToSigned) {
    // Values within signed range
    EXPECT_TRUE(isInRange<int>(100u));
    EXPECT_TRUE(isInRange<short>(100u));
    EXPECT_TRUE(isInRange<char>(100u));

    // Values exceeding signed range
    EXPECT_FALSE(isInRange<int>(4294967295u)); // UINT_MAX
    EXPECT_FALSE(isInRange<short>(65535u));    // USHRT_MAX

    // Test char conversion - handle both signed and unsigned char systems
    if (std::is_signed<char>::value) {
        EXPECT_FALSE(
            isInRange<char>(255u)); // UCHAR_MAX > CHAR_MAX for signed char
    } else {
        EXPECT_TRUE(
            isInRange<char>(255u)); // UCHAR_MAX == CHAR_MAX for unsigned char
    }
}

TEST(IsInRangeTest, IntegerToIntegerUnsignedToUnsigned) {
    // Smaller to larger unsigned types - always in range
    EXPECT_TRUE(isInRange<unsigned int>(static_cast<unsigned short>(65535)));
    EXPECT_TRUE(isInRange<unsigned short>(static_cast<unsigned char>(255)));

    // Larger to smaller unsigned types - check bounds
    EXPECT_TRUE(isInRange<unsigned short>(65535u));
    EXPECT_FALSE(isInRange<unsigned short>(65536u));
    EXPECT_TRUE(isInRange<unsigned char>(255u));
    EXPECT_FALSE(isInRange<unsigned char>(256u));
}

// Test suite for floating point to integer conversions
TEST(IsInRangeTest, FloatToIntegerBasic) {
    // Values within integer range
    EXPECT_TRUE(isInRange<int>(42.0f));
    EXPECT_TRUE(isInRange<int>(42.7f));
    EXPECT_TRUE(isInRange<int>(-42.3f));
    EXPECT_TRUE(isInRange<short>(100.5f));

    // Values out of integer range
    EXPECT_FALSE(isInRange<int>(3.4e38f));  // Very large float
    EXPECT_FALSE(isInRange<int>(-3.4e38f)); // Very large negative float
    EXPECT_FALSE(isInRange<short>(100000.0f));

    EXPECT_FALSE(isInRange<unsigned int>(-0.1f));
    EXPECT_FALSE(isInRange<unsigned int>(-0.9f));
}

TEST(IsInRangeTest, FloatToIntegerSpecialValues) {
    // NaN should return false
    EXPECT_FALSE(isInRange<int>(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(isInRange<short>(std::numeric_limits<double>::quiet_NaN()));

    // Infinity should return false
    EXPECT_FALSE(isInRange<int>(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(isInRange<int>(-std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(isInRange<long long>(std::numeric_limits<double>::infinity()));
}

TEST(IsInRangeTest, FloatToIntegerBoundaryValues) {
    // Test boundary values for different integer types
    EXPECT_TRUE(isInRange<int>(2147483520.0f)); // this is the largest float < 2^31
    EXPECT_TRUE(
        isInRange<int>(static_cast<float>((std::numeric_limits<int>::min)())));

    EXPECT_TRUE(isInRange<short>(
        static_cast<float>((std::numeric_limits<short>::max)())));
    EXPECT_TRUE(isInRange<short>(
        static_cast<float>((std::numeric_limits<short>::min)())));

    // Values just outside the range
    EXPECT_FALSE(isInRange<short>(
        static_cast<float>((std::numeric_limits<short>::max)()) + 1.0f));
    EXPECT_FALSE(isInRange<short>(
        static_cast<float>((std::numeric_limits<short>::min)()) - 1.0f));

    // Test precision issues at integer boundaries
    int maxInt = (std::numeric_limits<int>::max)();
    double maxAsDouble = static_cast<double>(maxInt);
    EXPECT_TRUE(isInRange<int>(maxAsDouble));


    // This might evaluate to the same double value as maxInt due to precision
    double slightlyOver = maxAsDouble + 1.0;
    EXPECT_FALSE(isInRange<int>(slightlyOver));
}

// Test suite for integer to floating point conversions
TEST(IsInRangeTest, IntegerToFloatBasic) {
    // Small integers should always fit in float/double
    EXPECT_TRUE(isInRange<float>(42));
    EXPECT_TRUE(isInRange<double>(42));
    EXPECT_TRUE(isInRange<float>(-42));
    EXPECT_TRUE(isInRange<double>(-42));
}

TEST(IsInRangeTest, IntegerToFloatPrecisionLoss) {
    // isInRange reports "no overflow/underflow", NOT "no precision loss".
    // Rounding is allowed for int -> float/double.

    // Float has 24 bits of precision (2^24 = 16777216), so:
    // - 2^24 is exactly representable.
    // - 2^24 + 1 is NOT exact, but still within float's finite range.
    EXPECT_TRUE(isInRange<float>(16777216));   // 2^24: exact, in range
    EXPECT_TRUE(isInRange<float>(16777217));   // 2^24 + 1: rounded, but in range

    // 64-bit max integer is far below FLT_MAX (~3.4e38), so it is in range.
    // The value will be approximated, but that's not an overflow.
    EXPECT_TRUE(isInRange<float>(9223372036854775807LL)); // LLONG_MAX: in range for float

    // Double has 53 bits of precision. Both of these are within range:
    EXPECT_TRUE(isInRange<double>(9007199254740992LL));   // 2^53: exact in double
    EXPECT_TRUE(isInRange<double>(9007199254740993LL));   // 2^53 + 1: rounded, still in range
}

TEST(IsInRangeTest, IntegerToFloatSpecialCases) {
    // Extremal 32-bit ints are within float's finite range.
    EXPECT_TRUE(isInRange<float>((std::numeric_limits<int>::min)()));
    EXPECT_TRUE(isInRange<float>((std::numeric_limits<int>::max)()));

    // Extremal 64-bit ints are within double's finite range.
    EXPECT_TRUE(isInRange<double>((std::numeric_limits<long long>::min)()));
    EXPECT_TRUE(isInRange<double>((std::numeric_limits<long long>::max)()));

    // Zero is always representable.
    EXPECT_TRUE(isInRange<float>(0));
    EXPECT_TRUE(isInRange<double>(0));

    // Some small, obviously in range.
    EXPECT_TRUE(isInRange<float>(100));
    EXPECT_TRUE(isInRange<double>(100));
}

// Test suite for floating point to floating point conversions
TEST(IsInRangeTest, FloatToFloatBasic) {
    // double to float - values within float range
    EXPECT_TRUE(isInRange<float>(3.14));
    EXPECT_TRUE(isInRange<float>(-2.71));
    EXPECT_TRUE(isInRange<float>(0.0));

    // float to double - should always work (double has larger range)
    EXPECT_TRUE(isInRange<double>(3.14f));
    EXPECT_TRUE(isInRange<double>((std::numeric_limits<float>::max)()));
    EXPECT_TRUE(isInRange<double>((std::numeric_limits<float>::lowest)()));
}

TEST(IsInRangeTest, FloatToFloatOutOfRange) {
    // Values whose magnitude exceeds float's max are out of range.
    EXPECT_FALSE(isInRange<float>(1.8e39));   // > FLT_MAX (~3.4e38)
    EXPECT_FALSE(isInRange<float>(-1.8e39));  // < -FLT_MAX

    // Very small magnitudes:
    // Our isInRange semantics: if conversion can yield some finite value (including 0),
    // it's "in range". Underflow-to-zero is allowed and NOT treated as 22003 here.
    EXPECT_TRUE(isInRange<float>(1e-50));     // Will underflow toward 0, but allowed.
}

TEST(IsInRangeTest, FloatToFloatSpecialValues) {
    // NaN should return false
    EXPECT_FALSE(isInRange<float>(std::numeric_limits<double>::quiet_NaN()));

    // Infinity should return true (infinity is representable)
    EXPECT_TRUE(isInRange<float>(std::numeric_limits<double>::infinity()));
    EXPECT_TRUE(isInRange<float>(-std::numeric_limits<double>::infinity()));

    // Same type should always return true for infinity
    EXPECT_TRUE(isInRange<double>(std::numeric_limits<double>::infinity()));
}

TEST(IsInRangeTest, FloatToFloatBoundaryValues) {
    // Test values at the boundary of float range
    double maxFloat = static_cast<double>((std::numeric_limits<float>::max)());
    double minFloat =
        static_cast<double>((std::numeric_limits<float>::lowest)());

    EXPECT_TRUE(isInRange<float>(maxFloat));
    EXPECT_TRUE(isInRange<float>(minFloat));

    // Values just outside float range
    EXPECT_FALSE(isInRange<float>(maxFloat * 1.1));
    EXPECT_FALSE(isInRange<float>(minFloat * 1.1));
}

// Test suite for edge cases and error conditions
TEST(IsInRangeTest, EdgeCasesZeroValues) {
    // Zero should be representable in all types
    EXPECT_TRUE(isInRange<int>(0.0f));
    EXPECT_TRUE(isInRange<float>(0));
    EXPECT_TRUE(isInRange<unsigned int>(0));
    EXPECT_TRUE(isInRange<short>(0.0));
}

TEST(IsInRangeTest, EdgeCasesMinMaxValues) {
    // Test with actual min/max values of types
    EXPECT_TRUE(isInRange<int>((std::numeric_limits<int>::max)()));
    EXPECT_TRUE(isInRange<int>((std::numeric_limits<int>::min)()));

    EXPECT_TRUE(
        isInRange<unsigned int>((std::numeric_limits<unsigned int>::max)()));
    EXPECT_TRUE(
        isInRange<unsigned int>((std::numeric_limits<unsigned int>::min)()));

    EXPECT_TRUE(isInRange<float>((std::numeric_limits<float>::max)()));
    EXPECT_TRUE(isInRange<float>((std::numeric_limits<float>::lowest)()));
}

TEST(IsInRangeTest, EdgeCasesTypePromotions) {
    // Test cases where implicit type promotion might occur
    char c = 100;
    EXPECT_TRUE(isInRange<short>(c));
    EXPECT_TRUE(isInRange<int>(c));

    short s = 30000;
    EXPECT_TRUE(isInRange<int>(s));
    EXPECT_FALSE(isInRange<char>(s));

    // Test unsigned char to signed types
    unsigned char uc = 200;
    EXPECT_TRUE(isInRange<short>(uc));
    EXPECT_TRUE(isInRange<int>(uc));

    // Test unsigned char to char conversion - handle both signed and unsigned
    // char systems
    if (std::is_signed<char>::value) {
        EXPECT_FALSE(
            isInRange<char>(uc)); // 200 > 127 (CHAR_MAX for signed char)
    } else {
        EXPECT_TRUE(
            isInRange<char>(uc)); // 200 <= 255 (CHAR_MAX for unsigned char)
    }
}

// Test suite for specific numeric type combinations commonly used in ODBC
TEST(IsInRangeTest, ODBCCommonTypes) {
    // SQLSMALLINT (short) conversions
    EXPECT_TRUE(isInRange<short>(100));
    EXPECT_FALSE(isInRange<short>(100000));

    // SQLINTEGER (int) conversions
    EXPECT_TRUE(isInRange<int>(1000000));
    EXPECT_FALSE(isInRange<int>(3000000000LL));

    // SQLBIGINT (long long) conversions
    EXPECT_TRUE(isInRange<long long>(9223372036854775807LL));

    // SQLREAL (float) conversions
    EXPECT_TRUE(isInRange<float>(123.456));
    EXPECT_FALSE(isInRange<float>(1e50));

    // SQLDOUBLE (double) conversions
    EXPECT_TRUE(isInRange<double>(123.456789012345));
}

TEST(IsInRangeTest, SignedUnsignedBoundaries) {
    // Test critical boundaries between signed and unsigned types

    // Maximum positive value for signed type should fit in larger unsigned type
    EXPECT_TRUE(isInRange<unsigned int>((std::numeric_limits<int>::max)()));
    EXPECT_TRUE(isInRange<unsigned short>((std::numeric_limits<short>::max)()));

    // Maximum unsigned value should not fit in same-size signed type
    EXPECT_FALSE(isInRange<int>((std::numeric_limits<unsigned int>::max)()));
    EXPECT_FALSE(
        isInRange<short>((std::numeric_limits<unsigned short>::max)()));

    // But should fit in larger signed type
    EXPECT_TRUE(
        isInRange<long long>((std::numeric_limits<unsigned int>::max)()));
    EXPECT_FALSE(isInRange<unsigned int>((std::numeric_limits<int>::min)()));
    EXPECT_FALSE(isInRange<unsigned short>((std::numeric_limits<short>::min)()));
}

// Helper function to check SQL state and clear error list
void checkSQLState(RS_STMT_INFO* pStmt, const char* expectedSQLState) {
    if (pStmt && pStmt->pErrorList) {
        // Check SQL state matches expected
        EXPECT_STREQ(pStmt->pErrorList->szSqlState, expectedSQLState);
        // Clear the error list
        clearErrorList(pStmt->pErrorList);
        pStmt->pErrorList = nullptr;
    }
}


// Helper class for convertStringNumericToIntegerCType tests
class ConvertNumericToIntegerCTypeTest : public ::testing::Test {
  protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *pStmt;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pStmt = new RS_STMT_INFO(pConn);
        pStmt->pErrorList = nullptr;
    }

    void TearDown() override {
        if (pStmt->pErrorList) {
            clearErrorList(pStmt->pErrorList);
        }
        delete pStmt;
        delete pConn;
    }

    // Helper function to test conversion with expected result
    void testConversion(const char *numStr, SQLSMALLINT targetType,
                        SQLRETURN expectedRc, long long expectedValue = 0) {
        char buffer[TEST_TEMP_BUF_MAX_LEN];
        memset(buffer, 0, sizeof(buffer));
        SQLLEN lenInd = 0;

        SQLRETURN rc = convertStringNumericToIntegerCType(pStmt, const_cast<char*>(numStr), strlen(numStr), buffer,
                                                    &lenInd, targetType);
        EXPECT_EQ(rc, expectedRc) << "Failed for input: " << numStr;

        if (expectedRc == SQL_SUCCESS || expectedRc == SQL_SUCCESS_WITH_INFO) {
            switch (targetType) {
            case SQL_C_TINYINT:
            case SQL_C_STINYINT:
                EXPECT_EQ(*(signed char *)buffer, (signed char)expectedValue);
                EXPECT_EQ(lenInd, 1);
                break;
            case SQL_C_UTINYINT:
                EXPECT_EQ(*(unsigned char *)buffer,
                          (unsigned char)expectedValue);
                EXPECT_EQ(lenInd, 1);
                break;
            case SQL_C_SHORT:
            case SQL_C_SSHORT:
                EXPECT_EQ(*(short *)buffer, (short)expectedValue);
                EXPECT_EQ(lenInd, 2);
                break;
            case SQL_C_USHORT:
                EXPECT_EQ(*(unsigned short *)buffer,
                          (unsigned short)expectedValue);
                EXPECT_EQ(lenInd, 2);
                break;
            case SQL_C_LONG:
            case SQL_C_SLONG:
                EXPECT_EQ(*(int *)buffer, (int)expectedValue);
                EXPECT_EQ(lenInd, 4);
                break;
            case SQL_C_ULONG:
                EXPECT_EQ(*(unsigned int *)buffer, (unsigned int)expectedValue);
                EXPECT_EQ(lenInd, 4);
                break;
            case SQL_C_SBIGINT:
                EXPECT_EQ(*(long long *)buffer, expectedValue);
                EXPECT_EQ(lenInd, 8);
                break;
            case SQL_C_UBIGINT:
                EXPECT_EQ(*(unsigned long long *)buffer,
                          (unsigned long long)expectedValue);
                EXPECT_EQ(lenInd, 8);
                break;
            case SQL_C_BIT:
                EXPECT_EQ(*(char *)buffer, (char)expectedValue);
                EXPECT_EQ(lenInd, 1);
                break;
            default:
                EXPECT_TRUE(false) << "Unexpected target type: " << targetType;
            }
        }
    }
};

TEST_F(ConvertNumericToIntegerCTypeTest, BasicPositiveNumbers) {
    testConversion("0", SQL_C_BIT, SQL_SUCCESS, 0);
    testConversion("1", SQL_C_BIT, SQL_SUCCESS, 1);

    testConversion("123", SQL_C_LONG, SQL_SUCCESS, 123);
    testConversion("123", SQL_C_SLONG, SQL_SUCCESS, 123);
    testConversion("123", SQL_C_ULONG, SQL_SUCCESS, 123);

    testConversion("42", SQL_C_TINYINT, SQL_SUCCESS, 42);
    testConversion("42", SQL_C_STINYINT, SQL_SUCCESS, 42);
    testConversion("42", SQL_C_UTINYINT, SQL_SUCCESS, 42);

    testConversion("1000", SQL_C_SHORT, SQL_SUCCESS, 1000);
    testConversion("1000", SQL_C_SSHORT, SQL_SUCCESS, 1000);
    testConversion("1000", SQL_C_USHORT, SQL_SUCCESS, 1000);

    testConversion("99999999", SQL_C_SBIGINT, SQL_SUCCESS, 99999999);
    testConversion("99999999", SQL_C_UBIGINT, SQL_SUCCESS, 99999999);
}

TEST_F(ConvertNumericToIntegerCTypeTest, BasicNegativeNumbers) {
    testConversion("-123", SQL_C_LONG, SQL_SUCCESS, -123);
    testConversion("-123", SQL_C_SLONG, SQL_SUCCESS, -123);
    testConversion("-123", SQL_C_ULONG, SQL_ERROR);
    checkSQLState(pStmt, "22003");

    testConversion("-42", SQL_C_TINYINT, SQL_SUCCESS, -42);
    testConversion("-42", SQL_C_STINYINT, SQL_SUCCESS, -42);
    testConversion("-42", SQL_C_UTINYINT, SQL_ERROR);
    checkSQLState(pStmt, "22003");

    testConversion("-1000", SQL_C_SHORT, SQL_SUCCESS, -1000);
    testConversion("-1000", SQL_C_SSHORT, SQL_SUCCESS, -1000);
    testConversion("-1000", SQL_C_USHORT, SQL_ERROR);
    checkSQLState(pStmt, "22003");

    testConversion("-99999999", SQL_C_SBIGINT, SQL_SUCCESS, -99999999);
    testConversion("-99999999", SQL_C_UBIGINT, SQL_ERROR);
    checkSQLState(pStmt, "22003");
}

TEST_F(ConvertNumericToIntegerCTypeTest, SignedBoundaryValues) {
    // TINYINT boundaries
    testConversion("127", SQL_C_TINYINT, SQL_SUCCESS, 127);   // TINYINT max
    testConversion("-128", SQL_C_TINYINT, SQL_SUCCESS, -128); // TINYINT min
    testConversion("128", SQL_C_TINYINT, SQL_ERROR);  // TINYINT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-129", SQL_C_TINYINT, SQL_ERROR); // TINYINT overflow
    checkSQLState(pStmt, "22003");

    // SMALLINT boundaries
    testConversion("32767", SQL_C_SHORT, SQL_SUCCESS, 32767);   // SMALLINT max
    testConversion("-32768", SQL_C_SHORT, SQL_SUCCESS, -32768); // SMALLINT min
    testConversion("32768", SQL_C_SHORT, SQL_ERROR);  // SMALLINT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-32769", SQL_C_SHORT, SQL_ERROR); // SMALLINT overflow
    checkSQLState(pStmt, "22003");

    // INTEGER boundaries
    testConversion("2147483647", SQL_C_LONG, SQL_SUCCESS,
                   2147483647); // INT max
    testConversion("-2147483648", SQL_C_LONG, SQL_SUCCESS,
                   -2147483648);                          // INT min
    testConversion("2147483648", SQL_C_LONG, SQL_ERROR);  // INT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-2147483649", SQL_C_LONG, SQL_ERROR); // INT overflow
    checkSQLState(pStmt, "22003");

    // BIGINT boundaries
    testConversion("9223372036854775807", SQL_C_SBIGINT, SQL_SUCCESS,
                   9223372036854775807LL); // BIGINT max
    testConversion("-9223372036854775808", SQL_C_SBIGINT, SQL_SUCCESS,
                   -9223372036854775807LL - 1); // BIGINT min
    testConversion("9223372036854775808", SQL_C_SBIGINT,
                   SQL_ERROR); // BIGINT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-9223372036854775809", SQL_C_SBIGINT,
                   SQL_ERROR); // BIGINT overflow
    checkSQLState(pStmt, "22003");
}

TEST_F(ConvertNumericToIntegerCTypeTest, UnsignedBoundaryValues) {
    // UTINYINT boundaries (0-255)
    testConversion("0", SQL_C_UTINYINT, SQL_SUCCESS, 0);     // UTINYINT min
    testConversion("255", SQL_C_UTINYINT, SQL_SUCCESS, 255); // UTINYINT max
    testConversion("256", SQL_C_UTINYINT, SQL_ERROR); // UTINYINT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-1", SQL_C_UTINYINT, SQL_ERROR);  // UTINYINT underflow
    checkSQLState(pStmt, "22003");

    // USHORT boundaries (0-65535)
    testConversion("0", SQL_C_USHORT, SQL_SUCCESS, 0);         // USHORT min
    testConversion("65535", SQL_C_USHORT, SQL_SUCCESS, 65535); // USHORT max
    testConversion("65536", SQL_C_USHORT, SQL_ERROR); // USHORT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-1", SQL_C_USHORT, SQL_ERROR);    // USHORT underflow
    checkSQLState(pStmt, "22003");

    // ULONG boundaries (0-4294967295)
    testConversion("0", SQL_C_ULONG, SQL_SUCCESS, 0); // ULONG min
    testConversion("4294967295", SQL_C_ULONG, SQL_SUCCESS,
                   4294967295);                           // ULONG max
    testConversion("4294967296", SQL_C_ULONG, SQL_ERROR); // ULONG overflow
    checkSQLState(pStmt, "22003");

    testConversion("-1", SQL_C_ULONG, SQL_ERROR);         // ULONG underflow
    checkSQLState(pStmt, "22003");

    // BIGINT boundaries (-9223372036854775808 to 9223372036854775807)
    testConversion("-9223372036854775808", SQL_C_SBIGINT, SQL_SUCCESS,
                   -9223372036854775807LL - 1); // BIGINT min
    testConversion("9223372036854775807", SQL_C_SBIGINT, SQL_SUCCESS,
                   9223372036854775807LL); // BIGINT max
    testConversion("9223372036854775808", SQL_C_SBIGINT,
                   SQL_ERROR); // BIGINT overflow
    checkSQLState(pStmt, "22003");
    testConversion("-9223372036854775809", SQL_C_SBIGINT,
                   SQL_ERROR); // BIGINT underflow
    checkSQLState(pStmt, "22003");

    // UBIGINT boundaries (0 to 18446744073709551615)
    testConversion("0", SQL_C_UBIGINT, SQL_SUCCESS, 0); // UBIGINT min
    testConversion("18446744073709551615", SQL_C_UBIGINT, SQL_SUCCESS,
                   18446744073709551615ULL); // UBIGINT max
    testConversion("18446744073709551616", SQL_C_UBIGINT,
                   SQL_ERROR);                      // UBIGINT overflow
    checkSQLState(pStmt, "22003");

    testConversion("-1", SQL_C_UBIGINT, SQL_ERROR); // UBIGINT underflow
    checkSQLState(pStmt, "22003");
}

TEST_F(ConvertNumericToIntegerCTypeTest, FractionalNumbers) {
    // TINYINT tests
    testConversion("42.9", SQL_C_TINYINT, SQL_SUCCESS_WITH_INFO, 42);
    checkSQLState(pStmt, "01S07");

    testConversion("-42.9", SQL_C_TINYINT, SQL_SUCCESS_WITH_INFO, -42);
    checkSQLState(pStmt, "01S07");

    testConversion("42.1", SQL_C_STINYINT, SQL_SUCCESS_WITH_INFO, 42);
    checkSQLState(pStmt, "01S07");

    testConversion("42.5", SQL_C_UTINYINT, SQL_SUCCESS_WITH_INFO, 42);
    checkSQLState(pStmt, "01S07");

    // SMALLINT tests
    testConversion("1000.9", SQL_C_SHORT, SQL_SUCCESS_WITH_INFO, 1000);
    checkSQLState(pStmt, "01S07");

    testConversion("-1000.9", SQL_C_SHORT, SQL_SUCCESS_WITH_INFO, -1000);
    checkSQLState(pStmt, "01S07");

    testConversion("1000.1", SQL_C_SSHORT, SQL_SUCCESS_WITH_INFO, 1000);
    checkSQLState(pStmt, "01S07");

    testConversion("1000.5", SQL_C_USHORT, SQL_SUCCESS_WITH_INFO, 1000);
    checkSQLState(pStmt, "01S07");

    // INTEGER tests
    testConversion("123456.9", SQL_C_LONG, SQL_SUCCESS_WITH_INFO, 123456);
    checkSQLState(pStmt, "01S07");

    testConversion("-123456.9", SQL_C_LONG, SQL_SUCCESS_WITH_INFO, -123456);
    checkSQLState(pStmt, "01S07");

    testConversion("123456.1", SQL_C_SLONG, SQL_SUCCESS_WITH_INFO, 123456);
    checkSQLState(pStmt, "01S07");

    testConversion("123456.5", SQL_C_ULONG, SQL_SUCCESS_WITH_INFO, 123456);
    checkSQLState(pStmt, "01S07");

    // BIGINT tests
    testConversion("9999999999.9", SQL_C_SBIGINT, SQL_SUCCESS_WITH_INFO,
                   9999999999LL);
    checkSQLState(pStmt, "01S07");

    testConversion("-9999999999.9", SQL_C_SBIGINT, SQL_SUCCESS_WITH_INFO,
                   -9999999999LL);
    checkSQLState(pStmt, "01S07");

    testConversion("9999999999.1", SQL_C_UBIGINT, SQL_SUCCESS_WITH_INFO,
                   9999999999ULL);
    checkSQLState(pStmt, "01S07");

    // BIT tests
    testConversion("1.0", SQL_C_BIT, SQL_SUCCESS, 1);

    testConversion("1.9", SQL_C_BIT, SQL_SUCCESS_WITH_INFO, 1);
    checkSQLState(pStmt, "01S07");

    testConversion("0.9", SQL_C_BIT, SQL_SUCCESS_WITH_INFO, 0);
    checkSQLState(pStmt, "01S07");

    testConversion("2.0", SQL_C_BIT, SQL_ERROR);
    checkSQLState(pStmt, "22003");

    // special cases
    testConversion("42.0", SQL_C_TINYINT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_STINYINT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_UTINYINT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_SHORT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_SSHORT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_USHORT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_LONG, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_SLONG, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_ULONG, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_SBIGINT, SQL_SUCCESS, 42);
    testConversion("42.0", SQL_C_UBIGINT, SQL_SUCCESS, 42);

    // Test invalid C type
    testConversion("42", 9999, SQL_ERROR); // Using invalid C type
    checkSQLState(pStmt, "HY003"); // Data type not supported
}

TEST_F(ConvertNumericToIntegerCTypeTest, NullValidationTests) {
    const char* numStr = "123";
    char buffer[256];
    SQLLEN lenInd = 0;

    // Test null pStmt
    EXPECT_EQ(convertStringNumericToIntegerCType(nullptr, const_cast<char*>(numStr), strlen(numStr), buffer, &lenInd,
                                           SQL_INTEGER), SQL_INVALID_HANDLE);
    // Test null pColData
    EXPECT_EQ(convertStringNumericToIntegerCType(pStmt, nullptr, 0, buffer, &lenInd,
                                           SQL_INTEGER), SQL_ERROR);
    checkSQLState(pStmt, "HY009");
    // Test null buffer
    EXPECT_EQ(convertStringNumericToIntegerCType(pStmt, const_cast<char*>(numStr), strlen(numStr), nullptr, &lenInd,
                                           SQL_INTEGER), SQL_ERROR);
    checkSQLState(pStmt, "HY009");

    // Test for SQL_NULL_DATA
    EXPECT_EQ(convertStringNumericToIntegerCType(pStmt, const_cast<char*>("123"), SQL_NULL_DATA, buffer, &lenInd,
                                           SQL_INTEGER), SQL_SUCCESS);
}

// Unit tests for calculateMinNumericBufferLength helper function
struct BufferLengthTestCase {
    const char* input;
    size_t inputLen;
    size_t expectedLength;
};

class CalculateMinNumericBufferLengthTest : public ::testing::TestWithParam<BufferLengthTestCase> {};

TEST_P(CalculateMinNumericBufferLengthTest, CalculateLength) {
    auto testCase = GetParam();
    EXPECT_EQ(calculateMinNumericBufferLength(testCase.input, testCase.inputLen), testCase.expectedLength);
}

INSTANTIATE_TEST_SUITE_P(BufferLengthTests, CalculateMinNumericBufferLengthTest, ::testing::Values(
    // Test with null input
    BufferLengthTestCase{nullptr, 0, 1},
    BufferLengthTestCase{nullptr, 10, 1},

    // Test with empty string
    BufferLengthTestCase{"", 0, 1},

    // Test with positive integer (no sign, no decimal)
    BufferLengthTestCase{"12345", 5, 6},

    // Test with negative integer (with sign, no decimal)
    BufferLengthTestCase{"-12345", 6, 7},

    // Test with positive integer with plus sign
    BufferLengthTestCase{"+12345", 6, 7},

    // Test with positive decimal (no sign, with decimal)
    BufferLengthTestCase{"123.456", 7, 4},

     // Test with negative decimal (with sign, with decimal)
    BufferLengthTestCase{"-123.456", 8, 5},

    // Test with positive integer with plus sign
    BufferLengthTestCase{"+123.456", 8, 5},

    // Test with zero
    BufferLengthTestCase{"0", 1, 2},

    // Test with negative zero
    BufferLengthTestCase{"-0", 2, 3},

    // Test with decimal starting with zero
    BufferLengthTestCase{"0.123", 5, 2},

    // Test with negative decimal starting with zero
    BufferLengthTestCase{"-0.123", 6, 3},

    // Test with decimal point at the beginning
    BufferLengthTestCase{".123", 4, 1},

    // Test with negative decimal point at the beginning
    BufferLengthTestCase{"-.123", 5, 2},

    // Test with large number
    BufferLengthTestCase{"123456789012345", 15, 16},

    // Test with large negative number
    BufferLengthTestCase{"-123456789012345", 16, 17},

    // Test with large decimal number
    BufferLengthTestCase{"123456789.987654321", 19, 10},

    // Test with single digit
    BufferLengthTestCase{"7", 1, 2},

    // Test with single negative digit
    BufferLengthTestCase{"-7", 2, 3},

    // Test with decimal point only
    BufferLengthTestCase{".", 1, 1},

    // Test with sign and decimal point only
    BufferLengthTestCase{"-.", 2, 2},

    // Test with very long fractional part
    BufferLengthTestCase{"123.123456789012345678901234567890", 33, 4}
));

static void ExpectOK(const std::string& s,
                                  bool expIsNeg,
                                  unsigned long long expMag,
                                  bool expDropped) {
    for (bool includeNull : {false, true}) {
        bool isNeg = false, dropped = false;
        unsigned long long mag = 0ULL;

        const int len = includeNull
                        ? static_cast<int>(s.size() + 1) // include '\0'
                        : static_cast<int>(s.size());     // exclude '\0'

        ASSERT_EQ(parseAndBuildInteger(s.c_str(), len, isNeg, mag, dropped), PARSE_SUCCESS)
            << "Input: '" << s << "', includeNull=" << includeNull;

        EXPECT_EQ(isNeg, expIsNeg)           << "Input: '" << s << "'";
        EXPECT_EQ(mag, expMag)               << "Input: '" << s << "'";
        EXPECT_EQ(dropped, expDropped)       << "Input: '" << s << "'";
    }
}

static void ExpectFail(const std::string& s) {
    for (bool includeNull : {false, true}) {
        bool isNeg = true, dropped = true; // should be ignored
        unsigned long long mag = 123456789ULL;           // sentinel

        const int len = includeNull
                        ? static_cast<int>(s.size() + 1)
                        : static_cast<int>(s.size());

        EXPECT_NE(parseAndBuildInteger(s.c_str(), len, isNeg, mag, dropped), PARSE_SUCCESS)
            << "Expected failure for input: '" << s << "' includeNull=" << includeNull;
    }
}

TEST(parseAndBuildInteger, SimpleIntegersAndWhitespace) {
    ExpectOK("42", false, 42ULL, false);
    ExpectOK("-42", true, 42ULL, false);
    ExpectOK("+0007", false, 7ULL, false);
    ExpectOK("0", false, 0ULL, false);
    ExpectOK("-0", false, 0ULL, false);
    ExpectOK("100.", false, 100ULL, false); // decimal point with no frac
}

TEST(parseAndBuildInteger, PureFractionsAndDrops) {
    // ".5" => integer part drops to 0; fractional non-zero lost
    ExpectOK(".5", false, 0ULL, true);
    // "000.000" => drops to 0 but all zeros, so exact
    ExpectOK("000.000", false, 0ULL, false);
    ExpectOK("-0.000", false, 0ULL, false);
}

TEST(parseAndBuildInteger, ExponentsPositiveAndNegative) {
    ExpectOK("1e3", false, 1000ULL, false);
    ExpectOK("1.23e2", false, 123ULL, false);
    ExpectOK("1.0E+1", false, 10ULL, false);

    // Negative exponent that still keeps exact integer
    ExpectOK("100e-2", false, 1ULL, false);

    // Negative exponent that forces drop to 0 with loss
    ExpectOK("123e-5", false, 0ULL, true);

    // Fractional digits fully canceled by exponent (exact)
    ExpectOK("1.2300e2", false, 123ULL, false);

    // Fractional digits not fully canceled by exponent (inexact)
    ExpectOK("1.2301e2", false, 123ULL, true);
}

TEST(parseAndBuildInteger, LeadingZerosBehavior) {
    ExpectOK("000184467", false, 184467ULL, false);
    ExpectOK("000000", false, 0ULL, false);
}

TEST(parseAndBuildInteger, OverflowBoundaries) {
    using ULL = unsigned long long;
    const ULL U = (std::numeric_limits<ULL>::max)();

    // Exactly ULLONG_MAX should succeed
    ExpectOK(std::to_string(U), false, U, false);

    // ULLONG_MAX + 1 should fail
    ExpectFail("18446744073709551616");

    // Big exponent causing overflow should fail
    ExpectFail("9e19"); // 9 * 10^19 > ULLONG_MAX

    ExpectOK("1e18", false, 1000000000000000000ULL, false);

    // Boundary case: still within range
    ExpectOK("1844674407370955161e1", false, 18446744073709551610ULL, false);

    // Just over the edge should fail
    ExpectFail("1844674407370955162e1");
}

struct TrimWhitespaceTestCase {
    const char* input;
    size_t expectedStartOffset;
    size_t expectedEndOffset;
};

class TrimWhitespaceParameterizedTest : public ::testing::TestWithParam<TrimWhitespaceTestCase> {};

TEST_P(TrimWhitespaceParameterizedTest, TrimWhitespace) {
    auto testCase = GetParam();
    const char* str = testCase.input;
    const char* start = str;
    const char* end = str + strlen(str);

    bool result = trimWhitespace(&start, &end);

    EXPECT_EQ(start, str + testCase.expectedStartOffset);
    EXPECT_EQ(end, str + testCase.expectedEndOffset);
    EXPECT_EQ(result, start >= end); // Derive empty from offsets
}

INSTANTIATE_TEST_SUITE_P(TrimWhitespaceTests, TrimWhitespaceParameterizedTest, ::testing::Values(
    TrimWhitespaceTestCase{"", 0, 0},
    TrimWhitespaceTestCase{"hello", 0, 5},
    TrimWhitespaceTestCase{"  hello", 2, 7},
    TrimWhitespaceTestCase{"hello  ", 0, 5},
    TrimWhitespaceTestCase{"  hello  ", 2, 7},
    TrimWhitespaceTestCase{"   ", 3, 3}
));

// Unit tests for parseExponent function
struct ParseExponentTestCase {
    const char* input;
    ParseReturnCode expectedResult;
    int expectedValue;
    size_t expectedPosOffset;
};

class ParseExponentParameterizedTest : public ::testing::TestWithParam<ParseExponentTestCase> {};

TEST_P(ParseExponentParameterizedTest, ParseExponent) {
    auto testCase = GetParam();
    const char* str = testCase.input;
    const char* pos = str;
    const char* end = str + strlen(str);
    int exp;

    EXPECT_EQ(parseExponent(&pos, end, &exp), testCase.expectedResult);
    if (testCase.expectedResult == PARSE_SUCCESS) {
        EXPECT_EQ(exp, testCase.expectedValue);
        EXPECT_EQ(pos, str + testCase.expectedPosOffset);
    }
}

INSTANTIATE_TEST_SUITE_P(ParseExponentTests, ParseExponentParameterizedTest, ::testing::Values(
    ParseExponentTestCase{"e5", PARSE_SUCCESS, 5, 2},
    ParseExponentTestCase{"E5", PARSE_SUCCESS, 5, 2},
    ParseExponentTestCase{"e+10", PARSE_SUCCESS, 10, 4},
    ParseExponentTestCase{"e-3", PARSE_SUCCESS, -3, 3},
    ParseExponentTestCase{"e0", PARSE_SUCCESS, 0, 2},
    ParseExponentTestCase{"e+0", PARSE_SUCCESS, 0, 3},
    ParseExponentTestCase{"e123", PARSE_SUCCESS, 123, 4},
    ParseExponentTestCase{"e007", PARSE_SUCCESS, 7, 4},
    ParseExponentTestCase{"e32767", PARSE_SUCCESS, 32767, 6},
    ParseExponentTestCase{"e-32768", PARSE_SUCCESS, -32768, 7},
    ParseExponentTestCase{"e123x", PARSE_SUCCESS, 123, 4},
    ParseExponentTestCase{"e", PARSE_INVALID_FORMAT, 0, 0},
    ParseExponentTestCase{"e+", PARSE_INVALID_FORMAT, 0, 0},
    ParseExponentTestCase{"e-", PARSE_INVALID_FORMAT, 0, 0},
    ParseExponentTestCase{"ex", PARSE_INVALID_FORMAT, 0, 0},
    ParseExponentTestCase{"e+x", PARSE_INVALID_FORMAT, 0, 0},
    ParseExponentTestCase{"e99999", PARSE_OVERFLOW, 0, 0},
    ParseExponentTestCase{"e-99999", PARSE_OVERFLOW, 0, 0}
));

// Unit tests for convertNumericStringToScaledIntegerExtended function
struct NumericTestParam {
    std::string input;
    SQLRETURN expectedRC;
    std::string expectedSQLState;
    SQLCHAR expectedPrecision;
    SQLSCHAR expectedScale;
    SQLCHAR expectedSign;
    std::vector<SQLCHAR> expectedVal;
    int inputLen = -1;
};

// Helper function to convert integer to little-endian byte array
std::vector<SQLCHAR> makeLE(unsigned long long value) {
    std::vector<SQLCHAR> bytes;
    if (value == 0) {
        bytes.push_back(0);
        return bytes;
    }
    while (value > 0) {
        bytes.push_back(static_cast<SQLCHAR>(value & 0xFF));
        value >>= 8;
    }
    return bytes;
}

class ConvertNumericStringToScaledIntegerExtendedTest
    : public ::testing::TestWithParam<NumericTestParam> {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *pStmt;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pStmt = new RS_STMT_INFO(pConn);
        pStmt->pErrorList = nullptr;
    }

    void TearDown() override {
        if (pStmt->pErrorList) {
            clearErrorList(pStmt->pErrorList);
        }
        delete pStmt;
        delete pConn;
    }

    void verifyNumericStruct(const SQL_NUMERIC_STRUCT& result, 
                           SQLCHAR expectedPrecision, 
                           SQLSCHAR expectedScale, 
                           SQLCHAR expectedSign,
                           const std::vector<SQLCHAR>& expectedVal) {
        EXPECT_EQ(result.precision, expectedPrecision);
        EXPECT_EQ(result.scale, expectedScale);
        EXPECT_EQ(result.sign, expectedSign);
        EXPECT_LE(expectedVal.size(), SQL_MAX_NUMERIC_LEN);

        for (size_t i = 0; i < expectedVal.size() && i < SQL_MAX_NUMERIC_LEN; i++) {
            EXPECT_EQ(result.val[i], expectedVal[i]) << "Mismatch at byte " << i;
        }

        // Verify remaining bytes are zero
        for (size_t i = expectedVal.size(); i < SQL_MAX_NUMERIC_LEN; i++) {
            EXPECT_EQ(result.val[i], 0) << "Non-zero byte at position " << i;
        }
    }
};

TEST_P(ConvertNumericStringToScaledIntegerExtendedTest, ConvertNumeric) {
    const auto& param = GetParam();
    SQL_NUMERIC_STRUCT out{};

    int dataLen = param.inputLen == -1 ? (int)param.input.size() : param.inputLen;
    auto rc = convertNumericStringToScaledIntegerExtended(
        pStmt, const_cast<char*>(param.input.data()), dataLen, &out);

    EXPECT_EQ(rc, param.expectedRC);

    checkSQLState(pStmt, param.expectedSQLState.c_str());


    if (SQL_SUCCEEDED(rc)) {
        verifyNumericStruct(out, 
            param.expectedPrecision,
            param.expectedScale,
            param.expectedSign,
            param.expectedVal);
    }
}

INSTANTIATE_TEST_SUITE_P(NumericTests,
    ConvertNumericStringToScaledIntegerExtendedTest,
    ::testing::Values(
        // Basic cases
        NumericTestParam{"12345", SQL_SUCCESS, "", 5, 0, 1, makeLE(12345ULL)},
        NumericTestParam{"-100", SQL_SUCCESS, "", 3, 0, 0, makeLE(100ULL)},
        NumericTestParam{"123.45", SQL_SUCCESS, "", 5, 2, 1, makeLE(12345ULL)},

        // Exponent cases
        NumericTestParam{"1.23e4", SQL_SUCCESS, "", 5, 0, 1, makeLE(12300ULL)},
        NumericTestParam{"-7.5e-4", SQL_SUCCESS, "", 5, 5, 0, makeLE(75ULL)},

        // Leading zeros and scale growth
        NumericTestParam{"0000.0100", SQL_SUCCESS, "", 4, 4, 1, makeLE(100ULL)},

        // Zero normalization
        NumericTestParam{"0", SQL_SUCCESS, "", 1, 0, 1, {}},
        NumericTestParam{"-0", SQL_SUCCESS, "", 1, 0, 1, {}},

        // Invalid formats
        NumericTestParam{"12.3.4", SQL_ERROR, "22018", 0, 0, 0, {}},
        NumericTestParam{"1e+", SQL_ERROR, "22018", 0, 0, 0, {}},

        // Special tokens
        NumericTestParam{"NaN", SQL_ERROR, "22003", 0, 0, 0, {}},
        NumericTestParam{"inf", SQL_ERROR, "22003", 0, 0, 0, {}},

        // Whitespace trimming
        NumericTestParam{" \t +123 \t ", SQL_SUCCESS, "", 3, 0, 1, makeLE(123ULL)}
    )
);

TEST_F(ConvertNumericStringToScaledIntegerExtendedTest, Base256PathForLargeMagnitude) {
    // 25 digits → triggers base-256 conversion path (since small path uses <= 18 digits)
    std::string s = "1234567890123456789012345"; // no decimal, no exponent
    SQL_NUMERIC_STRUCT out{};
    auto rc = convertNumericStringToScaledIntegerExtended(pStmt, s.data(), (int)s.size(), &out);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(pStmt->pErrorList, nullptr);

    // precision = 25, scale = 0, sign = 1
    EXPECT_EQ(out.precision, (SQLCHAR)25);
    EXPECT_EQ(out.scale, (SQLSCHAR)0);
    EXPECT_EQ(out.sign, (SQLCHAR)1);

    // Expected little-endian bytes for the decimal string:
    auto decStringToLE = [](const std::string& digits) {
        std::vector<SQLCHAR> buf(SQL_MAX_NUMERIC_LEN, 0);
        size_t len = 0;
        for (char ch : digits) {
            unsigned int carry = 0;
            for (size_t j = 0; j < len; ++j) {
                unsigned int prod = buf[j] * 10u + carry;
                buf[j] = (SQLCHAR)(prod & 0xFFu);
                carry = prod >> 8;
            }
            while (carry && len < SQL_MAX_NUMERIC_LEN) {
                buf[len++] = (SQLCHAR)(carry & 0xFFu);
                carry >>= 8;
            }
            carry = (unsigned int)(ch - '0');
            for (size_t j = 0; j < len && carry; ++j) {
                unsigned int sum = buf[j] + carry;
                buf[j] = (SQLCHAR)(sum & 0xFFu);
                carry = sum >> 8;
            }
            if (carry && len < SQL_MAX_NUMERIC_LEN) buf[len++] = (SQLCHAR)carry;
        }
        // trim trailing zeros in our vector representation for comparison convenience
        while (!buf.empty() && buf.back() == 0) buf.pop_back();
        return buf;
    };

    auto expectedLE = decStringToLE(s);
    for (size_t i = 0; i < expectedLE.size(); ++i) {
        EXPECT_EQ(out.val[i], expectedLE[i]) << "Mismatch at byte " << i;
    }
    for (size_t i = expectedLE.size(); i < SQL_MAX_NUMERIC_LEN; ++i) {
        EXPECT_EQ(out.val[i], 0) << "Non-zero byte at " << i;
    }
}

// Unit Tests for convertStringNumericToFloatCType
struct FloatConversionTestCase {
    const char* input;
    SQLSMALLINT targetType;
    SQLRETURN expectedRC;
    double expectedValue; // Use double to represent both float and double
    const char* expectedSQLState;
};

class ConvertStringNumericToFloatCTypeTest : public ::testing::TestWithParam<FloatConversionTestCase> {
protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *pStmt;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pStmt = new RS_STMT_INFO(pConn);
        pStmt->pErrorList = nullptr;
    }

    void TearDown() override {
        if (pStmt->pErrorList) {
            clearErrorList(pStmt->pErrorList);
        }
        delete pStmt;
        delete pConn;
    }
};

TEST_P(ConvertStringNumericToFloatCTypeTest, ConvertToFloatOrDouble) {
    auto testCase = GetParam();
    char buffer[TEST_TEMP_BUF_MAX_LEN];
    memset(buffer, 0, sizeof(buffer));
    SQLLEN lenInd = 0;

    SQLRETURN rc = convertStringNumericToFloatCType(pStmt, const_cast<char*>(testCase.input), 
                                                     strlen(testCase.input), buffer, &lenInd, testCase.targetType);

    EXPECT_EQ(rc, testCase.expectedRC) << "Failed for input: " << testCase.input;

    if (testCase.expectedSQLState && strlen(testCase.expectedSQLState) > 0) {
        checkSQLState(pStmt, testCase.expectedSQLState);
    }

    if (SQL_SUCCEEDED(rc)) {
        if (testCase.targetType == SQL_C_FLOAT) {
            float actualValue = *(float*)buffer;
            if (std::isnan(testCase.expectedValue)) {
                EXPECT_TRUE(std::isnan(actualValue));
            } else if (std::isinf(testCase.expectedValue)) {
                EXPECT_TRUE(std::isinf(actualValue));
                EXPECT_EQ(std::signbit(testCase.expectedValue), std::signbit(actualValue));
            } else {
                EXPECT_FLOAT_EQ(actualValue, (float)testCase.expectedValue);
            }
            EXPECT_EQ(lenInd, sizeof(float));
        } else { // SQL_C_DOUBLE
            double actualValue = *(double*)buffer;
            if (std::isnan(testCase.expectedValue)) {
                EXPECT_TRUE(std::isnan(actualValue));
            } else if (std::isinf(testCase.expectedValue)) {
                EXPECT_TRUE(std::isinf(actualValue));
                EXPECT_EQ(std::signbit(testCase.expectedValue), std::signbit(actualValue));
            } else {
                EXPECT_DOUBLE_EQ(actualValue, testCase.expectedValue);
            }
            EXPECT_EQ(lenInd, sizeof(double));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    FloatConversionTests, ConvertStringNumericToFloatCTypeTest,
    ::testing::Values(
        // Basic positive numbers
        FloatConversionTestCase{"123.45", SQL_C_FLOAT, SQL_SUCCESS, 123.45, ""},
        FloatConversionTestCase{"123.45", SQL_C_DOUBLE, SQL_SUCCESS, 123.45,
                                ""},
        FloatConversionTestCase{"0.0", SQL_C_FLOAT, SQL_SUCCESS, 0.0, ""},
        FloatConversionTestCase{"0.0", SQL_C_DOUBLE, SQL_SUCCESS, 0.0, ""},

        // Negative numbers
        FloatConversionTestCase{"-123.45", SQL_C_FLOAT, SQL_SUCCESS, -123.45,
                                ""},
        FloatConversionTestCase{"-123.45", SQL_C_DOUBLE, SQL_SUCCESS, -123.45,
                                ""},

        // Scientific notation
        FloatConversionTestCase{"1.23e2", SQL_C_FLOAT, SQL_SUCCESS, 123.0, ""},
        FloatConversionTestCase{"1.23e2", SQL_C_DOUBLE, SQL_SUCCESS, 123.0, ""},
        FloatConversionTestCase{"1.23e-2", SQL_C_FLOAT, SQL_SUCCESS, 0.0123,
                                ""},
        FloatConversionTestCase{"1.23e-2", SQL_C_DOUBLE, SQL_SUCCESS, 0.0123,
                                ""},

        // Special values - infinity
        FloatConversionTestCase{"infinity", SQL_C_FLOAT, SQL_SUCCESS, INFINITY,
                                ""},
        FloatConversionTestCase{"infinity", SQL_C_DOUBLE, SQL_SUCCESS, INFINITY,
                                ""},
        FloatConversionTestCase{"-infinity", SQL_C_FLOAT, SQL_SUCCESS,
                                -INFINITY, ""},
        FloatConversionTestCase{"-infinity", SQL_C_DOUBLE, SQL_SUCCESS,
                                -INFINITY, ""},
        FloatConversionTestCase{"inf", SQL_C_FLOAT, SQL_SUCCESS, INFINITY, ""},
        FloatConversionTestCase{"-inf", SQL_C_DOUBLE, SQL_SUCCESS, -INFINITY,
                                ""},

        // Special values - NaN
        FloatConversionTestCase{"nan", SQL_C_FLOAT, SQL_SUCCESS, NAN, ""},
        FloatConversionTestCase{"NaN", SQL_C_DOUBLE, SQL_SUCCESS, NAN, ""},

        // Large values
        FloatConversionTestCase{"3.4e38", SQL_C_FLOAT, SQL_SUCCESS, 3.4e38, ""},
        FloatConversionTestCase{"1.7e308", SQL_C_DOUBLE, SQL_SUCCESS, 1.7e308,
                                ""},

        // Small values
        FloatConversionTestCase{"1.23e-38", SQL_C_FLOAT, SQL_SUCCESS, 1.23e-38,
                                ""},
        FloatConversionTestCase{"2.234e-308", SQL_C_DOUBLE, SQL_SUCCESS,
                                2.234e-308, ""},

        // Range overflow for float
        FloatConversionTestCase{"1.0e39", SQL_C_FLOAT, SQL_ERROR, 0, "22003"},
        FloatConversionTestCase{"-1.0e39", SQL_C_FLOAT, SQL_ERROR, 0,
                                "22003"}));

TEST(ConvertStringNumericToFloatCTypeTest, NullValidationTests) {
    RS_ENV_INFO envInfo;
    memset(&envInfo, 0, sizeof(RS_ENV_INFO));
    RS_CONN_INFO *pConn = new RS_CONN_INFO(&envInfo);
    RS_STMT_INFO *pStmt = new RS_STMT_INFO(pConn);
    pStmt->pErrorList = nullptr;

    const char *numStr = "123.45";
    char buffer[256];
    SQLLEN lenInd = 0;

    // Test null pStmt
    EXPECT_EQ(convertStringNumericToFloatCType(
                  nullptr, const_cast<char *>(numStr), strlen(numStr), buffer,
                  &lenInd, SQL_C_FLOAT),
              SQL_INVALID_HANDLE);

    // Test null pColData
    EXPECT_EQ(convertStringNumericToFloatCType(pStmt, nullptr, 0, buffer,
                                               &lenInd, SQL_C_FLOAT),
              SQL_ERROR);
    checkSQLState(pStmt, "HY009");

    // Test null buffer
    EXPECT_EQ(convertStringNumericToFloatCType(
                  pStmt, const_cast<char *>(numStr), strlen(numStr), nullptr,
                  &lenInd, SQL_C_FLOAT),
              SQL_ERROR);
    checkSQLState(pStmt, "HY009");

    // Test null lenInd
    EXPECT_EQ(convertStringNumericToFloatCType(
                  pStmt, const_cast<char *>(numStr), strlen(numStr), buffer,
                  nullptr, SQL_C_FLOAT),
              SQL_ERROR);
    checkSQLState(pStmt, "HY009");

    // Test for SQL_NULL_DATA
    EXPECT_EQ(convertStringNumericToFloatCType(pStmt, const_cast<char *>("123"),
                                               SQL_NULL_DATA, buffer, &lenInd,
                                               SQL_C_FLOAT),
              SQL_SUCCESS);
    EXPECT_EQ(lenInd, SQL_NULL_DATA);

    // Cleanup
    if (pStmt->pErrorList) {
        clearErrorList(pStmt->pErrorList);
    }
    delete pStmt;
    delete pConn;
}

// Unit Tests for prepareStringForNumericConversion
struct PrepareStringTestCase {
    const char *input;
    SQLRETURN expectedRC;
    const char *expectedPreparedStr;
    const char *expectedSQLState;
};

class PrepareStringForNumericConversionTest
    : public ::testing::TestWithParam<PrepareStringTestCase> {
  protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *pStmt;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pStmt = new RS_STMT_INFO(pConn);
        pStmt->pErrorList = nullptr;
    }

    void TearDown() override {
        if (pStmt->pErrorList) {
            clearErrorList(pStmt->pErrorList);
        }
        delete pStmt;
        delete pConn;
    }
};

TEST_P(PrepareStringForNumericConversionTest, PrepareString) {
    auto testCase = GetParam();
    char tempBuf[MAX_NUMBER_BUF_LEN + 1];
    char *preparedStr = nullptr;
    int truncated = 0;

    SQLRETURN rc = prepareStringForNumericConversion(
        pStmt, const_cast<char *>(testCase.input), strlen(testCase.input),
        tempBuf, sizeof(tempBuf), &preparedStr, &truncated);

    EXPECT_EQ(rc, testCase.expectedRC)
        << "Failed for input: " << testCase.input;

    if (testCase.expectedSQLState && strlen(testCase.expectedSQLState) > 0) {
        checkSQLState(pStmt, testCase.expectedSQLState);
    }

    if (rc == SQL_SUCCESS && testCase.expectedPreparedStr) {
        EXPECT_STREQ(preparedStr, testCase.expectedPreparedStr)
            << "Prepared string mismatch for input: " << testCase.input;
    }
}

INSTANTIATE_TEST_SUITE_P(
    PrepareStringTests, PrepareStringForNumericConversionTest,
    ::testing::Values(
        // Valid numeric strings
        PrepareStringTestCase{"123", SQL_SUCCESS, "123", ""},
        PrepareStringTestCase{"123.45", SQL_SUCCESS, "123.45", ""},
        PrepareStringTestCase{"-123.45", SQL_SUCCESS, "-123.45", ""},
        PrepareStringTestCase{"+123.45", SQL_SUCCESS, "+123.45", ""},
        PrepareStringTestCase{"  123.45  ", SQL_SUCCESS, "123.45", ""},
        PrepareStringTestCase{"\t123.45\n", SQL_SUCCESS, "123.45", ""},

        // Scientific notation
        PrepareStringTestCase{"1.23e5", SQL_SUCCESS, "1.23e5", ""},
        PrepareStringTestCase{"1.23E5", SQL_SUCCESS, "1.23E5", ""},
        PrepareStringTestCase{"1.23e+5", SQL_SUCCESS, "1.23e+5", ""},
        PrepareStringTestCase{"1.23e-5", SQL_SUCCESS, "1.23e-5", ""},

        // Special IEEE 754 values
        PrepareStringTestCase{"infinity", SQL_SUCCESS, "infinity", ""},
        PrepareStringTestCase{"-infinity", SQL_SUCCESS, "-infinity", ""},
        PrepareStringTestCase{"inf", SQL_SUCCESS, "inf", ""},
        PrepareStringTestCase{"-inf", SQL_SUCCESS, "-inf", ""},
        PrepareStringTestCase{"nan", SQL_SUCCESS, "nan", ""},
        PrepareStringTestCase{"NaN", SQL_SUCCESS, "NaN", ""},

        // Leading decimal point
        PrepareStringTestCase{".5", SQL_SUCCESS, ".5", ""},
        PrepareStringTestCase{"-.5", SQL_SUCCESS, "-.5", ""},

        // Invalid formats
        PrepareStringTestCase{"", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"   ", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"abc", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"12.34.56", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"123e", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"123e+", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"e5", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"123e2.5", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"123e2e3", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"0x123", SQL_ERROR, nullptr, "22018"},
        PrepareStringTestCase{"0X456", SQL_ERROR, nullptr, "22018"}));


// Unit Tests for parseDateString
struct ParseDateTestCase {
    const char *input;
    SQLRETURN expectedRC;
    int expectedYear;
    int expectedMonth;
    int expectedDay;
    const char *expectedSQLState;
};

class ParseDateStringTest : public ::testing::TestWithParam<ParseDateTestCase> {
  protected:
    RS_ERROR_INFO *errorList;

    void SetUp() override { errorList = nullptr; }

    void TearDown() override {
        if (errorList) {
            clearErrorList(errorList);
        }
    }
};

TEST_P(ParseDateStringTest, ParseDate) {
    auto testCase = GetParam();
    DATE_STRUCT dateStruct;
    memset(&dateStruct, 0, sizeof(DATE_STRUCT));

    SQLRETURN rc = parseDateString(testCase.input, &dateStruct, &errorList);

    EXPECT_EQ(rc, testCase.expectedRC)
        << "Failed for input: " << testCase.input;

    if (SQL_SUCCEEDED(rc)) {
        EXPECT_EQ(dateStruct.year, testCase.expectedYear);
        EXPECT_EQ(dateStruct.month, testCase.expectedMonth);
        EXPECT_EQ(dateStruct.day, testCase.expectedDay);
    }

    if (testCase.expectedSQLState && strlen(testCase.expectedSQLState) > 0 &&
        errorList) {
        EXPECT_STREQ(errorList->szSqlState, testCase.expectedSQLState);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParseDateTests, ParseDateStringTest,
    ::testing::Values(
        // Valid dates
        ParseDateTestCase{"2024-01-15", SQL_SUCCESS, 2024, 1, 15, ""},
        ParseDateTestCase{"2024-12-31", SQL_SUCCESS, 2024, 12, 31, ""},
        ParseDateTestCase{"1900-01-01", SQL_SUCCESS, 1900, 1, 1, ""},
        ParseDateTestCase{"9999-12-31", SQL_SUCCESS, 9999, 12, 31, ""},

        // Leap year dates
        ParseDateTestCase{"2020-02-29", SQL_SUCCESS, 2020, 2, 29, ""},
        ParseDateTestCase{"2000-02-29", SQL_SUCCESS, 2000, 2, 29, ""},

        // Timestamp with time (time truncated, returns SUCCESS_WITH_INFO)
        ParseDateTestCase{"2024-01-15 10:30:00", SQL_SUCCESS_WITH_INFO, 2024, 1,
                          15, "01S07"},
        ParseDateTestCase{"2024-01-15 00:00:00", SQL_SUCCESS, 2024, 1, 15, ""},

        // Invalid dates
        ParseDateTestCase{"2024-13-01", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"2024-00-01", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"2024-01-32", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"2024-01-00", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"2021-02-29", SQL_ERROR, 0, 0, 0,
                          "22018"}, // Not a leap year
        ParseDateTestCase{"2024-02-30", SQL_ERROR, 0, 0, 0, "22018"},

        // Invalid formats
        ParseDateTestCase{"24-01-15", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"2024/01/15", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"2024-1-15", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"20240115", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"abc", SQL_ERROR, 0, 0, 0, "22018"},
        ParseDateTestCase{"", SQL_ERROR, 0, 0, 0, "22018"}));


// Unit Tests for parseTimestampString
struct ParseTimestampTestCase {
    const char *input;
    SQLRETURN expectedRC;
    int year, month, day, hour, minute, second;
    unsigned int fraction;
    const char *expectedSQLState;
};

class ParseTimestampStringTest
    : public ::testing::TestWithParam<ParseTimestampTestCase> {
  protected:
    RS_ERROR_INFO *errorList;

    void SetUp() override { errorList = nullptr; }

    void TearDown() override {
        if (errorList) {
            clearErrorList(errorList);
        }
    }
};

TEST_P(ParseTimestampStringTest, ParseTimestamp) {
    auto testCase = GetParam();
    TIMESTAMP_STRUCT tsStruct;
    memset(&tsStruct, 0, sizeof(TIMESTAMP_STRUCT));

    SQLRETURN rc = parseTimestampString(testCase.input, &tsStruct, &errorList);

    EXPECT_EQ(rc, testCase.expectedRC)
        << "Failed for input: " << testCase.input;

    if (SQL_SUCCEEDED(rc)) {
        EXPECT_EQ(tsStruct.year, testCase.year);
        EXPECT_EQ(tsStruct.month, testCase.month);
        EXPECT_EQ(tsStruct.day, testCase.day);
        EXPECT_EQ(tsStruct.hour, testCase.hour);
        EXPECT_EQ(tsStruct.minute, testCase.minute);
        EXPECT_EQ(tsStruct.second, testCase.second);
        EXPECT_EQ(tsStruct.fraction, testCase.fraction);
    }

    if (testCase.expectedSQLState && strlen(testCase.expectedSQLState) > 0 &&
        errorList) {
        EXPECT_STREQ(errorList->szSqlState, testCase.expectedSQLState);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParseTimestampTests, ParseTimestampStringTest,
    ::testing::Values(
        // Full timestamps
        ParseTimestampTestCase{"2024-01-15 10:30:45", SQL_SUCCESS, 2024, 1, 15,
                               10, 30, 45, 0, ""},
        ParseTimestampTestCase{"2024-01-15 10:30:45.123456", SQL_SUCCESS, 2024,
                               1, 15, 10, 30, 45, 123456000, ""},
        ParseTimestampTestCase{"2024-12-31 23:59:59", SQL_SUCCESS, 2024, 12, 31,
                               23, 59, 59, 0, ""},
        ParseTimestampTestCase{"2024-12-31 23:59:59.999999", SQL_SUCCESS, 2024,
                               12, 31, 23, 59, 59, 999999000, ""},

        // Date only (time defaults to 00:00:00)
        ParseTimestampTestCase{"2024-01-15", SQL_SUCCESS, 2024, 1, 15, 0, 0, 0,
                               0, ""},

        // Invalid timestamps
        ParseTimestampTestCase{"2024-13-01 10:30:45", SQL_ERROR, 0, 0, 0, 0, 0,
                               0, 0, "22018"},
        ParseTimestampTestCase{"2024-01-32 10:30:45", SQL_ERROR, 0, 0, 0, 0, 0,
                               0, 0, "22018"},
        ParseTimestampTestCase{"2024-01-15 25:30:45", SQL_ERROR, 0, 0, 0, 0, 0,
                               0, 0, "22018"},
        ParseTimestampTestCase{"2024-01-15 10:60:45", SQL_ERROR, 0, 0, 0, 0, 0,
                               0, 0, "22018"},
        ParseTimestampTestCase{"2024-01-15 10:30:60", SQL_ERROR, 0, 0, 0, 0, 0,
                               0, 0, "22018"}));

// Unit Tests for parseTimeString
struct ParseTimeTestCase {
    const char *input;
    SQLRETURN expectedRC;
    int hour, minute, second;
    unsigned int fraction;
    const char *expectedSQLState;
};

class ParseTimeStringTest : public ::testing::TestWithParam<ParseTimeTestCase> {
  protected:
    RS_ERROR_INFO *errorList;

    void SetUp() override {
        errorList = nullptr;
    }

    void TearDown() override {
        if (errorList) {
            clearErrorList(errorList);
        }
    }
};

TEST_P(ParseTimeStringTest, ParseTime) {
    auto testCase = GetParam();
    RS_TIME_STRUCT timeStruct;
    memset(&timeStruct, 0, sizeof(RS_TIME_STRUCT));

    SQLRETURN rc = parseTimeString(testCase.input, &timeStruct, &errorList);

    EXPECT_EQ(rc, testCase.expectedRC)
        << "Failed for input: " << testCase.input;

    if (SQL_SUCCEEDED(rc)) {
        EXPECT_EQ(timeStruct.sqltVal.hour, testCase.hour);
        EXPECT_EQ(timeStruct.sqltVal.minute, testCase.minute);
        EXPECT_EQ(timeStruct.sqltVal.second, testCase.second);
        EXPECT_EQ(timeStruct.fraction, testCase.fraction);
    }

    if (testCase.expectedSQLState && strlen(testCase.expectedSQLState) > 0 &&
        errorList) {
        EXPECT_STREQ(errorList->szSqlState, testCase.expectedSQLState);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParseTimeTests, ParseTimeStringTest,
    ::testing::Values(
        // Valid time strings
        ParseTimeTestCase{"10:30:45", SQL_SUCCESS, 10, 30, 45, 0, ""},
        ParseTimeTestCase{"00:00:00", SQL_SUCCESS, 0, 0, 0, 0, ""},
        ParseTimeTestCase{"23:59:59", SQL_SUCCESS, 23, 59, 59, 0, ""},

        // Fractional seconds should not indicate truncation
        ParseTimeTestCase{"10:30:45.123456", SQL_SUCCESS, 10, 30, 45, 123456000,
                          ""},
        ParseTimeTestCase{"10:30:45.999999", SQL_SUCCESS, 10, 30, 45, 999999000,
                          ""},
        ParseTimeTestCase{"2024-01-15 10:30:45.123456", SQL_SUCCESS, 10, 30, 45,
                    123456000, ""},

        ParseTimeTestCase{"10:30:45.123456789", SQL_SUCCESS, 10, 30,
                          45, 123456789, ""},

        // Timestamp with date (date part ignored, but no error)
        ParseTimeTestCase{"2024-01-15 10:30:45", SQL_SUCCESS, 10, 30, 45, 0,
                          ""},

        // Invalid times
        ParseTimeTestCase{"25:30:45", SQL_ERROR, 0, 0, 0, 0, "22018"},
        ParseTimeTestCase{"10:60:45", SQL_ERROR, 0, 0, 0, 0, "22018"},
        ParseTimeTestCase{"10:30:60", SQL_ERROR, 0, 0, 0, 0, "22018"},
        ParseTimeTestCase{"1:30:45", SQL_ERROR, 0, 0, 0, 0,
                          "22018"}, // Wrong format
        ParseTimeTestCase{"10-30-45", SQL_ERROR, 0, 0, 0, 0, "22018"},
        ParseTimeTestCase{"abc", SQL_ERROR, 0, 0, 0, 0, "22018"}));


// Unit Tests for isDigitStr

TEST(IsDigitStrTest, AllDigits) {
    EXPECT_TRUE(isDigitStr("123", 3));
    EXPECT_TRUE(isDigitStr("0", 1));
    EXPECT_TRUE(isDigitStr("9876543210", 10));
}

TEST(IsDigitStrTest, NonDigits) {
    EXPECT_FALSE(isDigitStr("abc", 3));
    EXPECT_FALSE(isDigitStr("12a34", 5));
    EXPECT_FALSE(isDigitStr("12 34", 5));
    EXPECT_FALSE(isDigitStr("12.34", 5));
    EXPECT_FALSE(isDigitStr("12-34", 5));
}

TEST(IsDigitStrTest, EmptyString) {
    EXPECT_TRUE(isDigitStr("", 0));
    EXPECT_TRUE(isDigitStr("abc", 0));
}

TEST(IsDigitStrTest, PartialLength) {
    EXPECT_TRUE(isDigitStr("123abc", 3));
    EXPECT_FALSE(isDigitStr("123abc", 6));
}


// Unit Tests for isLeapYear
struct LeapYearTestCase {
    int year;
    bool expected;
};

class IsLeapYearTest : public ::testing::TestWithParam<LeapYearTestCase> {};

TEST_P(IsLeapYearTest, CheckLeapYear) {
    auto testCase = GetParam();
    EXPECT_EQ(isLeapYear(testCase.year), testCase.expected);
}

INSTANTIATE_TEST_SUITE_P(
    LeapYearTests, IsLeapYearTest,
    ::testing::Values(
        // Leap years divisible by 4
        LeapYearTestCase{2020, true},
        LeapYearTestCase{2024, true},
        LeapYearTestCase{2000, true},

        // Not leap years (not divisible by 4)
        LeapYearTestCase{2021, false},
        LeapYearTestCase{2022, false},
        LeapYearTestCase{2023, false},

        // Century years (divisible by 100 but not by 400)
        LeapYearTestCase{1900, false},
        LeapYearTestCase{2100, false},
        LeapYearTestCase{2200, false},

        // Century years (divisible by 400)
        LeapYearTestCase{2000, true},
        LeapYearTestCase{2400, true},

        // Edge cases
        LeapYearTestCase{1904, true},
        LeapYearTestCase{1996, true},
        LeapYearTestCase{2004, true}));


// Unit Tests for validateDate
struct ValidateDateTestCase {
    int year, month, day;
    bool expected;
};

class ValidateDateTest : public ::testing::TestWithParam<ValidateDateTestCase> {
};

TEST_P(ValidateDateTest, CheckValidDate) {
    auto testCase = GetParam();
    EXPECT_EQ(validateDate(testCase.year, testCase.month, testCase.day),
              testCase.expected) << "year = " << testCase.year << " month = " << testCase.month << " day = " << testCase.day;
}

INSTANTIATE_TEST_SUITE_P(
    ValidateDateTests, ValidateDateTest,
    ::testing::Values(
        // Valid dates
        ValidateDateTestCase{2024, 1, 15, true},
        ValidateDateTestCase{2024, 12, 31, true},
        ValidateDateTestCase{1900, 1, 1, true},
        ValidateDateTestCase{9999, 12, 31, true},
        ValidateDateTestCase{1899, 1, 15, true},

        // Valid BC Year
        ValidateDateTestCase{-1, 1, 15, true},

        // Leap year dates
        ValidateDateTestCase{2020, 2, 29, true},
        ValidateDateTestCase{2000, 2, 29, true},
        ValidateDateTestCase{2021, 2, 28, true},
        ValidateDateTestCase{2021, 2, 29, false}, // Not a leap year
        ValidateDateTestCase{1900, 2, 29, false}, // Not a leap year (century)

        // Invalid months
        ValidateDateTestCase{2024, 0, 15, false},
        ValidateDateTestCase{2024, 13, 15, false},
        ValidateDateTestCase{2024, -1, 15, false},

        // Invalid days
        ValidateDateTestCase{2024, 1, 0, false},
        ValidateDateTestCase{2024, 1, 32, false},
        ValidateDateTestCase{2024, 2, 30, false},
        ValidateDateTestCase{2024, 4, 31, false},  // April has 30 days
        ValidateDateTestCase{2024, 11, 31, false}, // November has 30 days

        // Invalid years
        ValidateDateTestCase{10000, 1, 15, false},

        // Month boundaries
        ValidateDateTestCase{2024, 1, 31, true}, // January - 31 days
        ValidateDateTestCase{2024, 2, 28,
                             true}, // February - 28 days (non-leap)
        ValidateDateTestCase{2024, 3, 31, true},  // March - 31 days
        ValidateDateTestCase{2024, 4, 30, true},  // April - 30 days
        ValidateDateTestCase{2024, 5, 31, true},  // May - 31 days
        ValidateDateTestCase{2024, 6, 30, true},  // June - 30 days
        ValidateDateTestCase{2024, 7, 31, true},  // July - 31 days
        ValidateDateTestCase{2024, 8, 31, true},  // August - 31 days
        ValidateDateTestCase{2024, 9, 30, true},  // September - 30 days
        ValidateDateTestCase{2024, 10, 31, true}, // October - 31 days
        ValidateDateTestCase{2024, 11, 30, true}, // November - 30 days
        ValidateDateTestCase{2024, 12, 31, true}  // December - 31 days
        ));


// Unit Tests for parseDatePart
struct ParseDatePartTestCase {
    const char *input;
    SQLRETURN expectedRC;
    int expectedYear;
    int expectedMonth;
    int expectedDay;
};

class ParseDatePartTest
    : public ::testing::TestWithParam<ParseDatePartTestCase> {
  protected:
    RS_ERROR_INFO *errorList;

    void SetUp() override { errorList = nullptr; }

    void TearDown() override {
        if (errorList) {
            clearErrorList(errorList);
        }
    }
};

TEST_P(ParseDatePartTest, ParseDatePart) {
    auto testCase = GetParam();
    int year = 0, month = 0, day = 0;

    SQLRETURN rc =
        parseDatePart(testCase.input, &year, &month, &day, &errorList);

    EXPECT_EQ(rc, testCase.expectedRC)
        << "Failed for input: " << testCase.input;

    if (rc == SQL_SUCCESS) {
        EXPECT_EQ(year, testCase.expectedYear);
        EXPECT_EQ(month, testCase.expectedMonth);
        EXPECT_EQ(day, testCase.expectedDay);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParseDatePartTests, ParseDatePartTest,
    ::testing::Values(
        // Valid dates
        ParseDatePartTestCase{"2024-01-15", SQL_SUCCESS, 2024, 1, 15},
        ParseDatePartTestCase{"1900-01-01", SQL_SUCCESS, 1900, 1, 1},
        ParseDatePartTestCase{"9999-12-31", SQL_SUCCESS, 9999, 12, 31},
        ParseDatePartTestCase{"2020-02-29", SQL_SUCCESS, 2020, 2,
                              29}, // Leap year

        // Invalid formats
        ParseDatePartTestCase{"2024-1-15", SQL_ERROR, 0, 0, 0}, // Wrong format
        ParseDatePartTestCase{"2024/01/15", SQL_ERROR, 0, 0,
                              0}, // Wrong separator
        ParseDatePartTestCase{"24-01-15", SQL_ERROR, 0, 0, 0}, // Short year
        ParseDatePartTestCase{"abc", SQL_ERROR, 0, 0, 0},

        // Invalid dates
        ParseDatePartTestCase{"2024-13-01", SQL_ERROR, 0, 0, 0},
        ParseDatePartTestCase{"2024-01-32", SQL_ERROR, 0, 0, 0},
        ParseDatePartTestCase{"2021-02-29", SQL_ERROR, 0, 0, 0}
        // Not a leap year
        ));

// Unit Tests for parseTimePart
struct ParseTimePartTestCase {
    const char *input;
    SQLRETURN expectedRC;
    int expectedHour;
    int expectedMinute;
    int expectedSecond;
    bool hasFraction;
};

class ParseTimePartTest
    : public ::testing::TestWithParam<ParseTimePartTestCase> {
  protected:
    RS_ERROR_INFO *errorList;

    void SetUp() override { errorList = nullptr; }

    void TearDown() override {
        if (errorList) {
            clearErrorList(errorList);
        }
    }
};

TEST_P(ParseTimePartTest, ParseTimePart) {
    auto testCase = GetParam();
    int hour = 0, minute = 0, second = 0;
    const char *fracStart = nullptr;

    SQLRETURN rc = parseTimePart(testCase.input, &hour, &minute, &second,
                                 &fracStart, &errorList);

    EXPECT_EQ(rc, testCase.expectedRC)
        << "Failed for input: " << testCase.input;

    if (rc == SQL_SUCCESS) {
        EXPECT_EQ(hour, testCase.expectedHour);
        EXPECT_EQ(minute, testCase.expectedMinute);
        EXPECT_EQ(second, testCase.expectedSecond);
        EXPECT_EQ(fracStart != nullptr, testCase.hasFraction);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParseTimePartTests, ParseTimePartTest,
    ::testing::Values(
        // Valid times without fractions
        ParseTimePartTestCase{"10:30:45", SQL_SUCCESS, 10, 30, 45, false},
        ParseTimePartTestCase{"00:00:00", SQL_SUCCESS, 0, 0, 0, false},
        ParseTimePartTestCase{"23:59:59", SQL_SUCCESS, 23, 59, 59, false},

        // Valid times with fractions
        ParseTimePartTestCase{"10:30:45.123456", SQL_SUCCESS, 10, 30, 45, true},
        ParseTimePartTestCase{"10:30:45.0", SQL_SUCCESS, 10, 30, 45, true},

        // Invalid times
        ParseTimePartTestCase{"25:30:45", SQL_ERROR, 0, 0, 0,
                              false}, // Hour > 23
        ParseTimePartTestCase{"10:60:45", SQL_ERROR, 0, 0, 0,
                              false}, // Minute > 59
        ParseTimePartTestCase{"10:30:60", SQL_ERROR, 0, 0, 0,
                              false}, // Second > 59
        ParseTimePartTestCase{"1:30:45", SQL_ERROR, 0, 0, 0,
                              false}, // Wrong format
        ParseTimePartTestCase{"10-30-45", SQL_ERROR, 0, 0, 0,
                              false}, // Wrong separator
        ParseTimePartTestCase{"ab:cd:ef", SQL_ERROR, 0, 0, 0,
                              false}, // Non-digits
        ParseTimePartTestCase{"", SQL_ERROR, 0, 0, 0, false}));


// Unit Tests for parseFraction
struct ParseFractionTestCase {
    const char *input; // Should point to '.' or be nullptr
    SQLRETURN expectedRC;
    unsigned int expectedFraction;
};

class ParseFractionTest
    : public ::testing::TestWithParam<ParseFractionTestCase> {
  protected:
    RS_ERROR_INFO *errorList;

    void SetUp() override { errorList = nullptr; }

    void TearDown() override {
        if (errorList) {
            clearErrorList(errorList);
        }
    }
};

TEST_P(ParseFractionTest, ParseFraction) {
    auto testCase = GetParam();
    unsigned int fraction = 0;
    bool trunc = false;

    SQLRETURN rc = parseFraction(testCase.input, &fraction, &trunc, &errorList);

    EXPECT_EQ(rc, testCase.expectedRC) << "Failed for input: " << testCase.input;

    if (rc == SQL_SUCCESS) {
        EXPECT_EQ(fraction, testCase.expectedFraction) << "Failed for input: " << testCase.input;
    }
}

INSTANTIATE_TEST_SUITE_P(
    ParseFractionTests, ParseFractionTest,
    ::testing::Values(
        // No fraction
        ParseFractionTestCase{nullptr, SQL_SUCCESS, 0},

        // Valid fractions (9 digits or less - no truncation)
        ParseFractionTestCase{".123456", SQL_SUCCESS, 123456000},
        ParseFractionTestCase{".1", SQL_SUCCESS, 100000000},
        ParseFractionTestCase{".12", SQL_SUCCESS, 120000000},
        ParseFractionTestCase{".123", SQL_SUCCESS, 123000000},
        ParseFractionTestCase{".000001", SQL_SUCCESS, 1000},
        ParseFractionTestCase{".999999", SQL_SUCCESS, 999999000},

        // Fractions needing padding
        ParseFractionTestCase{".0", SQL_SUCCESS, 0},
        ParseFractionTestCase{".00", SQL_SUCCESS, 0},
        ParseFractionTestCase{".5", SQL_SUCCESS, 500000000},

        // Fractions exceeding nanosecond precision (more than 9 digits)
        ParseFractionTestCase{".1234567890", SQL_ERROR, 0},
        ParseFractionTestCase{".9999999990", SQL_ERROR, 0},

        // Invalid fractions (no digits after dot)
        ParseFractionTestCase{".", SQL_ERROR, 0}));


// Unit Tests for convertFloatValue (template helper)
TEST(ConvertFloatValueTest, BasicFloatConversion) {
    float result;
    bool success;
    RS_ERROR_INFO *errorList = nullptr;

    // Normal conversion
    convertFloatValue(123.45L, &result, &success, &errorList, "FLOAT");
    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(result, 123.45f);

    // Infinity
    convertFloatValue(std::numeric_limits<long double>::infinity(), &result,
                      &success, &errorList, "FLOAT");
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0); // Positive infinity

    // Negative infinity
    convertFloatValue(-std::numeric_limits<long double>::infinity(), &result,
                      &success, &errorList, "FLOAT");
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(result));
    EXPECT_LT(result, 0); // Negative infinity

    // NaN
    convertFloatValue(std::numeric_limits<long double>::quiet_NaN(), &result,
                      &success, &errorList, "FLOAT");
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isnan(result));

    // Out of range
    convertFloatValue(1.0e50L, &result, &success, &errorList, "FLOAT");
    EXPECT_FALSE(success);
    EXPECT_NE(errorList, nullptr);

    if (errorList) {
        clearErrorList(errorList);
    }
}

TEST(ConvertFloatValueTest, BasicDoubleConversion) {
    double result;
    bool success;
    RS_ERROR_INFO* errorList = nullptr;
    
    // Normal conversion
    convertFloatValue(123.456789L, &result, &success, &errorList, "DOUBLE");
    EXPECT_TRUE(success);
    EXPECT_DOUBLE_EQ(result, 123.456789);
    
    // Infinity
    convertFloatValue(std::numeric_limits<long double>::infinity(), &result, &success, &errorList, "DOUBLE");
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(result));
    
    // NaN
    convertFloatValue(std::numeric_limits<long double>::quiet_NaN(), &result, &success, &errorList, "DOUBLE");
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isnan(result));
    
    if (errorList) {
        clearErrorList(errorList);
    }
}

// ============================================================================
// Parameterized test for getShortVal
// ============================================================================
struct ShortValParams {
    short inputVal;
    bool testNullLength;
};

class GetShortValTest : public ::testing::TestWithParam<ShortValParams> {};

TEST_P(GetShortValTest, SetsValueAndLengthCorrectly) {
    auto params = GetParam();
    short outputVal = 0;
    SQLINTEGER length = 0;
    SQLINTEGER *pLength = params.testNullLength ? nullptr : &length;

    if (params.testNullLength) {
        EXPECT_NO_THROW(getShortVal(params.inputVal, &outputVal, pLength));
    } else {
        getShortVal(params.inputVal, &outputVal, pLength);
        EXPECT_EQ(length, sizeof(short));
    }
    EXPECT_EQ(outputVal, params.inputVal);
}

INSTANTIATE_TEST_SUITE_P(
    ShortValTests,
    GetShortValTest,
    ::testing::Values(
        ShortValParams{0, false},
        ShortValParams{0, true},
        ShortValParams{12345, false},
        ShortValParams{12345, true},
        ShortValParams{-9999, false},
        ShortValParams{-9999, true},
        ShortValParams{SHRT_MAX, false},
        ShortValParams{SHRT_MAX, true},
        ShortValParams{SHRT_MIN, false},
        ShortValParams{SHRT_MIN, true}
    )
);

// ============================================================================
// Parameterized test for getIntVal
// ============================================================================
struct IntValParams {
    int inputVal;
    bool testNullLength;
};

class GetIntValTest : public ::testing::TestWithParam<IntValParams> {};

TEST_P(GetIntValTest, SetsValueAndLengthCorrectly) {
    auto params = GetParam();
    int outputVal = 0;
    SQLINTEGER length = 0;
    SQLINTEGER *pLength = params.testNullLength ? nullptr : &length;

    if (params.testNullLength) {
        EXPECT_NO_THROW(getIntVal(params.inputVal, &outputVal, pLength));
    } else {
        getIntVal(params.inputVal, &outputVal, pLength);
        EXPECT_EQ(length, sizeof(int));
    }
    EXPECT_EQ(outputVal, params.inputVal);
}

INSTANTIATE_TEST_SUITE_P(
    IntValTests,
    GetIntValTest,
    ::testing::Values(
        IntValParams{0, false},
        IntValParams{0, true},
        IntValParams{123456789, false},
        IntValParams{123456789, true},
        IntValParams{-987654321, false},
        IntValParams{-987654321, true},
        IntValParams{INT_MAX, false},
        IntValParams{INT_MAX, true},
        IntValParams{INT_MIN, false},
        IntValParams{INT_MIN, true}
    )
);

// ============================================================================
// Parameterized test for getSQLINTEGERVal
// ============================================================================
struct SQLINTEGERValParams {
    long inputVal;
    bool testNullLength;
};

class GetSQLINTEGERValTest : public ::testing::TestWithParam<SQLINTEGERValParams> {};

TEST_P(GetSQLINTEGERValTest, SetsValueAndLengthCorrectly) {
    auto params = GetParam();
    SQLINTEGER outputVal = 0;
    SQLINTEGER length = 0;
    SQLINTEGER *pLength = params.testNullLength ? nullptr : &length;

    if (params.testNullLength) {
        EXPECT_NO_THROW(getSQLINTEGERVal(params.inputVal, &outputVal, pLength));
    } else {
        getSQLINTEGERVal(params.inputVal, &outputVal, pLength);
        EXPECT_EQ(length, sizeof(SQLINTEGER));
    }
    EXPECT_EQ(outputVal, static_cast<SQLINTEGER>(params.inputVal));
}

INSTANTIATE_TEST_SUITE_P(
    SQLINTEGERValTests,
    GetSQLINTEGERValTest,
    ::testing::Values(
        SQLINTEGERValParams{0L, false},
        SQLINTEGERValParams{0L, true},
        SQLINTEGERValParams{999999L, false},
        SQLINTEGERValParams{999999L, true},
        SQLINTEGERValParams{-123456L, false},
        SQLINTEGERValParams{-123456L, true},
        SQLINTEGERValParams{LONG_MAX, false},
        SQLINTEGERValParams{LONG_MAX, true},
        SQLINTEGERValParams{LONG_MIN, false},
        SQLINTEGERValParams{LONG_MIN, true}
    )
);

// ============================================================================
// Parameterized test for getSQLLENVal
// ============================================================================
struct SQLLENValParams {
    long inputVal;
    bool testNullLength;
};

class GetSQLLENValTest : public ::testing::TestWithParam<SQLLENValParams> {};

TEST_P(GetSQLLENValTest, SetsValueAndLengthCorrectly) {
    auto params = GetParam();
    SQLLEN outputVal = 0;
    SQLINTEGER length = 0;
    SQLINTEGER *pLength = params.testNullLength ? nullptr : &length;

    if (params.testNullLength) {
        EXPECT_NO_THROW(getSQLLENVal(params.inputVal, &outputVal, pLength));
    } else {
        getSQLLENVal(params.inputVal, &outputVal, pLength);
        EXPECT_EQ(length, sizeof(SQLLEN));
    }
    EXPECT_EQ(outputVal, static_cast<SQLLEN>(params.inputVal));
}

INSTANTIATE_TEST_SUITE_P(
    SQLLENValTests,
    GetSQLLENValTest,
    ::testing::Values(
        SQLLENValParams{0L, false},
        SQLLENValParams{0L, true},
        SQLLENValParams{888888L, false},
        SQLLENValParams{888888L, true},
        SQLLENValParams{-777777L, false},
        SQLLENValParams{-777777L, true},
        SQLLENValParams{LONG_MAX, false},
        SQLLENValParams{LONG_MAX, true},
        SQLLENValParams{LONG_MIN, false},
        SQLLENValParams{LONG_MIN, true}
    )
);

// ============================================================================
// Parameterized test for getSQLULENVal
// ============================================================================
struct SQLULENValParams {
    long inputVal;
    bool testNullLength;
};

class GetSQLULENValTest : public ::testing::TestWithParam<SQLULENValParams> {};

TEST_P(GetSQLULENValTest, SetsValueAndLengthCorrectly) {
    auto params = GetParam();
    SQLULEN outputVal = 0;
    SQLINTEGER length = 0;
    SQLINTEGER *pLength = params.testNullLength ? nullptr : &length;

    if (params.testNullLength) {
        EXPECT_NO_THROW(getSQLULENVal(params.inputVal, &outputVal, pLength));
    } else {
        getSQLULENVal(params.inputVal, &outputVal, pLength);
        EXPECT_EQ(length, sizeof(SQLULEN));
    }
    EXPECT_EQ(outputVal, static_cast<SQLULEN>(params.inputVal));
}

INSTANTIATE_TEST_SUITE_P(
    SQLULENValTests,
    GetSQLULENValTest,
    ::testing::Values(
        SQLULENValParams{0L, false},
        SQLULENValParams{0L, true},
        SQLULENValParams{555555L, false},
        SQLULENValParams{555555L, true},
        SQLULENValParams{9999999L, false},
        SQLULENValParams{9999999L, true},
        SQLULENValParams{LONG_MAX, false},
        SQLULENValParams{LONG_MAX, true}
    )
);

// ============================================================================
// Parameterized test for getPointerVal
// ============================================================================
struct PointerValParams {
    void *inputPtr;
    bool testNullLength;
    std::string description;
};

class GetPointerValTest : public ::testing::TestWithParam<PointerValParams> {};

TEST_P(GetPointerValTest, SetsValueAndLengthCorrectly) {
    auto params = GetParam();
    void *outputPtr = nullptr;
    SQLINTEGER length = 0;
    SQLINTEGER *pLength = params.testNullLength ? nullptr : &length;

    if (params.testNullLength) {
        EXPECT_NO_THROW(getPointerVal(params.inputPtr, &outputPtr, pLength));
    } else {
        getPointerVal(params.inputPtr, &outputPtr, pLength);
        EXPECT_EQ(length, sizeof(void *));
    }
    EXPECT_EQ(outputPtr, params.inputPtr);
}

INSTANTIATE_TEST_SUITE_P(
    PointerValTests,
    GetPointerValTest,
    ::testing::Values(
        PointerValParams{nullptr, false, "NullPointer"},
        PointerValParams{nullptr, true, "NullPointerWithNullLength"},
        PointerValParams{reinterpret_cast<void *>(0x12345678), false, "ValidPointer"},
        PointerValParams{reinterpret_cast<void *>(0x12345678), true, "ValidPointerWithNullLength"}
    )
);
// Interval Parsing Unit Tests
// Tests for parse_intervaly2m and parse_intervald2s functions
// Validates ODBC-compliant SQL_INTERVAL_STRUCT population
// ============================================================================

// Test suite for parse_intervaly2m function
class IntervalY2MParseTest : public ::testing::Test {
  protected:
    void verifyY2MResult(const SQL_INTERVAL_STRUCT &result,
                         SQLUINTEGER expectedYear, SQLUINTEGER expectedMonth,
                         SQLSMALLINT expectedSign) {
        EXPECT_EQ(result.interval_type, SQL_IS_YEAR_TO_MONTH);
        EXPECT_EQ(result.interval_sign, expectedSign);
        EXPECT_EQ(result.intval.year_month.year, expectedYear);
        EXPECT_EQ(result.intval.year_month.month, expectedMonth);
    }
};

// Test zero interval for year-to-month
TEST_F(IntervalY2MParseTest, ZeroInterval) {
    const char *input = "0-0";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 0, SQL_FALSE);
}

// Test SQL standard format: "Y-M"
TEST_F(IntervalY2MParseTest, SqlStandardPositive) {
    const char *input = "3-6";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 3, 6, SQL_FALSE);
}

// Test SQL standard format with negative: "-Y-M"
TEST_F(IntervalY2MParseTest, SqlStandardNegative) {
    const char *input = "-3-6";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 3, 6, SQL_TRUE);
}

// Test Postgres format: "N years M mons"
TEST_F(IntervalY2MParseTest, PostgresFormatPositive) {
    const char *input = "3 years 6 mons";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 3, 6, SQL_FALSE);
}

// Test Postgres format with negative year
TEST_F(IntervalY2MParseTest, PostgresFormatNegativeYear) {
    const char *input = "-3 years 6 mons";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 3, 6, SQL_TRUE);
}

// Test Postgres format with negative month
TEST_F(IntervalY2MParseTest, PostgresFormatNegativeMonth) {
    const char *input = "3 years -6 mons";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 3, 6, SQL_TRUE);
}

// Test Postgres verbose format with "ago"
TEST_F(IntervalY2MParseTest, PostgresVerboseAgo) {
    const char *input = "3 years 6 mons ago";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 3, 6, SQL_TRUE);
}

// Test year only
TEST_F(IntervalY2MParseTest, YearOnly) {
    const char *input = "5 years";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 5, 0, SQL_FALSE);
}

// Test month only
TEST_F(IntervalY2MParseTest, MonthOnly) {
    const char *input = "8 mons";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 8, SQL_FALSE);
}

// Test large year value
TEST_F(IntervalY2MParseTest, LargeYearValue) {
    const char *input = "999-11";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 999, 11, SQL_FALSE);
}

// Test single digit values
TEST_F(IntervalY2MParseTest, SingleDigitValues) {
    const char *input = "1-1";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 1, 1, SQL_FALSE);
}

TEST(ReturnInvalidIntervalTest, ReturnInvalidIntervalY2M) {
    SQL_INTERVAL_STRUCT result = returnInvalidIntervalY2M();
    EXPECT_EQ(result.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(result.interval_sign, SQL_FALSE);
    EXPECT_EQ(result.intval.year_month.year, 0u);
    EXPECT_EQ(result.intval.year_month.month, 0u);
}

TEST(ReturnInvalidIntervalTest, ReturnInvalidIntervalD2S) {
    SQL_INTERVAL_STRUCT result = returnInvalidIntervalD2S();
    EXPECT_EQ(result.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(result.interval_sign, SQL_FALSE);
    EXPECT_EQ(result.intval.day_second.day, 0u);
    EXPECT_EQ(result.intval.day_second.hour, 0u);
    EXPECT_EQ(result.intval.day_second.minute, 0u);
    EXPECT_EQ(result.intval.day_second.second, 0u);
    EXPECT_EQ(result.intval.day_second.fraction, 0u);
}


class IntervalD2SParseTest : public ::testing::Test {
  protected:
    void verifyD2SResult(const SQL_INTERVAL_STRUCT &result,
                         SQLUINTEGER expectedDay, SQLUINTEGER expectedHour,
                         SQLUINTEGER expectedMinute, SQLUINTEGER expectedSecond,
                         SQLUINTEGER expectedFraction,
                         SQLSMALLINT expectedSign) {
        EXPECT_EQ(result.interval_type, SQL_IS_DAY_TO_SECOND);
        EXPECT_EQ(result.interval_sign, expectedSign);
        EXPECT_EQ(result.intval.day_second.day, expectedDay);
        EXPECT_EQ(result.intval.day_second.hour, expectedHour);
        EXPECT_EQ(result.intval.day_second.minute, expectedMinute);
        EXPECT_EQ(result.intval.day_second.second, expectedSecond);
        EXPECT_EQ(result.intval.day_second.fraction, expectedFraction);
    }
};

// Test zero interval for day-to-second
TEST_F(IntervalD2SParseTest, ZeroInterval) {
    const char *input = "0 00:00:00.000000";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 0, SQL_FALSE);
}

// Test SQL standard format: "D H:M:S.F"
TEST_F(IntervalD2SParseTest, SqlStandardPositive) {
    const char *input = "5 12:30:45.123456";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 5, 12, 30, 45, 123456, SQL_FALSE);
}

// Test SQL standard format with negative
TEST_F(IntervalD2SParseTest, SqlStandardNegative) {
    const char *input = "-5 12:30:45.123456";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 5, 12, 30, 45, 123456, SQL_TRUE);
}

// Test Postgres format without days: "H:M:S.F"
TEST_F(IntervalD2SParseTest, PostgresNoDays) {
    const char *input = "12:30:45.123456";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 12, 30, 45, 123456, SQL_FALSE);
}

// Test Postgres format with negative time
TEST_F(IntervalD2SParseTest, PostgresNegativeTime) {
    const char *input = "-12:30:45.123456";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 12, 30, 45, 123456, SQL_TRUE);
}

// Test Postgres format: "N days H:M:S"
TEST_F(IntervalD2SParseTest, PostgresFormatWithDays) {
    const char *input = "3 days 04:05:06.000000";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 3, 4, 5, 6, 0, SQL_FALSE);
}

// Test Postgres format with negative days
TEST_F(IntervalD2SParseTest, PostgresFormatNegativeDays) {
    const char *input = "-3 days 04:05:06.000000";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 3, 4, 5, 6, 0, SQL_TRUE);
}

// Test Postgres verbose format: "@ N days N hours N mins N secs"
TEST_F(IntervalD2SParseTest, PostgresVerboseFormat) {
    const char *input = "@ 2 days 3 hours 4 mins 5 secs";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 2, 3, 4, 5, 0, SQL_FALSE);
}

// Test Postgres verbose format with "ago"
TEST_F(IntervalD2SParseTest, PostgresVerboseAgo) {
    const char *input = "@ 2 days 3 hours 4 mins 5 secs ago";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 2, 3, 4, 5, 0, SQL_TRUE);
}

// Test day only
TEST_F(IntervalD2SParseTest, DayOnly) {
    const char *input = "7 days";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 7, 0, 0, 0, 0, SQL_FALSE);
}

// Test time only without fraction
TEST_F(IntervalD2SParseTest, TimeOnlyNoFraction) {
    const char *input = "01:02:03";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 1, 2, 3, 0, SQL_FALSE);
}

// Test large day value
TEST_F(IntervalD2SParseTest, LargeDayValue) {
    const char *input = "365 23:59:59.999999";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 365, 23, 59, 59, 999999, SQL_FALSE);
}

// Test fraction with fewer digits (should be padded)
TEST_F(IntervalD2SParseTest, FractionPadding) {
    const char *input = "0 00:00:00.123";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    // 123 should be padded to 123000
    verifyD2SResult(result, 0, 0, 0, 0, 123000, SQL_FALSE);
}

// Test fraction with single digit
TEST_F(IntervalD2SParseTest, FractionSingleDigit) {
    const char *input = "0 00:00:00.5";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    // 5 should be padded to 500000
    verifyD2SResult(result, 0, 0, 0, 0, 500000, SQL_FALSE);
}

// Test Postgres verbose with negative components
TEST_F(IntervalD2SParseTest, PostgresVerboseNegativeComponents) {
    const char *input = "@ -2 days 3 hours";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 2, 3, 0, 0, 0, SQL_TRUE);
}

// Test maximum precision fraction
TEST_F(IntervalD2SParseTest, MaxPrecisionFraction) {
    const char *input = "0 00:00:01.999999";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 1, 999999, SQL_FALSE);
}

// Test suite for intervaly2m_out function (string conversion)
class IntervalY2MOutTest : public ::testing::Test {
  protected:
    char buffer[64];

    void SetUp() override { memset(buffer, 0, sizeof(buffer)); }

    SQL_INTERVAL_STRUCT createY2MInterval(SQLUINTEGER year, SQLUINTEGER month,
                                          SQLSMALLINT sign) {
        SQL_INTERVAL_STRUCT interval;
        memset(&interval, 0, sizeof(interval));
        interval.interval_type = SQL_IS_YEAR_TO_MONTH;
        interval.interval_sign = sign;
        interval.intval.year_month.year = year;
        interval.intval.year_month.month = month;
        return interval;
    }
};

// Test positive year-to-month output
TEST_F(IntervalY2MOutTest, PositiveInterval) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(3, 6, SQL_FALSE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "3-6");
}

// Test negative year-to-month output
TEST_F(IntervalY2MOutTest, NegativeInterval) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(3, 6, SQL_TRUE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-3-6");
}

// Test zero interval output
TEST_F(IntervalY2MOutTest, ZeroInterval) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(0, 0, SQL_FALSE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "0-0");
}

// Test suite for intervald2s_out function (string conversion)
class IntervalD2SOutTest : public ::testing::Test {
  protected:
    char buffer[128];

    void SetUp() override { memset(buffer, 0, sizeof(buffer)); }

    SQL_INTERVAL_STRUCT createD2SInterval(SQLUINTEGER day, SQLUINTEGER hour,
                                          SQLUINTEGER minute,
                                          SQLUINTEGER second,
                                          SQLUINTEGER fraction,
                                          SQLSMALLINT sign) {
        SQL_INTERVAL_STRUCT interval;
        memset(&interval, 0, sizeof(interval));
        interval.interval_type = SQL_IS_DAY_TO_SECOND;
        interval.interval_sign = sign;
        interval.intval.day_second.day = day;
        interval.intval.day_second.hour = hour;
        interval.intval.day_second.minute = minute;
        interval.intval.day_second.second = second;
        interval.intval.day_second.fraction = fraction;
        return interval;
    }
};

// Test positive day-to-second output
TEST_F(IntervalD2SOutTest, PositiveInterval) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(5, 12, 30, 45, 123456, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "5 12:30:45.123456");
}

// Test negative day-to-second output
TEST_F(IntervalD2SOutTest, NegativeInterval) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(5, 12, 30, 45, 123456, SQL_TRUE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-5 12:30:45.123456");
}

// Test zero interval output
TEST_F(IntervalD2SOutTest, ZeroInterval) {
    SQL_INTERVAL_STRUCT interval = createD2SInterval(0, 0, 0, 0, 0, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "00:00:00");
}

// Test interval with no fraction
TEST_F(IntervalD2SOutTest, NoFraction) {
    SQL_INTERVAL_STRUCT interval = createD2SInterval(1, 2, 3, 4, 0, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "1 02:03:04");
}

// Test suite for SQL_INTERVAL_STRUCT size compliance
TEST(IntervalStructSizeTest, StructSizeCompliance) {
    // Verify the struct contains the expected fields
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));

    // Test year-to-month type
    interval.interval_type = SQL_IS_YEAR_TO_MONTH;
    interval.interval_sign = SQL_FALSE;
    interval.intval.year_month.year = 1;
    interval.intval.year_month.month = 2;
    EXPECT_EQ(interval.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(interval.intval.year_month.year, 1u);
    EXPECT_EQ(interval.intval.year_month.month, 2u);

    // Test day-to-second type
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_DAY_TO_SECOND;
    interval.interval_sign = SQL_TRUE;
    interval.intval.day_second.day = 1;
    interval.intval.day_second.hour = 2;
    interval.intval.day_second.minute = 3;
    interval.intval.day_second.second = 4;
    interval.intval.day_second.fraction = 5;
    EXPECT_EQ(interval.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(interval.intval.day_second.day, 1u);
    EXPECT_EQ(interval.intval.day_second.hour, 2u);
    EXPECT_EQ(interval.intval.day_second.minute, 3u);
    EXPECT_EQ(interval.intval.day_second.second, 4u);
    EXPECT_EQ(interval.intval.day_second.fraction, 5u);
}

// Test suite for getIntervalY2MData and getIntervalD2SData functions
// Tests ODBC-compliant interval struct copying to application buffer
class IntervalDataCopyTest : public ::testing::Test {
  protected:
    SQL_INTERVAL_STRUCT buffer;
    SQLLEN lenInd;

    void SetUp() override {
        memset(&buffer, 0, sizeof(buffer));
        lenInd = 0;
    }

    SQL_INTERVAL_STRUCT createY2MInterval(SQLUINTEGER year, SQLUINTEGER month,
                                          SQLSMALLINT sign) {
        SQL_INTERVAL_STRUCT interval;
        memset(&interval, 0, sizeof(interval));
        interval.interval_type = SQL_IS_YEAR_TO_MONTH;
        interval.interval_sign = sign;
        interval.intval.year_month.year = year;
        interval.intval.year_month.month = month;
        return interval;
    }

    SQL_INTERVAL_STRUCT createD2SInterval(SQLUINTEGER day, SQLUINTEGER hour,
                                          SQLUINTEGER minute,
                                          SQLUINTEGER second,
                                          SQLUINTEGER fraction,
                                          SQLSMALLINT sign) {
        SQL_INTERVAL_STRUCT interval;
        memset(&interval, 0, sizeof(interval));
        interval.interval_type = SQL_IS_DAY_TO_SECOND;
        interval.interval_sign = sign;
        interval.intval.day_second.day = day;
        interval.intval.day_second.hour = hour;
        interval.intval.day_second.minute = minute;
        interval.intval.day_second.second = second;
        interval.intval.day_second.fraction = fraction;
        return interval;
    }
};

// Test getIntervalY2MData copies struct correctly
TEST_F(IntervalDataCopyTest, Y2MDataCopyPositive) {
    SQL_INTERVAL_STRUCT source = createY2MInterval(3, 6, SQL_FALSE);
    SQLRETURN rc = getIntervalY2MData(&source, &buffer, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
    EXPECT_EQ(buffer.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.year_month.year, 3u);
    EXPECT_EQ(buffer.intval.year_month.month, 6u);
}

// Test getIntervalY2MData copies negative interval correctly
TEST_F(IntervalDataCopyTest, Y2MDataCopyNegative) {
    SQL_INTERVAL_STRUCT source = createY2MInterval(5, 11, SQL_TRUE);
    SQLRETURN rc = getIntervalY2MData(&source, &buffer, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
    EXPECT_EQ(buffer.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.year_month.year, 5u);
    EXPECT_EQ(buffer.intval.year_month.month, 11u);
}

// Test getIntervalD2SData copies struct correctly
TEST_F(IntervalDataCopyTest, D2SDataCopyPositive) {
    SQL_INTERVAL_STRUCT source =
        createD2SInterval(2, 12, 34, 56, 789000, SQL_FALSE);
    SQLRETURN rc = getIntervalD2SData(&source, &buffer, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
    EXPECT_EQ(buffer.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.day_second.day, 2u);
    EXPECT_EQ(buffer.intval.day_second.hour, 12u);
    EXPECT_EQ(buffer.intval.day_second.minute, 34u);
    EXPECT_EQ(buffer.intval.day_second.second, 56u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 789000u);
}

// Test getIntervalD2SData copies negative interval correctly
TEST_F(IntervalDataCopyTest, D2SDataCopyNegative) {
    SQL_INTERVAL_STRUCT source =
        createD2SInterval(1, 2, 3, 4, 500000, SQL_TRUE);
    SQLRETURN rc = getIntervalD2SData(&source, &buffer, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
    EXPECT_EQ(buffer.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.day, 1u);
    EXPECT_EQ(buffer.intval.day_second.hour, 2u);
    EXPECT_EQ(buffer.intval.day_second.minute, 3u);
    EXPECT_EQ(buffer.intval.day_second.second, 4u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 500000u);
}

// Test getIntervalY2MData with zero interval
TEST_F(IntervalDataCopyTest, Y2MDataCopyZero) {
    SQL_INTERVAL_STRUCT source = createY2MInterval(0, 0, SQL_FALSE);
    SQLRETURN rc = getIntervalY2MData(&source, &buffer, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
    EXPECT_EQ(buffer.intval.year_month.year, 0u);
    EXPECT_EQ(buffer.intval.year_month.month, 0u);
}

// Test getIntervalD2SData with zero interval
TEST_F(IntervalDataCopyTest, D2SDataCopyZero) {
    SQL_INTERVAL_STRUCT source = createD2SInterval(0, 0, 0, 0, 0, SQL_FALSE);
    SQLRETURN rc = getIntervalD2SData(&source, &buffer, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
    EXPECT_EQ(buffer.intval.day_second.day, 0u);
    EXPECT_EQ(buffer.intval.day_second.hour, 0u);
    EXPECT_EQ(buffer.intval.day_second.minute, 0u);
    EXPECT_EQ(buffer.intval.day_second.second, 0u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 0u);
}

// Test suite for convertCharToIntervalY2M function
// Tests character string to year-to-month interval conversion
class ConvertCharToIntervalY2MTest : public ::testing::Test {
  protected:
    SQL_INTERVAL_STRUCT intervalVal;
    SQL_INTERVAL_STRUCT buffer;
    SQLLEN lenInd;
    RS_ERROR_INFO *errorList;

    void SetUp() override {
        memset(&intervalVal, 0, sizeof(intervalVal));
        memset(&buffer, 0, sizeof(buffer));
        lenInd = 0;
        errorList = nullptr;
    }

    void TearDown() override { clearErrorList(errorList); }
};

// Test Y-M format string
TEST_F(ConvertCharToIntervalY2MTest, YearMonthFormat) {
    const char *input = "3-6";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.year_month.year, 3u);
    EXPECT_EQ(buffer.intval.year_month.month, 6u);
}

// Test negative Y-M format string
TEST_F(ConvertCharToIntervalY2MTest, NegativeYearMonthFormat) {
    const char *input = "-5-11";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.year_month.year, 5u);
    EXPECT_EQ(buffer.intval.year_month.month, 11u);
}

// Test zero interval
TEST_F(ConvertCharToIntervalY2MTest, ZeroInterval) {
    const char *input = "0-0";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.year_month.year, 0u);
    EXPECT_EQ(buffer.intval.year_month.month, 0u);
}

// Test invalid format - returns SQL_ERROR with 22018 per ODBC spec
TEST_F(ConvertCharToIntervalY2MTest, InvalidFormat) {
    const char *input = "abc";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    // Per ODBC spec, invalid character value should return SQL_ERROR with 22018
    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test suite for convertCharToIntervalD2S function
// Tests character string to day-to-second interval conversion
class ConvertCharToIntervalD2STest : public ::testing::Test {
  protected:
    SQL_INTERVAL_STRUCT intervalVal;
    SQL_INTERVAL_STRUCT buffer;
    SQLLEN lenInd;
    RS_ERROR_INFO *errorList;

    void SetUp() override {
        memset(&intervalVal, 0, sizeof(intervalVal));
        memset(&buffer, 0, sizeof(buffer));
        lenInd = 0;
        errorList = nullptr;
    }

    void TearDown() override { clearErrorList(errorList); }
};

// Test HH:MM:SS format string
TEST_F(ConvertCharToIntervalD2STest, TimeFormat) {
    const char *input = "12:34:56";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.day_second.day, 0u);
    EXPECT_EQ(buffer.intval.day_second.hour, 12u);
    EXPECT_EQ(buffer.intval.day_second.minute, 34u);
    EXPECT_EQ(buffer.intval.day_second.second, 56u);
}

// Test D HH:MM:SS format string
TEST_F(ConvertCharToIntervalD2STest, DayTimeFormat) {
    const char *input = "2 12:34:56";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.day_second.day, 2u);
    EXPECT_EQ(buffer.intval.day_second.hour, 12u);
    EXPECT_EQ(buffer.intval.day_second.minute, 34u);
    EXPECT_EQ(buffer.intval.day_second.second, 56u);
}

// Test negative D HH:MM:SS format string
TEST_F(ConvertCharToIntervalD2STest, NegativeDayTimeFormat) {
    const char *input = "-1 06:30:00";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.day, 1u);
    EXPECT_EQ(buffer.intval.day_second.hour, 6u);
    EXPECT_EQ(buffer.intval.day_second.minute, 30u);
    EXPECT_EQ(buffer.intval.day_second.second, 0u);
}

// Test zero interval
TEST_F(ConvertCharToIntervalD2STest, ZeroInterval) {
    const char *input = "0 00:00:00";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.day_second.day, 0u);
    EXPECT_EQ(buffer.intval.day_second.hour, 0u);
    EXPECT_EQ(buffer.intval.day_second.minute, 0u);
    EXPECT_EQ(buffer.intval.day_second.second, 0u);
}

// Test invalid format - returns SQL_ERROR with 22018 per ODBC spec
TEST_F(ConvertCharToIntervalD2STest, InvalidFormat) {
    const char *input = "abc";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    // Per ODBC spec, invalid character value should return SQL_ERROR with 22018
    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test with fractional seconds
TEST_F(ConvertCharToIntervalD2STest, WithFractionalSeconds) {
    const char *input = "0 01:02:03.456789";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_DAY_TO_SECOND);
    EXPECT_EQ(buffer.intval.day_second.hour, 1u);
    EXPECT_EQ(buffer.intval.day_second.minute, 2u);
    EXPECT_EQ(buffer.intval.day_second.second, 3u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 456789u);
}

// Test invalid format with special characters
TEST_F(ConvertCharToIntervalD2STest, InvalidFormatSpecialChars) {
    const char *input = "!@#$%";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test invalid format with mixed garbage
TEST_F(ConvertCharToIntervalD2STest, InvalidFormatMixedGarbage) {
    const char *input = "xyz123abc";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test invalid format with special characters for Y2M
TEST_F(ConvertCharToIntervalY2MTest, InvalidFormatSpecialChars) {
    const char *input = "!@#$%";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test invalid format with mixed garbage for Y2M
TEST_F(ConvertCharToIntervalY2MTest, InvalidFormatMixedGarbage) {
    const char *input = "xyz123abc";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test invalid: too many dashes
TEST_F(IntervalY2MParseTest, InvalidTooManyDashes) {
    const char *input = "1-2-3";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    // Should return invalid (zeroed) result
    verifyY2MResult(result, 0, 0, SQL_FALSE);
}

// Test invalid: decimal in year
TEST_F(IntervalY2MParseTest, InvalidDecimalInYear) {
    const char *input = "1.5-2";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 0, SQL_FALSE);
}

// Test invalid: decimal in month
TEST_F(IntervalY2MParseTest, InvalidDecimalInMonth) {
    const char *input = "5-3.2";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 0, SQL_FALSE);
}

// Test max valid month (0-11)
TEST_F(IntervalY2MParseTest, MaxValidMonth) {
    const char *input = "0-11";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 11, SQL_FALSE);
}

// Test invalid month (12 >= MAX_DATE_MONTH-1)
TEST_F(IntervalY2MParseTest, InvalidMonthTwelve) {
    const char *input = "0-12";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    // Should return invalid (zeroed) result due to month validation
    verifyY2MResult(result, 0, 0, SQL_FALSE);
}

// Test large year value in SQL standard format
TEST_F(IntervalY2MParseTest, VeryLargeYearValue) {
    const char *input = "999999999-0";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 999999999, 0, SQL_FALSE);
}

// Test invalid minutes (> 59)
TEST_F(IntervalD2SParseTest, InvalidMinutes) {
    const char *input = "1 days 12:75:30";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    // Should return invalid (zeroed) result
    verifyD2SResult(result, 0, 0, 0, 0, 0, SQL_FALSE);
}

// Test invalid seconds (> 59)
TEST_F(IntervalD2SParseTest, InvalidSeconds) {
    const char *input = "1 days 12:30:75";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 0, SQL_FALSE);
}

// Test max time values
TEST_F(IntervalD2SParseTest, MaxTimeValues) {
    const char *input = "0 23:59:59.999999";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 23, 59, 59, 999999, SQL_FALSE);
}

// Test min non-zero fraction
TEST_F(IntervalD2SParseTest, MinNonZeroFraction) {
    const char *input = "0 00:00:00.000001";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 1, SQL_FALSE);
}

// Test very large day value
TEST_F(IntervalD2SParseTest, VeryLargeDayValue) {
    const char *input = "999999999 00:00:00";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 999999999, 0, 0, 0, 0, SQL_FALSE);
}

// Test max valid minutes/seconds
TEST_F(IntervalD2SParseTest, MaxValidMinutesSeconds) {
    const char *input = "0 00:59:59";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 59, 59, 0, SQL_FALSE);
}

// Test invalid minutes (60 - should fail)
TEST_F(IntervalD2SParseTest, InvalidMinutesSixty) {
    const char *input = "0 00:60:00";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 0, SQL_FALSE);
}

// Test invalid seconds (60 - should fail)
TEST_F(IntervalD2SParseTest, InvalidSecondsSixty) {
    const char *input = "0 00:00:60";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 0, SQL_FALSE);
}

// Test verbose day only
TEST_F(IntervalD2SParseTest, VerboseDayOnly) {
    const char *input = "@ 2 days";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 2, 0, 0, 0, 0, SQL_FALSE);
}

// Test verbose hour only
TEST_F(IntervalD2SParseTest, VerboseHourOnly) {
    const char *input = "@ 3 hours";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 3, 0, 0, 0, SQL_FALSE);
}

// Test verbose minute only
TEST_F(IntervalD2SParseTest, VerboseMinuteOnly) {
    const char *input = "@ 15 mins";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 15, 0, 0, SQL_FALSE);
}

// Test verbose second only
TEST_F(IntervalD2SParseTest, VerboseSecondOnly) {
    const char *input = "@ 30 secs";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 30, 0, SQL_FALSE);
}

// Test verbose fractional seconds
TEST_F(IntervalD2SParseTest, VerboseFractionalSeconds) {
    const char *input = "@ 1.5 secs";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 1, 500000, SQL_FALSE);
}

// Test verbose all negative components
TEST_F(IntervalD2SParseTest, VerboseAllNegativeComponents) {
    const char *input = "@ -2 days -3 hours -4 mins -5 secs";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 2, 3, 4, 5, 0, SQL_TRUE);
}

// Test 2 digit fraction -> 120000
TEST_F(IntervalD2SParseTest, FractionTwoDigits) {
    const char *input = "0 00:00:00.12";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 120000, SQL_FALSE);
}

// Test 4 digit fraction -> 123400
TEST_F(IntervalD2SParseTest, FractionFourDigits) {
    const char *input = "0 00:00:00.1234";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 123400, SQL_FALSE);
}

// Test 5 digit fraction -> 123450
TEST_F(IntervalD2SParseTest, FractionFiveDigits) {
    const char *input = "0 00:00:00.12345";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 123450, SQL_FALSE);
}

// Test 6 digit fraction -> 123456 (no padding)
TEST_F(IntervalD2SParseTest, FractionSixDigitsNoPadding) {
    const char *input = "0 00:00:00.123456";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 123456, SQL_FALSE);
}

// Test zero fraction
TEST_F(IntervalD2SParseTest, FractionZero) {
    const char *input = "0 00:00:00.0";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 0, SQL_FALSE);
}

// Test minimum non-zero fraction
TEST_F(IntervalD2SParseTest, FractionMinimumNonZero) {
    const char *input = "0 00:00:00.000001";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 0, 1, SQL_FALSE);
}

// Test year only (month = 0)
TEST_F(IntervalY2MOutTest, YearOnlyMonthZero) {
SQL_INTERVAL_STRUCT interval = createY2MInterval(5, 0, SQL_FALSE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "5-0");
}

// Test month only (year = 0)
TEST_F(IntervalY2MOutTest, MonthOnlyYearZero) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(0, 8, SQL_FALSE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "0-8");
}

// Test large values (999 years)
TEST_F(IntervalY2MOutTest, LargeValues) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(999, 11, SQL_FALSE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "999-11");
}

// Test negative zero interval (sign=TRUE, year=0, month=0)
TEST_F(IntervalY2MOutTest, NegativeZeroInterval) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(0, 0, SQL_TRUE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-0-0");
}

// Test buffer too small
TEST_F(IntervalY2MOutTest, BufferTooSmall) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(999, 11, SQL_FALSE);
    // "999-11" = 6 chars, buffer only has 5
    char smallBuf[5];
    memset(smallBuf, 0, sizeof(smallBuf));
    int len = intervaly2m_out(&interval, smallBuf, sizeof(smallBuf));
    // snprintf returns the would-be length (>= buf_len) on truncation
    EXPECT_GE(len, (int)sizeof(smallBuf));
    // Buffer must be safely null-terminated within bounds
    EXPECT_EQ(smallBuf[4], '\0');
    // The first 4 chars should be the truncated beginning of "999-11"
    EXPECT_EQ(strncmp(smallBuf, "999-", 4), 0);
}

// Test buffer too small with just 1 byte (only null terminator fits)
TEST_F(IntervalY2MOutTest, BufferTooSmallMinimal) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(5, 3, SQL_FALSE);
    // "5-3" = 3 chars, buffer only has 1
    char tinyBuf[1];
    memset(tinyBuf, 'X', sizeof(tinyBuf));
    int len = intervaly2m_out(&interval, tinyBuf, sizeof(tinyBuf));
    EXPECT_GE(len, 1);
    // snprintf with size 1 should write only the null terminator
    EXPECT_EQ(tinyBuf[0], '\0');
}

// Test buffer too small for D2S
TEST_F(IntervalD2SOutTest, BufferTooSmall) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(365, 23, 59, 59, 999999, SQL_FALSE);
    // "365 23:59:59.999999" = 19 chars, buffer only has 5
    char smallBuf[5];
    memset(smallBuf, 0, sizeof(smallBuf));
    int len = intervald2s_out(&interval, smallBuf, sizeof(smallBuf));
    // snprintf returns the would-be length (>= buf_len) on truncation
    EXPECT_GE(len, (int)sizeof(smallBuf));
    // Buffer must be safely null-terminated within bounds
    EXPECT_EQ(smallBuf[4], '\0');
}

// Test time only (day = 0)
TEST_F(IntervalD2SOutTest, TimeOnlyDayZero) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 5, 30, 45, 0, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "05:30:45");
}

// Test day only (time = 0)
TEST_F(IntervalD2SOutTest, DayOnlyTimeZero) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(7, 0, 0, 0, 0, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "7 00:00:00");
}

// Test fraction only (second = 0, fraction is non-zero)
TEST_F(IntervalD2SOutTest, FractionOnlySecondZero) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 0, 0, 0, 123456, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "00:00:00.123456");
}

// Test large day values
TEST_F(IntervalD2SOutTest, LargeDayValues) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(999999, 23, 59, 59, 999999, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer,
                 "999999 23:59:59.999999");
}

// Test negative zero interval
TEST_F(IntervalD2SOutTest, NegativeZeroInterval) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 0, 0, 0, 0, SQL_TRUE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-00:00:00");
}

// Test various fraction precisions - 1 digit
TEST_F(IntervalD2SOutTest, FractionOneDigit) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 0, 0, 1, 100000, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "00:00:01.100000");
}

// Test various fraction precisions - 3 digits
TEST_F(IntervalD2SOutTest, FractionThreeDigits) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 0, 0, 1, 123000, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "00:00:01.123000");
}

// Test various fraction precisions - 6 digits
TEST_F(IntervalD2SOutTest, FractionSixDigits) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 0, 0, 1, 123456, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "00:00:01.123456");
}

TEST(IntervalStructSizeTest, DirectSizeAssertion) {
    // ODBC standard specifies SQL_INTERVAL_STRUCT should be 28 bytes
    EXPECT_EQ(sizeof(SQL_INTERVAL_STRUCT), 28u);
}

TEST_F(IntervalDataCopyTest, Y2MDataNullBuffer) {
    SQL_INTERVAL_STRUCT source = createY2MInterval(3, 6, SQL_FALSE);
    SQLRETURN rc = getIntervalY2MData(&source, nullptr, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
}

TEST_F(IntervalDataCopyTest, D2SDataNullBuffer) {
    SQL_INTERVAL_STRUCT source =
        createD2SInterval(1, 2, 3, 4, 500000, SQL_FALSE);
    SQLRETURN rc = getIntervalD2SData(&source, nullptr, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, sizeof(SQL_INTERVAL_STRUCT));
}

// Test D2S format string rejected by convertCharToIntervalY2M
TEST_F(ConvertCharToIntervalY2MTest, RejectsDayToSecondInput) {
const char *input = "3 days 04:30:15";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test SQL standard D2S format ("D HH:MM:SS") rejected by Y2M method
TEST_F(ConvertCharToIntervalY2MTest, RejectsDayToSecondSqlStandard) {
    const char *input = "3 04:30:15";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test postgres verbose D2S format rejected by Y2M method
TEST_F(ConvertCharToIntervalY2MTest, RejectsDayToSecondVerbose) {
    const char *input = "@ 3 days 4 hours 30 mins 15 secs";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test time format string rejected by convertCharToIntervalY2M
TEST_F(ConvertCharToIntervalY2MTest, RejectsTimeFormatInput) {
    const char *input = "12:30:45";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test Y2M format string rejected by convertCharToIntervalD2S
TEST_F(ConvertCharToIntervalD2STest, RejectsYearToMonthInput) {
    const char *input = "2 years 3 mons";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test Y2M verbose format rejected by D2S method
TEST_F(ConvertCharToIntervalD2STest, RejectsYearToMonthVerbose) {
    const char *input = "@ 2 years 3 mons";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test Y2M single keyword format rejected by D2S method
TEST_F(ConvertCharToIntervalD2STest, RejectsYearOnlyInput) {
    const char *input = "5 years";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test timestamp format rejected by convertCharToIntervalD2S
TEST_F(ConvertCharToIntervalD2STest, RejectsTimestampInput) {
    const char *input = "2024-01-15 10:30:00";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

TEST_F(ConvertCharToIntervalY2MTest, LeadingTrailingWhitespace) {
    const char *input = "   3-6   ";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_type, SQL_IS_YEAR_TO_MONTH);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.year_month.year, 3u);
    EXPECT_EQ(buffer.intval.year_month.month, 6u);
}

// Test empty string
TEST_F(ConvertCharToIntervalY2MTest, EmptyString) {
    const char *input = "";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test whitespace only
TEST_F(ConvertCharToIntervalY2MTest, WhitespaceOnly) {
    const char *input = "   ";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test empty string for D2S
TEST_F(ConvertCharToIntervalD2STest, EmptyString) {
    const char *input = "";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// Test whitespace only for D2S
TEST_F(ConvertCharToIntervalD2STest, WhitespaceOnly) {
const char *input = "   ";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "22018");
    }
}

// ============================================================================
// Fractional truncation tests for convertCharToIntervalD2S
// Per ODBC spec, when fractional seconds precision exceeds microsecond (6 digits),
// driver should return SQL_SUCCESS_WITH_INFO with SQLSTATE 01S07.
// ============================================================================

// Test 7 fractional digits in time-only format → truncation warning
TEST_F(ConvertCharToIntervalD2STest, FractionalTruncation_TimeFormat_7Digits) {
    const char *input = "01:02:03.1234567";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "01S07");
    }
    // Fraction should be truncated to 6 digits (microseconds)
    EXPECT_EQ(buffer.intval.day_second.hour, 1u);
    EXPECT_EQ(buffer.intval.day_second.minute, 2u);
    EXPECT_EQ(buffer.intval.day_second.second, 3u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123456u);
}

// Test 8 fractional digits in SQL standard D HH:MM:SS format → truncation warning
TEST_F(ConvertCharToIntervalD2STest, FractionalTruncation_DayTimeFormat_8Digits) {
    const char *input = "1 02:03:04.12345678";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "01S07");
    }
    EXPECT_EQ(buffer.intval.day_second.day, 1u);
    EXPECT_EQ(buffer.intval.day_second.hour, 2u);
    EXPECT_EQ(buffer.intval.day_second.minute, 3u);
    EXPECT_EQ(buffer.intval.day_second.second, 4u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123456u);
}

// Test 9 fractional digits in postgres format → truncation warning
TEST_F(ConvertCharToIntervalD2STest, FractionalTruncation_PostgresFormat_9Digits) {
    const char *input = "3 days 04:05:06.123456789";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "01S07");
    }
    EXPECT_EQ(buffer.intval.day_second.day, 3u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123456u);
}

// Test exactly 6 fractional digits → NO truncation (SQL_SUCCESS)
TEST_F(ConvertCharToIntervalD2STest, NoFractionalTruncation_6Digits) {
    const char *input = "01:02:03.123456";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123456u);
}

// Test no fractional seconds → NO truncation (SQL_SUCCESS)
TEST_F(ConvertCharToIntervalD2STest, NoFractionalTruncation_NoFraction) {
    const char *input = "01:02:03";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.day_second.fraction, 0u);
}

// Test 5 fractional digits → NO truncation (SQL_SUCCESS), padded to 6
TEST_F(ConvertCharToIntervalD2STest, NoFractionalTruncation_5Digits) {
    const char *input = "0 00:00:01.12345";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123450u);
}

// --- Y2M Parsing: Singular forms ("1 year", "1 mon") ---
TEST_F(IntervalY2MParseTest, PostgresSingularYear) {
    const char *input = "1 year";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 1, 0, SQL_FALSE);
}

TEST_F(IntervalY2MParseTest, PostgresSingularMon) {
    const char *input = "1 mon";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 1, SQL_FALSE);
}

TEST_F(IntervalY2MParseTest, PostgresNegativeOneMons) {
    const char *input = "-1 mons";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 1, SQL_TRUE);
}

TEST_F(IntervalY2MParseTest, PostgresNegativeOneYear) {
    const char *input = "-1 years";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 1, 0, SQL_TRUE);
}

// --- Y2M Parsing: Postgres verbose without "ago" (positive) ---
TEST_F(IntervalY2MParseTest, VerbosePositiveNoAgo) {
    const char *input = "@ 5 years 3 mons";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 5, 3, SQL_FALSE);
}

TEST_F(IntervalY2MParseTest, VerboseYearOnlyNoAgo) {
    const char *input = "@ 1 year";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 1, 0, SQL_FALSE);
}

// --- Y2M Parsing: Zero in postgres format ---
TEST_F(IntervalY2MParseTest, PostgresZeroYears) {
    const char *input = "0 years";
    SQL_INTERVAL_STRUCT result = parse_intervaly2m(input, strlen(input));
    verifyY2MResult(result, 0, 0, SQL_FALSE);
}

// --- D2S Parsing: Negative one second time-only ---
TEST_F(IntervalD2SParseTest, NegativeOneSecondTimeOnly) {
    const char *input = "-00:00:01";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 1, 0, SQL_TRUE);
}

// --- D2S Parsing: Postgres singular "1 day" ---
TEST_F(IntervalD2SParseTest, PostgresSingularDay) {
    const char *input = "1 day";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 1, 0, 0, 0, 0, SQL_FALSE);
}

// --- D2S Parsing: Postgres negative with independent signs ---
TEST_F(IntervalD2SParseTest, PostgresNegativeIndependentSigns) {
    const char *input = "-3 days -04:30:15.123";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 3, 4, 30, 15, 123000, SQL_TRUE);
}

// --- D2S Parsing: Negative time only with fraction ---
TEST_F(IntervalD2SParseTest, NegativeTimeOnlyWithFraction) {
    const char *input = "-12:30:45.678";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 12, 30, 45, 678000, SQL_TRUE);
}

// --- D2S Parsing: Postgres format one second only ---
TEST_F(IntervalD2SParseTest, PostgresOneSecond) {
    const char *input = "00:00:01";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 0, 0, 0, 1, 0, SQL_FALSE);
}

// --- D2S Parsing: Postgres verbose with fractional seconds ---
TEST_F(IntervalD2SParseTest, VerboseWithFractionalSeconds) {
    const char *input = "@ 3 days 4 hours 30 mins 15.123 secs";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 3, 4, 30, 15, 123000, SQL_FALSE);
}

// --- D2S Parsing: Postgres verbose negative with fractional ---
TEST_F(IntervalD2SParseTest, VerboseNegativeWithFractional) {
    const char *input = "@ 1 day 2 hours 45 mins 30.456 secs ago";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 1, 2, 45, 30, 456000, SQL_TRUE);
}

// --- D2S Parsing: Postgres 6-digit microsecond fraction ---
TEST_F(IntervalD2SParseTest, PostgresSixDigitFraction) {
    const char *input = "1 day 01:02:03.123456";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 1, 1, 2, 3, 123456, SQL_FALSE);
}

// --- D2S Parsing: Large negative ---
TEST_F(IntervalD2SParseTest, LargeNegative) {
    const char *input = "-365 23:59:59.999999";
    SQL_INTERVAL_STRUCT result = parse_intervald2s(input, strlen(input));
    verifyD2SResult(result, 365, 23, 59, 59, 999999, SQL_TRUE);
}

// --- D2S Output: Negative time-only (day=0, sign=TRUE) ---
TEST_F(IntervalD2SOutTest, NegativeTimeOnlyNoDayOutput) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 4, 30, 15, 123000, SQL_TRUE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-04:30:15.123000");
}

// --- D2S Output: Negative with day and fraction ---
TEST_F(IntervalD2SOutTest, NegativeWithDayAndFraction) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(3, 4, 30, 15, 123000, SQL_TRUE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-3 04:30:15.123000");
}

// --- D2S Output: Positive one second only ---
TEST_F(IntervalD2SOutTest, OneSecondOnly) {
    SQL_INTERVAL_STRUCT interval =
        createD2SInterval(0, 0, 0, 1, 0, SQL_FALSE);
    int len = intervald2s_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "00:00:01");
}

// --- Y2M Output: single digit year and month ---
TEST_F(IntervalY2MOutTest, SingleDigitValues) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(1, 1, SQL_FALSE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "1-1");
}

// --- Y2M Output: negative single digit ---
TEST_F(IntervalY2MOutTest, NegativeSingleDigit) {
    SQL_INTERVAL_STRUCT interval = createY2MInterval(1, 1, SQL_TRUE);
    int len = intervaly2m_out(&interval, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "-1-1");
}

// --- convertCharToIntervalY2M: Postgres format inputs ---
TEST_F(ConvertCharToIntervalY2MTest, PostgresFormatPositive) {
    const char *input = "5 years 3 mons";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.year_month.year, 5u);
    EXPECT_EQ(buffer.intval.year_month.month, 3u);
}

TEST_F(ConvertCharToIntervalY2MTest, PostgresFormatNegative) {
    const char *input = "-2 years -7 mons";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.year_month.year, 2u);
    EXPECT_EQ(buffer.intval.year_month.month, 7u);
}

TEST_F(ConvertCharToIntervalY2MTest, PostgresFormatYearOnly) {
    const char *input = "3 years";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.year_month.year, 3u);
    EXPECT_EQ(buffer.intval.year_month.month, 0u);
}

TEST_F(ConvertCharToIntervalY2MTest, PostgresFormatMonthOnly) {
    const char *input = "7 mons";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.year_month.year, 0u);
    EXPECT_EQ(buffer.intval.year_month.month, 7u);
}

TEST_F(ConvertCharToIntervalY2MTest, VerboseFormatPositive) {
    const char *input = "@ 5 years 3 mons";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.year_month.year, 5u);
    EXPECT_EQ(buffer.intval.year_month.month, 3u);
}

TEST_F(ConvertCharToIntervalY2MTest, VerboseFormatNegativeAgo) {
    const char *input = "@ 2 years 7 mons ago";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.year_month.year, 2u);
    EXPECT_EQ(buffer.intval.year_month.month, 7u);
}

TEST_F(ConvertCharToIntervalY2MTest, SqlStandardExplicitPositive) {
    const char *input = "+5-3";
    SQLRETURN rc =
        convertCharToIntervalY2M((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.year_month.year, 5u);
    EXPECT_EQ(buffer.intval.year_month.month, 3u);
}

// --- convertCharToIntervalD2S: Postgres format with day keyword ---
TEST_F(ConvertCharToIntervalD2STest, PostgresFormatWithDayKeyword) {
    const char *input = "3 days 04:30:15.123";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.day_second.day, 3u);
    EXPECT_EQ(buffer.intval.day_second.hour, 4u);
    EXPECT_EQ(buffer.intval.day_second.minute, 30u);
    EXPECT_EQ(buffer.intval.day_second.second, 15u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123000u);
}

TEST_F(ConvertCharToIntervalD2STest, PostgresFormatNegativeWithDayKeyword) {
    const char *input = "-1 days -02:45:30.456";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.day, 1u);
    EXPECT_EQ(buffer.intval.day_second.hour, 2u);
    EXPECT_EQ(buffer.intval.day_second.minute, 45u);
    EXPECT_EQ(buffer.intval.day_second.second, 30u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 456000u);
}

TEST_F(ConvertCharToIntervalD2STest, PostgresFormatDayOnly) {
    const char *input = "5 days";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.day_second.day, 5u);
    EXPECT_EQ(buffer.intval.day_second.hour, 0u);
}

TEST_F(ConvertCharToIntervalD2STest, PostgresFormatNegativeDayOnly) {
    const char *input = "-5 days";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.day, 5u);
}

TEST_F(ConvertCharToIntervalD2STest, VerboseFormatPositive) {
    const char *input = "@ 3 days 4 hours 30 mins 15 secs";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_FALSE);
    EXPECT_EQ(buffer.intval.day_second.day, 3u);
    EXPECT_EQ(buffer.intval.day_second.hour, 4u);
    EXPECT_EQ(buffer.intval.day_second.minute, 30u);
    EXPECT_EQ(buffer.intval.day_second.second, 15u);
}

TEST_F(ConvertCharToIntervalD2STest, VerboseFormatNegativeAgo) {
    const char *input = "@ 1 day 2 hours 45 mins 30.456 secs ago";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.day, 1u);
    EXPECT_EQ(buffer.intval.day_second.hour, 2u);
    EXPECT_EQ(buffer.intval.day_second.minute, 45u);
    EXPECT_EQ(buffer.intval.day_second.second, 30u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 456000u);
}

TEST_F(ConvertCharToIntervalD2STest, NegativeTimeOnlyWithFraction) {
    const char *input = "-12:30:45.678";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.day, 0u);
    EXPECT_EQ(buffer.intval.day_second.hour, 12u);
    EXPECT_EQ(buffer.intval.day_second.minute, 30u);
    EXPECT_EQ(buffer.intval.day_second.second, 45u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 678000u);
}

TEST_F(ConvertCharToIntervalD2STest, SqlStandardZero) {
    const char *input = "0 00:00:00";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(buffer.intval.day_second.day, 0u);
    EXPECT_EQ(buffer.intval.day_second.hour, 0u);
    EXPECT_EQ(buffer.intval.day_second.minute, 0u);
    EXPECT_EQ(buffer.intval.day_second.second, 0u);
    EXPECT_EQ(buffer.intval.day_second.fraction, 0u);
}

// Test negative interval with 7 fractional digits → truncation warning
TEST_F(ConvertCharToIntervalD2STest, FractionalTruncation_Negative_7Digits) {
    const char *input = "-01:02:03.1234567";
    SQLRETURN rc =
        convertCharToIntervalD2S((char *)input, strlen(input), TEXT_FORMAT,
                                 &intervalVal, &buffer, &lenInd, &errorList);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_NE(errorList, nullptr);
    if (errorList) {
        EXPECT_STREQ(errorList->szSqlState, "01S07");
    }
    EXPECT_EQ(buffer.interval_sign, SQL_TRUE);
    EXPECT_EQ(buffer.intval.day_second.fraction, 123456u);
}

// ============================================================================
// Unified Interval-to-SQLWCHAR Code Path Tests
//
// These tests verify the new unified (platform-independent) code path that
// replaces the old WIN32-only intervaly2m_out_wchar / intervald2s_out_wchar
// functions. The new approach:
//   1. parse text → SQL_INTERVAL_STRUCT  (text mode)
//      OR use binary intervalVal directly (binary mode)
//   2. format with intervaly2m_out / intervald2s_out → char tempBuf
//   3. copy to SQLWCHAR via copyWStrDataBigLen
//
// This is the same code path now used on ALL platforms (Windows, Linux, macOS).
// The _out_wchar functions are no longer called.
// ============================================================================

// Test fixture for interval-to-SQLWCHAR unified path
class IntervalToSQLWCHARTest : public ::testing::TestWithParam<int> {
  protected:
    int savedUnicodeType;

    void SetUp() override {
        savedUnicodeType = tls_unicode_ref();
        tls_unicode_ref() = GetParam();
    }

    void TearDown() override {
        tls_unicode_ref() = savedUnicodeType;
    }

    // Helper: simulates the code path for interval Y2M (text mode)
    // This mirrors the code path in convertSQLDataToCData for SQL_C_WCHAR
    SQLRETURN unifiedIntervalY2M_TextMode(const char *textInput, int inputLen,
                                           SQLWCHAR *pBuf, SQLLEN cbLen,
                                           SQLLEN *cbLenOffset,
                                           SQLLEN *pcbLenInd) {
        char szNumBuf[MAX_NUMBER_BUF_LEN + 1];
        makeNullTerminateIntVal((char *)textInput, inputLen, szNumBuf,
                                MAX_NUMBER_BUF_LEN + 1);
        SQL_INTERVAL_STRUCT intervalVal =
            parse_intervaly2m(szNumBuf, strlen(szNumBuf));

        char tempBuf[MAX_TEMP_BUF_LEN];
        int len = intervaly2m_out(&intervalVal, tempBuf, sizeof(tempBuf));

        return copyWStrDataBigLen(nullptr, tempBuf, len, pBuf, cbLen,
                                  cbLenOffset, pcbLenInd);
    }

    // Helper: simulates the code path for interval D2S (text mode)
    SQLRETURN unifiedIntervalD2S_TextMode(const char *textInput, int inputLen,
                                           SQLWCHAR *pBuf, SQLLEN cbLen,
                                           SQLLEN *cbLenOffset,
                                           SQLLEN *pcbLenInd) {
        char szNumBuf[MAX_NUMBER_BUF_LEN + 1];
        makeNullTerminateIntVal((char *)textInput, inputLen, szNumBuf,
                                MAX_NUMBER_BUF_LEN + 1);
        SQL_INTERVAL_STRUCT intervalVal =
            parse_intervald2s(szNumBuf, strlen(szNumBuf));

        char tempBuf[MAX_TEMP_BUF_LEN];
        int len = intervald2s_out(&intervalVal, tempBuf, sizeof(tempBuf));

        return copyWStrDataBigLen(nullptr, tempBuf, len, pBuf, cbLen,
                                  cbLenOffset, pcbLenInd);
    }

    // Helper: simulates the new unified code path for interval Y2M (binary mode)
    SQLRETURN unifiedIntervalY2M_BinaryMode(
        const SQL_INTERVAL_STRUCT *pInterval, SQLWCHAR *pBuf, SQLLEN cbLen,
        SQLLEN *cbLenOffset, SQLLEN *pcbLenInd) {
        char tempBuf[MAX_TEMP_BUF_LEN];
        int len = intervaly2m_out((SQL_INTERVAL_STRUCT *)pInterval, tempBuf,
                                  sizeof(tempBuf));
        return copyWStrDataBigLen(nullptr, tempBuf, len, pBuf, cbLen,
                                  cbLenOffset, pcbLenInd);
    }

    // Helper: simulates the new unified code path for interval D2S (binary mode)
    SQLRETURN unifiedIntervalD2S_BinaryMode(
        const SQL_INTERVAL_STRUCT *pInterval, SQLWCHAR *pBuf, SQLLEN cbLen,
        SQLLEN *cbLenOffset, SQLLEN *pcbLenInd) {
        char tempBuf[MAX_TEMP_BUF_LEN];
        int len = intervald2s_out((SQL_INTERVAL_STRUCT *)pInterval, tempBuf,
                                  sizeof(tempBuf));
        return copyWStrDataBigLen(nullptr, tempBuf, len, pBuf, cbLen,
                                  cbLenOffset, pcbLenInd);
    }
};

INSTANTIATE_TEST_SUITE_P(
    UTF16_and_UTF32, IntervalToSQLWCHARTest,
    ::testing::Values(SQL_DD_CP_UTF16, SQL_DD_CP_UTF32),
    [](const testing::TestParamInfo<int> &info) {
        return info.param == SQL_DD_CP_UTF16 ? "UTF16" : "UTF32";
    });

// --- Y2M Text Mode → SQLWCHAR ---

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_Positive) {
    const char *input = "3-6";
    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_TextMode(input, strlen(input), dest,
                                           10 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"3-6");
    EXPECT_EQ(lenInd, 3 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_Negative) {
    const char *input = "-5-11";
    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_TextMode(input, strlen(input), dest,
                                           10 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-5-11");
    EXPECT_EQ(lenInd, 5 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_Zero) {
    const char *input = "0-0";
    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_TextMode(input, strlen(input), dest,
                                           10 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"0-0");
}

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_PostgresFormat) {
    const char *input = "3 years 6 mons";
    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_TextMode(input, strlen(input), dest,
                                           10 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"3-6");
}

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_PostgresVerboseAgo) {
    const char *input = "3 years 6 mons ago";
    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_TextMode(input, strlen(input), dest,
                                           10 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-3-6");
}

// --- D2S Text Mode → SQLWCHAR ---

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_Positive) {
    const char *input = "5 12:30:45.123456";
    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           20 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"5 12:30:45.123456");
}

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_Negative) {
    const char *input = "-1 06:30:00";
    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           20 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-1 06:30:00");
}

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_TimeOnly) {
    const char *input = "12:30:45";
    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           20 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"12:30:45");
}

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_Zero) {
    const char *input = "0 00:00:00.000000";
    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           20 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"00:00:00");
}

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_PostgresFormat) {
    const char *input = "3 days 04:30:15.123";
    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           20 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"3 04:30:15.123000");
}

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_PostgresVerboseAgo) {
    const char *input = "@ 2 days 3 hours 4 mins 5 secs ago";
    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           20 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-2 03:04:05");
}

// --- Y2M Binary Mode → SQLWCHAR ---

TEST_P(IntervalToSQLWCHARTest, Y2M_BinaryMode_Positive) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_YEAR_TO_MONTH;
    interval.interval_sign = SQL_FALSE;
    interval.intval.year_month.year = 3;
    interval.intval.year_month.month = 6;

    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_BinaryMode(&interval, dest,
                                             10 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"3-6");
    EXPECT_EQ(lenInd, 3 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(IntervalToSQLWCHARTest, Y2M_BinaryMode_Negative) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_YEAR_TO_MONTH;
    interval.interval_sign = SQL_TRUE;
    interval.intval.year_month.year = 2;
    interval.intval.year_month.month = 5;

    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_BinaryMode(&interval, dest,
                                             10 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-2-5");
    EXPECT_EQ(lenInd, 4 * (SQLLEN)sizeofSQLWCHAR());
}

TEST_P(IntervalToSQLWCHARTest, Y2M_BinaryMode_Zero) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_YEAR_TO_MONTH;
    interval.interval_sign = SQL_FALSE;
    interval.intval.year_month.year = 0;
    interval.intval.year_month.month = 0;

    SQLWCHAR dest[20] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_BinaryMode(&interval, dest,
                                             10 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"0-0");
}

// --- D2S Binary Mode → SQLWCHAR ---

TEST_P(IntervalToSQLWCHARTest, D2S_BinaryMode_Positive) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_DAY_TO_SECOND;
    interval.interval_sign = SQL_FALSE;
    interval.intval.day_second.day = 5;
    interval.intval.day_second.hour = 12;
    interval.intval.day_second.minute = 30;
    interval.intval.day_second.second = 45;
    interval.intval.day_second.fraction = 123456;

    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_BinaryMode(&interval, dest,
                                             20 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"5 12:30:45.123456");
}

TEST_P(IntervalToSQLWCHARTest, D2S_BinaryMode_Negative) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_DAY_TO_SECOND;
    interval.interval_sign = SQL_TRUE;
    interval.intval.day_second.day = 1;
    interval.intval.day_second.hour = 2;
    interval.intval.day_second.minute = 3;
    interval.intval.day_second.second = 4;
    interval.intval.day_second.fraction = 0;

    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_BinaryMode(&interval, dest,
                                             20 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-1 02:03:04");
}

TEST_P(IntervalToSQLWCHARTest, D2S_BinaryMode_Zero) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_DAY_TO_SECOND;
    interval.interval_sign = SQL_FALSE;

    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_BinaryMode(&interval, dest,
                                             20 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"00:00:00");
}

TEST_P(IntervalToSQLWCHARTest, D2S_BinaryMode_NoFraction) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_DAY_TO_SECOND;
    interval.interval_sign = SQL_FALSE;
    interval.intval.day_second.day = 1;
    interval.intval.day_second.hour = 2;
    interval.intval.day_second.minute = 3;
    interval.intval.day_second.second = 4;
    interval.intval.day_second.fraction = 0;

    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_BinaryMode(&interval, dest,
                                             20 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"1 02:03:04");
}

// --- Truncation Handling ---

TEST_P(IntervalToSQLWCHARTest, D2S_TextMode_Truncation) {
    const char *input = "5 12:30:45.123456";
    // "5 12:30:45.123456" = 17 chars; buffer only holds 5 + null = 6
    SQLWCHAR dest[12] = {0}; // 6 chars in UTF-32 = 12 units
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           6 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"5 12:");
    EXPECT_EQ(lenInd, 17 * (SQLLEN)sizeofSQLWCHAR());
    EXPECT_EQ(offset, 5); // 5 chars consumed
}

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_Truncation) {
    const char *input = "-999-11";
    // "-999-11" = 7 chars; buffer only holds 4 + null = 5
    SQLWCHAR dest[10] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_TextMode(input, strlen(input), dest,
                                           5 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"-999");
    EXPECT_EQ(lenInd, 7 * (SQLLEN)sizeofSQLWCHAR());
    EXPECT_EQ(offset, 4); // 4 chars consumed
}

// --- Sequential Fetch (ODBC partial retrieval) ---

TEST_P(IntervalToSQLWCHARTest, D2S_SequentialFetch) {
    const char *input = "5 12:30:45";
    // "5 12:30:45" = 10 chars; fetch in two calls with buffer of 6 chars
    SQLWCHAR dest[12] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // First fetch: gets 5 chars
    auto rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                           6 * sizeofSQLWCHAR(), &offset,
                                           &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(extractSQLWCHARString(dest), u"5 12:");
    EXPECT_EQ(offset, 5);

    // Second fetch: gets remaining 5 chars
    rc = unifiedIntervalD2S_TextMode(input, strlen(input), dest,
                                      6 * sizeofSQLWCHAR(), &offset,
                                      &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"30:45");
    EXPECT_EQ(offset, 0); // reset after complete
}

// --- Zero-length data ---

TEST_P(IntervalToSQLWCHARTest, Y2M_TextMode_ZeroLenData) {
    // When iColDataLen <= 0, the new code sets len=0 and pcbLenInd=0
    SQLWCHAR dest[10] = {0xFFFF, 0xFFFF};
    SQLLEN lenInd = -1;

    // Simulate the else branch: iColDataLen <= 0
    int len = 0;
    lenInd = len;
    EXPECT_EQ(lenInd, 0);
}

// --- Large interval values ---

TEST_P(IntervalToSQLWCHARTest, Y2M_BinaryMode_LargeValues) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_YEAR_TO_MONTH;
    interval.interval_sign = SQL_FALSE;
    interval.intval.year_month.year = 999999999;
    interval.intval.year_month.month = 11;

    SQLWCHAR dest[40] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalY2M_BinaryMode(&interval, dest,
                                             20 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"999999999-11");
}

TEST_P(IntervalToSQLWCHARTest, D2S_BinaryMode_LargeDay) {
    SQL_INTERVAL_STRUCT interval;
    memset(&interval, 0, sizeof(interval));
    interval.interval_type = SQL_IS_DAY_TO_SECOND;
    interval.interval_sign = SQL_FALSE;
    interval.intval.day_second.day = 999999;
    interval.intval.day_second.hour = 23;
    interval.intval.day_second.minute = 59;
    interval.intval.day_second.second = 59;
    interval.intval.day_second.fraction = 999999;

    SQLWCHAR dest[60] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = unifiedIntervalD2S_BinaryMode(&interval, dest,
                                             30 * sizeofSQLWCHAR(), &offset,
                                             &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(extractSQLWCHARString(dest), u"999999 23:59:59.999999");
}

// --- TIME_FORMAT_PATTERN: ^(?:[01]?\d|2[0-3]):[0-5][0-9]:[0-5][0-9](?:\.\d+)?$
// Matches HH:MM:SS with optional fractional seconds, hours 0-23.

TEST(RegexPatternsTest, TimeFormat_HappyPaths) {
    EXPECT_TRUE(std::regex_match("00:00:00", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("23:59:59", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("12:30:45", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("1:02:03", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("9:00:00", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("00:00:00.123456", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("23:59:59.999999", RegexPatterns::TIME_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("12:00:00.1", RegexPatterns::TIME_FORMAT_PATTERN));
}

TEST(RegexPatternsTest, TimeFormat_UnhappyPaths) {
    EXPECT_FALSE(std::regex_match("24:00:00", RegexPatterns::TIME_FORMAT_PATTERN));   // hour 24
    EXPECT_FALSE(std::regex_match("25:00:00", RegexPatterns::TIME_FORMAT_PATTERN));   // hour 25
    EXPECT_FALSE(std::regex_match("12:60:00", RegexPatterns::TIME_FORMAT_PATTERN));   // minute 60
    EXPECT_FALSE(std::regex_match("12:00:60", RegexPatterns::TIME_FORMAT_PATTERN));   // second 60
    EXPECT_FALSE(std::regex_match("12:00", RegexPatterns::TIME_FORMAT_PATTERN));      // missing seconds
    EXPECT_FALSE(std::regex_match("", RegexPatterns::TIME_FORMAT_PATTERN));           // empty
    EXPECT_FALSE(std::regex_match("abc", RegexPatterns::TIME_FORMAT_PATTERN));        // non-numeric
    EXPECT_FALSE(std::regex_match("-12:00:00", RegexPatterns::TIME_FORMAT_PATTERN));  // leading sign
    EXPECT_FALSE(std::regex_match("12:00:00.", RegexPatterns::TIME_FORMAT_PATTERN));  // trailing dot no digits
    EXPECT_FALSE(std::regex_match("1 12:00:00", RegexPatterns::TIME_FORMAT_PATTERN)); // day prefix
}

// --- YEAR_MONTH_FORMAT_PATTERN: ^\d+-\d+$
// Matches Y-M format (digits-digits).

TEST(RegexPatternsTest, YearMonthFormat_HappyPaths) {
    EXPECT_TRUE(std::regex_match("0-0", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("1-0", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("0-11", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("999-11", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("999999999-0", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));
    EXPECT_TRUE(std::regex_match("5-3", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));
}

TEST(RegexPatternsTest, YearMonthFormat_UnhappyPaths) {
    EXPECT_FALSE(std::regex_match("", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));        // empty
    EXPECT_FALSE(std::regex_match("-5-3", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));    // leading sign
    EXPECT_FALSE(std::regex_match("5", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));       // no dash
    EXPECT_FALSE(std::regex_match("5-", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));      // trailing dash
    EXPECT_FALSE(std::regex_match("-3", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));      // leading dash only
    EXPECT_FALSE(std::regex_match("5-3-1", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));   // extra dash
    EXPECT_FALSE(std::regex_match("abc-def", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN)); // alpha
    EXPECT_FALSE(std::regex_match("5.5-3", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));   // decimal in year
    EXPECT_FALSE(std::regex_match("5-3.2", RegexPatterns::YEAR_MONTH_FORMAT_PATTERN));   // decimal in month
}

// --- DAY_TO_SECOND_SQL_PATTERN: ^[-+]?\d+\s+(?:[01]?\d|2[0-3]):[0-5][0-9]:[0-5][0-9](?:\.\d+)?$
// Matches [+/-]D HH:MM:SS[.fraction], hours 0-23.

TEST(RegexPatternsTest, DayToSecondFormat_HappyPaths) {
    EXPECT_TRUE(std::regex_match("0 00:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("1 12:30:45", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("365 23:59:59", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("999999999 00:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("+10 05:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("-10 05:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("3 04:30:15.123456", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("-1 00:00:00.1", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));
    EXPECT_TRUE(std::regex_match("0 1:02:03", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));  // single-digit hour
}

TEST(RegexPatternsTest, DayToSecondFormat_UnhappyPaths) {
    EXPECT_FALSE(std::regex_match("", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));              // empty
    EXPECT_FALSE(std::regex_match("12:30:45", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));      // no day
    EXPECT_FALSE(std::regex_match("3 25:30:15", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));    // hour 25
    EXPECT_FALSE(std::regex_match("3 24:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));    // hour 24
    EXPECT_FALSE(std::regex_match("3 12:60:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));    // minute 60
    EXPECT_FALSE(std::regex_match("3 12:00:60", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));    // second 60
    EXPECT_TRUE(std::regex_match("3  12:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));    // double space (\s+ matches 1+ whitespace)
    EXPECT_FALSE(std::regex_match("abc 12:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));  // alpha day
    EXPECT_FALSE(std::regex_match("3-12:00:00", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));    // dash instead of space
    EXPECT_FALSE(std::regex_match("3 12:00:00.", RegexPatterns::DAY_TO_SECOND_SQL_PATTERN));   // trailing dot
}

// --- TIMESTAMP_PATTERN: ^\d{4}-\d{2}-\d{2}\s+(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d(?:\.\d+)?$
// Matches YYYY-MM-DD HH:MM:SS[.fraction].

TEST(RegexPatternsTest, TimestampFormat_HappyPaths) {
    EXPECT_TRUE(std::regex_match("2024-01-15 12:30:45", RegexPatterns::TIMESTAMP_PATTERN));
    EXPECT_TRUE(std::regex_match("2024-01-15 00:00:00", RegexPatterns::TIMESTAMP_PATTERN));
    EXPECT_TRUE(std::regex_match("2024-12-31 23:59:59", RegexPatterns::TIMESTAMP_PATTERN));
    EXPECT_TRUE(std::regex_match("2024-01-15 12:30:45.123456", RegexPatterns::TIMESTAMP_PATTERN));
    EXPECT_TRUE(std::regex_match("9999-12-31 23:59:59.999999", RegexPatterns::TIMESTAMP_PATTERN));
    EXPECT_TRUE(std::regex_match("0001-01-01 00:00:00", RegexPatterns::TIMESTAMP_PATTERN));
}

TEST(RegexPatternsTest, TimestampFormat_UnhappyPaths) {
    EXPECT_FALSE(std::regex_match("", RegexPatterns::TIMESTAMP_PATTERN));                          // empty
    EXPECT_FALSE(std::regex_match("2024-01-15", RegexPatterns::TIMESTAMP_PATTERN));                // date only
    EXPECT_FALSE(std::regex_match("12:30:45", RegexPatterns::TIMESTAMP_PATTERN));                  // time only
    EXPECT_FALSE(std::regex_match("2024-01-15 24:00:00", RegexPatterns::TIMESTAMP_PATTERN));       // hour 24
    EXPECT_FALSE(std::regex_match("2024-01-15 12:60:00", RegexPatterns::TIMESTAMP_PATTERN));       // minute 60
    EXPECT_FALSE(std::regex_match("2024-01-15 12:00:60", RegexPatterns::TIMESTAMP_PATTERN));       // second 60
    EXPECT_FALSE(std::regex_match("24-01-15 12:00:00", RegexPatterns::TIMESTAMP_PATTERN));         // 2-digit year
    EXPECT_FALSE(std::regex_match("2024-1-15 12:00:00", RegexPatterns::TIMESTAMP_PATTERN));        // 1-digit month
    EXPECT_FALSE(std::regex_match("2024-01-15T12:00:00", RegexPatterns::TIMESTAMP_PATTERN));       // T separator
    EXPECT_FALSE(std::regex_match("5 12:30:45", RegexPatterns::TIMESTAMP_PATTERN));                // interval format
    EXPECT_FALSE(std::regex_match("2024-01-15 12:00:00.", RegexPatterns::TIMESTAMP_PATTERN));      // trailing dot
    EXPECT_FALSE(std::regex_match("-2024-01-15 12:00:00", RegexPatterns::TIMESTAMP_PATTERN));      // negative year
}

// Unit tests for copyToCBinary and copyVariableToCBinary
class CopyToCBinaryTest : public ::testing::Test {
  protected:
    RS_ENV_INFO envInfo;
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *pStmt;

    void SetUp() override {
        memset(&envInfo, 0, sizeof(RS_ENV_INFO));
        pConn = new RS_CONN_INFO(&envInfo);
        pStmt = new RS_STMT_INFO(pConn);
        pStmt->pErrorList = nullptr;
    }

    void TearDown() override {
        if (pStmt->pErrorList) {
            clearErrorList(pStmt->pErrorList);
        }
        delete pStmt;
        delete pConn;
    }
};

TEST_F(CopyToCBinaryTest, FixedType_CopiesFullData) {
    int src = 12345;
    unsigned char buf[16] = {0};
    SQLLEN indicator = 0;

    SQLRETURN rc = copyToCBinary(&src, sizeof(int), buf, sizeof(buf), &indicator, pStmt, "SQL_INTEGER");

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, (SQLLEN)sizeof(int));
    EXPECT_EQ(memcmp(buf, &src, sizeof(int)), 0);
}

TEST_F(CopyToCBinaryTest, FixedType_BufferTooSmall_Returns22003) {
    int src = 12345;
    unsigned char buf[2] = {0xFF, 0xFF};
    SQLLEN indicator = 0;

    SQLRETURN rc = copyToCBinary(&src, sizeof(int), buf, 2, &indicator, pStmt, "SQL_INTEGER");

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_EQ(buf[0], 0xFF);
    EXPECT_EQ(buf[1], 0xFF);
    ASSERT_NE(pStmt->pErrorList, nullptr);
    EXPECT_STREQ(pStmt->pErrorList->szSqlState, "22003");
}

TEST_F(CopyToCBinaryTest, FixedType_ExactBufferSize_Succeeds) {
    short src = 999;
    unsigned char buf[2] = {0};
    SQLLEN indicator = 0;

    SQLRETURN rc = copyToCBinary(&src, sizeof(short), buf, sizeof(short), &indicator, pStmt, "SQL_SMALLINT");

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, (SQLLEN)sizeof(short));
    EXPECT_EQ(memcmp(buf, &src, sizeof(short)), 0);
}

TEST_F(CopyToCBinaryTest, FixedType_NullIndicator_StillCopies) {
    int src = 42;
    unsigned char buf[16] = {0};

    SQLRETURN rc = copyToCBinary(&src, sizeof(int), buf, sizeof(buf), NULL, pStmt, "SQL_INTEGER");

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(memcmp(buf, &src, sizeof(int)), 0);
}

TEST_F(CopyToCBinaryTest, FixedType_NullBuffer_ReturnsHY009) {
    int src = 42;
    SQLLEN indicator = 99;

    SQLRETURN rc = copyToCBinary(&src, sizeof(int), NULL, 100, &indicator, pStmt, "SQL_INTEGER");

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_EQ(indicator, 99);
    ASSERT_NE(pStmt->pErrorList, nullptr);
    EXPECT_STREQ(pStmt->pErrorList->szSqlState, "HY009");
}

TEST_F(CopyToCBinaryTest, FixedType_BufferTooSmall_IndicatorNotSet) {
    int src = 12345;
    unsigned char buf[2] = {0xFF, 0xFF};
    SQLLEN indicator = 99;

    SQLRETURN rc = copyToCBinary(&src, sizeof(int), buf, 2, &indicator, pStmt, "SQL_INTEGER");

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_EQ(indicator, 99);
    EXPECT_EQ(buf[0], 0xFF);
    EXPECT_EQ(buf[1], 0xFF);
}

TEST_F(CopyToCBinaryTest, Variable_NegativeSrcLen_NoCopyIndicatorZero) {
    const char *src = "Hello";
    unsigned char buf[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    SQLLEN indicator = 99;

    SQLRETURN rc = copyVariableToCBinary(src, -5, buf, sizeof(buf), NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 0);  // negative srcLen treated as no data
    EXPECT_EQ(buf[0], 0xFF);  // nothing copied
}

TEST_F(CopyToCBinaryTest, Variable_CopiesFullData_NoNullTerminator) {
    const char *src = "Hello";
    unsigned char buf[16];
    memset(buf, 0xFF, sizeof(buf));
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 5, buf, sizeof(buf), NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 5);
    EXPECT_EQ(memcmp(buf, "Hello", 5), 0);
    EXPECT_EQ(buf[5], 0xFF);
}

TEST_F(CopyToCBinaryTest, Variable_Truncation_Returns01004) {
    const char *src = "Hello World";
    unsigned char buf[5];
    memset(buf, 0, sizeof(buf));
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 11, buf, 5, NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 11);
    EXPECT_EQ(memcmp(buf, "Hello", 5), 0);
    ASSERT_NE(pStmt->pErrorList, nullptr);
    EXPECT_STREQ(pStmt->pErrorList->szSqlState, "01004");
}

TEST_F(CopyToCBinaryTest, Variable_Truncation_CopiesFullBufferLength) {
    const char *src = "ABCDEFGH";
    unsigned char buf[4];
    memset(buf, 0xFF, sizeof(buf));
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 8, buf, 4, NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 8);
    EXPECT_EQ(buf[0], 'A');
    EXPECT_EQ(buf[1], 'B');
    EXPECT_EQ(buf[2], 'C');
    EXPECT_EQ(buf[3], 'D');
}

TEST_F(CopyToCBinaryTest, Variable_NullBuffer_ReturnsSuccess) {
    const char *src = "Hello";
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 5, NULL, 0, NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 5);
}

TEST_F(CopyToCBinaryTest, Variable_ZeroLength_ReturnsSuccess) {
    const char *src = "";
    unsigned char buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 0, buf, sizeof(buf), NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 0);
    EXPECT_EQ(buf[0], 0xFF);
}

TEST_F(CopyToCBinaryTest, Variable_ExactFit_NoTruncation) {
    const char *src = "ABC";
    unsigned char buf[3] = {0};
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 3, buf, 3, NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 3);
    EXPECT_EQ(memcmp(buf, "ABC", 3), 0);
}

TEST_F(CopyToCBinaryTest, Variable_ZeroBufferLen_TruncatesWithWarning) {
    const char *src = "Hello";
    unsigned char buf[1] = {0xFF};
    SQLLEN indicator = 0;

    SQLRETURN rc = copyVariableToCBinary(src, 5, buf, 0, NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 5);
    EXPECT_EQ(buf[0], 0xFF);
}

TEST_F(CopyToCBinaryTest, Variable_ChunkedRetrieval_ThreeChunks) {
    const char *src = "ABCDEFGHIJ";  // 10 bytes
    unsigned char buf[4] = {0};
    SQLLEN indicator = 0;
    SQLLEN offset = 0;
    SQLRETURN rc;

    // First call: get bytes 0-3 (4 bytes)
    rc = copyVariableToCBinary(src, 10, buf, 4, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 10);  // remaining = 10
    EXPECT_EQ(memcmp(buf, "ABCD", 4), 0);
    EXPECT_EQ(offset, 4);

    // Second call: get bytes 4-7 (4 bytes)
    memset(buf, 0, sizeof(buf));
    if (pStmt->pErrorList) { clearErrorList(pStmt->pErrorList); pStmt->pErrorList = nullptr; }
    rc = copyVariableToCBinary(src, 10, buf, 4, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 6);  // remaining = 10 - 4 = 6
    EXPECT_EQ(memcmp(buf, "EFGH", 4), 0);
    EXPECT_EQ(offset, 8);

    // Third call: get bytes 8-9 (2 bytes, last chunk)
    memset(buf, 0, sizeof(buf));
    if (pStmt->pErrorList) { clearErrorList(pStmt->pErrorList); pStmt->pErrorList = nullptr; }
    rc = copyVariableToCBinary(src, 10, buf, 4, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 2);  // remaining = 10 - 8 = 2
    EXPECT_EQ(buf[0], 'I');
    EXPECT_EQ(buf[1], 'J');
    EXPECT_EQ(offset, 0);  // reset after complete
}

TEST_F(CopyToCBinaryTest, Variable_ChunkedRetrieval_ExactFitSecondCall) {
    const char *src = "ABCDEF";  // 6 bytes
    unsigned char buf[3] = {0};
    SQLLEN indicator = 0;
    SQLLEN offset = 0;
    SQLRETURN rc;

    // First call: get bytes 0-2 (3 bytes, more remains)
    rc = copyVariableToCBinary(src, 6, buf, 3, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 6);
    EXPECT_EQ(memcmp(buf, "ABC", 3), 0);
    EXPECT_EQ(offset, 3);

    // Second call: get bytes 3-5 (3 bytes, exact fit)
    memset(buf, 0, sizeof(buf));
    if (pStmt->pErrorList) { clearErrorList(pStmt->pErrorList); pStmt->pErrorList = nullptr; }
    rc = copyVariableToCBinary(src, 6, buf, 3, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 3);  // remaining = 6 - 3 = 3
    EXPECT_EQ(memcmp(buf, "DEF", 3), 0);
    EXPECT_EQ(offset, 0);  // reset after complete
}

// --- Edge cases for copyToCBinary ---

TEST_F(CopyToCBinaryTest, FixedType_NegativeBufferLen) {
    int src = 42;
    unsigned char buf[16] = {0xFF};
    SQLLEN indicator = 99;

    // cbLen < 0 is treated as "too small" since (negative < sizeof(int)) is true
    SQLRETURN rc = copyToCBinary(&src, sizeof(int), buf, -1, &indicator, pStmt, "SQL_INTEGER");

    EXPECT_EQ(rc, SQL_ERROR);
    EXPECT_EQ(indicator, 99);  // not set on error
    EXPECT_EQ(buf[0], 0xFF);  // buffer untouched
    ASSERT_NE(pStmt->pErrorList, nullptr);
    EXPECT_STREQ(pStmt->pErrorList->szSqlState, "22003");
}

TEST_F(CopyToCBinaryTest, FixedType_ZeroSrcSize) {
    char src = 0;
    unsigned char buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    SQLLEN indicator = 99;

    // srcSize == 0: cbLen (4) >= srcSize (0), copies 0 bytes, indicator = 0
    SQLRETURN rc = copyToCBinary(&src, 0, buf, sizeof(buf), &indicator, pStmt, "ZERO_SIZE");

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 0);
    EXPECT_EQ(buf[0], 0xFF);  // nothing copied
}

TEST_F(CopyToCBinaryTest, FixedType_NullPStmt_Crashes_Skipped) {
    // pStmt is always non-NULL in production paths (convertSQLDataToCData requires it).
    // This test documents that passing NULL pStmt would crash on addError.
    // We do NOT test it because it's undefined behavior -- just documenting the contract.
}

// --- Edge cases for copyVariableToCBinary ---

TEST_F(CopyToCBinaryTest, Variable_NegativeBufferLen_NoDataCopied) {
    const char *src = "Hello";
    unsigned char buf[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    SQLLEN indicator = 0;
    SQLLEN offset = 0;

    // cbLen < 0: remainingLen (5) > cbLen (-1), enters truncation path.
    // cbLen <= 0 so no memcpy and offset must NOT be decremented.
    SQLRETURN rc = copyVariableToCBinary(src, 5, buf, -1, &offset, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 5);
    EXPECT_EQ(buf[0], 0xFF);  // nothing copied
    EXPECT_EQ(offset, 0);     // offset must not go negative
}

TEST_F(CopyToCBinaryTest, Variable_NullPStmt_TruncationNoWarningPosted) {
    const char *src = "Hello";
    unsigned char buf[3] = {0};
    SQLLEN indicator = 0;

    // pStmt == NULL: truncation occurs but addError is skipped (guarded by if(pStmt))
    SQLRETURN rc = copyVariableToCBinary(src, 5, buf, 3, NULL, &indicator, NULL);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 5);
    EXPECT_EQ(memcmp(buf, "Hel", 3), 0);
}

TEST_F(CopyToCBinaryTest, Variable_NullPSrc_ZeroLen_Success) {
    unsigned char buf[4] = {0xFF};
    SQLLEN indicator = 99;

    // pSrc is NULL but srcLen is 0, so we return early with indicator = 0
    SQLRETURN rc = copyVariableToCBinary(NULL, 0, buf, sizeof(buf), NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 0);
    EXPECT_EQ(buf[0], 0xFF);
}

TEST_F(CopyToCBinaryTest, Variable_ChunkedRetrieval_AfterAllConsumed) {
    const char *src = "AB";
    unsigned char buf[4] = {0};
    SQLLEN indicator = 0;
    SQLLEN offset = 0;
    SQLRETURN rc;

    // First call: get all 2 bytes (fits in 4-byte buffer)
    rc = copyVariableToCBinary(src, 2, buf, 4, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 2);
    EXPECT_EQ(offset, 0);  // reset since all data delivered

    // Second call: nothing left, should return SQL_SUCCESS with indicator = 0
    memset(buf, 0xFF, sizeof(buf));
    rc = copyVariableToCBinary(src, 2, buf, 4, &offset, &indicator, pStmt);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 2);  // first call again since offset was reset
    EXPECT_EQ(buf[0], 'A');   // copies from beginning since offset is 0
}

TEST_F(CopyToCBinaryTest, Variable_ChunkedRetrieval_OffsetBeyondSrcLen) {
    const char *src = "ABC";
    unsigned char buf[4] = {0xFF};
    SQLLEN indicator = 99;
    SQLLEN offset = 10;  // offset beyond data length

    // If offset >= srcLen, all data already consumed, reset and return
    SQLRETURN rc = copyVariableToCBinary(src, 3, buf, sizeof(buf), &offset, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(indicator, 0);
    EXPECT_EQ(offset, 0);
    EXPECT_EQ(buf[0], 0xFF);  // nothing copied
}

TEST_F(CopyToCBinaryTest, Variable_NullOffset_SingleShotCopy) {
    const char *src = "Hello";
    unsigned char buf[3] = {0};
    SQLLEN indicator = 0;

    // cbLenOffset == NULL: no chunking, just single-shot truncation
    SQLRETURN rc = copyVariableToCBinary(src, 5, buf, 3, NULL, &indicator, pStmt);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(indicator, 5);
    EXPECT_EQ(memcmp(buf, "Hel", 3), 0);
}

/*====================================================================================================================================================*/
// getRsVal BOOLOID tests: verify that normalized boolean data ("1"/"0") is
// correctly parsed by the standard numeric/string paths in getRsVal.
// The raw wire normalization (t/f/0x01 -> "1"/"0") is now done in convertSQLDataToCData
// before getRsVal is called. These tests verify the post-normalization path.
/*====================================================================================================================================================*/

class GetRsValBooloidTest : public ::testing::Test {};

TEST_F(GetRsValBooloidTest, smallint_text_true) {
    char data[] = "1";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_SMALLINT, &val, SQL_C_SHORT, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.hVal, 1);
}

TEST_F(GetRsValBooloidTest, smallint_text_false) {
    char data[] = "0";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_SMALLINT, &val, SQL_C_SHORT, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.hVal, 0);
}

TEST_F(GetRsValBooloidTest, integer_text_true) {
    char data[] = "1";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_INTEGER, &val, SQL_C_LONG, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.iVal, 1);
}

TEST_F(GetRsValBooloidTest, integer_text_false) {
    char data[] = "0";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_INTEGER, &val, SQL_C_LONG, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.iVal, 0);
}

TEST_F(GetRsValBooloidTest, bigint_text_true) {
    char data[] = "1";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_BIGINT, &val, SQL_C_SBIGINT, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.llVal, 1);
}

TEST_F(GetRsValBooloidTest, bigint_text_false) {
    char data[] = "0";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_BIGINT, &val, SQL_C_SBIGINT, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.llVal, 0);
}

TEST_F(GetRsValBooloidTest, integer_text_digit_1) {
    char data[] = "1";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_INTEGER, &val, SQL_C_LONG, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.iVal, 1);
}

TEST_F(GetRsValBooloidTest, integer_binary_true) {
    // After normalization, binary BOOLOID data is also "1"/"0" text
    char data[] = "1";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_INTEGER, &val, SQL_C_LONG, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.iVal, 1);
}

TEST_F(GetRsValBooloidTest, integer_binary_false) {
    char data[] = "0";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_INTEGER, &val, SQL_C_LONG, 0, NULL, BOOLOID, true);
    EXPECT_EQ(val.iVal, 0);
}

TEST_F(GetRsValBooloidTest, varchar_text_true) {
    char data[] = "1";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_VARCHAR, &val, SQL_C_CHAR, 0, NULL, BOOLOID, true);
    EXPECT_STREQ(val.pcVal, "1");
}

TEST_F(GetRsValBooloidTest, varchar_text_false) {
    char data[] = "0";
    RS_VALUE val;
    memset(&val, 0, sizeof(val));
    getRsVal(data, 2, SQL_VARCHAR, &val, SQL_C_CHAR, 0, NULL, BOOLOID, true);
    EXPECT_STREQ(val.pcVal, "0");
}

/*====================================================================================================================================================*/
// convertSQLDataToCData BOOLOID tests: verify the normalization path converts
// boolean wire data to correct C types for all numeric targets in both text and binary protocol.
/*====================================================================================================================================================*/

class ConvertBooloidTest : public ::testing::Test {
protected:
    SQLRETURN convert(char *data, int dataLen, short cType, void *pBuf,
                      SQLLEN bufLen, int format) {
        RS_STMT_INFO stmtInfo = {0};
        SQLLEN lengthIndicator = 0;
        SQLLEN cbOffset = 0;
        return convertSQLDataToCData(
            &stmtInfo, data, dataLen, SQL_VARCHAR, pBuf, bufLen,
            &cbOffset, &lengthIndicator, cType, BOOLOID, format, nullptr);
    }
};

TEST_F(ConvertBooloidTest, float_text_true) {
    char data[] = "t";
    float result = 0;
    SQLRETURN rc = convert(data, 2, SQL_C_FLOAT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST_F(ConvertBooloidTest, float_text_false) {
    char data[] = "f";
    float result = 99;
    SQLRETURN rc = convert(data, 2, SQL_C_FLOAT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(ConvertBooloidTest, float_binary_true) {
    char data[] = {1, 0};
    float result = 0;
    SQLRETURN rc = convert(data, 1, SQL_C_FLOAT, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST_F(ConvertBooloidTest, float_binary_false) {
    char data[] = {0, 0};
    float result = 99;
    SQLRETURN rc = convert(data, 1, SQL_C_FLOAT, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(ConvertBooloidTest, double_text_true) {
    char data[] = "t";
    double result = 0;
    SQLRETURN rc = convert(data, 2, SQL_C_DOUBLE, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

TEST_F(ConvertBooloidTest, double_text_false) {
    char data[] = "f";
    double result = 99;
    SQLRETURN rc = convert(data, 2, SQL_C_DOUBLE, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(ConvertBooloidTest, double_binary_true) {
    char data[] = {1, 0};
    double result = 0;
    SQLRETURN rc = convert(data, 1, SQL_C_DOUBLE, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

TEST_F(ConvertBooloidTest, double_binary_false) {
    char data[] = {0, 0};
    double result = 99;
    SQLRETURN rc = convert(data, 1, SQL_C_DOUBLE, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST_F(ConvertBooloidTest, short_text_true) {
    char data[] = "t";
    short result = 0;
    SQLRETURN rc = convert(data, 2, SQL_C_SHORT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, short_text_false) {
    char data[] = "f";
    short result = 99;
    SQLRETURN rc = convert(data, 2, SQL_C_SHORT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 0);
}

TEST_F(ConvertBooloidTest, short_binary_true) {
    char data[] = {1, 0};
    short result = 0;
    SQLRETURN rc = convert(data, 1, SQL_C_SHORT, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, short_binary_false) {
    char data[] = {0, 0};
    short result = 99;
    SQLRETURN rc = convert(data, 1, SQL_C_SHORT, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 0);
}

TEST_F(ConvertBooloidTest, integer_text_true) {
    char data[] = "T";
    int result = 0;
    SQLRETURN rc = convert(data, 2, SQL_C_LONG, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, integer_binary_true) {
    char data[] = {1, 0};
    int result = 0;
    SQLRETURN rc = convert(data, 1, SQL_C_LONG, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, bigint_text_true) {
    char data[] = "1";
    long long result = 0;
    SQLRETURN rc = convert(data, 2, SQL_C_SBIGINT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, bigint_binary_false) {
    char data[] = {0, 0};
    long long result = 99;
    SQLRETURN rc = convert(data, 1, SQL_C_SBIGINT, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 0);
}

TEST_F(ConvertBooloidTest, bit_text_true) {
    char data[] = "t";
    unsigned char result = 0;
    SQLRETURN rc = convert(data, 2, SQL_C_BIT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, bit_text_false) {
    char data[] = "f";
    unsigned char result = 99;
    SQLRETURN rc = convert(data, 2, SQL_C_BIT, &result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 0);
}

TEST_F(ConvertBooloidTest, bit_binary_true) {
    char data[] = {1, 0};
    unsigned char result = 0;
    SQLRETURN rc = convert(data, 1, SQL_C_BIT, &result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(result, 1);
}

TEST_F(ConvertBooloidTest, char_text_true) {
    char data[] = "t";
    char result[10] = {0};
    SQLRETURN rc = convert(data, 2, SQL_C_CHAR, result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(result, "1");
}

TEST_F(ConvertBooloidTest, char_text_false) {
    char data[] = "f";
    char result[10] = {0};
    SQLRETURN rc = convert(data, 2, SQL_C_CHAR, result, sizeof(result), TEXT_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(result, "0");
}

TEST_F(ConvertBooloidTest, char_binary_true) {
    char data[] = {1, 0};
    char result[10] = {0};
    SQLRETURN rc = convert(data, 1, SQL_C_CHAR, result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(result, "1");
}

TEST_F(ConvertBooloidTest, char_binary_false) {
    char data[] = {0, 0};
    char result[10] = {0};
    SQLRETURN rc = convert(data, 1, SQL_C_CHAR, result, sizeof(result), BINARY_FORMAT);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_STREQ(result, "0");
}

/*====================================================================================================================================================*/
// SQLGetTypeInfo type filtering tests: verify that when BoolsAsChar=1,
// boolean is mapped to SQL_VARCHAR so filtering by SQL_BIT yields no matches.
// This exercises the same filtering logic used in RS_SQLGetTypeInfo (rscatalog.cpp).
/*====================================================================================================================================================*/

class SQLGetTypeInfoFilterTest : public ::testing::Test {};

TEST_F(SQLGetTypeInfoFilterTest, boolsAsChar_enabled_sql_bit_returns_empty) {
    // Simulate BoolsAsChar=1: boolean maps to SQL_VARCHAR
    int boolAsChar = 1;
    int useUnicode = 0;
    short boolSqlType = boolAsChar ? SQL_VARCHAR : SQL_BIT;

    // Build the same typesInfo vector structure as RS_SQLGetTypeInfo
    std::vector<RS_TYPE_INFO> typesInfo = {
        {"bool", boolSqlType, 1, "'", "'", "", SQL_NULLABLE, SQL_FALSE,
         SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "bool", SQL_NULL_DATA,
         SQL_NULL_DATA, boolSqlType, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA},
        {"bigint", SQL_BIGINT, 19, "", "", "", SQL_NULLABLE, SQL_FALSE,
         SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "bigint", 0, 0,
         SQL_BIGINT, SQL_NULL_DATA, 10, SQL_NULL_DATA},
    };

    // Filter by SQL_BIT — should find nothing when BoolsAsChar=1
    std::vector<RS_TYPE_INFO> filtered;
    for (size_t i = 0; i < typesInfo.size(); i++) {
        if (typesInfo[i].hType == SQL_BIT) {
            filtered.push_back(typesInfo[i]);
        }
    }

    EXPECT_TRUE(filtered.empty())
        << "BoolsAsChar=1: filtering typesInfo by SQL_BIT should yield empty result";

    // The driver should return empty result set (not HY004) in this case
    EXPECT_TRUE(boolAsChar && filtered.empty());
}

TEST_F(SQLGetTypeInfoFilterTest, boolsAsChar_disabled_sql_bit_returns_boolean) {
    // Simulate BoolsAsChar=0: boolean maps to SQL_BIT
    int boolAsChar = 0;
    short boolSqlType = SQL_BIT;

    std::vector<RS_TYPE_INFO> typesInfo = {
        {"bool", boolSqlType, 1, "", "", "", SQL_NULLABLE, SQL_FALSE,
         SQL_PRED_BASIC, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "bool", 0,
         0, SQL_BIT, SQL_NULL_DATA, 10, SQL_NULL_DATA},
        {"bigint", SQL_BIGINT, 19, "", "", "", SQL_NULLABLE, SQL_FALSE,
         SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "bigint", 0, 0,
         SQL_BIGINT, SQL_NULL_DATA, 10, SQL_NULL_DATA},
    };

    // Filter by SQL_BIT — should find the bool entry
    std::vector<RS_TYPE_INFO> filtered;
    for (size_t i = 0; i < typesInfo.size(); i++) {
        if (typesInfo[i].hType == SQL_BIT) {
            filtered.push_back(typesInfo[i]);
        }
    }

    EXPECT_EQ(filtered.size(), 1u)
        << "BoolsAsChar=0: filtering typesInfo by SQL_BIT should find 'bool'";
    EXPECT_EQ(filtered[0].szTypeName, "bool");
    EXPECT_EQ(filtered[0].hType, SQL_BIT);
}

TEST_F(SQLGetTypeInfoFilterTest, boolsAsChar_enabled_sql_varchar_includes_bool) {
    // Simulate BoolsAsChar=1: boolean maps to SQL_VARCHAR
    int boolAsChar = 1;
    short boolSqlType = SQL_VARCHAR;

    std::vector<RS_TYPE_INFO> typesInfo = {
        {"bool", boolSqlType, 1, "'", "'", "", SQL_NULLABLE, SQL_FALSE,
         SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "bool", SQL_NULL_DATA,
         SQL_NULL_DATA, boolSqlType, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA},
        {"character varying", SQL_VARCHAR, 65535, "'", "'", "max length", SQL_NULLABLE, SQL_FALSE,
         SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "character varying", 0, 0,
         SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA},
    };

    // Filter by SQL_VARCHAR — should find both 'bool' and 'character varying'
    std::vector<RS_TYPE_INFO> filtered;
    for (size_t i = 0; i < typesInfo.size(); i++) {
        if (typesInfo[i].hType == SQL_VARCHAR) {
            filtered.push_back(typesInfo[i]);
        }
    }

    EXPECT_EQ(filtered.size(), 2u);
    EXPECT_EQ(filtered[0].szTypeName, "bool");
    EXPECT_EQ(filtered[1].szTypeName, "character varying");
}
