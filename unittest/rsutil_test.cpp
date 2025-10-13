#include "common.h"
#include "rsutil.h"
#include <sstream>
#include <string>

// Test printHexSQLCHAR and printHexSQLWCHR
class PrintHexSQLCharAndWCharTest : public ::testing::Test {
  protected:
    std::stringstream logStream;

    void SetUp() override {
        // Redirect the logging function to write to our logStream
        logFunc = [this](const std::string &message) {
            logStream << message << std::endl;
        };
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

TEST_F(PrintHexSQLCharAndWCharTest, PrintsHexBytesForSQLWCHAR) {
    // Prepare the input data
    SQLWCHAR sqlwchr[] = {0x0101, 0x2323, 0x4545, 0x6767,
                          0x8989, 0xABAB, 0xCDCD, 0xEFEF};
    int len = sizeof(sqlwchr) / sizeof(SQLWCHAR);

    // Call the function under test
    printHexSQLWCHR(sqlwchr, len, logFunc);

    // Verify the output
    std::string logOutput = logStream.str();
    for (auto &str : {"Printing SQLWCHAR* as hex bytes:", "Hex bytes:",
                      "01 01 23 23 45 45 67 67 ", "89 89 AB AB CD CD EF EF "}) {
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

// Tests that copyWStrDataBigLen correctly handles zero-length input strings
TEST(CopyWStrDataBigLen, ZeroLengthInput) {
    char src[] = ""; // 0-length UTF-8
    WCHAR dest[2] = {L'X', L'X'};
    SQLLEN lenInd = -1;
    SQLLEN offset = 1;

    auto rc = copyWStrDataBigLen(nullptr, src, 0, dest, sizeof(dest), &offset,
                                 &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], L'\0');
    EXPECT_EQ(lenInd, 0);
    EXPECT_EQ(offset, 0);
}

// Tests that copyWStrDataBigLen correctly handles null input with SQL_NULL_DATA
TEST(CopyWStrDataBigLen, NullInput) {
    WCHAR dest[2] = {L'X', L'X'};
    SQLLEN lenInd = -1;
    SQLLEN offset = 1;

    auto rc = copyWStrDataBigLen(nullptr, nullptr, SQL_NULL_DATA, dest,
                                 sizeof(dest), &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(dest[0], L'X'); // should be untouched
    EXPECT_EQ(dest[1], L'X'); // should be untouched
    EXPECT_EQ(lenInd, SQL_NULL_DATA);
    EXPECT_EQ(offset, 0);
}

// Tests that copyWStrDataBigLen correctly copies a string that completely fits
// in the destination buffer
TEST(CopyWStrDataBigLen, FullStringFitsInBuffer) {
    const char *src = "abc";
    WCHAR dest[10] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"abc");
    EXPECT_EQ(dest[3], L'\0');  // Explicitly check for null terminator
    EXPECT_EQ(lenInd, 3 * sizeof(WCHAR));
    EXPECT_EQ(offset, 0); // reset
}

// Tests that copyWStrDataBigLen correctly handles truncation when the string is
// larger than the destination buffer
TEST(CopyWStrDataBigLen, StringTruncated) {
    const char *src = "abcdef";
    WCHAR dest[4] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // dest can hold only 3 + 1 null = 4 WCHARs
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"abc"); // truncated
    EXPECT_EQ(dest[3], L'\0');
    EXPECT_EQ(lenInd, 6 * sizeof(WCHAR));                // full length
    EXPECT_EQ(offset, 3);                                // updated
}

// Tests that copyWStrDataBigLen correctly handles sequential fetching of
// remaining data after truncation
TEST(CopyWStrDataBigLen, SequentialFetchRemainder) {
    const char *src = "abcdef";
    WCHAR dest[4] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 3; // continue from previous test

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"def");
    EXPECT_EQ(dest[3], L'\0');
    EXPECT_EQ(offset, 0); // reset after full
}

// Tests that copyWStrDataBigLen correctly handles cases where the buffer can
// only hold the null terminator
TEST(CopyWStrDataBigLen, BufferTooSmallForData) {
    const char *src = "abc";
    WCHAR dest[1] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // Only space for null terminator
    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(WCHAR),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(dest[0], L'\0');
    EXPECT_EQ(offset, 0);                             // offset unchanged
}

// Tests that copyWStrDataBigLen correctly sets rc when no destination buffer is
// provided
TEST(CopyWStrDataBigLen, NoDestinationBuffer) {
    const char *src = "abc";
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyWStrDataBigLen(nullptr, src, SQL_NTS, nullptr, 0, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_ERROR);
}

// Tests that copyWStrDataBigLen correctly sets rc when destination buffer is
// negative
TEST(CopyWStrDataBigLen, NegativeBufferLength) {
    const char *src = "abc";
    WCHAR dest[1] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc =
        copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, -1, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_ERROR);
}

