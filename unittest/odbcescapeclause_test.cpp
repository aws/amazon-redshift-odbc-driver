#define NOMINMAX
#include "common.h"
#include "rsescapeclause.h"
#include "rsutil.h"
#include <algorithm>
#include <sstream>
#include <string>

// Helper functions to reduce verbosity
namespace {
inline bool parseFunctionArguments(const std::string &sql, size_t &i,
                                   std::vector<std::string> &out,
                                   RS_STMT_INFO *pStmt, int numOfParamMarkers,
                                   int &paramNumber, int depth) {
    return ODBCEscapeClauseProcessor::parseFunctionArguments(
        sql, i, out, pStmt, numOfParamMarkers, paramNumber, depth);
}

inline const char *lookupSqlType(const std::string &odbcTypeName) {
    return ODBCEscapeClauseProcessor::lookupSqlType(odbcTypeName);
}

inline const char *lookupIntervalToken(const std::string &odbcIntervalToken) {
    return ODBCEscapeClauseProcessor::lookupIntervalToken(odbcIntervalToken);
}

inline unsigned char *
checkReplaceParamMarkerAndODBCEscapeClause(RS_STMT_INFO *pStmt, char *pData,
                                           size_t cbLen, RS_STR_BUF *pPaStrBuf,
                                           int iReplaceParamMarker) {
    return ODBCEscapeClauseProcessor::
        checkReplaceParamMarkerAndODBCEscapeClause(
            pStmt, pData, cbLen, pPaStrBuf, iReplaceParamMarker);
}
} // namespace

// Unit tests for parseFunctionArguments

// Test structure for basic parsing scenarios
struct ParseFunctionTestCase {
    std::string name;
    std::string sql;
    size_t startPos;
    bool expectedSuccess;
    std::vector<std::string> expectedArgs;
    size_t expectedEndPos;
};

class ParseFunctionArgumentsTest
    : public ::testing::TestWithParam<ParseFunctionTestCase> {};

