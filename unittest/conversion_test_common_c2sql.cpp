#include "conversion_test_common_c2sql.h"
#include <cstring>

// Main test function that performs the conversion and validation
// This function tests the conversion from C data types to SQL data types
// by calling the convertCParamDataToSQLData function and validating the results
void TestC2SqlConversion::runConversionTest(const C2SqlTestParam &testParam) {

    // Initialize test structures needed for the conversion function
    RS_STMT_INFO stmtInfo = {0};
    RS_STMT_INFO *stmtInfoPtr = &stmtInfo;
    RS_BIND_PARAM_STR_BUF buf = {0}; // Buffer to store conversion results
    int convErr = 0; // Will be set to TRUE if conversion error occurs
    stmtInfoPtr->pErrorList = nullptr; // Initialize error list to null

    // Set length indicator from test parameter
    // This can be SQL_NULL_DATA, SQL_NTS, or a specific length value
    SQLLEN lenInd = testParam.dataLen;

    // Prepare input data pointer - only set if we have non-NULL data
    // For NULL data, we'll pass nullptr to the conversion function
    char *dataPtr = nullptr;
    std::string dataCopy = {0};

    if (testParam.dataLen != SQL_NULL_DATA && !testParam.rawBytes.empty()) {
        // We use raw bytes to ensure proper memory representation of various
        // data types and to simulate how real ODBC applications pass data to
        // the driver

        // Copy binary data to string for the conversion function
        dataCopy.assign(
            reinterpret_cast<const char *>(testParam.rawBytes.data()),
            testParam.rawBytes.size());
        dataPtr = &dataCopy[0]; // Get modifiable pointer to string data
    }

    // Invoke the C to SQL conversion function
    // This is the main function being tested - it converts C data types to SQL
    // data types The function expects raw memory data in the format that would
    // be passed by an ODBC application For example, SQL_C_LONG would be a
    // 4-byte integer value in memory, SQL_C_DOUBLE would be an 8-byte IEEE-754
    // floating point value, and SQL_C_CHAR would be a null-terminated string
    char *result = convertCParamDataToSQLData(
        stmtInfoPtr, // Statement info structure for error reporting
        dataPtr,     // Pointer to the input data (raw bytes in memory)
        static_cast<int>(testParam.dataLen), // Length of the input data or
                                             // SQL_NULL_DATA/SQL_NTS
        &lenInd,          // Pointer to length indicator (may be modified by the
                          // function)
        testParam.hCType, // C data type of the input data (e.g., SQL_C_CHAR,
                          // SQL_C_LONG)
        testParam.hSQLType, // SQL data type to convert to (e.g., SQL_VARCHAR,
                            // SQL_INTEGER)
        testParam.hPrepSQLType, // Expected SQL type from SQLDescribeParam
        &buf,    // Buffer to store the conversion result as a string for SQL
        nullptr, // RS_DESC_REC *pDescRec - not used in tests
        &convErr // Will be set to TRUE if conversion error occurs
    );

    // Special handling for NULL input data
    // For NULL inputs, we expect the result to be nullptr and no conversion
    // error
    if (testParam.dataLen == SQL_NULL_DATA) {
        EXPECT_EQ(result, nullptr)
            << testParam.testName << ": NULL input should return nullptr";
        EXPECT_EQ(convErr, FALSE)
            << testParam.testName << ": NULL input should not set error";

        // Clean up allocated buffer if used
        if (buf.iAllocDataLen > 0) {
            free(buf.pBuf);
        }
        return;
    }

    // Handle cases where we expect specific SQL states/errors
    // We check SQL states before values because errors take precedence and
    // determine how to handle the result (continue, skip validation, or exit)
    if (testParam.expectedState.has_value()) {
        // Verify error list exists when we expect an SQL state
        ASSERT_TRUE(stmtInfoPtr->pErrorList &&
                    stmtInfoPtr->pErrorList->szSqlState[0] != '\0')
            << testParam.testName << ": Expected SQL state but no state found";

        // Extract and verify the actual SQL state
        std::string actualState(stmtInfoPtr->pErrorList->szSqlState,
                                SQL_SQLSTATE_SIZE);
        ASSERT_EQ(actualState, testParam.expectedState.value())
            << testParam.testName << ": SQL state mismatch"
            << "\nExpected State: " << testParam.expectedState.value()
            << "\nActual state: " << actualState;

        // Handle specific SQL states differently based on their meaning
        if (testParam.expectedState.value() ==
            SQL_STATE_STRING_DATA_RIGHT_TRUNCATION) { // String data, right
                                                      // truncation
            // For truncation warnings, we still expect valid data to be
            // returned even though some data was truncated
            ASSERT_NE(result, nullptr)
                << testParam.testName << ": Truncation case should return data";
        } else if (testParam.expectedState.value() ==
                   SQL_STATE_NUMERIC_VALUE_OUT_OF_RANGE) { // Numeric value out
                                                           // of range
            // For numeric overflow errors, we expect nullptr result and error
            // flag set
            EXPECT_EQ(result, nullptr)
                << testParam.testName
                << ": Numeric overflow should return nullptr";
            ASSERT_NE(convErr, SQL_TRUE)
                << testParam.testName << ": Numeric overflow should set error";

            // Clean up allocated buffer if used
            if (buf.iAllocDataLen > 0) {
                free(buf.pBuf);
            }
            return;
        } else {
            // Placeholder for handling other SQL states
            // Add more specific handling as needed for other error conditions
        }
    }

    // For successful conversions, verify no errors occurred
    ASSERT_EQ(convErr, FALSE)
        << testParam.testName << ": Unexpected conversion error";

    // Validate converted value against expected value
    // This section compares the actual conversion result with the expected
    // value using type-specific comparison methods The result from
    // convertCParamDataToSQLData is always a string representation that can be
    // used in SQL statements (e.g., '123', '2023-01-01', etc.)
    if (testParam.expectedValue.has_value()) {
        switch (testParam.hCType) {
        case SQL_C_FLOAT:
        case SQL_C_DOUBLE: {
            validateFloatingPoint(result, *testParam.expectedValue,
                                  testParam.testName);
            break;
        }

        case SQL_C_BINARY: {
            size_t resultLen = static_cast<size_t>(testParam.dataLen);
            validateBinary(result, resultLen, *testParam.expectedValue,
                           testParam.testName);
            break;
        }

        default: {
            // For other types, do direct string comparison
            // The convertCParamDataToSQLData function converts most C types to
            // SQL string literals For example, SQL_C_CHAR "test" becomes
            // 'test', SQL_C_LONG 123 becomes '123'
            validateString(result, *testParam.expectedValue,
                           testParam.testName);
            break;
        }
        }
    }
    // Cleanup: Free allocated buffer if used
    if (buf.iAllocDataLen > 0) {
        free(buf.pBuf);
    }
}

