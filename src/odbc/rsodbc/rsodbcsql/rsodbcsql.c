/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsodbcsql.h"

int giDisplayData = TRUE; 
int giCmdLineOptionQuery = FALSE;
int giBenchMarkMode = FALSE;


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// USAGE:	rsodbcsql DSN=<dsn name>   or
//			rsodbcsql FILEDSN=<file dsn> or
//			rsodbcsql DRIVER={driver name}
//
int main(int argc, char **argv)
{
	HENV	lpEnv = NULL;
	HDBC	lpDbc = NULL;
	HSTMT	lpStmt = NULL;
	char	*pszConnStr;
	char	szInput[SQL_QUERY_SIZE];
	FILE *fp = stdin;
    long long llStartTime;
    long long llEndTime;

	if(argc == 1)
	{
		// Usage
		printf("Usage: rsodbcsql \"DSN=<dsn name>\" | \"DRIVER={driver name};HOST=host_name;PORT=port_number;UID=user_name;Database=db_name[;PWD=password]\" [file.sql|query] \n");
		printf("\n");
		printf("DRIVER	- Name of the driver: Amazon Redshift ODBC Driver (x64)'. Enter name without quotes.\n");
		printf("DSN	- Name for the connection. \n");
		printf("HOST	- Host name or IP address for the Redshift server. \n");
		printf("PORT	- TCP/IP port number for the connection. Recommended port is the default port: 5439. \n");
		printf("UID	- User name to use when connecting to the database. \n");
		printf("Database - Name of the database for the connection. \n");
		printf("PWD	- Password to use when connecting to the database. \n");
		exit(1);
	}

	// Allocate an environment
	if (SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&lpEnv) == SQL_ERROR)
	{
		fprintf(stderr,"Unable to allocate an environment handle\n");
		exit(-1);
	}

	CHECK_ODBC_RC(lpEnv,
			        SQL_HANDLE_ENV,
			        SQLSetEnvAttr(lpEnv,
						  SQL_ATTR_ODBC_VERSION,
						  (SQLPOINTER)SQL_OV_ODBC3,
						  0));

	// Allocate a connection
	CHECK_ODBC_RC(lpEnv,
			        SQL_HANDLE_ENV,
			        SQLAllocHandle(SQL_HANDLE_DBC,lpEnv,&lpDbc));

	
    szInput[0] = '\0';

    // Process argument
	if (argc > 1)
	{
        // Connection string
		pszConnStr = *++argv;
	} 
    else
	{
		pszConnStr = NULL;
	}

	if(argc > 2)
	{
		char *pqueryArg = *++argv;

		if(strstr(pqueryArg,".sql") ||  strstr(pqueryArg,".SQL"))
		{
            // Query file name
			char *file_name = pqueryArg; // *++argv;
			fp = fopen(file_name,"rt");

			(fp) ? printf("Open: %s", file_name) : printf("Not open: %s", file_name); 
			printf("\n");
		}
		else
		{
            // Directly treat as query
			strncpy(szInput, pqueryArg,sizeof(szInput));
			fp = NULL;
            giCmdLineOptionQuery = TRUE;
		}
	}

	if(argc > 3)
	{
		char *pDispDataArg = *++argv;

        giDisplayData = atoi(pDispDataArg);
    }

	if(argc > 4)
	{
		char *pBenchmarkArg = *++argv;

        giBenchMarkMode = atoi(pBenchmarkArg);
    }

	// Connect to the driver.  Use the connection string if supplied
	// on the input, otherwise let the driver manager prompt for input.
	CHECK_ODBC_RC(lpDbc,
			SQL_HANDLE_DBC,
			SQLDriverConnect(lpDbc,
							 GetDesktopWindow(),
							 (SQLCHAR *)pszConnStr,
							 SQL_NTS,
							 NULL,
							 0,
							 NULL,
							 SQL_DRIVER_NOPROMPT)); // SQL_DRIVER_COMPLETE

	fprintf(stderr,"Connected.\n");

