/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#pragma once
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Detect C++ standard library
#if defined(_MSC_VER) && !defined(__clang__)
  #define USING_MSVC_STL 1       // MSVC STL (Windows / MSVC)
#else
  #define USING_MSVC_STL 0
#endif

#if defined(_LIBCPP_VERSION)
  #define USING_LIBCXX 1         // libc++ (Apple/LLVM, common on macOS)
#else
  #define USING_LIBCXX 0
#endif

#if defined(__GLIBCXX__) && !USING_LIBCXX
  #define USING_LIBSTDCXX 1      // libstdc++ (GNU, common on Linux)
#else
  #define USING_LIBSTDCXX 0
#endif
#define SQLWCHAR_LITERAL(c) ((SQLWCHAR)(c))


// Constants for numeric conversions
constexpr double FLOAT_EPSILON_FACTOR_T = 1e-6;

// Constants for date/time handling
constexpr int STRFTIME_BUFFER_SIZE_T = 80; // Buffer size for strftime
constexpr int TM_YEAR_OFFSET_T = 1900;     // tm_year is years since 1900
constexpr int TM_MONTH_OFFSET_T = 1; // tm_mon is 0-11, but months are 1-12
constexpr int TEST_TEMP_BUF_MAX_LEN = 512; // Maximum buffer size for temporary string operations
// Constant for microseconds formatting
constexpr int FRACTION_WIDTH_T = 6;

// Date and time format macros
#define DATE_FORMAT "%Y-%m-%d"
#define TIME_FORMAT "%H:%M:%S"
#define TIMESTAMP_FORMAT DATE_FORMAT " " TIME_FORMAT

// Platform-specific string type for wide character handling
#if defined(_MSC_VER) || defined(LINUX) || defined(__linux__) ||               \
    defined(__APPLE__) || defined(__MACH__)
using test_string_t = std::u16string;
#else
using test_string_t = std::wstring;
#endif

// ByteConverter class to handle conversion of various data types to byte vector
// representation
class ByteConverter {
  public:
    // Template function to convert any data type to a byte vector
    // Used for converting C data types to raw bytes for testing
    template <typename T> static std::vector<uint8_t> fromType(const T &v) {
        std::vector<uint8_t> bytes(sizeof(T));
        memcpy(bytes.data(), &v, sizeof(T));
        return bytes;
    }

    // Helper function specifically for handling char16_t strings
    // Converts a char16_t string to its byte representation including null
    // terminator
    static std::vector<uint8_t> fromChar16(const char16_t *str) {
        size_t len = std::char_traits<char16_t>::length(str);
        size_t byteLen = (len + 1) * sizeof(char16_t); // +1 for null terminator
        std::vector<uint8_t> bytes(byteLen);
        memcpy(bytes.data(), str, byteLen);
        return bytes;
    }
};

// Template function to convert any data type to a byte vector
// Used for converting C data types to raw bytes for testing
// Maintained for backward compatibility - delegates to ByteConverter
template <typename T> static std::vector<uint8_t> bytesOf(const T &v) {
    return ByteConverter::fromType(v);
}

// Helper function specifically for handling char16_t strings
// Converts a char16_t string to its byte representation including null
// terminator Maintained for backward compatibility - delegates to ByteConverter
static std::vector<uint8_t> char16Bytes(const char16_t *str) {
    return ByteConverter::fromChar16(str);
}