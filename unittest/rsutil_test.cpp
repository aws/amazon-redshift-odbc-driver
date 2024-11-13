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