/*	CHECK_ODBC_RC(lpDbc,
			SQL_HANDLE_DBC,
			SQLSetConnectOption(lpDbc,SQL_AUTOCOMMIT,SQL_FALSE)); */

	CHECK_ODBC_RC(lpDbc,
			SQL_HANDLE_DBC,
			SQLAllocHandle(SQL_HANDLE_STMT,lpDbc,&lpStmt));


	printf("Enter SQL commands, type quit to exit\nrsodbcsql>");

	// Loop to get input and execute queries
	while(ReadCmd(fp,szInput,SQL_QUERY_SIZE-1))
	{
		RETCODE		RetCode;
		SQLSMALLINT	sNumResults;
        size_t         len = strlen(szInput);
		SQLUINTEGER uiRowCount = 0;


        if(len > 0 && szInput[len - 1] == '\n')
            szInput[len - 1] = '\0';

		// Execute the query
		if (!(*szInput))
		{
			printf("rsodbcsql>");
			continue;
		}
        else
        if(_stricmp(szInput,"QUIT") == 0 || _stricmp(szInput,"QUIT;") == 0)
            break;

        llStartTime = currentTimeMillis();
		RetCode = SQLExecDirect(lpStmt,(SQLCHAR *)szInput,SQL_NTS);

		switch(RetCode)
		{
			case SQL_SUCCESS_WITH_INFO:
			{
				HandleError(lpStmt,SQL_HANDLE_STMT,RetCode);
				// fall through SQL_SUCCESS
			}

			// no break
			case SQL_SUCCESS:
			{
                do
                {
					uiRowCount = 0;

				    // If this is a row-returning query, display
				    // results
				    CHECK_ODBC_RC(lpStmt,
						    SQL_HANDLE_STMT,
						    SQLNumResultCols(lpStmt,&sNumResults));

				    if (sNumResults > 0)
				    {
					    DisplayResults(lpStmt,sNumResults,&uiRowCount);
				    } 
                    else
				    {
					    SQLLEN		siRowCount;

					    CHECK_ODBC_RC(lpStmt,
							    SQL_HANDLE_STMT,
							    SQLRowCount(lpStmt,&siRowCount));

					    if (siRowCount >= 0)
					    {
						    printf("%ld %s affected\n",
								    siRowCount,
								    siRowCount == 1 ? "row" : "rows");
					    }
				    }

                    // Check for more results
		            RetCode = SQLMoreResults(lpStmt);

                    if(RetCode == SQL_NO_DATA)
                        break;
                    else
                    if(RetCode == SQL_SUCCESS)
                    {
		                CHECK_ODBC_RC(lpStmt,
				                SQL_HANDLE_STMT,
				                SQLFreeStmt(lpStmt,SQL_UNBIND));

                        printf("\n\n");
                        continue;
                    }
                    else
                    if(RetCode == SQL_ERROR)
                    {
				        HandleError(lpStmt,SQL_HANDLE_STMT,RetCode);
                        break;
                    }
                    else
                        break;

                }while(TRUE);

				break;
			}

			case SQL_ERROR:
			{
				HandleError(lpStmt,SQL_HANDLE_STMT,RetCode);
				break;
			}

			default: fprintf(stderr,"Unexpected return code %d!\n",RetCode); break;

		} // Switch

        llEndTime = currentTimeMillis();

        printf("\n Total time taken for execute and fetch = %lld (milli seconds)\n", (llEndTime - llStartTime));

		if(giBenchMarkMode)
			printf("%u rows fetched from %hd columns.\n", (unsigned int)uiRowCount, sNumResults);



		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLFreeStmt(lpStmt,SQL_CLOSE));

		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLFreeStmt(lpStmt,SQL_UNBIND));


		printf("rsodbcsql>");

        if(giCmdLineOptionQuery)
            break;

	} // Loop

