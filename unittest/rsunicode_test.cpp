#include "common.h"
#include "rsunicode.h"

template <typename T>
bool compareArrays(const T *arr1, const T *arr2, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        // printf("'%c' vs '%c'\n", arr1[i] , arr2[i]);
        if (arr1[i] != arr2[i]) {
            printf("%d/%d: %x vs %x\n", i, n - 1, arr1[i], arr2[i]);
            return false;
        }
    }
    return true;
}

TEST(UNICODE_TEST_SUITE, wchar16_to_utf8_str) {
    // UTF-16 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned short wszStr[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    std::string result = u8"Hello, 你好";
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
    result = u8"Hello, 你";
    wchar16_to_utf8_str((const SQLWCHAR *)wszStr2, 8, szStr);
    ASSERT_EQ(szStr, result);
}

TEST(UNICODE_TEST_SUITE, char_utf8_to_str_utf16) {
    // UTF-8 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned char szStr[] = {'H',  'e',  'l',  'l',  'o',  ',',  ' ',
                                   0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD, 0x00};
    std::u16string result = u"Hello, 你好";
    std::u16string wszStr;
    char_utf8_to_str_utf16((const char *)szStr, result.size(), wszStr);
    ASSERT_EQ(wszStr, result);

    char_utf8_to_str_utf16((const char *)szStr, result.size() - 1, wszStr);
    ASSERT_EQ(wszStr, result.substr(0, result.size() - 1));
}

TEST(UNICODE_TEST_SUITE, char_utf8_to_wchar_utf16) {
    // UTF-8 encoded string: u"Hello, 你好"
    // Null terminated
    const unsigned char szStr[] = {'H',  'e',  'l',  'l',  'o',  ',',  ' ',
                                   0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD, 0x00};
    size_t lenSzStr = sizeof(szStr) / sizeof(szStr[0]);
    const unsigned short result[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F,
                                     0x002C, 0x0020, 0x4F60, 0x597D, 0x0000};
    size_t lenResult = sizeof(result) / sizeof(result[0]);
    unsigned short wszStr[50] = {0};
    int len = char_utf8_to_wchar_utf16((const char *)szStr, 9, wszStr);
    ASSERT_EQ(len, lenResult - 1);
    ASSERT_TRUE(compareArrays<unsigned short>(
        wszStr, result, (len + 1))); // including null terminator
}