TEST_P(ParseFunctionArgumentsTest, BasicParsing) {
    auto testCase = GetParam();
    size_t pos = testCase.startPos;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result = parseFunctionArguments(testCase.sql, pos, args, nullptr, 0,
                                         paramNumber, 0);

    EXPECT_EQ(result, testCase.expectedSuccess) << "Test: " << testCase.name;
    if (testCase.expectedSuccess) {
        EXPECT_EQ(args.size(), testCase.expectedArgs.size())
            << "Test: " << testCase.name;
        for (size_t i = 0;
             i < std::min(args.size(), testCase.expectedArgs.size()); ++i) {
            EXPECT_EQ(args[i], testCase.expectedArgs[i])
                << "Test: " << testCase.name << " - Arg " << i;
        }
        if (testCase.expectedEndPos > 0) {
            EXPECT_EQ(pos, testCase.expectedEndPos)
                << "Test: " << testCase.name;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    BasicScenarios, ParseFunctionArgumentsTest,
    ::testing::Values(
        ParseFunctionTestCase{"EmptyArgList", "FUNCTION()", 8, true, {}, 10},
        ParseFunctionTestCase{
            "SingleArg", "FUNCTION(arg1)", 8, true, {"arg1"}, 14},
        ParseFunctionTestCase{"MultipleArgs",
                              "FUNCTION(arg1, arg2, arg3)",
                              8,
                              true,
                              {"arg1", "arg2", "arg3"},
                              26},
        ParseFunctionTestCase{"ArgsWithSpaces",
                              "FUNCTION(  arg1  ,  arg2  )",
                              8,
                              true,
                              {"arg1", "arg2"},
                              27},
        ParseFunctionTestCase{"EmptyArgRemoved",
                              "FUNCTION(arg1, , arg3)",
                              8,
                              true,
                              {"arg1", "arg3"},
                              22},
        ParseFunctionTestCase{
            "WhitespaceBeforeParen", "FUNCTION  (arg1)", 8, true, {"arg1"}, 16},
        ParseFunctionTestCase{"TrailingWhitespace",
                              "FUNCTION(arg1   , arg2   )",
                              8,
                              true,
                              {"arg1", "arg2"},
                              26},
        ParseFunctionTestCase{"LeadingWhitespace",
                              "FUNCTION(   arg1,   arg2)",
                              8,
                              true,
                              {"arg1", "arg2"},
                              25},
        ParseFunctionTestCase{"MultilineArgs",
                              "FUNCTION(\n  arg1,\n  arg2\n)",
                              8,
                              true,
                              {"arg1", "arg2"},
                              0},
        ParseFunctionTestCase{"TabsAndNewlines",
                              "FUNCTION(\targ1\t,\r\narg2)",
                              8,
                              true,
                              {"arg1", "arg2"},
                              0}));

INSTANTIATE_TEST_SUITE_P(
    NestedStructures, ParseFunctionArgumentsTest,
    ::testing::Values(ParseFunctionTestCase{"NestedFunc",
                                            "FUNCTION(func(a, b), arg2)",
                                            8,
                                            true,
                                            {"func(a, b)", "arg2"},
                                            0},
                      ParseFunctionTestCase{
                          "DeeplyNested",
                          "FUNCTION(func1(func2(a, b), c), arg2)",
                          8,
                          true,
                          {"func1(func2(a, b), c)", "arg2"},
                          0},
                      ParseFunctionTestCase{"NestedBraces",
                                            "FUNCTION({literal}, arg2)",
                                            8,
                                            true,
                                            {"{literal}", "arg2"},
                                            0},
                      ParseFunctionTestCase{"ExtremeNesting",
                                            "FUNCTION(f1(f2(f3(f4(a)))), b)",
                                            8,
                                            true,
                                            {"f1(f2(f3(f4(a))))", "b"},
                                            0}));

INSTANTIATE_TEST_SUITE_P(
    QuotedStrings, ParseFunctionArgumentsTest,
    ::testing::Values(
        ParseFunctionTestCase{"SingleQuoted",
                              "FUNCTION('string value', arg2)",
                              8,
                              true,
                              {"'string value'", "arg2"},
                              0},
        ParseFunctionTestCase{"DoubleQuoted",
                              "FUNCTION(\"column_name\", arg2)",
                              8,
                              true,
                              {"\"column_name\"", "arg2"},
                              0},
        ParseFunctionTestCase{"EscapedSingleQuotes",
                              "FUNCTION('it''s quoted', arg2)",
                              8,
                              true,
                              {"'it''s quoted'", "arg2"},
                              0},
        ParseFunctionTestCase{"EscapedDoubleQuotes",
                              "FUNCTION(\"col\"\"name\", arg2)",
                              8,
                              true,
                              {"\"col\"\"name\"", "arg2"},
                              0},
        ParseFunctionTestCase{"CommaInString",
                              "FUNCTION('val, with, commas', arg2)",
                              8,
                              true,
                              {"'val, with, commas'", "arg2"},
                              0},
        ParseFunctionTestCase{"ParenInString",
                              "FUNCTION('val (with) parens', arg2)",
                              8,
                              true,
                              {"'val (with) parens'", "arg2"},
                              0}));

INSTANTIATE_TEST_SUITE_P(
    SQLComments, ParseFunctionArgumentsTest,
    ::testing::Values(
        ParseFunctionTestCase{"SingleLineComment",
                              "FUNCTION(arg1, -- comment\narg2)",
                              8,
                              true,
                              {"arg1", "arg2"},
                              0},
        ParseFunctionTestCase{"MultiLineComment",
                              "FUNCTION(arg1, /* comment */ arg2)",
                              8,
                              true,
                              {"arg1", "arg2"},
                              0},
        ParseFunctionTestCase{"CommentWithComma",
                              "FUNCTION(arg1 /* , fake */ , arg2)",
                              8,
                              true,
                              {"arg1", "arg2"},
                              0},
        ParseFunctionTestCase{
            "MultipleComments",
            "FUNCTION('str1', /* cmt1 */ 'str2', -- cmt2\narg3)",
            8,
            true,
            {"'str1'", "'str2'", "arg3"},
            0}));

INSTANTIATE_TEST_SUITE_P(
    ComplexExpressions, ParseFunctionArgumentsTest,
    ::testing::Values(ParseFunctionTestCase{"Arithmetic",
                                            "FUNCTION(a + b, c * d)",
                                            8,
                                            true,
                                            {"a + b", "c * d"},
                                            0},
                      ParseFunctionTestCase{"Comparison",
                                            "FUNCTION(a = b, c > d)",
                                            8,
                                            true,
                                            {"a = b", "c > d"},
                                            0},
                      ParseFunctionTestCase{"Logical",
                                            "FUNCTION(a AND b, c OR d)",
                                            8,
                                            true,
                                            {"a AND b", "c OR d"},
                                            0},
                      ParseFunctionTestCase{
                          "CaseExpr",
                          "FUNCTION(CASE WHEN a > 0 THEN 1 ELSE 0 END, arg2)",
                          8,
                          true,
                          {"CASE WHEN a > 0 THEN 1 ELSE 0 END", "arg2"},
                          0},
                      ParseFunctionTestCase{"SpecialChars",
                                            "FUNCTION(a.b, c::INT, d[1])",
                                            8,
                                            true,
                                            {"a.b", "c::INT", "d[1]"},
                                            0},
                      ParseFunctionTestCase{"NumericLiterals",
                                            "FUNCTION(123, 45.67, -89)",
                                            8,
                                            true,
                                            {"123", "45.67", "-89"},
                                            0},
                      ParseFunctionTestCase{"NullLiterals",
                                            "FUNCTION(NULL, arg2, NULL)",
                                            8,
                                            true,
                                            {"NULL", "arg2", "NULL"},
                                            0},
                      ParseFunctionTestCase{"Wildcard",
                                            "FUNCTION(*, COUNT(*))",
                                            8,
                                            true,
                                            {"*", "COUNT(*)"},
                                            0}));

INSTANTIATE_TEST_SUITE_P(
    ErrorCases, ParseFunctionArgumentsTest,
    ::testing::Values(
        ParseFunctionTestCase{
            "MissingOpenParen", "FUNCTION arg1, arg2)", 8, false, {}, 0},
        ParseFunctionTestCase{
            "MissingCloseParen", "FUNCTION(arg1, arg2", 8, false, {}, 0},
        ParseFunctionTestCase{
            "UnbalancedParens", "FUNCTION(func(arg1, arg2)", 8, false, {}, 0},
        ParseFunctionTestCase{"EmptyString", "", 0, false, {}, 0},
        ParseFunctionTestCase{"OnlyWhitespace", "   ", 0, false, {}, 0},
        ParseFunctionTestCase{"UnclosedSingleQuote",
                              "FUNCTION('unclosed, arg2)",
                              8,
                              false,
                              {},
                              0},
        ParseFunctionTestCase{"UnclosedDoubleQuote",
                              "FUNCTION(\"unclosed, arg2)",
                              8,
                              false,
                              {},
                              0},
        ParseFunctionTestCase{"UnclosedComment",
                              "FUNCTION(arg1, /* unclosed, arg2)",
                              8,
                              false,
                              {},
                              0}));

// Test structure for parameter marker scenarios
struct ParseFunctionParamMarkerTestCase {
    std::string name;
    std::string sql;
    size_t startPos;
    int numOfParamMarkers;
    int initialParamNumber;
    std::vector<std::string> expectedArgs;
    int expectedFinalParamNumber;
};

class ParseFunctionArgumentsParamMarkerTest
    : public ::testing::TestWithParam<ParseFunctionParamMarkerTestCase> {};

TEST_P(ParseFunctionArgumentsParamMarkerTest, ParameterMarkerHandling) {
    auto testCase = GetParam();
    size_t pos = testCase.startPos;
    std::vector<std::string> args;
    int paramNumber = testCase.initialParamNumber;

    bool result =
        parseFunctionArguments(testCase.sql, pos, args, nullptr,
                               testCase.numOfParamMarkers, paramNumber, 0);

    EXPECT_TRUE(result) << "Test: " << testCase.name;
    EXPECT_EQ(args.size(), testCase.expectedArgs.size())
        << "Test: " << testCase.name;
    for (size_t i = 0; i < std::min(args.size(), testCase.expectedArgs.size());
         ++i) {
        EXPECT_EQ(args[i], testCase.expectedArgs[i])
            << "Test: " << testCase.name << " - Arg " << i;
    }
    EXPECT_EQ(paramNumber, testCase.expectedFinalParamNumber)
        << "Test: " << testCase.name;
}

INSTANTIATE_TEST_SUITE_P(
    ParameterMarkers, ParseFunctionArgumentsParamMarkerTest,
    ::testing::Values(
        ParseFunctionParamMarkerTestCase{
            "SingleMarker", "FUNCTION(?, arg2)", 8, 2, 0, {"$1", "arg2"}, 1},
        ParseFunctionParamMarkerTestCase{"MultipleMarkers",
                                         "FUNCTION(?, ?, ?)",
                                         8,
                                         3,
                                         0,
                                         {"$1", "$2", "$3"},
                                         3},
        ParseFunctionParamMarkerTestCase{"MarkerWithOthers",
                                         "FUNCTION(col1, ?, 'lit')",
                                         8,
                                         1,
                                         0,
                                         {"col1", "$1", "'lit'"},
                                         1},
        ParseFunctionParamMarkerTestCase{"MarkerStartingFrom5",
                                         "FUNCTION(col1, ?, 'lit')",
                                         8,
                                         10,
                                         5,
                                         {"col1", "$6", "'lit'"},
                                         6},
        ParseFunctionParamMarkerTestCase{
            "NoReplacement", "FUNCTION(?, arg2)", 8, 0, 0, {"?", "arg2"}, 0},
        ParseFunctionParamMarkerTestCase{"NestedWithMarker",
                                         "FUNCTION(func(?, b), ?)",
                                         8,
                                         2,
                                         0,
                                         {"func($1, b)", "$2"},
                                         2},
        ParseFunctionParamMarkerTestCase{"MarkersInQuotesIgnored",
                                         "FUNCTION('how?', 'why ?', 'ok', ?)",
                                         8,
                                         1,
                                         0,
                                         {"'how?'", "'why ?'", "'ok'", "$1"},
                                         1}));

// Test real-world complex scenarios
TEST(ParseFunctionArguments, ComplexRealWorld_CastAndSubstring) {
    std::string sql =
        "FUNCTION(CAST(col1 AS VARCHAR), SUBSTRING(col2, 1, 10), ?)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 1, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "CAST(col1 AS VARCHAR)");
    EXPECT_EQ(args[1], "SUBSTRING(col2, 1, 10)");
    EXPECT_EQ(args[2], "$1");
    EXPECT_EQ(paramNumber, 1);
}

TEST(ParseFunctionArguments, ComplexRealWorld_NestedQuotesAndComments) {
    std::string sql = "FUNCTION(func1('str', func2(a, /* cmt */ b)), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    // Comments are skipped but internal whitespace is preserved
    EXPECT_EQ(args[0], "func1('str', func2(a,  b))");
    EXPECT_EQ(args[1], "arg2");
}

TEST(ParseFunctionArguments, ComplexRealWorld_MixedExpressions) {
    std::string sql = "FUNCTION(col1 + 10, UPPER('text'), ? * 2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 1, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "col1 + 10");
    EXPECT_EQ(args[1], "UPPER('text')");
    EXPECT_EQ(args[2], "$1 * 2");
}

// Test position handling after parsing
TEST(ParseFunctionArguments, PositionUpdate_WithTrailingContent) {
    std::string sql = "FUNCTION(arg1, arg2) AND MORE";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(pos, 20); // Position after ')'
    EXPECT_EQ(sql.substr(pos), " AND MORE");
}

TEST(ParseFunctionArguments, PositionUpdate_NestedFunction) {
    std::string sql = "OUTER(INNER(a, b), c) + 1";
    size_t pos = 5; // Start at OUTER's '('
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(pos, 21); // Position after OUTER's ')'
}

// Test edge cases with various data types
TEST(ParseFunctionArguments, EdgeCase_VeryLongArgument) {
    std::string longArg(1000, 'x');
    std::string sql = "FUNCTION(" + longArg + ", arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], longArg);
    EXPECT_EQ(args[1], "arg2");
}

TEST(ParseFunctionArguments, EdgeCase_ManyArguments) {
    // Build SQL with 50 arguments
    std::string sql = "FUNCTION(";
    std::vector<std::string> expected;
    for (int i = 1; i <= 50; ++i) {
        if (i > 1)
            sql += ", ";
        sql += "arg" + std::to_string(i);
        expected.push_back("arg" + std::to_string(i));
    }
    sql += ")";

    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 50);
    EXPECT_EQ(args, expected);
}

TEST(ParseFunctionArguments, EdgeCase_OnlyWhitespaceArguments) {
    std::string sql = "FUNCTION(   ,   ,   )";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 0); // All empty args should be removed
}

