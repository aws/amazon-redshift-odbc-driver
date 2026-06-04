/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include "conversion_test_common_sql2c.h"
#include <ctime> // For strftime
#include <locale>
#include <map>
#include <rsunicode.h>

// Static map of C data types to their buffer sizes
static const std::map<SQLSMALLINT, SQLLEN> cTypeToBufferSize = {
    // 1-byte types
    {SQL_C_BIT, sizeof(uint8_t)},
    {SQL_C_TINYINT, sizeof(uint8_t)},
    {SQL_C_STINYINT, sizeof(uint8_t)},
    {SQL_C_UTINYINT, sizeof(uint8_t)},

    // 2-byte types
    {SQL_C_SHORT, sizeof(short)},
    {SQL_C_SSHORT, sizeof(short)},
    {SQL_C_USHORT, sizeof(unsigned short)},

    // 4-byte types
    {SQL_C_LONG, sizeof(int32_t)},
    {SQL_C_SLONG, sizeof(int32_t)},
    {SQL_C_ULONG, sizeof(uint32_t)},

    // 8-byte types
    {SQL_C_SBIGINT, sizeof(int64_t)},
    {SQL_C_UBIGINT, sizeof(uint64_t)},

    // Floating point types
    {SQL_C_FLOAT, sizeof(float)},
    {SQL_C_DOUBLE, sizeof(double)},

    // String types
    {SQL_C_CHAR, CHAR_BUFFER_SIZE_T},
    {SQL_C_BINARY, CHAR_BUFFER_SIZE_T},
    {SQL_C_WCHAR, UTF16_BUFFER_SIZE_T},

    // Structured types
    {SQL_C_NUMERIC, sizeof(SQL_NUMERIC_STRUCT)},
    {SQL_C_TYPE_DATE, sizeof(SQL_DATE_STRUCT)},
    {SQL_C_DATE, sizeof(SQL_DATE_STRUCT)},
    {SQL_C_TYPE_TIME, sizeof(SQL_TIME_STRUCT)},
    {SQL_C_TIME, sizeof(SQL_TIME_STRUCT)},
    {SQL_C_TYPE_TIMESTAMP, sizeof(SQL_TIMESTAMP_STRUCT)},
    {SQL_C_TIMESTAMP, sizeof(SQL_TIMESTAMP_STRUCT)},
    {SQL_C_INTERVAL_YEAR_TO_MONTH, sizeof(SQL_YEAR_MONTH_STRUCT)},
    {SQL_C_INTERVAL_DAY_TO_SECOND, sizeof(SQL_DAY_SECOND_STRUCT)}};

/**
 * Tests the conversion from SQL data types to C data types with various
 * parameters and validates the results against expected values.
 *
 * @param param Test parameters containing input values and expected results
 */
