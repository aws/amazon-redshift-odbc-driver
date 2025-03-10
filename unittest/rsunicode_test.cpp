#include "common.h"
#include "rsunicode.h"#include <iostream>
#include <string>
#include <codecvt> // For std::wstring_convert (C++11 and later)

TEST(UNICODE_TEST_SUITE, basic_representation__not_a_test) {
    std::stringstream ss;
    std::string value1 = u8"Hello, 你好";
    std::string value2 = "Hello, 你好";
    ss << "value1: " << value1 << " " << value1.size() << std::endl;
    ss << "value2: " << value2 << " " << value2.size() << std::endl;
    
    ss << "Byte representation of value1: ";
    for (char c : value1) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    ss << "Byte representation of value2: ";
    for (char c : value2) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    // Convert std::string to std::wstring (UTF-16)
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring utf16_value1 = converter.from_bytes(value1);
    std::wstring utf16_value2 = converter.from_bytes(value2);
    
    // Output byte representations
    ss << "Byte representation of utf16_value1: ";
    for (char c : utf16_value1) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    ss << "Byte representation of utf16_value2: ";
    for (char c : utf16_value2) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    // Convert std::wstring (UTF-16) back to std::string (UTF-8)
    std::string utf8_value1 = converter.to_bytes(utf16_value1);
    std::string utf8_value2 = converter.to_bytes(utf16_value2);
    
    // Output byte representations
    ss << "Byte representation of utf8_value1: ";
    for (char c : utf8_value1) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    ss << "Byte representation of utf8_value2: ";
    for (char c : utf8_value2) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    ASSERT_EQ(0, memcmp(value1.data(), utf8_value1.data(), std::max<size_t>(utf8_value1.size(), value1.size()))) << ss.str();
    ASSERT_EQ(0, memcmp(value2.data(), utf8_value2.data(), std::max<size_t>(utf8_value2.size(), value2.size()))) << ss.str();
}

// tests are grouped by util function - each test validates multiple cases
TEST(UNICODE_TEST_SUITE, test_wchar16_to_utf8_char) {
    // UTF-16 encoded string: u"Hello, 你好"
    // Null terminated - converts as expected
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    std::string result = "Hello, 你好";
    char szStr[100] = "123"; // dirty
    size_t len =
        wchar16_to_utf8_char((const SQLWCHAR *)wszStr, SQL_NTS, szStr, 100);
    ASSERT_EQ(0, memcmp(szStr, result.data(), len));

    // UTF-16 encoded string: u"Hello, 你好"
    // Not Null terminated - converts as expected
    const unsigned short wszStr2[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                      0x002C, 0x0020, 0x4F60, 0x597D};
    len = wchar16_to_utf8_char((const SQLWCHAR *)wszStr2, 9, szStr, 100);
    ASSERT_EQ(13, len); // 你好 is 6 bytes not 2 (4 bytes exta) 9 - 2 + 6 = 13.
    ASSERT_EQ(0, memcmp(szStr, result.data(), len));
    ASSERT_EQ(0, result[len]);

    // Reusing buffer szStr is safe
    std::string result2 = "Hello, 你";
    len = wchar16_to_utf8_char((const SQLWCHAR *)wszStr2, 8, szStr, 8);
    ASSERT_EQ(0, memcmp(szStr, result2.data(), len));
    len = wchar16_to_utf8_char((const SQLWCHAR *)wszStr2, 8, szStr,
                               6);             // smaller buffer
    ASSERT_EQ(5, len);                         // -1 for null terminator
    ASSERT_EQ(0, memcmp(szStr, result2.data(), len)); // same check as below
    ASSERT_EQ(0, memcmp(szStr, "Hello", 5));
    ASSERT_EQ(0, szStr[5]);
}

TEST(UNICODE_TEST_SUITE, test_wchar16_to_utf8_str) {
    // UTF-16 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    std::string result = "Hello, 你好";
    std::string szStr;
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr, SQL_NTS, szStr);
    ASSERT_EQ(szStr, result);

    // UTF-16 encoded string: u"Hello, 你好"
    // Not Null terminated
    const unsigned short wszStr2[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                      0x002C, 0x0020, 0x4F60, 0x597D};
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr2, 9, szStr);
    ASSERT_EQ(szStr, result);
    // Reusing buffer szStr is safe
    result = "Hello, 你";
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr2, 8, szStr);
    ASSERT_EQ(szStr, result);
}

TEST(UNICODE_TEST_SUITE, test_char_utf8_to_utf16_str) {
    std::string szStr = "Hello, 你好";
    int cchLenBytes = 13; // "Hello, ":7 + 你:3 + 好:3
    int lenResultInCharacters = 9;  // "Hello, ":7 + 你:1 + 好:1
    std::u16string result = u"Hello, 你好";
    std::u16string wszStr;
    int len = char_utf8_to_utf16_str((const char *)szStr.data(), cchLenBytes, wszStr);
    ASSERT_EQ(lenResultInCharacters, len);

    std::stringstream ss;
    ss << "szStr: " << szStr << " " << szStr.size() << std::endl;
    ss << "wszStr size: " << wszStr.size() << std::endl;
    ss << "result size: " << result.size() << std::endl;
    
    ss << "Byte representation of wszStr: ";
    for (const char c : wszStr) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    ss << "Byte representation of result: ";
    for (char c : result) {
        ss << "0x" << std::hex << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    }
    ss << std::endl;
    
    ASSERT_EQ(0, memcmp(wszStr.data(), result.data(), cchLenBytes)) << ss.str();
    // Skip the last character (which is 3 bytes)
    len = char_utf8_to_utf16_str((const char *)szStr.data(), cchLenBytes - 3, wszStr);
    ASSERT_EQ(0, memcmp((char*)wszStr.data(), result.data(), cchLenBytes - 3)) << ss.str();
    ASSERT_EQ(lenResultInCharacters - 1, len) << ss.str();
}