// Test special SQL constructs
TEST(ParseFunctionArguments, SpecialSQL_DateLiteral) {
    std::string sql = "FUNCTION({d '2023-01-01'}, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "{d '2023-01-01'}");
    EXPECT_EQ(args[1], "arg2");
}

TEST(ParseFunctionArguments, SpecialSQL_TimestampLiteral) {
    std::string sql = "FUNCTION({ts '2023-01-01 12:00:00'}, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

TEST(ParseFunctionArguments, SpecialSQL_EscapeSequence) {
    std::string sql = "FUNCTION({fn UCASE(col)}, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Test combinations of features
TEST(ParseFunctionArguments, Combined_QuotesAndNested) {
    std::string sql = "FUNCTION('value', func(a, b))";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'value'");
    EXPECT_EQ(args[1], "func(a, b)");
}

TEST(ParseFunctionArguments, Combined_CommentAndNested) {
    std::string sql = "FUNCTION(func(a /* cmt */, b), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "func(a , b)");
    EXPECT_EQ(args[1], "arg2");
}

TEST(ParseFunctionArguments, Combined_AllFeatures) {
    std::string sql =
        "FUNCTION(CAST('val' AS INT), func(?, /* cmt */ b), {d '2023-01-01'})";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 1, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "CAST('val' AS INT)");
}

// Test boundary conditions
TEST(ParseFunctionArguments, Boundary_StartPositionAtEnd) {
    std::string sql = "FUNCTION()";
    size_t pos = 10; // Already at end
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_FALSE(result); // Should fail - no '(' found
}

TEST(ParseFunctionArguments, Boundary_StartPositionBeyondEnd) {
    std::string sql = "FUNCTION()";
    size_t pos = 100; // Beyond string length
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_FALSE(result);
}

TEST(ParseFunctionArguments, Boundary_VeryFirstPosition) {
    std::string sql = "(arg1, arg2)";
    size_t pos = 0;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2");
}

// Test with nullptr for optional parameters
TEST(ParseFunctionArguments, NullOptionalParams_NoParamMarkers) {
    std::string sql = "FUNCTION(?, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    // Pass 0 for numOfParamMarkers so ? is not replaced
    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "?"); // Should not be replaced
    EXPECT_EQ(args[1], "arg2");
}

// Test starting position variations
TEST(ParseFunctionArguments, StartPos_BeforeOpenParen) {
    std::string sql = "  (arg1, arg2)";
    size_t pos = 0; // Before whitespace
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

TEST(ParseFunctionArguments, StartPos_OnOpenParen) {
    std::string sql = "(arg1, arg2)";
    size_t pos = 0; // Exactly at '('
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Test with complex SQL types
TEST(ParseFunctionArguments, SQLTypes_DateTimeOperations) {
    std::string sql =
        "FUNCTION(DATE '2023-01-01', TIMESTAMP '2023-01-01 12:00:00')";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

TEST(ParseFunctionArguments, SQLTypes_IntervalOperations) {
    std::string sql = "FUNCTION(INTERVAL '1 day', col + INTERVAL '1 hour')";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Test whitespace variations
TEST(ParseFunctionArguments, Whitespace_AllTypes) {
    std::string sql = "FUNCTION(arg1\t,\r\n arg2\v,\f arg3)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2");
    EXPECT_EQ(args[2], "arg3");
}

// Test subquery scenarios
TEST(ParseFunctionArguments, Subquery_Simple) {
    std::string sql = "FUNCTION((SELECT a FROM t), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "(SELECT a FROM t)");
    EXPECT_EQ(args[1], "arg2");
}

TEST(ParseFunctionArguments, Subquery_WithCommas) {
    std::string sql = "FUNCTION((SELECT a, b, c FROM t), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "(SELECT a, b, c FROM t)");
    EXPECT_EQ(args[1], "arg2");
}

// Test string concatenation
TEST(ParseFunctionArguments, StringConcatenation) {
    std::string sql = "FUNCTION('str1' || 'str2', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'str1' || 'str2'");
    EXPECT_EQ(args[1], "arg2");
}

// Test type casts
TEST(ParseFunctionArguments, TypeCast_DoubleColon) {
    std::string sql = "FUNCTION(col::VARCHAR, 123::BIGINT)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "col::VARCHAR");
    EXPECT_EQ(args[1], "123::BIGINT");
}

TEST(ParseFunctionArguments, TypeCast_CastFunction) {
    std::string sql = "FUNCTION(CAST(col AS INT), CAST('2023-01-01' AS DATE))";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "CAST(col AS INT)");
    EXPECT_EQ(args[1], "CAST('2023-01-01' AS DATE)");
}

// Test operator precedence scenarios
TEST(ParseFunctionArguments, Operators_Precedence) {
    std::string sql = "FUNCTION(a + b * c, d / e - f)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "a + b * c");
    EXPECT_EQ(args[1], "d / e - f");
}

// Test boolean expressions
TEST(ParseFunctionArguments, Boolean_ComplexConditions) {
    std::string sql = "FUNCTION(a > 0 AND b < 100, c = d OR e != f)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "a > 0 AND b < 100");
    EXPECT_EQ(args[1], "c = d OR e != f");
}

// Test aggregate functions
TEST(ParseFunctionArguments, Aggregate_Functions) {
    std::string sql = "FUNCTION(SUM(col), AVG(col2), COUNT(*))";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "SUM(col)");
    EXPECT_EQ(args[1], "AVG(col2)");
    EXPECT_EQ(args[2], "COUNT(*)");
}

