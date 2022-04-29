/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#ifdef LINUX
#pragma once

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#define MAX_PATH          260

/*
 * Sizes for buffers used by the _makepath() and _splitpath() functions.
 * note that the sizes include space for 0-terminator
 */
#define _MAX_PATH   260 /* max. length of full pathname */
#define _MAX_DRIVE  3   /* max. length of drive component */
#define _MAX_DIR    256 /* max. length of path component */
#define _MAX_FNAME  256 /* max. length of file name component */
#define _MAX_EXT    256 /* max. length of extension component */

#define _stricmp 	strcasecmp
#define _strnicmp 	strncasecmp
#define _strlwr 	strlwr
#define Sleep		sleep

#ifndef REDSHIFT_IAM
#define redshift_max(x, y) (((x) > (y)) ? (x) : (y))
#define redshift_min(x, y) (((x) < (y)) ? (x) : (y))
#endif // REDSHIFT_IAM

#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'

#define SQL_CURSOR_FORWARD_ONLY         0UL
#ifndef LLONG_MAX
#define LLONG_MAX     9223372036854775807LL       /* maximum signed long long int value */
#endif

#if (SIZEOF_LONG == 4)
typedef unsigned long		RS_DWORD;
#else
typedef unsigned int		RS_DWORD;
#endif


//#define NAMEDATALEN 128

typedef void * HANDLE;
typedef void * HMODULE;
typedef void * HKEY;

typedef pthread_t THREAD_HANDLE;
typedef pthread_mutex_t * MUTEX_HANDLE;
typedef sem_t * SEM_HANDLE;

#ifdef __cplusplus
extern "C"
{
#endif /* C++ */

char *strlwr(char *str);
char *_strupr(char *str);
int GetTempPath(int size, char *pBuf);

#ifdef __cplusplus
}
#endif /* C++ */


#ifndef SQL_CURSOR_FORWARD_ONLY
#define SQL_CURSOR_FORWARD_ONLY         0UL
#endif

#ifndef SQL_CONCUR_READ_ONLY
#define SQL_CONCUR_READ_ONLY            1
#endif


#endif // LINUX