void TestSql2Cconversion::runConversionTest(const Sql2CTestParam &param) {
    // Union to hold converted values for different data types
    // Allows efficient memory usage by sharing space between different type
    // representations.
    union {
        uint8_t u8;
        int8_t i8;
        short i16;
        unsigned short u16;
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
        char str[CHAR_BUFFER_SIZE_T];
        char16_t utf16str[UTF16_BUFFER_SIZE_T];
        SQL_NUMERIC_STRUCT numeric;
        SQL_DATE_STRUCT date;
        SQL_TIME_STRUCT time;
        SQL_TIMESTAMP_STRUCT timestamp;
        SQL_INTERVAL_STRUCT interval;
        SQL_YEAR_MONTH_STRUCT y2m_interval;
        SQL_DAY_SECOND_STRUCT d2s_interval;
        // Additional data types can be added here as needed for future test
        // cases
    } resultBuf;

    void *pResult = &resultBuf;
    SQLLEN resultSize = getResultSizeForCType(param);
    if (resultSize == -1) {
        FAIL() << "Unsupported C data type: " << param.cType;
        return;
    }

    // Prepare input data, handling null cases
    char *colData =
        param.isNull ? nullptr : const_cast<char *>(param.colValue.c_str());
    int dataLen = param.isNull ? 0 : static_cast<int>(param.colValue.length());

    // Initialize statement info and length indicators
    RS_STMT_INFO stmtInfo = {0};
    RS_STMT_INFO *stmt = &stmtInfo;
    SQLLEN lengthIndicator = 0;
    SQLLEN cbOffset = 0;

    SQLRETURN rc = convertSQLDataToCData(
        stmt,             // Statement info
        colData,          // Input data buffer
        dataLen,          // Length of input data
        param.sqlType,    // SQL data type to convert from
        pResult,          // Output buffer for converted data
        resultSize,      // Size of output buffer
        &cbOffset,        // Offset in the buffer
        &lengthIndicator, // Pointer to store length indicator
        param.cType,      // C data type to convert to
        0,                // hRsSpecialType is not applicable here
        TEXT_FORMAT,      // Format of the data (text vs binary)
        nullptr           // Descriptor record is not required here
    );

    // Verify conversion result matches expected return code
    ASSERT_EQ(rc, param.expectedReturnCode) << "Failed for: " << param.testName;

    // Verify SQLSTATE if expected
    if (param.expectedState.has_value()) {
        ASSERT_NE(stmt->pErrorList, nullptr)
            << "Expected SQLSTATE but pErrorList is null for: "
            << param.testName;
        std::string actualState((char *)stmt->pErrorList->szSqlState, 5);
        EXPECT_EQ(actualState, param.expectedState.value())
            << "SQLSTATE mismatch in: " << param.testName;
    }

    // Flag to indicate whether to skip the final string comparison assertion
    // Set to true for data types that handle their own assertions (binary, wide
    // strings)
    bool skipFinalStrAssert = false;

    // If conversion succeeded and input wasn't null, verify the converted value
    if ((rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) && !param.isNull) {
        std::string actualStr;

        // Convert the result to string for comparison, handling each C data
        // type
        switch (param.cType) {
        case SQL_C_BIT:
        case SQL_C_UTINYINT:
            actualStr = std::to_string(resultBuf.u8);
            break;
        case SQL_C_TINYINT:
        case SQL_C_STINYINT:
            actualStr = std::to_string(resultBuf.i8);
            break;
        case SQL_C_SHORT:
        case SQL_C_SSHORT:
            actualStr = std::to_string(resultBuf.i16);
            break;
        case SQL_C_USHORT:
            actualStr = std::to_string(resultBuf.u16);
            break;
        case SQL_C_LONG:
        case SQL_C_SLONG:
            actualStr = std::to_string(resultBuf.i32);
            break;
        case SQL_C_ULONG:
            actualStr = std::to_string(resultBuf.u32);
            break;
        case SQL_C_SBIGINT:
            actualStr = std::to_string(resultBuf.i64);
            break;
        case SQL_C_UBIGINT:
            actualStr = std::to_string(resultBuf.u64);
            break;
        case SQL_C_FLOAT:
            actualStr = std::to_string(resultBuf.f);
            break;
        case SQL_C_DOUBLE:
            actualStr = std::to_string(resultBuf.d);
            break;
        case SQL_C_CHAR:
            actualStr = std::string(
                resultBuf.str, strnlen(resultBuf.str, sizeof(resultBuf.str)));
            break;
        case SQL_C_BINARY: {
            // Skip the default string comparison since binary data requires
            // special handling. Binary data may contain non-printable characters
            // or null bytes that can't be meaningfully compared as strings. We
            // need to compare the raw byte values directly.
            skipFinalStrAssert = true;

            ASSERT_GE(resultSize, lengthIndicator)
                << "Result buffer is smaller than lengthIndicator bytes for: "
                << param.testName;

            // Extract the actual binary bytes from the result buffer
            std::vector<uint8_t> actualBytes(resultBuf.str,
                                             resultBuf.str + lengthIndicator);
            std::vector<uint8_t> expectedBytes;

            // Convert expected string to byte array
            for (size_t i = 0; i < param.expectedValueStr.length(); i += 2) {
                std::string byteStr = param.expectedValueStr.substr(i, 2);
                uint8_t byte =
                    static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
                expectedBytes.push_back(byte);
            }

            ASSERT_EQ(actualBytes.size(), expectedBytes.size())
                << "Binary data length mismatch for: " << param.testName;

            // Instead of the byte-by-byte loop:
            ASSERT_EQ(actualBytes, expectedBytes)
                << "Binary data mismatch for: " << param.testName;

            break;
        }
        case SQL_C_WCHAR: {
            // Skip the default string comparison since wide character strings
            // require special handling. Wide character strings use different
            // encoding (UTF-16) than regular strings (UTF-8), so we need to
            // convert the expected UTF-8 string to UTF-16 before comparison.
            // Additionally, we need to compare by character count rather than
            // byte count.
            skipFinalStrAssert = true;

            // First verify if the buffer size is sufficient, or did the server
            // return more than the buffer size
            ASSERT_GE(resultSize, lengthIndicator)
                << "Wide character data buffer overflow for: "
                << param.testName;

            // Convert expected UTF-8 string to UTF-16
            test_string_t expectedU16Str;
            char_utf8_to_utf16_str(
                param.expectedValueStr.c_str(),
                static_cast<int>(param.expectedValueStr.length()),
                expectedU16Str);

            // Get number of characters (not bytes)
            size_t numChars = lengthIndicator / sizeof(char16_t);

            // Create string view of the actual data
            test_string_t actualStr(resultBuf.utf16str, numChars);

            EXPECT_EQ(actualStr, expectedU16Str)
                << "String mismatch for: " << param.testName;

            break;
        }
        case SQL_C_NUMERIC: {
            // Special handling for SQL_NUMERIC_STRUCT
            // SQL_NUMERIC_STRUCT stores numbers in a binary format that needs
            // to be converted to a decimal string for comparison. We use the
            // library function convertScaledIntegerToNumericString to handle
            // this conversion.

            const SQL_NUMERIC_STRUCT &actual = resultBuf.numeric;

            // Create a non-const copy of the numeric struct for the conversion
            // function
            SQL_NUMERIC_STRUCT numericCopy = actual;

            // Buffer to hold the string representation
            char numBuffer[MAX_NUMBER_BUF_LEN] = {0};

            // Convert the SQL_NUMERIC_STRUCT to a string
            convertScaledIntegerToNumericString(&numericCopy, numBuffer,
                                                MAX_NUMBER_BUF_LEN);

            actualStr = std::string(numBuffer);
            break;
        }
        case SQL_C_DATE:
        case SQL_C_TYPE_DATE: {
            // Convert SQL_DATE_STRUCT to struct tm
            const SQL_DATE_STRUCT &date = resultBuf.date;
            struct tm timeInfo = {0};
            timeInfo.tm_year =
                date.year - TM_YEAR_OFFSET_T; // tm_year is years since 1900
            timeInfo.tm_mon = date.month - TM_MONTH_OFFSET_T; // tm_mon is 0-11
            timeInfo.tm_mday = date.day;

            char buffer[STRFTIME_BUFFER_SIZE_T];
            strftime(buffer, sizeof(buffer), DATE_FORMAT, &timeInfo);
            actualStr = std::string(buffer);
            break;
        }

        case SQL_C_TIME:
        case SQL_C_TYPE_TIME: {
            // Convert SQL_TIME_STRUCT to struct tm
            const SQL_TIME_STRUCT &time = resultBuf.time;
            struct tm timeInfo = {0};
            timeInfo.tm_hour = time.hour;
            timeInfo.tm_min = time.minute;
            timeInfo.tm_sec = time.second;

            char buffer[STRFTIME_BUFFER_SIZE_T];
            strftime(buffer, sizeof(buffer), TIME_FORMAT, &timeInfo);
            actualStr = std::string(buffer);
            break;
        }

        case SQL_C_TIMESTAMP:
        case SQL_C_TYPE_TIMESTAMP: {
            // Convert SQL_TIMESTAMP_STRUCT to struct tm
            const SQL_TIMESTAMP_STRUCT &ts = resultBuf.timestamp;
            struct tm timeInfo = {0};
            timeInfo.tm_year =
                ts.year - TM_YEAR_OFFSET_T; // tm_year is years since 1900
            timeInfo.tm_mon = ts.month - TM_MONTH_OFFSET_T; // tm_mon is 0-11
            timeInfo.tm_mday = ts.day;
            timeInfo.tm_hour = ts.hour;
            timeInfo.tm_min = ts.minute;
            timeInfo.tm_sec = ts.second;

            // Format date and time with strftime
            char buffer[STRFTIME_BUFFER_SIZE_T];
            strftime(buffer, sizeof(buffer), TIMESTAMP_FORMAT, &timeInfo);

            // Append fraction separately since strftime doesn't support
            // microsecond formatting The C standard library's strftime function
            // only handles up to seconds precision, so we need to manually
            // append the microseconds part using stringstream
            std::stringstream ss;
            ss << buffer << "." << std::setfill('0')
               << std::setw(FRACTION_WIDTH_T) << ts.fraction;
            actualStr = ss.str();
            break;
        }
        // TODO (psubhku) : The logic of comparison should be updated when
        // original SIM (https://issues.amazon.com/issues/Redshift-115437) gets
        // fixed
        case SQL_C_INTERVAL_YEAR_TO_MONTH: {
            // Format as string without leading zeros
            std::stringstream ss;
            ss << resultBuf.y2m_interval.year << "-"
               << resultBuf.y2m_interval.month;
            actualStr = ss.str();
            break;
        }
        // TODO (psubhku) : The logic of comparison should be updated when
        // original SIM (https://issues.amazon.com/issues/Redshift-115437) gets
        // fixed
        case SQL_C_INTERVAL_DAY_TO_SECOND: {
            // Format as string
            std::stringstream ss;
            ss << resultBuf.d2s_interval.day << " " // No leading zeros for days
               << std::setfill('0') << std::setw(2)
               << resultBuf.d2s_interval.hour << ":" << std::setfill('0')
               << std::setw(2) << resultBuf.d2s_interval.minute << ":"
               << std::setfill('0') << std::setw(2)
               << resultBuf.d2s_interval.second << "." << std::setfill('0')
               << std::setw(6) << resultBuf.d2s_interval.fraction;
            actualStr = ss.str();
            break;
        }
        default:
            FAIL() << "Unsupported CType in value check: " << param.cType;
        }
        if (!skipFinalStrAssert) {
            if (param.cType == SQL_C_DOUBLE || param.cType == SQL_C_FLOAT) {
                double expected = std::stod(param.expectedValueStr);
                double actual =
                    (param.cType == SQL_C_DOUBLE) ? resultBuf.d : resultBuf.f;
                // Special handling for floating-point comparisons
                // Floating-point values (like 1.1) can't always be represented
                // exactly in binary. For example, 0.1 might be stored as
                // 0.10000000000000001, causing direct equality checks to fail.
                // We use an epsilon-based comparison instead, where the allowed
                // difference scales with the value's size.
                double epsilon = std::abs(expected) *
                                 FLOAT_EPSILON_FACTOR_T; // Relative precision
                ASSERT_NEAR(actual, expected, epsilon)
                    << "Floating-point mismatch for: " << param.testName
                    << "\n  Expected: " << expected
                    << "\n  Actual:   " << actual;
            } else {
                // Direct string comparison for other types
                ASSERT_EQ(actualStr, param.expectedValueStr)
                    << "Value mismatch for: " << param.testName
                    << "\n  Expected: " << param.expectedValueStr
                    << "\n  Actual:   " << actualStr;
            }
        }
    }
}