// Test window functions
TEST(ParseFunctionArguments, Window_Functions) {
    std::string sql = "FUNCTION(ROW_NUMBER() OVER (ORDER BY col), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "ROW_NUMBER() OVER (ORDER BY col)");
    EXPECT_EQ(args[1], "arg2");
}

// Test IN clause
TEST(ParseFunctionArguments, IN_Clause) {
    std::string sql = "FUNCTION(col IN (1, 2, 3), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "col IN (1, 2, 3)");
    EXPECT_EQ(args[1], "arg2");
}

// Test BETWEEN clause
TEST(ParseFunctionArguments, BETWEEN_Clause) {
    std::string sql = "FUNCTION(col BETWEEN 1 AND 10, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "col BETWEEN 1 AND 10");
    EXPECT_EQ(args[1], "arg2");
}

// Test LIKE clause
TEST(ParseFunctionArguments, LIKE_Clause) {
    std::string sql = "FUNCTION(col LIKE '%pattern%', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "col LIKE '%pattern%'");
    EXPECT_EQ(args[1], "arg2");
}

// Test IS NULL / IS NOT NULL
TEST(ParseFunctionArguments, IS_NULL_Clause) {
    std::string sql = "FUNCTION(col IS NULL, col2 IS NOT NULL)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "col IS NULL");
    EXPECT_EQ(args[1], "col2 IS NOT NULL");
}

// Test EXISTS with subquery
TEST(ParseFunctionArguments, EXISTS_Subquery) {
    std::string sql = "FUNCTION(EXISTS (SELECT 1 FROM t WHERE x = y), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "EXISTS (SELECT 1 FROM t WHERE x = y)");
    EXPECT_EQ(args[1], "arg2");
}

// Test negative numbers and signs
TEST(ParseFunctionArguments, Negative_Numbers) {
    std::string sql = "FUNCTION(-123, +456, -78.9)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "-123");
    EXPECT_EQ(args[1], "+456");
    EXPECT_EQ(args[2], "-78.9");
}

// Test scientific notation
TEST(ParseFunctionArguments, Scientific_Notation) {
    std::string sql = "FUNCTION(1.23e10, 4.56E-5)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "1.23e10");
    EXPECT_EQ(args[1], "4.56E-5");
}

// Test with schema-qualified names
TEST(ParseFunctionArguments, Schema_Qualified) {
    std::string sql = "FUNCTION(schema.table.column, public.func(x))";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "schema.table.column");
    EXPECT_EQ(args[1], "public.func(x)");
}

// Test with binary operators in complex nesting
TEST(ParseFunctionArguments, Complex_BinaryOperators) {
    std::string sql = "FUNCTION((a + b) * (c - d), (e / f) % g)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "(a + b) * (c - d)");
    EXPECT_EQ(args[1], "(e / f) % g");
}

// Test stress scenarios
TEST(ParseFunctionArguments, Stress_DeeplyNestedQuotesCommentsParens) {
    std::string sql =
        "FUNCTION(func1('str()', func2(/* cmt(, */ a, b)), 'val''s')";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Test multiple levels of nesting with all features
TEST(ParseFunctionArguments, Stress_AllFeaturesCombined) {
    std::string sql = "FUNCTION(CAST(func(?, 'str''s'), -- comment\nfunc2({d "
                      "'2023-01-01'}, /* multi\nline */ b)) AS INT, arg3)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 1, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Test with malformed SQL that should fail gracefully
TEST(ParseFunctionArguments, Malformed_ExtraClosingParen) {
    std::string sql = "FUNCTION(arg1, arg2))";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result); // Should succeed and stop at first ')'
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(pos, 20); // Position after consuming first ')' (one past it)
}

// Test special characters that need careful handling
TEST(ParseFunctionArguments, SpecialChars_Backslash) {
    std::string sql = "FUNCTION('path\\to\\file', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'path\\to\\file'");
}

// Test with Unicode/UTF-8 characters
TEST(ParseFunctionArguments, Unicode_Characters) {
    std::string sql = "FUNCTION('Привет', '你好', 'مرحبا')";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
}

// Test semicolon handling (should not terminate parsing)
TEST(ParseFunctionArguments, Semicolon_InString) {
    std::string sql = "FUNCTION('value; with; semicolons', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'value; with; semicolons'");
    EXPECT_EQ(args[1], "arg2");
}

// Unclosed multi-line comment at end of argument list
// Parser must detect unclosed comments and reject parsing
TEST(ParseFunctionArguments, UnclosedMultiLineComment_EndOfInput) {
    std::string sql = "FUNCTION('needle', 'haystack' /* unclosed comment";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    // Parser should REJECT this input by returning false
    EXPECT_FALSE(result)
        << "Parser must reject SQL with unclosed multi-line comment";
}

//  Unclosed multi-line comment before closing parenthesis
// Parser must not treat ')' as argument terminator when inside comment
TEST(ParseFunctionArguments, UnclosedMultiLineComment_BeforeClosingParen) {
    std::string sql = "FUNCTION(arg1, arg2 /* comment without */ end)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    // This SQL has properly closed comment, so should succeed
    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);

    // Now test with UNCLOSED comment
    sql = "FUNCTION(arg1, arg2 /* unclosed )";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    // Parser should REJECT because comment is not closed before ')'
    EXPECT_FALSE(result)
        << "Parser must reject SQL when ')' appears inside unclosed comment";
}

// Unclosed single-line comment
// Single-line comments are terminated by newline or end of input
TEST(ParseFunctionArguments, SingleLineComment_ProperTermination) {
    // Single-line comment properly terminated with newline
    std::string sql = "FUNCTION(arg1, -- comment\narg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2");

    // Single-line comment at end of input (implicitly "closed" by EOF)
    sql = "FUNCTION(arg1, arg2) -- comment at end";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Single-line comment at EOF should be handled gracefully";
    EXPECT_EQ(args.size(), 2);
}

// Multi-line comment containing SQL-like syntax
// Parser must not interpret SQL keywords inside comments
TEST(ParseFunctionArguments, CommentContainingSQLSyntax) {
    std::string sql = "FUNCTION(arg1, /* SELECT, INSERT, ), } */ arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2")
        << "Parser must ignore SQL keywords inside comments";
}

// Nested comment-like sequences (not actual nested comments)
// Parser must handle /* inside comments correctly
TEST(ParseFunctionArguments, CommentLikeSequencesInComment) {
    std::string sql = "FUNCTION(arg1, /* comment with /* inside */ arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    // The first */ closes the comment
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2");
}

// Comment at various positions
// Parser must handle comments at any position without state corruption
TEST(ParseFunctionArguments, CommentAtDifferentPositions) {
    // Comment before first argument
    std::string sql = "FUNCTION(/* cmt */ arg1, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);

    // Comment between arguments
    sql = "FUNCTION(arg1 /* cmt */, arg2)";
    pos = 8;
    args.clear();
    paramNumber = 0;
    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);

    // Comment after last argument
    sql = "FUNCTION(arg1, arg2 /* cmt */)";
    pos = 8;
    args.clear();
    paramNumber = 0;
    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);

    // Multiple comments
    sql = "FUNCTION(/* c1 */ arg1 /* c2 */, /* c3 */ arg2 /* c4 */)";
    pos = 8;
    args.clear();
    paramNumber = 0;
    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Unclosed comment with embedded quotes and special characters
