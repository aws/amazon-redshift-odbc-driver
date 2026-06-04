#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include "common.h"
#include <optional>
#include <rsodbc.h>
#include <rsutil.h>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

#define FLOAT_EPSILON_FACTOR_T 1e-6

// SQL State Error Codes
#define SQL_STATE_STRING_DATA_RIGHT_TRUNCATION "01004"  // String data, right truncation
#define SQL_STATE_NUMERIC_VALUE_OUT_OF_RANGE   "22003"  // Numeric value out of range

// Helper function declaration for validating floating point expected results
// Uses relative epsilon approach to handle different magnitudes of values
void validateFloatingPoint(
    const char* result, 
    const std::string& expectedValue, 
    const std::string& testName);
    
// Helper function declaration for validating binary data
// Compares binary data byte-by-byte after validating lengths match
void validateBinary(
    const char* result,
    size_t resultLen,
    const std::string& expectedValue,
    const std::string& testName);

// Helper function declaration for validating string data
// Handles quote removal (both single and double quotes) and string comparison
void validateString(
    const char* result,
    const std::string& expectedValue,
    const std::string& testName);

// Generic test‐parameter struct for C to SQL data type conversion testing
struct C2SqlTestParam {
    std::string testName;          // Unique name for each test case
    std::vector<uint8_t> rawBytes; // Raw binary representation of the C data to convert
    SQLLEN dataLen;                // Length indicator: actual length, SQL_NTS, or SQL_NULL_DATA
    short hCType;                  // C data type identifier (e.g., SQL_C_CHAR, SQL_C_LONG)
    short hSQLType;                // Target SQL data type (e.g., SQL_VARCHAR, SQL_INTEGER)
    short hPrepSQLType;            // Expected SQL type from SQLDescribeParam
    std::optional<std::string> expectedValue; // Expected output string after conversion
    std::optional<std::string> expectedState; // Expected SQLSTATE if error occurs
};

// Base test fixture class for C to SQL conversion tests
// Inherits from Google Test's TestWithParam using C2SqlTestParam
class TestC2SqlConversion : public ::testing::TestWithParam<C2SqlTestParam> {
  protected:
    // Main test execution method that runs the conversion test with given
    // parameters
    void runConversionTest(const C2SqlTestParam &p);
};
