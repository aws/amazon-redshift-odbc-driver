/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include "common.h"
#include "data_conversion_constants.h"
#include <iomanip>
#include <optional>
#include <rsodbc.h>
#include <rsutil.h>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

#define CLONE_CHAR_TESTS_TO_SQLTYPE(suiteName, testClass, baseVector,          \
                                    newSqlType)                                \
    INSTANTIATE_TEST_SUITE_P(                                                  \
        suiteName, testClass,                                                  \
        ::testing::ValuesIn(                                                   \
            CloneTestsForSqlType(baseVector, newSqlType, #newSqlType)),        \
        [](const ::testing::TestParamInfo<testClass::ParamType> &info) {       \
            return info.param.testName;                                        \
        })

// Generic test structure for SQL to C data type conversion tests
struct Sql2CTestParam {
    std::string testName;         // Unique name for the test case
    std::string colValue;         // Input string to convert
    SQLSMALLINT sqlType;          // SQL source data type
    SQLSMALLINT cType;            // C target data type
    bool isNull;                  // Whether to treat input as NULL
    SQLRETURN expectedReturnCode; // Expected return code from conversion
    std::string expectedValueStr; // Expected value after conversion (string
                                  // representation)
    std::optional<std::string>
        expectedState;                  // Expected SQLSTATE if error expected
    std::optional<SQLLEN> bufferLength; // Custom buffer size for C data type
};

// Base test class
class TestSql2Cconversion : public ::testing::TestWithParam<Sql2CTestParam> {
  protected:
    void runConversionTest(const Sql2CTestParam &param);
};

// Derived test for specific SQL types
class SQLBitConversionTest : public TestSql2Cconversion {};
class SQLBigIntConversionTest : public TestSql2Cconversion {};
class SQLCharConversionTest : public TestSql2Cconversion {};
class SQLVarcharConversionTest : public TestSql2Cconversion {};
class SQLLongVarcharConversionTest : public TestSql2Cconversion {};
class SQLDoubleConversionTest : public TestSql2Cconversion {};
class SQLFloatConversionTest : public TestSql2Cconversion {};
class SQLIntegerConversionTest : public TestSql2Cconversion {};
class SQLTinyIntConversionTest : public TestSql2Cconversion {};
class SQLRealConversionTest : public TestSql2Cconversion {};
class SQLSmallIntConversionTest : public TestSql2Cconversion {};
class SQLBinaryConversionTest : public TestSql2Cconversion {};
class SQLDateConversionTest : public TestSql2Cconversion {};
class SQLTimestampConversionTest : public TestSql2Cconversion {};
class SQLTimeConversionTest : public TestSql2Cconversion {};
class SQLDecimalConversionTest : public TestSql2Cconversion {};
class SQLNumericConversionTest : public TestSql2Cconversion {};
class SQLIntervalYearToMonthConversionTest : public TestSql2Cconversion {};
class SQLIntervalDayToSecondConversionTest : public TestSql2Cconversion {};

// === Utility Functions ===
std::string getCurrentDateStr();
std::string getCurrentDateWithTime(const std::string &timeStr,
                                   bool addFraction);
SQLLEN getResultSizeForCType(const Sql2CTestParam &param);

/**
 * @brief Creates a new set of test parameters based on existing tests but with
 * a different SQL type
 *
 * @param baseTests Vector of original test parameters to clone
 * @param newSqlType The new SQL type to apply to all cloned tests
 * @param suffix String suffix to append to test names for identification
 *
 * @return std::vector<Sql2CTestParam> Vector of cloned test parameters with
 * updated SQL type
 *
 * @details This utility function makes it easy to reuse test cases across
 * multiple SQL data types. Each test case in the base tests is copied with its
 * name extended by the provided suffix, and its SQL type changed to the
 * specified new type. This avoids redundant test definitions while ensuring
 * comprehensive coverage across SQL types.
 */
inline std::vector<Sql2CTestParam>
CloneTestsForSqlType(const std::vector<Sql2CTestParam> &baseTests,
                     SQLSMALLINT newSqlType, const std::string &suffix) {
    std::vector<Sql2CTestParam> cloned;
    for (const auto &test : baseTests) {
        auto copy = test;
        copy.testName += "_" + suffix;
        copy.sqlType = newSqlType;
        cloned.push_back(copy);
    }
    return cloned;
}