TEST(UNICODE_TEST_SUITE, test_char_utf8_to_utf16_wchar) {
    // UTF-8 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned char szStr[] = {'H',  'e',  'l',  'l',  'o',  ',', ' ',
                                   0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
    size_t cchLenBytes = sizeof(szStr) / sizeof(szStr[0]);
    const unsigned short result[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D};
    size_t lenResult = sizeof(result) / sizeof(result[0]);
    unsigned short wszStr[50] = {0};
    // We want one character less : "Hello, 你" ( sizeof("Hello, ") + 3 = 10)
    int len = char_utf8_to_utf16_wchar((const char *)szStr, 10,
                                       (SQLWCHAR *)wszStr, 50);
    ASSERT_EQ(len, lenResult - 1);
    ASSERT_EQ(0, memcmp(wszStr, result, len)); // excluding null terminator
    ASSERT_EQ(0, wszStr[len]);                 // null terminator
}

TEST(UNICODE_TEST_SUITE, test_null_output) {
    int anyNumber = 10;
    std::vector<char> szStr(10, 'a');
    unsigned short *wszStrNULL = NULL;
    int len = char_utf8_to_utf16_wchar(szStr.data(), anyNumber,
                                       (SQLWCHAR *)wszStrNULL, anyNumber);
    ASSERT_EQ(len, 0);
    ASSERT_TRUE(wszStrNULL == NULL);

    char *szStrNULL = NULL;
    std::string utf8;
    std::vector<SQLWCHAR> wszStr(10, 'a');
    len = wchar16_to_utf8_char(wszStr.data(), anyNumber, szStrNULL, anyNumber);
    ASSERT_EQ(len, 0);
    ASSERT_TRUE(szStrNULL == NULL);
}

TEST(UNICODE_TEST_SUITE, test_null_input) {
    for (int strSize : {-1, 0, SQL_NTS, 15}) {
        std::string err = "error occured when testing with variable strSize=" +
                          std::to_string(strSize);
        char *szStrNULL = NULL;
        unsigned short wszStr[50] = {0};
        int len = char_utf8_to_utf16_wchar(szStrNULL, strSize,
                                           (SQLWCHAR *)wszStr, strSize);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(wszStr[0], 0) << err;
        std::u16string utf16;
        len = char_utf8_to_utf16_str(szStrNULL, strSize, utf16);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf16.empty()) << err;

        char szStr[50] = {0};
        std::string utf8;
        SQLWCHAR *wszStrNULL = NULL;
        len = wchar16_to_utf8_char(wszStrNULL, strSize, szStr, 10);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(szStr[0], 0) << err;
        len = wchar16_to_utf8_str(wszStrNULL, strSize, utf8);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf8.empty()) << err;
    }
}

TEST(UNICODE_TEST_SUITE, test_empty_input) {
    for (int strSize : {-1, SQL_NTS, 0, 10}) {
        std::string err = "error occured when testing with variable strSize=" +
                          std::to_string(strSize);
        std::vector<char> szStrAllZero(10, 0);
        unsigned short wszStr[50] = {0};
        int len = char_utf8_to_utf16_wchar(szStrAllZero.data(), strSize,
                                           (SQLWCHAR *)wszStr, strSize + 1);
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_EQ(wszStr[0], 0) << err;
        std::u16string utf16;
        len = char_utf8_to_utf16_str(szStrAllZero.data(), strSize, utf16);
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_EQ(utf16, std::u16string((strSize > 0 ? strSize : 0), 0)) << err;

        char szStr[50] = {0};
        std::string utf8;
        std::vector<SQLWCHAR> wszStrAllZero(10, 0);
        len = wchar16_to_utf8_char(wszStrAllZero.data(), strSize, szStr,
                                   sizeof(szStr) / sizeof(szStr[0]));
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_EQ(szStr[0], 0) << err;
        len = wchar16_to_utf8_str(wszStrAllZero.data(), strSize, utf8);
        ASSERT_EQ(len, (strSize > 0 ? strSize : 0)) << err;
        ASSERT_TRUE((strSize > 0 ? utf8.length() : utf8.empty())) << err;
    }

    for (int strSize : {-1, SQL_NTS, 0, /* 10: undefined behavior */}) {
        std::string err = "error occured when testing with variable strSize=" +
                          std::to_string(strSize);
        std::vector<char> szStrEmpty;
        unsigned short wszStr[50] = {0};
        int len = char_utf8_to_utf16_wchar(szStrEmpty.data(), strSize,
                                           (SQLWCHAR *)wszStr, strSize);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(wszStr[0], 0) << err;
        std::u16string utf16;
        len = char_utf8_to_utf16_str(szStrEmpty.data(), strSize, utf16);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf16.empty()) << err;

        char szStr[50] = {0};
        std::string utf8;
        std::vector<SQLWCHAR> wszStrEmpty;
        len = wchar16_to_utf8_char(wszStrEmpty.data(), strSize, szStr, 10);
        ASSERT_EQ(len, 0) << err;
        ASSERT_EQ(szStr[0], 0) << err;
        len = wchar16_to_utf8_str(wszStrEmpty.data(), strSize, utf8);
        ASSERT_EQ(len, 0) << err;
        ASSERT_TRUE(utf8.empty()) << err;
    }
}