// Parser must reject malformed input regardless of content
TEST(ParseFunctionArguments, UnclosedCommentWithComplexContent) {
    // Unclosed comment containing quotes
    std::string sql = "FUNCTION(arg1, /* 'string', \"identifier\" unclosed";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_FALSE(result)
        << "Parser must reject unclosed comment even with quotes inside";

    // Unclosed comment containing parentheses
    sql = "FUNCTION(arg1, /* nested (parens) unclosed";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_FALSE(result)
        << "Parser must reject unclosed comment with parentheses inside";
}

// Comment state transition validation
// Parser must correctly transition between comment states
TEST(ParseFunctionArguments, CommentStateTransitions) {
    // Start in comment, exit comment, continue normal parsing
    std::string sql = "FUNCTION(/* comment */ arg1, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2");

    // Multiple state transitions
    sql = "FUNCTION(/* c1 */ 'str' /* c2 */, func(/* c3 */ a))";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
}

// Comment containing comment start sequences
// Parser must handle comment start sequences inside comments
TEST(ParseFunctionArguments, CommentStartSequenceInComment) {
    // Multi-line comment containing --
    std::string sql = "FUNCTION(arg1, /* comment with -- inside */ arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2")
        << "-- inside /* */ should be treated as comment text";
}

// Comment with only opening delimiter at end
// Parser must reject input ending with comment opening
TEST(ParseFunctionArguments, CommentOpeningAtVeryEnd) {
    std::string sql = "FUNCTION(arg1, arg2 /*";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_FALSE(result) << "Parser must reject input ending with /*";

    // Single-line comment at end is OK (terminated by EOF)
    sql = "FUNCTION(arg1, arg2) --";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Single-line comment at EOF is acceptable";
}

// Malformed comment delimiters
// Parser must not be confused by partial comment delimiters
TEST(ParseFunctionArguments, PartialCommentDelimiters) {
    // Single slash followed by non-star
    std::string sql = "FUNCTION(arg1 / 2, arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "arg1 / 2")
        << "Single / should not be treated as comment start";

    // Star not followed by slash
    sql = "FUNCTION(arg1 * 2, arg2)";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args[0], "arg1 * 2")
        << "Single * should not be treated as comment end";
}

// Allowlisted function names are properly parsed
// Parser must correctly extract function names for allowlist validation
TEST(ParseFunctionArguments, AllowlistedFunctionExtraction) {
    // Test various allowlisted ODBC functions
    std::string sql = "UCASE('hello')";
    size_t pos = 5; // Start at opening paren
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 1);
    EXPECT_EQ(args[0], "'hello'")
        << "Arguments for allowlisted functions parsed correctly";
}

// Unknown function names don't cause parser failure
// Parser must handle unknown function names safely
TEST(ParseFunctionArguments, UnknownFunctionNamesHandled) {
    // Unknown function - parser should still parse arguments correctly
    std::string sql = "UNKNOWN_FUNCTION(arg1, arg2, arg3)";
    size_t pos = 16;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Parser must handle unknown function names without crashing";
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "arg1");
    EXPECT_EQ(args[1], "arg2");
    EXPECT_EQ(args[2], "arg3");
}

// Potentially malicious function names with special characters
// Parser must safely handle function names with special chars
TEST(ParseFunctionArguments, FunctionNameWithSpecialChars) {
    // Function names with special SQL characters
    std::vector<std::string> testFunctions = {
        "DROP_TABLE", "EXEC_CMD", "PG_SLEEP", "SYSTEM_CALL", "ADMIN_FUNCTION"};

    for (const auto &funcName : testFunctions) {
        std::string sql = funcName + "(arg1, arg2)";
        size_t pos = funcName.length();
        std::vector<std::string> args;
        int paramNumber = 0;

        bool result =
            parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

        EXPECT_TRUE(result) << "Parser must handle function name: " << funcName;
        EXPECT_EQ(args.size(), 2) << "Args parsed correctly for: " << funcName;
    }
}

// Function name case insensitivity
// Parser must handle case variations consistently
TEST(ParseFunctionArguments, FunctionNameCaseInsensitivity) {
    // Same function in different cases
    std::vector<std::string> caseVariations = {
        "UCASE('test')", "ucase('test')", "UCase('test')", "uCaSe('test')"};

    for (const auto &sql : caseVariations) {
        size_t pos = sql.find('(');
        std::vector<std::string> args;
        int paramNumber = 0;

        bool result =
            parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

        EXPECT_TRUE(result);
        EXPECT_EQ(args.size(), 1);
        EXPECT_EQ(args[0], "'test'");
    }
}

// Function arguments containing function-like patterns
// Parser must not confuse nested function calls with function names
TEST(ParseFunctionArguments, ArgumentsWithFunctionLikePatterns) {
    // Arguments that look like function calls
    std::string sql = "FUNC(EXECUTE('cmd'), SYSTEM('ls'), DROP_TABLE('users'))";
    size_t pos = 4;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "EXECUTE('cmd')")
        << "Nested function calls preserved in arguments";
    EXPECT_EQ(args[1], "SYSTEM('ls')");
    EXPECT_EQ(args[2], "DROP_TABLE('users')");
}

// Argument count validation is handled by caller
// parseFunctionArguments returns all arguments for validation
TEST(ParseFunctionArguments, AllArgumentsExtracted) {
    // Functions with various argument counts
    struct TestCase {
        std::string sql;
        size_t startPos;
        int expectedCount;
    };

    std::vector<TestCase> tests = {{"FUNC()", 4, 0},
                                   {"FUNC(a)", 4, 1},
                                   {"FUNC(a, b)", 4, 2},
                                   {"FUNC(a, b, c, d, e)", 4, 5}};

    for (const auto &test : tests) {
        size_t pos = test.startPos;
        std::vector<std::string> args;
        int paramNumber = 0;

        bool result = parseFunctionArguments(test.sql, pos, args, nullptr, 0,
                                             paramNumber, 0);

        EXPECT_TRUE(result);
        EXPECT_EQ(args.size(), test.expectedCount)
            << "All arguments must be extracted for: " << test.sql;
    }
}

// SQL injection attempts via function arguments
// Parser should accept valid SQL expressions in function arguments
TEST(ParseFunctionArguments, SQLInjectionInArguments) {
    // Test 1: "'value' OR '1'='1'" is VALID SQL
    // - 'value' is a string literal
    // - OR is a valid operator
    // - '1'='1' is a boolean comparison
    // This should be ACCEPTED as it's syntactically correct
    std::string sql = "FUNC('value' OR '1'='1', arg2)";
    size_t pos = 4;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'value' OR '1'='1'");
    EXPECT_EQ(args[1], "arg2");

    // Test 2: Valid SQL with proper operators is ACCEPTED
    sql = "FUNC('value', 'a', arg2)";
    pos = 4;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "'value'");
    EXPECT_EQ(args[1], "'a'");
    EXPECT_EQ(args[2], "arg2");

    // Test 3: SQL keywords in string literals are OK (they're quoted)
    sql = "FUNC('DROP TABLE users', 'SELECT * FROM passwords')";
    pos = 4;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'DROP TABLE users'");
    EXPECT_EQ(args[1], "'SELECT * FROM passwords'");
}

