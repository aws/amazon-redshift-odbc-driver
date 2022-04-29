// connect.c : Defines the entry point for the console application.
// Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
//
// Command line arguments: DSN UID PWD
// Note that password is visible when it prompt for it.

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>

#if defined LINUX || defined SunOS || defined AIX
#include <string.h>
#endif



int CheckForError(SQLHANDLE hHandle,SQLSMALLINT hType,SQLRETURN	iRc);
void readInput(char *prompt, char *buf,int iMaxLen);

/*====================================================================================================*/

int main(int argc, char**ppargv)
{
	HENV	phenv = NULL;
	HDBC	phdbc = NULL;
    SQLRETURN iRc;
    char szDSN[256];
    char szUser[256];
    char szPassword[256];

	// Allocate environment
	if (SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&phenv) == SQL_ERROR)
	{
		printf("Unable to allocate an environment handle\n");
		exit(-1);
	}

	iRc = SQLSetEnvAttr(phenv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if(CheckForError(phenv,SQL_HANDLE_ENV,iRc))
    {
        SQLFreeEnv(phenv);
        return 1;
    }

    iRc = SQLAllocHandle(SQL_HANDLE_DBC,phenv,&phdbc);
    if(CheckForError(phenv,SQL_HANDLE_ENV,iRc))
    {
        SQLFreeEnv(phenv);
        return 1;
    }

    szDSN[0] = '\0';
    szUser[0] = '\0';
    szPassword[0] = '\0';

	if (argc > 1)
	{
		strncpy(szDSN,ppargv[1],255);

        if(argc > 2)
        {
		    strncpy(szUser,ppargv[2],255);
            if(argc > 3)
		        strncpy(szPassword,ppargv[3],255);
			else
				readInput("Enter password:", szPassword,255);
        }
        else
		{
            readInput("Enter user name:", szUser,255);
			readInput("Enter password:", szPassword,255);
		}
	}
    else
    {
/*		printf("You can pass positional command line arguments: DSN UID PWD\n"); */
        readInput("Enter DSN name:",  szDSN,255);
        readInput("Enter user name:", szUser,255);
        readInput("Enter password:", szPassword,255);
    }

    printf("Connecting using DSN=%s...\n",szDSN);
    iRc = SQLConnect(phdbc,(SQLCHAR *)szDSN, SQL_NTS,
        (szUser[0] != '\0') ? (SQLCHAR *)szUser : NULL, (szUser[0] != '\0') ? SQL_NTS : 0,
        (szPassword[0] != '\0') ? (SQLCHAR *) szPassword : NULL, (szPassword[0] != '\0') ? SQL_NTS : 0);

    if(CheckForError(phdbc,SQL_HANDLE_DBC,iRc))
    {
	 SQLFreeConnect(phdbc);
        SQLFreeEnv(phenv);

        return 1;
    }

    printf("\nConnection successful\n");

    SQLDisconnect(phdbc);
    SQLFreeConnect(phdbc);
    SQLFreeEnv(phenv);

	return 0;
}

/*====================================================================================================*/

int CheckForError(SQLHANDLE hHandle,SQLSMALLINT hType,SQLRETURN	iRc)
{
	SQLSMALLINT	iRec = 0;
	SQLINTEGER	iNativeError;
	char		szMessage[512];
	char		szState[SQL_SQLSTATE_SIZE + 1];
    int         iError = 0;

	if(iRc == SQL_INVALID_HANDLE)
	{
		printf("Invalid handle\n");
		return 1;
	}

	while (SQLGetDiagRec(hType,
						 hHandle,
						 ++iRec,
						 (SQLCHAR *)szState,
						 &iNativeError,
						 (SQLCHAR *)szMessage,
						 (SQLSMALLINT)(sizeof(szMessage) / sizeof(char)),
						 (SQLSMALLINT *)NULL) == SQL_SUCCESS)
	{
	    printf("SQLState=%5.5s Error Message=%s\n",szState,szMessage);
        iError++;
	}

    return iError;
}

/*====================================================================================================*/

void readInput(char *prompt, char *buf,int iMaxLen)
{
    printf(prompt);
    fgets(buf,iMaxLen,stdin);
    if(strlen(buf) > 0)
        buf[strlen(buf) - 1] = '\0';
}
