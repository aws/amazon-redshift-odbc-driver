/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsunicode.h"
#include "rsutil.h"

#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <vector>

#if defined LINUX 

char g_utf8_len_data[128] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x80-0x8f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x90-0x9f */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xa0-0xaf */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xb0-0xbf */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xc0-0xcf */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xd0-0xdf */
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* 0xe0-0xef */
    3,3,3,3,3,3,3,3,4,4,4,4,5,5,0,0  /* 0xf0-0xff */
};

unsigned char g_utf8_mask_data[6]   =  { 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
unsigned int  g_utf8_minval_data[6] =  { 0x0, 0x80, 0x800, 0x10000, 0x200000, 0x4000000 };

int unix_wchar_to_utf8_len(WCHAR *pwStr, int cchLen);
int unix_wchar_to_utf8(WCHAR *wszStr, int cchLen, char *szStr, int cbLen);
int unix_utf8_to_wchar_len(const char *szStr, int cbLen);
int unix_utf8_to_wchar(const char *szStr, int cbLen, WCHAR *wszStr, int cchLen);

#endif // LINUX 


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Convert SQLWCHAR to UTF-8.
//
size_t wchar16_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr,
                            int bufferSize) {
    std::string utf8str;
    size_t strLen = wchar16_to_utf8_str(wszStr, cchLen, utf8str);
    int limitBytes = strLen * sizeof(char);
    if (cchLen >= 0) {
        limitBytes = std::min<int>(cchLen, strLen) * sizeof(char);
    }
    if (bufferSize >= 0) {
        limitBytes = std::min<int>(limitBytes, (bufferSize - 1) * sizeof(char));
    }
    limitBytes = limitBytes < 0 ? 0 : limitBytes;
    memcpy((char *)szStr, utf8str.c_str(), (size_t)limitBytes);
    memset(((char *)szStr) + limitBytes, 0, sizeof(char)); // null termination

    return (size_t)limitBytes / sizeof(char);
}

// cchLen : length of wszStr in characters (not bytes)
// non null terminated wszStr with negative cchLen is undefined behavior
size_t wchar16_to_utf8_str(const SQLWCHAR *wszStr, int cchLen,
                           std::string &szStr) {
    szStr.clear();
    std::u16string u16 = (cchLen == SQL_NTS)
                             ? std::u16string((char16_t *)wszStr)
                             : std::u16string((char16_t *)wszStr, cchLen);
        
    /*
    // Uncomment only for debugging
    {
        // Convert UTF-16 string to wide character string (wstring)
        std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
        std::cout << cchLen << ":" << u16.size() << " U16:" << converter.to_bytes(u16) << std::endl;
    }
    */
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    szStr = convert.to_bytes(u16);
    return szStr.size();
}

// non null terminated szStr is undefined behavior
size_t char_utf8_to_str_utf16(const char *szStr, int cchLen,
                              std::u16string &utf16) {
    if (cchLen <= 0) return 0;
    utf16.clear();
    std::string utf8 = std::string(szStr);

    std::vector<unsigned long> unicode;
    size_t i = 0;
    while (i < utf8.size())
    {
        unsigned long uni;
        size_t todo;
        bool error = false;
        unsigned char ch = utf8[i++];
        if (ch <= 0x7F)
        {
            uni = ch;
            todo = 0;
        }
        else if (ch <= 0xBF)
        {
            throw std::logic_error("not a UTF-8 string");
        }
        else if (ch <= 0xDF)
        {
            uni = ch&0x1F;
            todo = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch&0x0F;
            todo = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch&0x07;
            todo = 3;
        }
        else
        {
            throw std::logic_error("not a UTF-8 string");
        }
        for (size_t j = 0; j < todo; ++j)
        {
            if (i == utf8.size())
                throw std::logic_error("not a UTF-8 string");
            unsigned char ch = utf8[i++];
            if (ch < 0x80 || ch > 0xBF)
                throw std::logic_error("not a UTF-8 string");
            uni <<= 6;
            uni += ch & 0x3F;
        }
        if (uni >= 0xD800 && uni <= 0xDFFF)
            throw std::logic_error("not a UTF-8 string");
        if (uni > 0x10FFFF)
            throw std::logic_error("not a UTF-8 string");
        unicode.push_back(uni);
    }

    for (size_t i = 0; utf16.size() < cchLen && i < unicode.size(); ++i)
    {
        unsigned long uni = unicode[i];
        if (uni <= 0xFFFF)
        {
            utf16 += (char16_t)uni;
        }
        else
        {
            uni -= 0x10000;
            utf16 += (char16_t)((uni >> 10) + 0xD800);
            utf16 += (char16_t)((uni & 0x3FF) + 0xDC00);
        }
    }
    /*
    // Uncomment only for debugging
    {
        // Convert UTF-16 string to wide character string (wstring)
        std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
        std::cout << cchLen << ":" << utf16.size() << " U16:" << converter.to_bytes(utf16) << std::endl;
    }
    */
    return utf16.size();
}