error:

	if(fp && fp != stdin)
	{
		fclose(fp);
		fp = NULL;
	}

	// Free ODBC handles and exit
	if (lpDbc)
	{
		SQLDisconnect(lpDbc);
		SQLFreeConnect(lpDbc);
	}

	if (lpEnv)
		SQLFreeEnv(lpEnv);

	printf("\nDisconnected.\n");

	return 0;

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Display results of a select query
//
void DisplayResults(HSTMT		lpStmt,
			   		SQLSMALLINT	cCols,
					SQLUINTEGER *puiRowCount)
{
	BINDING			*pFirstBinding, *pThisBinding;			
	SQLSMALLINT		siDisplaySize;
	RETCODE			RetCode;
    SQLUINTEGER     uiRowCount = 0;      

	// Allocate memory for each column 
	AllocateBindings(lpStmt,cCols,&pFirstBinding, &siDisplaySize);

    if(!giBenchMarkMode)
    {
	    // Set the display mode and write the titles
	    DisplayTitles(lpStmt,siDisplaySize, pFirstBinding);
    }


	// Fetch and display the data
	do 
    {
		// Fetch a row
		CHECK_ODBC_RC(lpStmt,SQL_HANDLE_STMT, RetCode = SQLFetch(lpStmt));

		if (RetCode == SQL_NO_DATA_FOUND)
			break;
			

        uiRowCount++;

        if(!giBenchMarkMode && giDisplayData)
        {
		    // Display the data.   Ignore truncations
		    for (pThisBinding = pFirstBinding;
			     pThisBinding;
			     pThisBinding = pThisBinding->pNext)
		    {
			    if (pThisBinding->indPtr != SQL_NULL_DATA)
			    {
				    printf(pThisBinding->fChar ? DISPLAY_FORMAT_C:
											       DISPLAY_FORMAT,
						    PIPE,
						    (int)(pThisBinding->siDisplaySize),
						    (int)(pThisBinding->siDisplaySize),
						    pThisBinding->szBuffer);
			    } 
                else
			    {
				    printf(DISPLAY_FORMAT_C,
						    PIPE,
						    (int)(pThisBinding->siDisplaySize),
						    (int)(pThisBinding->siDisplaySize),
						    "<NULL>");
			    }

		    }

            if(!giBenchMarkMode)
		        printf(" %c\n",PIPE);
        }
	}while (1);

//	printf("%*.*s",siDisplaySize+2,siDisplaySize+2," ");
    if(!giBenchMarkMode)
	    printf("%u rows fetched from %hd columns.\n", (unsigned int)uiRowCount, cCols);
	else
	if(puiRowCount)
		*puiRowCount = uiRowCount;

error:
	// Clean up the allocated buffers
    pFirstBinding = FreeBindings(pFirstBinding);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get column information and allocate bindings for each column.  
//
void AllocateBindings(HSTMT			lpStmt,
					  SQLSMALLINT	cCols,
					  BINDING		**lppBinding,
					  SQLSMALLINT	*lpDisplay)
{
	SQLSMALLINT		iCol;
	BINDING			*lpThisBinding, *lpLastBinding = NULL;
	SQLLEN		    cchDisplay, ssType;
    SQLSMALLINT     cchColumnNameLength;

	*lpDisplay = 0;

	for (iCol = 1; iCol <= cCols; iCol++)
	{
		lpThisBinding = (BINDING *)(calloc(1,sizeof(BINDING)));
		if (!(lpThisBinding))
		{
			fprintf(stderr,"Out of memory!\n");
			exit(-100);
		}

		if (iCol == 1)
		{
			*lppBinding = lpThisBinding;
		}
		else
		{
			lpLastBinding->pNext = lpThisBinding;
		}

		lpLastBinding=lpThisBinding;


		// Figure out the display length of the column 
		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLColAttribute(lpStmt,
							   iCol,
							   SQL_DESC_DISPLAY_SIZE,
							   NULL,
							   0,
							   NULL,
							   &cchDisplay));


		// Figure out if this is a character or numeric column
		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLColAttribute(lpStmt,
								iCol,
								SQL_DESC_CONCISE_TYPE,
								NULL,
								0,
								NULL,
								&ssType));


		lpThisBinding->fChar = (ssType == SQL_CHAR ||
								ssType == SQL_VARCHAR ||
								ssType == SQL_LONGVARCHAR);

		lpThisBinding->pNext = NULL;

		// Arbitrary limit on display size
		if (cchDisplay > DISPLAY_MAX || !cchDisplay || cchDisplay == -1) 
			cchDisplay = DISPLAY_MAX;

		// Allocate a buffer big enough to hold the text representation
		// of the data.  Add one character for the null terminator
		lpThisBinding->szBuffer = (char *)calloc((cchDisplay+1), sizeof(char));

		if (!(lpThisBinding->szBuffer))
		{
			fprintf(stderr,"Out of memory!\n");
			exit(-100);
		}

		// Map this buffer to the driver's buffer.   At Fetch time,
		// the driver will fill in this data.  Note that the size is 
		// count of bytes (for Unicode).  All ODBC functions that take
		// SQLPOINTER use count of bytes; all functions that take only
		// strings use count of characters.
		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLBindCol(lpStmt,
						   iCol,
						   SQL_C_CHAR,
						   (SQLPOINTER) lpThisBinding->szBuffer,
						   (cchDisplay + 1) * sizeof(char),
						   &lpThisBinding->indPtr));


		// Now set the display size that we will use to display
		// the data.   Figure out the length of the column name
		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLColAttribute(lpStmt,
								iCol,
								SQL_DESC_NAME,
								NULL,
								0,
								&cchColumnNameLength,
								NULL));

		lpThisBinding->siDisplaySize = max(cchDisplay, cchColumnNameLength);
		if (lpThisBinding->siDisplaySize < NULL_SIZE)
			lpThisBinding->siDisplaySize = NULL_SIZE;

		*lpDisplay += (SQLSMALLINT)(lpThisBinding->siDisplaySize + DISPLAY_FORMAT_EXTRA);
		
	}