// Helper function to validate floating point expected results
// Uses relative epsilon approach to handle different magnitudes of values
// The actual epsilon used is calculated as: |expected_value| *
// FLOAT_EPSILON_FACTOR_T Examples:
// - For expected=12.0: epsilon = 12.0 * 1e-6 = 0.000012
// - For expected=12.12345: epsilon = 12.12345 * 1e-6 = 0.00001212345
void validateFloatingPoint(const char *result, const std::string &expectedValue,
                           const std::string &testName) {

    // Convert strings to double values
    double actual, expected;
    try {
        actual = std::stod(result);
        expected = std::stod(expectedValue);
    } catch (const std::exception &) {
        FAIL() << testName << ": Failed to convert string to double";
        return;
    }

    // Calculate epsilon based on the magnitude of the expected value
    double epsilon = std::abs(expected) * FLOAT_EPSILON_FACTOR_T;

    // Use EXPECT_NEAR for the comparison with appropriate error message
    EXPECT_NEAR(actual, expected, epsilon)
        << testName << "\nExpected: " << expectedValue
        << "\nActual: " << result;
}

// Helper function to validate binary data
// Compares binary data byte-by-byte after validating lengths match
void validateBinary(const char *result, size_t resultLen,
                    const std::string &expectedValue,
                    const std::string &testName) {

    // First check that lengths match
    size_t expectedLen = expectedValue.length();
    ASSERT_EQ(resultLen, expectedLen) << testName << ": Binary length mismatch";

    // memcmp performs a byte-by-byte comparison of the binary data
    EXPECT_EQ(memcmp(result, expectedValue.c_str(), resultLen), 0)
        << testName << ": Binary content mismatch";
}

// Helper function to validate string data
// Handles quote removal (both single and double quotes) and string comparison
void validateString(const char *result, const std::string &expectedValue,
                    const std::string &testName) {

    std::string actualValue(result);

    // Strip surrounding quotes if present (both single and double quotes)
    // Longvarchar strings are often returned with surrounding quotes that need
    // to be removed for proper comparison with our expected values
    if (actualValue.size() >= 2) {
        // Check for either single quotes or double quotes
        if ((actualValue.front() == '\'' && actualValue.back() == '\'') ||
            (actualValue.front() == '"' && actualValue.back() == '"')) {
            actualValue = actualValue.substr(1, actualValue.size() - 2);
        }
    }

    // Compare the actual value with the expected value
    EXPECT_EQ(actualValue, expectedValue)
        << testName << "\nExpected: " << expectedValue
        << "\nActual: " << actualValue;
}