// Function name length limits
// Parser must handle very long function names without buffer overflow
TEST(ParseFunctionArguments, VeryLongFunctionName) {
    // Very long function name (potential buffer overflow attempt)
    std::string longName(1000, 'A');
    std::string sql = longName + "(arg1, arg2)";
    size_t pos = longName.length();
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Parser must handle long function names safely";
    EXPECT_EQ(args.size(), 2);
}

// Basic escaped single quote handling
//  Parser must correctly handle '' (doubled single quotes)
// as escaped quote
TEST(ParseFunctionArguments, EscapedQuotes_Basic) {
    std::string sql = "FUNCTION('it''s working', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Parser must correctly handle escaped single quotes";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'it''s working'")
        << "Escaped quote (two consecutive single quotes) must be preserved in "
           "argument";
    EXPECT_EQ(args[1], "arg2");
}

// Multiple escaped quotes in single string
//  Parser must handle multiple escape sequences without
// buffer overflow
TEST(ParseFunctionArguments, EscapedQuotes_Multiple) {
    std::string sql = "FUNCTION('can''t won''t don''t', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Parser must handle multiple escaped quotes in one string";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'can''t won''t don''t'")
        << "All escaped quotes must be correctly preserved";
    EXPECT_EQ(args[1], "arg2");
}

// Escaped quote at string boundaries
//  Parser must correctly track string boundaries with
// escaped quotes
TEST(ParseFunctionArguments, EscapedQuotes_AtBoundaries) {
    // Escaped quote at beginning
    std::string sql = "FUNCTION('''quoted', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Parser must handle escaped quote at string start";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'''quoted'");

    // Escaped quote at end
    sql = "FUNCTION('quoted''', arg2)";
    pos = 8;
    args.clear();
    paramNumber = 0;

    result = parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Parser must handle escaped quote at string end";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'quoted'''");
}

// String containing only escaped quotes
//  Parser must handle edge case of string with only escape
// sequences
TEST(ParseFunctionArguments, EscapedQuotes_OnlyEscapes) {
    std::string sql = "FUNCTION('''''', arg2)"; // Four single quotes = one
                                                // escaped quote in string
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Parser must handle string with only escaped quotes";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "''''''")
        << "All four quotes must be preserved (represents two literal quotes)";
}

// Escaped quotes followed by actual string terminator
//  Parser must distinguish between escaped quotes and
// string terminators
TEST(ParseFunctionArguments, EscapedQuotes_ThenTerminator) {
    std::string sql = "FUNCTION('test''value', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Parser must distinguish escaped quote from terminator";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'test''value'");
    EXPECT_EQ(args[1], "arg2");
}

// Comma inside string with escaped quotes
//  Parser must not treat comma as separator when inside
// quoted string
TEST(ParseFunctionArguments, EscapedQuotes_WithCommaInside) {
    std::string sql = "FUNCTION('val1, val2, it''s all', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Comma inside quoted string must not split arguments";
    EXPECT_EQ(args.size(), 2)
        << "Must have exactly 2 arguments, not split by commas in string";
    EXPECT_EQ(args[0], "'val1, val2, it''s all'");
    EXPECT_EQ(args[1], "arg2");
}

// Boundary check - position tracking with escaped quotes
//  Parser must correctly track position and not overflow
// buffer
TEST(ParseFunctionArguments, EscapedQuotes_PositionTracking) {
    std::string sql = "FUNCTION('a''b''c') EXTRA";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(args.size(), 1);
    EXPECT_EQ(args[0], "'a''b''c'");
    EXPECT_EQ(pos, 19)
        << "Position must be correctly updated after parsing escaped quotes";
    EXPECT_EQ(sql.substr(pos), " EXTRA")
        << "Position must point right after closing paren";
}

// Nested function with escaped quotes
//  Parser must maintain state across nested contexts
TEST(ParseFunctionArguments, EscapedQuotes_NestedFunction) {
    std::string sql = "FUNCTION(func('it''s nested', 'can''t fail'), arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Nested functions with escaped quotes must parse correctly";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "func('it''s nested', 'can''t fail')");
    EXPECT_EQ(args[1], "arg2");
}

// Long string with many escaped quotes (stress test)
//  Parser must handle repeated escape sequences without
// overflow
TEST(ParseFunctionArguments, EscapedQuotes_StressTest) {
    // Build string with many escaped quotes
    std::string longString = "'";
    for (int i = 0; i < 100; i++) {
        longString += "a''"; // 100 escaped quotes interspersed with 'a'
    }
    longString += "'";

    std::string sql = "FUNCTION(" + longString + ", arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Parser must handle many escaped quotes without buffer overflow";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], longString);
    EXPECT_EQ(args[1], "arg2");
}

// Escaped quotes with special SQL characters
//  Parser must maintain correct state across multiple
// special characters
TEST(ParseFunctionArguments, EscapedQuotes_WithSpecialChars) {
    std::string sql = "FUNCTION('val''s (with) {braces} and, commas', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Escaped quotes with special chars must parse correctly";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'val''s (with) {braces} and, commas'")
        << "String with escaped quotes and special chars must be preserved "
           "exactly";
}

// Consecutive escaped quotes
//  Parser must handle sequences of escaped quotes
// correctly
TEST(ParseFunctionArguments, EscapedQuotes_Consecutive) {
    // Four consecutive quotes = escaped quote + string boundary
    std::string sql = "FUNCTION('test''''more', arg2)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result) << "Parser must handle consecutive escaped quotes";
    EXPECT_EQ(args.size(), 2);
    EXPECT_EQ(args[0], "'test''''more'")
        << "Four quotes: two escaped quotes inside string";
}

// Escaped quote followed by closing paren (boundary validation)
//  Parser must correctly identify string terminator vs
// escaped quote
TEST(ParseFunctionArguments, EscapedQuotes_BeforeCloseParen) {
    std::string sql = "FUNCTION('test''')";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Parser must correctly parse escaped quote before close paren";
    EXPECT_EQ(args.size(), 1);
    EXPECT_EQ(args[0], "'test'''")
        << "Escaped quote at end of string must be preserved";
}

// Mixed single and double quotes with escapes
//  Parser must independently track single and double quote
// states
TEST(ParseFunctionArguments, MixedQuotes_WithEscapes) {
    std::string sql = "FUNCTION('single''s', \"double\"\"s\", arg3)";
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_TRUE(result)
        << "Parser must handle both single and double quote escaping";
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "'single''s'");
    EXPECT_EQ(args[1], "\"double\"\"s\"");
    EXPECT_EQ(args[2], "arg3");
}

// Escaped quote near buffer boundary (edge case)
//  Parser must not read beyond string length when checking
// for escaped quote
TEST(ParseFunctionArguments, EscapedQuotes_NearBoundary) {
    // String that ends exactly at an escaped quote
    std::string sql = "FUNCTION('end''"; // Malformed: unclosed string
    size_t pos = 8;
    std::vector<std::string> args;
    int paramNumber = 0;

    bool result =
        parseFunctionArguments(sql, pos, args, nullptr, 0, paramNumber, 0);

    EXPECT_FALSE(result)
        << "Parser must reject unclosed string even with escaped quote at end";
}

