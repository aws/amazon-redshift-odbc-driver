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
functions return the size of output in characters (not necessarily bytes). Characters are
char or SQLWCHAR.
*/

/*
wszStr : input variable
cchLen : Length of input (wszStr) in characters
szStr  : Output buffer
bufferSize : Output buffer size in bytes (char) including null termination character
Returns the length of szStr in characters
*/

size_t wchar16_to_utf8_char(const SQLWCHAR *wszStr, int cchLen, char *szStr,
                            int bufferSize);
/*
wszStr : input variable
cchLen : Length of input (wszStr) in characters
szStr  : Output buffer
Returns the length of szStr in characters
*/
size_t wchar16_to_utf8_str(const SQLWCHAR *wszStr, int cchLen,
                           std::string &szStr);

/*
szStr  : Input variable
cchLen : Input length in bytes (not characters)
wszStr : Output buffer
Returns 0 if szStr is NULL or cchLen is negative, else
Returns the length of wszStr in characters
*/
size_t char_utf8_to_utf16_str(const char *szStr, int cchLen,
                              std::u16string &wszStr);

/*
szStr  : Input variable
cchLen : Input length in bytes (not characters)
wszStr : Output buffer
bufferSize : Output buffer size in characters (SQLWCHAR) including null termination character
Returns 0 if szStr is NULL or cchLen is negative, else
Returns the length of wszStr in characters
*/
size_t char_utf8_to_utf16_wchar(const char *szStr, int cchLen,
                                SQLWCHAR *wszStr, int bufferSize);

//Deprecated
size_t wchar_to_utf8(WCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen);
//Deprecated
size_t utf8_to_wchar(const char *szStr,size_t cbLen,WCHAR *wszStr,size_t cchLen);
//Deprecated
unsigned char *convertWcharToUtf8(WCHAR *wData, size_t cchLen);
//Deprecated
WCHAR *convertUtf8ToWchar(unsigned char *szData, size_t cbLen);
size_t calculate_utf8_len(WCHAR* wszStr, size_t cchLen);
size_t calculate_wchar_len(const char* szStr, size_t cbLen, size_t *pcchLen);

