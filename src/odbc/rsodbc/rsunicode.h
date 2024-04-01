/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <stdlib.h>

#include "rsodbc.h"
#include <string>

/*
The folowing three functions is a set of conversions between 16-bit (unsigned
short) and 8-bit representation of characters. Regardless of the size of wide
character (wchar_t) in Windows(16 bit) or Unix based systems(32 bits), these
functions assume that a wide character is represented by ODBC's SQLWCHAR and is
uniformly 16 bits across all platforms. This makes it easy to maneuver with
various string types like std::string and std::u16string in arguments. The
functions return the size of output in characters (not bytes) which could be a
char or SQLWCHAR.
*/

/*
wszStr : input variable
cchLen : Length of input (wszStr) in characters
szStr  : Output buffer
bufferSize : Output buffer size in bytes (it is in bytes since we don't know how
many bytes will a wide string take) Note: Non null terminated wszStr with
negative cchLen is undefined behavior
*/

size_t wchar16_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr,
                            int bufferSize);
/*
wszStr : input variable
cchLen : Length of input (wszStr) in characters
szStr  : Output buffer
Note: Non null terminated wszStr with negative cchLen is undefined behavior
*/
size_t wchar16_to_utf8_str(const SQLWCHAR *wszStr, int cchLen,
                           std::string &szStr);

/*
szStr  : Input variable
cchLen : Maximum length of output (wszStr) in characters
wszStr : Output buffer
Note: Non null terminated szStr is undefined behavior
*/
size_t char_utf8_to_str_utf16(const char *szStr, int cchLen,
                              std::u16string &wszStr);

/*
szStr  : Input variable
cchLen : Maximum length of output (wszStr) in characters
wszStr : Output buffer
bufferSize : Output buffer size in wide characters
Note: Non null terminated szStr is undefined behavior
*/
size_t char_utf8_to_wchar_utf16(const char *szStr, int cchLen,
                                SQLWCHAR *wszStr, int bufferSize);

size_t wchar_to_utf8(WCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen);
size_t utf8_to_wchar(const char *szStr,size_t cbLen,WCHAR *wszStr,size_t cchLen);
unsigned char *convertWcharToUtf8(WCHAR *wData, size_t cchLen);
WCHAR *convertUtf8ToWchar(unsigned char *szData, size_t cbLen);
size_t calculate_utf8_len(WCHAR* wszStr, size_t cchLen);
size_t calculate_wchar_len(const char* szStr, size_t cbLen, size_t *pcchLen);

