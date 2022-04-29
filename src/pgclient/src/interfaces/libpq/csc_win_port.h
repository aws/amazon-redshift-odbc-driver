/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

#ifdef WIN32
#pragma once

#include <windows.h>

#define	RS_DWORD DWORD

typedef HANDLE THREAD_HANDLE;
typedef HANDLE MUTEX_HANDLE;
typedef HANDLE SEM_HANDLE;

#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_CHAR '\\'

#ifndef SQL_CURSOR_FORWARD_ONLY
#define SQL_CURSOR_FORWARD_ONLY         0UL
#endif

#ifndef SQL_CONCUR_READ_ONLY
#define SQL_CONCUR_READ_ONLY            1
#endif

#endif // WIN32