// non null terminated szStr is undefined behavior
size_t char_utf8_to_wchar_utf16(const char *szStr, int cchLen,
                                SQLWCHAR *wszStr, int bufferSize) {
    if (cchLen <= 0) return 0;
    std::u16string utf16;
    int strLen = char_utf8_to_str_utf16(szStr, cchLen, utf16);

    /*
    // Uncomment only for debugging
    {
        std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
        std::cout << "U16:" << converter.to_bytes(utf16) << std::endl;
    }
    */
    int limitBytes = std::min<int>(cchLen, strLen) * sizeof(SQLWCHAR);
    if (bufferSize >= 0) {
        limitBytes = std::min<int>(limitBytes, (bufferSize - 1) * sizeof(SQLWCHAR));
    }
    limitBytes = limitBytes < 0 ? 0 : limitBytes;
    memcpy((char *)wszStr, utf16.c_str(), (size_t)limitBytes);
    memset(((char *)wszStr) + limitBytes, 0, sizeof(SQLWCHAR)); // null termination
    if ((size_t)limitBytes != strLen * sizeof(SQLWCHAR)) {
        RS_LOG_INFO("RSUNI", "Unicode conversion truncated %u/%u", limitBytes,
                    strLen * sizeof(SQLWCHAR));
    }
    return (size_t)limitBytes/sizeof(SQLWCHAR);
}

