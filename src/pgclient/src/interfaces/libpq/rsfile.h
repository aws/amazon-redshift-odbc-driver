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

#if defined LINUX 
/* off_t, fseeko, ftello already defined */
#include <unistd.h>

#endif

#include <stdio.h>

#ifdef WIN32
#if defined(_WIN64)
#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef __int64 off_t;
#endif // _OFF_T_DEFINED
#define rs_fseeko _fseeki64
#define rs_ftello _ftelli64
#else // 64 bit
typedef long off_t;
#define rs_fseeko fseek
#define rs_ftello ftell
#endif // 32 bit
#endif // WIN32


#if defined LINUX 
#define rs_fseeko fseeko
#define rs_ftello ftello

extern FILE *fopen64 (__const char *__restrict __filename,
		      	  	  __const char *__restrict __modes);
#endif

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

FILE *rs_fopen(const char * _Filename, const char * _Mode);
int rs_fileno(FILE *_file);
int rs_fsync(int fd);
long long getCurrentTimeInMilli(void);
int fileExists(char * pFileName); // Define in file_util.c

#ifdef __cplusplus
}
#endif /* C++ */