/**
 * @brief Gets the current date as a formatted string
 *
 * @return std::string Date string in "YYYY-MM-DD" format
 *
 * @details Uses local time and formats the date with zero-padded values:
 *          - Year is padded to 4 digits
 *          - Month and day are padded to 2 digits
 */
std::string getCurrentDateStr() {
    time_t now = time(0);
    struct tm ltm;

#ifdef WIN32
    // Windows thread-safe version
    if (localtime_s(&ltm, &now) != 0) {
        throw std::runtime_error("Failed to get local time");
    }
#elif defined(LINUX) || defined(__linux__) || defined(__APPLE__) ||            \
    defined(__MACH__)
    // POSIX thread-safe version (Linux and macOS)
    if (localtime_r(&now, &ltm) == nullptr) {
        throw std::runtime_error("Failed to get local time");
    }
#endif

    char buffer[STRFTIME_BUFFER_SIZE_T];
    strftime(buffer, sizeof(buffer), DATE_FORMAT, &ltm);
    return std::string(buffer);
}

/**
 * @brief Combines current date with a time string
 *
 * @param timeStr The time string to append to the date
 * @param addFraction Whether to add microseconds (.000000) to the result
 * (default: true)
 * @return std::string Combined date and time string in "YYYY-MM-DD
 * HH:MM:SS[.000000]" format
 *
 * @details Combines the current date from getCurrentDateStr() with the provided
 * time string. Optionally adds microseconds precision if addFraction is true.
 */
std::string getCurrentDateWithTime(const std::string &timeStr,
                                   bool addFraction = true) {
    return getCurrentDateStr() + " " + timeStr + (addFraction ? ".000000" : "");
}

/**
 * @brief Determines the buffer size needed for a given C data type
 *
 * @param param Test parameters containing the C data type and optional buffer
 * length
 * @return SQLLEN The size of the buffer needed for the specified C data type
 *
 * @details If param.bufferLength is provided, that value is used directly.
 *          Otherwise, the size is determined by looking up the C data type
 *          in a static map.
 */
SQLLEN getResultSizeForCType(const Sql2CTestParam &param) {
    // If a custom buffer length is provided, use it
    if (param.bufferLength.has_value()) {
        return param.bufferLength.value();
    }

    // Look up the buffer size in the map
    auto it = cTypeToBufferSize.find(param.cType);
    if (it != cTypeToBufferSize.end()) {
        return it->second;
    }

    // Return -1 for unsupported types
    return -1;
}