// Tests that copyWStrDataBigLen correctly handles cases where the string
// exactly fits in the destination buffer
TEST(CopyWStrDataBigLen, ExactFitBuffer) {
    const char *src = "abc";
    WCHAR dest[4] = {0}; // 3 chars + null
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, SQL_NTS, dest, sizeof(dest),
                                 &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"abc");
    EXPECT_EQ(dest[3], L'\0');
    EXPECT_EQ(lenInd, 3 * sizeof(WCHAR));
}

// Tests that copyWStrDataBigLen correctly handles explicit length parameter
// instead of SQL_NTS
TEST(CopyWStrDataBigLen, ExplicitLengthInput) {
    const char *src = "abcdef";
    int len = 3; // Only "abc"
    WCHAR dest[5] = {0};
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, len, dest, sizeof(dest), &offset,
                                 &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"abc");
    EXPECT_EQ(dest[3], L'\0');
    EXPECT_EQ(lenInd, 3 * sizeof(WCHAR));
}

// Tests copyWStrDataBigLen with non-null terminated input string
TEST(CopyWStrDataBigLen, NonNullTerminatedInput) {
    const char src[5] = {'H', 'e', 'l', 'l', 'o'}; // No null terminator
    int length = 3;
    WCHAR dest[5] = {0}; // 3 chars + null
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    auto rc = copyWStrDataBigLen(nullptr, src, 5, dest, 6, &offset, &lenInd);

    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"He");
    EXPECT_EQ(dest[2], L'\0');
    EXPECT_EQ(lenInd, 10);

    rc = copyWStrDataBigLen(nullptr, src, 5, dest, 6, &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"ll");
    EXPECT_EQ(dest[2], L'\0');
    EXPECT_EQ(lenInd, 6);

    rc = copyWStrDataBigLen(nullptr, src, 5, dest, 6, &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(std::u16string((char16_t *)dest), u"o");
    EXPECT_EQ(dest[1], L'\0');
    EXPECT_EQ(lenInd, 2);
}

// Tests copyWStrDataBigLen with wide characters (emoji, special symbols)
TEST(CopyWStrDataBigLen, WideCharacters) {
    // String with emoji and special characters using std::string
    std::string src =
        "I am smiling 🙂©👀"; // 24 characters: I am smiling : 13 + 🙂: 4 + ©: 2
                              // + 👀: 4 + null character: 1 = 24 bytes
    WCHAR dest[5] = {0}; // Small buffer to force multiple chunks
    SQLLEN lenInd = -1;
    SQLLEN offset = 0;

    // First call - should get partial string
    auto rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest,
                                 sizeof(dest), &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, 36); // Number of wide bytes: I am smiling : 13 + 🙂: 2 +
                           // ©: 1 + 👀: 2 = 18 wide chars * 2 = 36 bytes
    EXPECT_EQ(dest[4], L'\0');
    EXPECT_EQ(offset, 4); // 4 characters read

    rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, sizeof(dest),
                            &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, 28);
    EXPECT_EQ(dest[4], L'\0');
    EXPECT_EQ(offset, 8); // 4 more characters read

    rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, sizeof(dest),
                            &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, 20);
    EXPECT_EQ(dest[4], L'\0');
    EXPECT_EQ(offset, 12); // 4 more characters read

    rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, sizeof(dest),
                            &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(lenInd, 12);
    EXPECT_EQ(dest[4], L'\0');
    EXPECT_EQ(offset, 16); // 4 more characters read

    // Last call to read the remaining data
    rc = copyWStrDataBigLen(nullptr, src.data(), src.size(), dest, sizeof(dest),
                            &offset, &lenInd);
    EXPECT_EQ(rc, SQL_SUCCESS);
    EXPECT_EQ(lenInd, 4);
    EXPECT_EQ(dest[2], L'\0');
    EXPECT_EQ(offset, 0); // Offset should be reset after completion
}

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

// Test copyStrDataLargeLen function behavior with NULL pDest and non-NULL pcbLen
TEST(copyStrDataLargeLen, test_copyStrDataLargeLen_null_pDest_with_pcbLen) {
    const char* testStr = "test";
    SQLINTEGER pcbLen = 0;
    
    SQLRETURN rc = copyStrDataLargeLen(testStr, SQL_NTS, NULL, 0, &pcbLen);
    
    EXPECT_EQ(rc, SQL_SUCCESS_WITH_INFO);
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