size_t wchar_to_utf8(WCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen)
{
    size_t len = 0;
    
    if(wszStr && wszStr[0] != '\0')
    {
#ifdef WIN32
        len = (size_t) WideCharToMultiByte(CP_UTF8, 0, wszStr, (INT_LEN(cchLen) == SQL_NTS) ? -1 : (int)cchLen, szStr,  
                                        (int) cbLen, NULL,NULL);
#endif
#if defined LINUX 
        if(((int)cchLen) == SQL_NTS && wszStr)
            cchLen = unix_wchar_to_utf8_len(wszStr, cchLen) + 1;

        len = unix_wchar_to_utf8(wszStr,cchLen,szStr,cbLen);
#endif
    }

    if(szStr)
    {
        if(len < cbLen)
            szStr[len] = '\0';
    }

    return len;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Convert UTF-8 to WCHAR.
//
size_t utf8_to_wchar(const char *szStr,size_t cbLen,WCHAR *wszStr,size_t cchLen)
{
    size_t len =  0;
    
    if(szStr && szStr[0] != '\0')
    {
#ifdef WIN32
        len = (size_t) MultiByteToWideChar(CP_UTF8,  0, szStr,  (INT_LEN(cbLen) == SQL_NTS ) ? -1 : (int) cbLen, wszStr, (int) cchLen);
#endif
#if defined LINUX 
        if(cbLen == SQL_NTS && szStr)
             cbLen = strlen(szStr) + 1;

        len = unix_utf8_to_wchar(szStr,cbLen,wszStr,cchLen);
#endif
    }

    if(wszStr)
    {
        if(len < cchLen)
            wszStr[len] = L'\0';
    }

    return len;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Allocate a buffer and then convert WCHAR to UTF-8.
//
unsigned char *convertWcharToUtf8(WCHAR *wData, size_t cchLen)
{
    unsigned char *szData = NULL;

    if(wData != NULL)
    {
        size_t cbLen;

        cbLen = calculate_utf8_len(wData, cchLen);

        szData = (cbLen) ? (unsigned char *) rs_malloc(cbLen) : NULL;

        wchar_to_utf8(wData, cchLen, (char *)szData, cbLen);
    }

    return szData;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Allocate a buffer and then convert UTF-8 to WCHAR.
//
WCHAR *convertUtf8ToWchar(unsigned char *szData, size_t cbLen)
{
    WCHAR *wData = NULL;

    if(szData != NULL)
    {
        size_t cchLen;
        size_t len = calculate_wchar_len((char *)szData, cbLen, &cchLen);

        wData = (len) ?  (WCHAR *) rs_malloc(len) : NULL;

        utf8_to_wchar((char *)szData, cbLen, wData, cchLen);
    }

    return wData;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Calculate UTF-8 len.
//
size_t calculate_utf8_len(WCHAR* wszStr, size_t cchLen)
{
    size_t len;

    if(wszStr)
    {
#ifdef WIN32
        len = (cchLen == SQL_NTS) ? wcslen(wszStr) : ( (INT_LEN(cchLen) == SQL_NULL_DATA) ? 0 : cchLen);

        if(len != 0)
        {
            size_t len1 = (size_t) WideCharToMultiByte(CP_UTF8, 0, wszStr, (INT_LEN(cchLen) == SQL_NTS) ? -1 : (int)cchLen, NULL,  
                                                        0, NULL,NULL);

            if(len1 > len)
                len = len1;
        }

#endif
#if defined LINUX 
        len = (((int)cchLen) == SQL_NTS) ? unix_wchar_to_utf8_len(wszStr, SQL_NTS) : ( (cchLen == SQL_NULL_DATA) ? 0 : cchLen);
#endif

        len++; // +1 for '\0'
    }
    else
        len = 0;

    return len;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Calculate WCHAR len.
//
size_t calculate_wchar_len(const char* szStr, size_t cbLen, size_t *pcchLen)
{
    size_t len;

    if(szStr)
    {
        len = (INT_LEN(cbLen) == SQL_NTS) ? strlen(szStr) : cbLen;
        len++; // +1 for '\0'
        if(pcchLen)
            *pcchLen = len;
        len *= sizeof(WCHAR);
    }
    else
    {
        len = 0;
        if(pcchLen)
            *pcchLen = len;
    }

    return len;
}

/*====================================================================================================================================================*/

#if defined LINUX 

//---------------------------------------------------------------------------------------------------------igarish
// Calculate UTF-8 len on Linux.
//
int unix_wchar_to_utf8_len(WCHAR *pwStr, int cchLen)
{
    int iLen = 0;

    if(pwStr)
    {
        if(cchLen == SQL_NTS)
        {
            for (iLen = 0; *pwStr; pwStr++, iLen++)
            {
                 if (*pwStr >= 0x80)
                 {
                     iLen++;
                     if (*pwStr >= 0x800) iLen++;
                 }
            } // Loop
        }
        else
        {
            for (iLen = 0; cchLen; cchLen--, pwStr++, iLen++)
            {
                 if (*pwStr >= 0x80)
                 {
                     iLen++;
                     if (*pwStr >= 0x800) iLen++;
                 }
            } // Loop
        }
    }

    return iLen;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Convert WCHAR to UTF-8 on Linux.
//
int unix_wchar_to_utf8(WCHAR *wszStr, int cchLen, char *szStr, int cbLen)
{
    char *pStart = szStr;

    if (!cbLen)
        return unix_wchar_to_utf8_len(wszStr, cchLen);

    if(wszStr)
    {
        for (; cchLen; cchLen--, wszStr++)
        {
             WCHAR wch = *wszStr;

             if (wch < 0x80)  /* 1 byte */
             {
                 if (!cbLen--)
                     return 0;

                 *szStr++ = (char) wch;
                 continue;
             }

             if (wch < 0x800)  /* 2 bytes */
             {
                 if ((cbLen -= 2) < 0)
                     return 0;

                 szStr[1] = 0x80 | (wch & 0x3f);
                 wch >>= 6;
                 szStr[0] = 0xc0 | wch;
                 szStr += 2;
                 continue;
             }

             /*  3 bytes */
            if ((cbLen -= 3) < 0) return 0;
            szStr[2] = 0x80 | (wch & 0x3f);
            wch >>= 6;
            szStr[1] = 0x80 | (wch & 0x3f);
            wch >>= 6;
            szStr[0] = 0xe0 | wch;
            szStr += 3;
        } // Loop
    }

    return (int)(szStr - pStart);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Calculate WCHAR len on Linux.
//
int unix_utf8_to_wchar_len(const char *szStr, int cbLen)
{
    int iLen;
    const char *pEnd = szStr + cbLen;

    for (iLen = 0; szStr < pEnd; iLen++)
    {
        char ch = *szStr++;

        if ((unsigned char)ch < 0xc0)
            continue;

        switch(g_utf8_len_data[((unsigned char)ch)-0x80])
        {
            case 5:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 4:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 3:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 2:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            case 1:
            {
                if (szStr >= pEnd)
                    return iLen;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    continue;
                szStr++;
            }

            // no break
            default:
            {
                break;
            }
        } // Switch
    } // Loop

    return iLen;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Convert UTF-8 to WCHAR on Linux.
//
int unix_utf8_to_wchar(const char *szStr, int cbLen, WCHAR *wszStr, int cchLen)
{
    int iLen, iCount;
    int wch;
    const char *pEnd = szStr + cbLen;

    if (!cchLen)
        return unix_utf8_to_wchar_len(szStr, cbLen);

    for (iCount = cchLen; iCount && (szStr < pEnd); iCount--, wszStr++)
    {
        char ch = *szStr++;

        wch = 0;

        if ((unsigned char)ch < 0x80)  /* 7-bit ASCII */
        {
            wch = ch;
            *wszStr = wch;

            continue;
        }

        iLen = g_utf8_len_data[((unsigned char)ch)-0x80];
        wch = ch & g_utf8_mask_data[iLen];

        switch(iLen)
        {
            case 5:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 4:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 3:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 2:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
            }

            // no break
            case 1:
            {
                if (szStr >= pEnd)
                    goto end;
                if ((ch = *szStr ^ 0x80) >= 0x40)
                    return 0;
                wch = (wch << 6) | ch;
                szStr++;
                if (wch < g_utf8_minval_data[iLen])
                    return 0;
                if (wch >= 0x10000)
                    return 0;
                *wszStr = wch;

                continue;
            }
        } // Switch
    } // Loop

    if (szStr < pEnd)
        return 0;
end:
    return cchLen - iCount;
}

#endif // LINUX