// Known ODBC function is transformed correctly
// Verifies that allowlisted functions are properly converted
TEST(ODBCEscapeClauseProcessing, KnownFunction_UCASE_Transformed) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn UCASE('hello')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_THAT((char *)result, testing::HasSubstr("UPPER('hello')"))
        << "UCASE should be transformed to UPPER";
    EXPECT_THAT((char *)result, testing::Not(testing::HasSubstr("{fn")))
        << "Escape clause wrapper should be removed";

    releasePaStrBuf(&paStrBuf);
}

// Unknown function passes through COMPLETELY UNCHANGED
// Verifies ODBC spec compliance: "If driver doesn't understand grammar, pass
// through unchanged"
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing, DISABLED_UnknownFunction_PassThrough_Exact) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn PG_SLEEP(1000)}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    std::string resultStr((char *)result);

    // Unknown function MUST be passed through COMPLETELY UNCHANGED with {fn }
    // wrapper
    EXPECT_STREQ((char *)result, input)
        << "Unknown function escape clause must be passed through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Multiple unknown functions - ALL passed through unchanged
// Verifies consistent handling of multiple unknown escape clauses
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing, DISABLED_MultipleUnknownFunctions_Exact) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn CUSTOM_FUNC1('a')}, {fn CUSTOM_FUNC2('b')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    std::string resultStr((char *)result);

    // Both escape clauses MUST be completely unchanged with {fn } wrappers
    EXPECT_STREQ((char *)result, input)
        << "Multiple unknown functions must be passed through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Unknown function with complex arguments - passed through exactly
// Verifies that complex argument structures are preserved completely
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing,
     DISABLED_UnknownFunction_ComplexArguments_Exact) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn CUSTOM(col1, CAST(col2 AS INT), 'literal')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    std::string resultStr((char *)result);

    // Complete escape clause with all arguments MUST be unchanged
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with complex args must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Function name with SQL injection attempt
// Verifies that function names are treated as identifiers, not executable SQL
TEST(ODBCEscapeClauseProcessing, FunctionNameSQLInjection) {
    RS_STR_BUF paStrBuf;
    // Attempt to inject SQL through function name (won't work due to parser
    // design)
    char input[] = "SELECT {fn FUNC'OR'1'='1('arg')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    // The malformed escape clause should be handled safely
    // Either rejected or passed through - either way is safe

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with no arguments
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing, DISABLED_UnknownFunction_NoArgs_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn CUSTOM_FUNC()}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with no args must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with single argument
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing, DISABLED_UnknownFunction_SingleArg_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn MY_FUNC('value')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with single arg must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with special characters in arguments
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing,
     DISABLED_UnknownFunction_SpecialChars_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn FUNC('a!@#$%^&*()b')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with special chars must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with comments in arguments
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing,
     DISABLED_UnknownFunction_CommentsInArgs_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn FUNC(arg1 /* comment */, arg2)}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with comments must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with CAST expression
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing, DISABLED_UnknownFunction_CastExpr_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn CONVERT_CUSTOM(CAST(col AS VARCHAR))}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with CAST must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with wildcard (*)
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing, DISABLED_UnknownFunction_Wildcard_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn COUNT_CUSTOM(*)}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with wildcard must pass through UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function with boolean expression
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing,
     DISABLED_UnknownFunction_BooleanExpr_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn CHECK(a > 0 AND b < 100)}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function with boolean expression must pass through "
           "UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// Test: Unknown function that resembles known function name
// DISABLED: Unknown functions or Redshift functions might be used in query
// directly instead of using ODBC escape function. Driver removes {fn } wrapper
// to allow direct usage.
TEST(ODBCEscapeClauseProcessing,
     DISABLED_UnknownFunction_SimilarToKnown_Unchanged) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn UCASE_CUSTOM('value')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, input)
        << "Unknown function similar to known function must pass through "
           "UNCHANGED";

    releasePaStrBuf(&paStrBuf);
}

// LOCATE - Basic 2-argument reordering
// Verifies exact argument preservation during reordering
TEST(ArgumentReordering, LOCATE_TwoArgs_ExactMatch) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn LOCATE('needle', 'haystack')}";
    char expected[] = "SELECT POSITION('needle' IN 'haystack')";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, expected)
        << "LOCATE must transform to POSITION with exact argument preservation";

    releasePaStrBuf(&paStrBuf);
}

// LOCATE with escaped quotes
// Ensures escaped quotes preserved exactly during reordering
TEST(ArgumentReordering, LOCATE_EscapedQuotes_ExactMatch) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn LOCATE('it''s', 'can''t')}";
    char expected[] = "SELECT POSITION('it''s' IN 'can''t')";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, expected)
        << "Escaped quotes must be preserved exactly during argument "
           "reordering";

    releasePaStrBuf(&paStrBuf);
}

// LOCATE with comma inside string literal
// Critical test - commas in strings must not split arguments
TEST(ArgumentReordering, LOCATE_CommaInString_ExactMatch) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn LOCATE('a, b', 'x, y')}";
    char expected[] = "SELECT POSITION('a, b' IN 'x, y')";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, expected)
        << "Commas inside strings must not split arguments - exact match "
           "required";

    releasePaStrBuf(&paStrBuf);
}

// CONVERT function - type token lookup and reordering
// Validates type allowlist and argument swap
TEST(ArgumentReordering, CONVERT_TypeLookup_ExactMatch) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn CONVERT('123', SQL_INTEGER)}";
    char expected[] = "SELECT CAST('123' AS INTEGER)";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, expected)
        << "CONVERT must map SQL types with exact argument preservation";

    releasePaStrBuf(&paStrBuf);
}

// IFNULL - Simple function with no reordering
// Validates basic transformation preserves argument order
TEST(ArgumentReordering, IFNULL_NoReorder_ExactMatch) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn IFNULL(col, 'default')}";
    char expected[] = "SELECT COALESCE(col,'default')";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, expected)
        << "IFNULL must transform to COALESCE with arguments in same order";

    releasePaStrBuf(&paStrBuf);
}

// LOCATE with parentheses inside string
// Ensures special characters in strings don't affect argument extraction
TEST(ArgumentReordering, LOCATE_SpecialCharsInString_ExactMatch) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn LOCATE('test ()', 'data')}";
    char expected[] = "SELECT POSITION('test ()' IN 'data')";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    EXPECT_STREQ((char *)result, expected)
        << "Special characters inside strings must be preserved exactly";

    releasePaStrBuf(&paStrBuf);
}

// LOCATE with escaped quotes in arguments
// Ensures escaped quotes don't break argument boundaries during reordering
TEST(ArgumentReordering, LOCATE_EscapedQuotes) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn LOCATE('it''s', 'can''t find')}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    std::string resultStr((char *)result);

    // Verify escaped quotes preserved in both arguments
    EXPECT_THAT(resultStr,
                testing::HasSubstr("POSITION('it''s' IN 'can''t find')"))
        << "Escaped quotes must be preserved during argument reordering";

    releasePaStrBuf(&paStrBuf);
}

// LOCATE 3-argument form with complex reordering
// Tests complex transformation preserves all three arguments exactly
TEST(ArgumentReordering, LOCATE_ThreeArgs) {
    RS_STR_BUF paStrBuf;
    char input[] = "SELECT {fn LOCATE('test', column_name, 5)}";

    unsigned char *result = checkReplaceParamMarkerAndODBCEscapeClause(
        nullptr, input, SQL_NTS, &paStrBuf, 0);

    ASSERT_NE(result, nullptr);
    std::string resultStr((char *)result);

    // Verify complex 3-arg transformation
    EXPECT_THAT(resultStr, testing::HasSubstr(
                               "POSITION('test' IN SUBSTRING(column_name FROM"))
        << "3-argument LOCATE must properly transform with all args preserved";
    EXPECT_THAT(resultStr, testing::HasSubstr("CAST(5 AS INTEGER)"))
        << "Third argument must be preserved in CAST expression";

    releasePaStrBuf(&paStrBuf);
}

