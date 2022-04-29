/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


#define	CHECK_ODBC_RC(h,ht,x) { \
                                RETCODE	iRet=x;\
								if (iRet != SQL_SUCCESS) \
								{ \
									HandleError(h,ht,iRet); \
								} \
								if (iRet == SQL_ERROR) \
								{ \
									fprintf(stderr,"Error in " #x "\n"); \
									goto error;	\
								}  \
							 }

typedef struct _BINDING
{
    // Size to display
	SQLLEN	siDisplaySize;

    // Display buffer
	char		*szBuffer;

    // Size or null
	SQLLEN	indPtr;

    // Character col indicator
	BOOL		fChar;

    // Linked list
	struct _BINDING	*pNext;
}BINDING;

void HandleError(SQLHANDLE	hHandle,
				SQLSMALLINT	hType,
			    RETCODE	RetCode);

void DisplayResults(HSTMT		lpStmt,
			        SQLSMALLINT	cCols,
					SQLUINTEGER *puiRowCount);

void AllocateBindings(HSTMT	lpStmt,
					  SQLSMALLINT	cCols,
					  BINDING		**lppBinding,
					  SQLSMALLINT	*lpDisplay);


void DisplayTitles(HSTMT		lpStmt,
					  DWORD		siDisplaySize,
					  BINDING	*pBinding);

BINDING		*FreeBindings(BINDING		*pFirstBinding);
int         ReadCmd(FILE *fp, char *buf,int bufLen);
void        TrimTrailingWhitespaces(char *str);
long long currentTimeMillis();

#define	DISPLAY_MAX	50			// Arbitrary limit on column width to display
#define	DISPLAY_FORMAT_EXTRA 3	// Per column extra display bytes (| <data> )
#define	DISPLAY_FORMAT		"%c %*.*s "
#define	DISPLAY_FORMAT_C	"%c %-*.*s "
#define	NULL_SIZE			6	// <NULL>
#define	SQL_QUERY_SIZE		32000 // Max. Num characters for SQL Query passed in.

#define	PIPE				'|'


#if defined LINUX 

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#define _stricmp 	strcasecmp
#define _strnicmp 	strncasecmp
#define _strlwr 	strlwr
#define Sleep		sleep

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

void *GetDesktopWindow();


#endif