error:
        // TODO: Free memory
		return;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Print the titles of all the columns and set the shell window's width
//
void	DisplayTitles(HSTMT		lpStmt,
					  DWORD		siDisplaySize,
					  BINDING	*pBinding)
{
	char			szTitle[DISPLAY_MAX];
	SQLSMALLINT		iCol = 1;

	for(;pBinding;pBinding=pBinding->pNext)
	{
		CHECK_ODBC_RC(lpStmt,
				SQL_HANDLE_STMT,
				SQLColAttribute(lpStmt,
								iCol++,
								SQL_DESC_NAME,
								szTitle,
								sizeof(szTitle),	
								NULL,
								NULL));

		printf(DISPLAY_FORMAT_C, PIPE,
				 (int)(pBinding->siDisplaySize),
				 (int)(pBinding->siDisplaySize),
				 szTitle);

	}

error:

	printf(" %c",PIPE);
	printf("\n");

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Display error information
//
void HandleError(SQLHANDLE	hHandle,	
				SQLSMALLINT	hType,	
			    RETCODE	RetCode)
{
	SQLSMALLINT	iRec = 0;
	SQLINTEGER	iError;
	SQLCHAR		szMessage[1000];
	SQLCHAR		szState[SQL_SQLSTATE_SIZE];

	if (RetCode == SQL_INVALID_HANDLE)
	{
		fprintf(stderr,"Invalid handle!\n");
		return;
	}

	while (SQLGetDiagRec(hType,
						 hHandle,
						 ++iRec,
						 szState,
						 &iError,
						 szMessage,
						 (SQLSMALLINT)(sizeof(szMessage) / sizeof(char)),
						 (SQLSMALLINT *)NULL) == SQL_SUCCESS)
	{
		
		// Hide data truncated..
		if (strncmp((char *)szState,"01004",5))
			fprintf(stderr,"[%5.5s] %s (%d)\n",szState,szMessage,(int)iError);
	}

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Free buffers
//
BINDING		*FreeBindings(BINDING		*pFirstBinding)
{
	BINDING	 *pThisBinding, *pNextBinding;			

	for (pThisBinding = pFirstBinding;
		 pThisBinding;
		 pThisBinding = pNextBinding)
    {
        pNextBinding = pThisBinding->pNext;
        if(pThisBinding->szBuffer)
        {
            free(pThisBinding->szBuffer);
            pThisBinding->szBuffer = NULL;
        }

        free(pThisBinding);

    } // Loop

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read full command until semicolon entered.
// Return TRUE if command read, FALSE means EOF.
//
int ReadCmd(FILE *fp, char *buf,int bufLen)
{
    int rc = TRUE;

    if(!giCmdLineOptionQuery)
    {
        size_t len = 0;

	    while(TRUE)
        {
            rc = (fgets(buf+len, (int)(bufLen-len), (fp) ? fp : stdin) != NULL);
            if(!rc)
                break;

            len = strlen(buf);
            if(len > 0 && buf[len - 1] == '\n')
                buf[len - 1] = ' ';
            len = strlen(buf);
            if((len > 0 && buf[len - 1] == ';')
                || (_stricmp(buf,"QUIT") == 0)
                || (_stricmp(buf,"QUIT ") == 0)
                || (len == bufLen)
                || strrchr(buf,';'))
                break;

            if((len >= bufLen) || ((bufLen-len) == 1))
            {
                printf("Command size exceed than supported %d max size \n", bufLen);
                rc = FALSE;
                break;
            }
        }
    }

    TrimTrailingWhitespaces(buf);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Trim leading and trailing white spaces.
//
void TrimTrailingWhitespaces(char *str) 
{ 
  char *end;
  char *temp;
 
  // Trim trailing space 
  end = str + strlen(str) - 1; 
  temp = end;
  while(temp > str && isspace(*temp)) 
	  temp--; 
 
  // Write new null terminator 
  if(temp != end)
	*(temp+1) = '\0'; 
} 

/*====================================================================================================================================================*/

long long currentTimeMillis()
{
    return (long long) (time(NULL) * 1000);
//    return (long long)(((double)clock()/CLOCKS_PER_SEC) * 1000);
}

/*====================================================================================================================================================*/

#if defined LINUX 

//---------------------------------------------------------------------------------------------------------igarish
// Dummy function for Windows
//
void *GetDesktopWindow()
{
    return NULL;
}

#endif