// Unit tests for lookup_sql_type
TEST(LookupSqlType, ValidTypes) {
    // Test valid ODBC SQL type names
    EXPECT_STREQ(lookupSqlType("SQL_BIGINT"), "BIGINT");
    EXPECT_STREQ(lookupSqlType("SQL_BIT"), "BOOL");
    EXPECT_STREQ(lookupSqlType("SQL_CHAR"), "CHAR");
    EXPECT_STREQ(lookupSqlType("SQL_DECIMAL"), "DECIMAL");
    EXPECT_STREQ(lookupSqlType("SQL_DOUBLE"), "FLOAT8");
    EXPECT_STREQ(lookupSqlType("SQL_FLOAT"), "FLOAT8");
    EXPECT_STREQ(lookupSqlType("SQL_INTEGER"), "INTEGER");
    EXPECT_STREQ(lookupSqlType("SQL_NUMERIC"), "NUMERIC");
    EXPECT_STREQ(lookupSqlType("SQL_REAL"), "REAL");
    EXPECT_STREQ(lookupSqlType("SQL_SMALLINT"), "SMALLINT");
    EXPECT_STREQ(lookupSqlType("SQL_TYPE_DATE"), "DATE");
    EXPECT_STREQ(lookupSqlType("SQL_DATE"), "DATE");
    EXPECT_STREQ(lookupSqlType("SQL_TYPE_TIMESTAMP"), "TIMESTAMP");
    EXPECT_STREQ(lookupSqlType("SQL_TIMESTAMP"), "TIMESTAMP");
    EXPECT_STREQ(lookupSqlType("SQL_TYPE_TIME"), "TIME");
    EXPECT_STREQ(lookupSqlType("SQL_TIME"), "TIME");
    EXPECT_STREQ(lookupSqlType("SQL_VARCHAR"), "VARCHAR");
    EXPECT_STREQ(lookupSqlType("SQL_INTERVAL_YEAR_TO_MONTH"), "INTERVALY2M");
    EXPECT_STREQ(lookupSqlType("SQL_INTERVAL_DAY_TO_SECOND"), "INTERVALD2S");
}

TEST(LookupSqlType, CaseInsensitive) {
    // Test case insensitivity
    EXPECT_STREQ(lookupSqlType("sql_bigint"), "BIGINT");
    EXPECT_STREQ(lookupSqlType("SQL_BIGINT"), "BIGINT");
    EXPECT_STREQ(lookupSqlType("Sql_BigInt"), "BIGINT");
    EXPECT_STREQ(lookupSqlType("sql_varchar"), "VARCHAR");
    EXPECT_STREQ(lookupSqlType("SQL_VARCHAR"), "VARCHAR");
}

TEST(LookupSqlType, InvalidType) {
    // Test invalid/unknown type names return nullptr
    EXPECT_EQ(lookupSqlType("SQL_UNKNOWN_TYPE"), nullptr);
    EXPECT_EQ(lookupSqlType("INVALID"), nullptr);
    EXPECT_EQ(lookupSqlType(""), nullptr);
    EXPECT_EQ(lookupSqlType("SQL_"), nullptr);
}

TEST(LookupSqlType, EmptyString) {
    // Test empty string input
    EXPECT_EQ(lookupSqlType(""), nullptr);
}

TEST(LookupSqlType, PartialMatch) {
    // Test that partial matches don't work
    EXPECT_EQ(lookupSqlType("SQL_BIG"), nullptr);
    EXPECT_EQ(lookupSqlType("BIGINT"), nullptr);
    EXPECT_EQ(lookupSqlType("SQL_INTEG"), nullptr);
}

TEST(LookupSqlType, ExtraCharacters) {
    // Test with extra characters
    EXPECT_EQ(lookupSqlType("SQL_BIGINT "), nullptr);
    EXPECT_EQ(lookupSqlType(" SQL_BIGINT"), nullptr);
    EXPECT_EQ(lookupSqlType("SQL_BIGINT_EXTRA"), nullptr);
}

// Unit tests for lookupIntervalToken
TEST(LookupIntervalToken, ValidTokens) {
    // Test valid ODBC interval token names
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_FRAC_SECOND"), "us");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_SECOND"), "s");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_MINUTE"), "m");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_HOUR"), "h");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_DAY"), "d");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_WEEK"), "w");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_MONTH"), "mon");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_QUARTER"), "qtr");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_YEAR"), "y");
}

TEST(LookupIntervalToken, CaseInsensitive) {
    // Test case insensitivity
    EXPECT_STREQ(lookupIntervalToken("sql_tsi_day"), "d");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_DAY"), "d");
    EXPECT_STREQ(lookupIntervalToken("Sql_Tsi_Day"), "d");
    EXPECT_STREQ(lookupIntervalToken("sql_tsi_month"), "mon");
    EXPECT_STREQ(lookupIntervalToken("SQL_TSI_MONTH"), "mon");
}

TEST(LookupIntervalToken, InvalidToken) {
    // Test invalid/unknown token names return nullptr
    EXPECT_EQ(lookupIntervalToken("SQL_TSI_INVALID"), nullptr);
    EXPECT_EQ(lookupIntervalToken("INVALID"), nullptr);
    EXPECT_EQ(lookupIntervalToken(""), nullptr);
    EXPECT_EQ(lookupIntervalToken("SQL_TSI_"), nullptr);
}

TEST(LookupIntervalToken, EmptyString) {
    // Test empty string input
    EXPECT_EQ(lookupIntervalToken(""), nullptr);
}

TEST(LookupIntervalToken, PartialMatch) {
    // Test that partial matches don't work
    EXPECT_EQ(lookupIntervalToken("SQL_TSI_DA"), nullptr);
    EXPECT_EQ(lookupIntervalToken("DAY"), nullptr);
    EXPECT_EQ(lookupIntervalToken("SQL_TSI_MON"), nullptr);
}

TEST(LookupIntervalToken, ExtraCharacters) {
    // Test with extra characters
    EXPECT_EQ(lookupIntervalToken("SQL_TSI_DAY "), nullptr);
    EXPECT_EQ(lookupIntervalToken(" SQL_TSI_DAY"), nullptr);
    EXPECT_EQ(lookupIntervalToken("SQL_TSI_DAY_EXTRA"), nullptr);
}

TEST(LookupIntervalToken, AllTokensReturnNonNull) {
    // Verify all tokens in the mapping return valid values
    const char *tokens[] = {
        "SQL_TSI_FRAC_SECOND", "SQL_TSI_SECOND",  "SQL_TSI_MINUTE",
        "SQL_TSI_HOUR",        "SQL_TSI_DAY",     "SQL_TSI_WEEK",
        "SQL_TSI_MONTH",       "SQL_TSI_QUARTER", "SQL_TSI_YEAR"};

    for (const char *token : tokens) {
        const char *result = lookupIntervalToken(token);
        EXPECT_NE(result, nullptr)
            << "Token '" << token << "' should have a mapping";
        EXPECT_NE(result[0], '\0')
            << "Token '" << token << "' should not map to empty string";
    }
}
