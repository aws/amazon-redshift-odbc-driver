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

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

size_t wchar_to_utf8(WCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen);
size_t utf8_to_wchar(char *szStr,size_t cbLen,WCHAR *wszStr,size_t cchLen);
unsigned char *convertWcharToUtf8(WCHAR *wData, size_t cchLen);
WCHAR *convertUtf8ToWchar(unsigned char *szData, size_t cbLen);
size_t calculate_utf8_len(WCHAR* wszStr, size_t cchLen);
size_t calculate_wchar_len(char* szStr, size_t cbLen, size_t *pcchLen);

#ifdef __cplusplus
}
#endif /* C++ */
