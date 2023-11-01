/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"
#include "rslock.h"
#include "rsini.h"

#include "RsIamEntry.h"
#include "RsErrorException.h"
#include <aws/core/utils/StringUtils.h>
#include<algorithm>

#include <cstdio>
#include <iostream>
#include <map>
#ifdef WIN32
#include "resource.h"
#endif

const bool CAN_OVERRIDE_DSN = true;

RS_GLOBAL_VARS gRsGlobalVars;

#ifdef WIN32
BOOL SQL_API driverConnectProcLoop(HWND    hdlg,
                                    WORD    wMsg,
                                    WORD    wParam,
                                    LPARAM  lParam);
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Call back function. Define in MessageLoopState.h also.
typedef short CSC_RESULT_HANDLER_CALLBACK(void *pCallerContext, PGresult *pgResult);

short RS_ResultHandlerCallbackFunc(void *pCallerContext, PGresult *pgResult);
struct _CscStatementContext *createCscStatementContext(void *pCallerContext, CSC_RESULT_HANDLER_CALLBACK *pResultHandlerCallbackFunc, 
                                                        PGconn *conn, int iUseMsgLoop);
struct _CscStatementContext *releaseCscStatementContext(struct _CscStatementContext *pCscStatementContext);
#ifdef __cplusplus
}
#endif




/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.x function SQLAllocEnv has been replaced by SQLAllocHandle. 
//
SQLRETURN  SQL_API SQLAllocEnv(SQLHENV *pphenv)
{
    SQLRETURN rc;

#if defined LINUX 
    sharedObjectAttach();
#endif

    beginApiMutex(NULL, NULL);

    rc = RS_ENV_INFO::RS_SQLAllocEnv(pphenv);

    endApiMutex(NULL, NULL);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLAllocEnv and SQLAllocHandle. 
//
SQLRETURN  SQL_API RS_ENV_INFO::RS_SQLAllocEnv(SQLHENV *pphenv)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_ENV_INFO *phenv = NULL;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocEnv(FUNC_CALL, 0, pphenv);

    if(pphenv != NULL)
    {
        phenv = (RS_ENV_INFO *) new RS_ENV_INFO();

        if(phenv != NULL)
        {
            // Initialize HENV
            phenv->hApiMutex = rsCreateMutex();
            if(phenv->pEnvAttr != NULL && phenv->hApiMutex != NULL)
            {
                *pphenv = phenv;
            }
            else
            {
                rc = SQL_ERROR;
                goto error;
            }
        }
        else
        {
            rc = SQL_ERROR;
            goto error;
        }
    }
    else
    {
        rc = SQL_ERROR;
        goto error;
    }

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocEnv(FUNC_RETURN, rc, pphenv);

    return rc;

error:

    // Clean-up
    if(phenv)
    {
        if(phenv->hApiMutex)
        {
            rsDestroyMutex(phenv->hApiMutex);
            phenv->hApiMutex = NULL;
        }

        if (phenv->pEnvAttr != NULL) {
          delete phenv->pEnvAttr;
          phenv->pEnvAttr = NULL;
        }

        if (phenv != NULL) {
          delete phenv;
          phenv = NULL;
        }
    }

    // Error condition.
    if(pphenv != NULL)
        *pphenv = SQL_NULL_HENV;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocEnv(FUNC_RETURN, rc, pphenv);

    return rc; 
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.x function SQLAllocConnect has been replaced by SQLAllocHandle.
//
SQLRETURN  SQL_API SQLAllocConnect(SQLHENV phenv,
                                   SQLHDBC *pphdbc)
{
    SQLRETURN rc;

    beginApiMutex((RS_ENV_INFO *)phenv, NULL);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocConnect(FUNC_CALL, 0, phenv, pphdbc);

    rc = RS_ENV_INFO::RS_SQLAllocConnect(phenv, pphdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocConnect(FUNC_RETURN, rc, phenv, pphdbc);

    endApiMutex((RS_ENV_INFO *)phenv, NULL);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLAllocConnect and SQLAllocHandle.
//
SQLRETURN  SQL_API RS_ENV_INFO::RS_SQLAllocConnect(SQLHENV phenv,
                                        SQLHDBC *pphdbc)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
    RS_CONN_INFO *pConn = NULL;

    if(!VALID_HENV(phenv))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pEnv->pErrorList = clearErrorList(pEnv->pErrorList);

    if(pphdbc == NULL)
    {
        rc = SQL_ERROR;
        goto error;
    }
    else
    {
        pConn = (RS_CONN_INFO *) new RS_CONN_INFO((RS_ENV_INFO *)phenv);

        if(pConn != NULL)
        {
            pConn->pConnectProps = (RS_CONNECT_PROPS_INFO *) new  RS_CONNECT_PROPS_INFO();
            pConn->pConnAttr = (RS_CONN_ATTR_INFO *) new RS_CONN_ATTR_INFO();
            pConn->pCmdBuf =   (RS_STR_BUF *)rs_calloc(1, sizeof(RS_STR_BUF));
            pConn->hSemMultiStmt = rsCreateBinarySem(1);
            pConn->hApiMutex = rsCreateMutex();

            if(pConn->pConnectProps == NULL
                || pConn->pConnAttr == NULL
                || pConn->pCmdBuf == NULL
                || pConn->hSemMultiStmt == NULL
                || pConn->hApiMutex == NULL
            )
            {
                rc = SQL_ERROR;
                addError(&pEnv->pErrorList,"HY001", "Memory allocation error", 0, NULL);
                goto error;
            }

            pConn->resetConnectProps();


            // Put HDBC in front in HENV list
            addConnection(pEnv, pConn);
            
            *pphdbc = pConn;
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pEnv->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }
    }

    return rc;

error:

    // Cleanup
    if(pConn)
    {
        if (pConn->pConnectProps != NULL) {
          delete pConn->pConnectProps;
          pConn->pConnectProps = NULL;
        }

        if (pConn->pConnAttr != NULL) {
          delete pConn->pConnAttr;
          pConn->pConnAttr = NULL;
        }

        pConn->pCmdBuf = (struct _RS_STR_BUF *)rs_free(pConn->pCmdBuf);

        if(pConn->hApiMutex)
        {
            rsDestroyMutex(pConn->hApiMutex);
            pConn->hApiMutex = NULL;
        }

        if(pConn->hSemMultiStmt)
        {
            rsDestroySem(pConn->hSemMultiStmt);
            pConn->hSemMultiStmt = NULL;
        }

        if (pConn != NULL) {
          delete pConn;
          pConn = NULL;
        }
    }

    // Error condition.
    if(pphdbc != NULL)
        *pphdbc = SQL_NULL_HDBC;

    return rc; 
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLConnect establishes connections to a driver and a data source. 
// The connection handle references storage of all information about the connection to the data source, 
// including status, transaction state, and error information.
//
SQLRETURN  SQL_API SQLConnect(SQLHDBC phdbc,
                                SQLCHAR *szDSN, 
                                SQLSMALLINT cchDSN,
                                SQLCHAR *szUID, 
                                SQLSMALLINT cchUID,
                                SQLCHAR *szAuthStr, 
                                SQLSMALLINT cchAuthStr)
{
    SQLRETURN rc;

    beginApiMutex(NULL, (RS_CONN_INFO *)phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLConnect(FUNC_CALL, 0, phdbc, szDSN, cchDSN, szUID, cchUID, szAuthStr, cchAuthStr);

    rc = RS_CONN_INFO::RS_SQLConnect(phdbc, szDSN, cchDSN, szUID, cchUID, szAuthStr, cchAuthStr);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLConnect(FUNC_RETURN, rc, phdbc, szDSN, cchDSN, szUID, cchUID, szAuthStr, cchAuthStr);

    endApiMutex(NULL, (RS_CONN_INFO *)phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLConnect and SQLConnectW.
//
SQLRETURN  SQL_API RS_CONN_INFO::RS_SQLConnect(SQLHDBC phdbc,
                                SQLCHAR *szDSN, 
                                SQLSMALLINT cchDSN,
                                SQLCHAR *szUID, 
                                SQLSMALLINT cchUID,
                                SQLCHAR *szAuthStr, 
                                SQLSMALLINT cchAuthStr)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
    char *szDsnName = NULL;
    char *szUserName = NULL;
    char *szPassword = NULL;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    szDsnName = (char *)makeNullTerminatedStr((char *)szDSN, cchDSN, NULL);
    szUserName = (char *)makeNullTerminatedStr((char *)szUID, cchUID, NULL);
    szPassword = (char *)makeNullTerminatedStr((char *)szAuthStr, cchAuthStr, NULL);

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    if(szDsnName != NULL  && *szDsnName != '\0')
    {
        RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;

        if(!pConn->iInternal)
          pConn->resetConnectProps();

        strncpy(pConnectProps->szDSN, szDsnName, MAX_IDEN_LEN-1);
        pConnectProps->szDSN[MAX_IDEN_LEN-1] = '\0';

        if(szUserName)
        {
            strncpy(pConnectProps->szUser, szUserName, MAX_IDEN_LEN-1);
            pConnectProps->szUser[MAX_IDEN_LEN-1] = '\0';
        }

        if(szPassword) {
            strncpy(pConnectProps->szPassword, szPassword, MAX_IDEN_LEN-1);
            pConnectProps->szPassword[MAX_IDEN_LEN-1] = '\0';
        }

        // Read properties using DSN
        if(!pConn->iInternal) {
          pConn->readMoreConnectPropsFromRegistry(FALSE);
        }

		// Check for AuthProfile
		rc = pConn->readAuthProfile(FALSE);
		if (rc == SQL_ERROR)
		{
			goto error;
		}

        // Check for mandatory values
        if(!pConnectProps->isIAMAuth && !pConnectProps->isNativeAuth && pConnectProps->szUser[0] == '\0') {
            rc = SQL_ERROR;
            addError(&pConn->pErrorList, "HY000",
                    "User name is NULL or empty", 0, NULL);
            goto error;
        }

        rc = doConnection(pConn);

        if(rc == SQL_ERROR) 
        {
            goto error; 
        }
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pConn->pErrorList,"HY000", "DSN is NULL or empty", 0, NULL);
        goto error; 
    }

error:

    if(szDsnName != (char *)szDSN)
        szDsnName = (char *)rs_free(szDsnName);

    if(szUserName != (char *)szUID)
        szUserName = (char *)rs_free(szUserName);

    if(szPassword != (char *)szAuthStr)
        szPassword = (char *)rs_free(szPassword);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLConnect.
//
SQLRETURN SQL_API SQLConnectW(SQLHDBC             phdbc,
                                SQLWCHAR*            wszDSN,
                                SQLSMALLINT         cchDSN,
                                SQLWCHAR*            wszUID,
                                SQLSMALLINT         cchUID,
                                SQLWCHAR*            wszAuthStr,
                                SQLSMALLINT         cchAuthStr)
{
    SQLRETURN rc;
    char szDsnName[MAX_IDEN_LEN];
    char szUserName[MAX_IDEN_LEN];
    char szPassword[MAX_IDEN_LEN]; 
    
    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLConnectW(FUNC_CALL, 0, phdbc, wszDSN, cchDSN, wszUID, cchUID, wszAuthStr, cchAuthStr);

    wchar_to_utf8(wszDSN, cchDSN, szDsnName, MAX_IDEN_LEN);
    wchar_to_utf8(wszUID, cchUID, szUserName, MAX_IDEN_LEN);
    wchar_to_utf8(wszAuthStr, cchAuthStr, szPassword, MAX_IDEN_LEN);

    rc = RS_CONN_INFO::RS_SQLConnect(phdbc, (SQLCHAR *)szDsnName, SQL_NTS, (SQLCHAR *)((wszUID) ? szUserName : NULL), SQL_NTS,
                              (SQLCHAR *)((wszAuthStr) ? szPassword : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLConnectW(FUNC_RETURN, rc, phdbc, wszDSN, cchDSN, wszUID, cchUID, wszAuthStr, cchAuthStr);

    endApiMutex(NULL, phdbc);

    return rc; 
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLDriverConnect is an alternative to SQLConnect. It supports data sources that require more connection 
// information than the three arguments in SQLConnect, dialog boxes to prompt the user for all connection information, 
// and data sources that are not defined in the system information.
// SQLDriverConnect provides the following connection attributes:
//  Establish a connection using a connection string that contains the data source name, one or more user IDs, one or more passwords, 
//   and other information required by the data source.
//  Establish a connection using a partial connection string or no additional information; in this case, the Driver Manager and the 
//   driver can each prompt the user for connection information.
//  Establish a connection to a data source that is not defined in the system information. If the application supplies a partial 
//   connection string, the driver can prompt the user for connection information.
//  Establish a connection to a data source using a connection string constructed from the information in a .dsn file.
// After a connection is established, SQLDriverConnect returns the completed connection string. 
// The application can use this string for subsequent connection requests. 
//
SQLRETURN SQL_API SQLDriverConnect(SQLHDBC            phdbc,
                                    SQLHWND           hwnd,
                                    SQLCHAR           *szConnStrIn,
                                    SQLSMALLINT       cbConnStrIn,
                                    SQLCHAR           *szConnStrOut,
                                    SQLSMALLINT       cbConnStrOut,
                                    SQLSMALLINT       *pcbConnStrOut,
                                    SQLUSMALLINT       hDriverCompletion)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDriverConnect(FUNC_CALL, 0, phdbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOut, pcbConnStrOut, hDriverCompletion);

    rc = RS_CONN_INFO::RS_SQLDriverConnect(phdbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOut, pcbConnStrOut, hDriverCompletion);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDriverConnect(FUNC_RETURN, rc, phdbc, hwnd, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOut, pcbConnStrOut, hDriverCompletion);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLDriverConnect and SQLDriverConnectW.
//
SQLRETURN SQL_API RS_CONN_INFO::RS_SQLDriverConnect(SQLHDBC            phdbc,
                                    SQLHWND           hwnd,
                                    SQLCHAR           *szConnStrIn,
                                    SQLSMALLINT       cbConnStrIn,
                                    SQLCHAR           *szConnStrOut,
                                    SQLSMALLINT       cbConnStrOut,
                                    SQLSMALLINT       *pcbConnStrOut,
                                    SQLUSMALLINT       hDriverCompletion)
{
    SQLRETURN rc = SQL_SUCCESS;
#ifdef WIN32
    short iRet;
#endif
    int   iPrompt = FALSE;
    RS_CONNECT_PROPS_INFO *pConnectProps;
    size_t actualOutputLen = 0;
    char *pKeyword = NULL;


    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }
    else
    {
        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

        // Clear error list
        pConn->pErrorList = clearErrorList(pConn->pErrorList);

        if ((hDriverCompletion        != SQL_DRIVER_COMPLETE) 
            &&(hDriverCompletion    != SQL_DRIVER_PROMPT) 
            && (hDriverCompletion    != SQL_DRIVER_COMPLETE_REQUIRED) 
            && (hDriverCompletion    != SQL_DRIVER_NOPROMPT))
        {
            addError(&pConn->pErrorList,"HY000", "Invalid driver completion option", 0, NULL);
            rc = SQL_ERROR;
            goto error;
        }

        if ((cbConnStrIn != SQL_NTS) && (cbConnStrIn < 0))
        {
            addError(&pConn->pErrorList,"HY000","Invalid connection string length", 0, NULL);
            rc = SQL_ERROR;
            goto error;
        }
         
        pConnectProps = pConn->pConnectProps;
        pConn->resetConnectProps();

        if ((szConnStrIn == NULL) || (!cbConnStrIn) ||
            ((cbConnStrIn == SQL_NTS) && (!szConnStrIn[0])))  
        {
            iPrompt = TRUE;
        }

        // Parse the input string for DSN
        pConn->parseConnectString((char *)szConnStrIn, cbConnStrIn, FALSE , TRUE);

        // Read reg using DSN
        pConn->readMoreConnectPropsFromRegistry(TRUE);

        // Parse the input string for all
        pConn->parseConnectString((char *)szConnStrIn, cbConnStrIn, TRUE , FALSE);

        if(hDriverCompletion == SQL_DRIVER_NOPROMPT) 
            iPrompt = FALSE;
        else 
        {
            if(pConnectProps->szHost[0] == '\0'
                || pConnectProps->szPort[0] == '\0'
                || pConnectProps->szDatabase[0] == '\0'
                || pConnectProps->szUser[0] == '\0'
                || pConnectProps->szPassword[0] == '\0')
            {
                iPrompt = TRUE;
            }
        }
            
        if(iPrompt) 
        {
#ifdef WIN32
            if(hwnd == NULL)
            {
                addError(&pConn->pErrorList,"HY092", "Invalid attribute/option identifier", 0, NULL);
                rc = SQL_ERROR;
                goto error;
            }

            iRet = (short)DialogBoxParam((HINSTANCE)gRsGlobalVars.hModule,  
                                             MAKEINTRESOURCE(DRIVER_CONNECT_DIALOG), (HWND) hwnd,
                                             (DLGPROC) driverConnectProcLoop, (LPARAM)phdbc);
          

            if(iRet == -1)
            {
                addError(&pConn->pErrorList,"HY000", "Dialog couldn't created", 0, NULL);
                rc = SQL_ERROR;
                goto error;
            }
            else 
            if(iRet == DRV_CONNECT_DLG_ERROR) 
                rc = SQL_ERROR;
            else 
            if (!iRet)
                rc = SQL_NO_DATA_FOUND;
            else
                rc = SQL_SUCCESS;
#endif
#if defined LINUX 
            addError(&pConn->pErrorList,"HY000", "Dialog couldn't created", 0, NULL);
            rc = SQL_ERROR;
            goto error;
#endif

            if(rc != SQL_SUCCESS)
                goto error;
        }
        else
        {
            if (pConnectProps->szDSN[0] != '\0')
            {
                pConn->iInternal = TRUE;
                rc = RS_CONN_INFO::RS_SQLConnect(phdbc, (SQLCHAR *)(pConnectProps->szDSN), SQL_NTS, (SQLCHAR *)(pConnectProps->szUser), SQL_NTS, (SQLCHAR *)(pConnectProps->szPassword), SQL_NTS);
                pConn->iInternal = FALSE;
            }
            else
            { 
				// Check for AuthProfile
				rc = pConn->readAuthProfile(TRUE);
				if (rc == SQL_ERROR)
				{
					goto error;
				}


                /* DSN less connection 
                */
				if (pConnectProps->szPort[0] == '\0')
					strncpy(pConnectProps->szPort, DEFAULT_PORT, sizeof(pConnectProps->szPort));

                if(pConnectProps->szHost[0] == '\0'
                    || pConnectProps->szDatabase[0] == '\0')
                {
					if (pConnectProps->szHost[0] == '\0')
						addError(&pConn->pErrorList,"HY000", "Required keyword HOST does not found in connection string", 0, NULL);
					else
					if (pConnectProps->szDatabase[0] == '\0')
						addError(&pConn->pErrorList, "HY000", "Required keyword Database does not found in connection string", 0, NULL);
					rc = SQL_ERROR;
                }
                else
                {
                    rc = doConnection(pConn);
                }
            }

            if (rc != SQL_SUCCESS) 
                goto error;
        }

        if(szConnStrOut && (cbConnStrOut > 0))
            *szConnStrOut = '\0';

        if(pConnectProps->szDSN[0])
        {
            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat((char *)szConnStrOut, "DSN=", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat((char *)szConnStrOut, pConnectProps->szDSN, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat((char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen("DSN=");
            actualOutputLen += strlen(pConnectProps->szDSN);
            actualOutputLen += strlen(";");
        }

        if(pConnectProps->szDriver[0])
        {
            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat((char *)szConnStrOut, "Driver=", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat((char *)szConnStrOut, pConnectProps->szDriver, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat((char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen("Driver=");
            actualOutputLen += strlen(pConnectProps->szDriver);
            actualOutputLen += strlen(";");
        }

        if(pConnectProps->szDatabase[0])
        {
            pKeyword = (char *)((pConnectProps->iDatabaseKeyWordType == SHORT_NAME_KEYWORD) ? "DB=" : "Database=");

            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat((char *)szConnStrOut, pKeyword, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat((char *)szConnStrOut, pConnectProps->szDatabase, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat((char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen(pKeyword);
            actualOutputLen += strlen(pConnectProps->szDatabase);
            actualOutputLen += strlen(";");
        }

        if(pConnectProps->szUser[0])
        {
            pKeyword = (char *)((pConnectProps->iUserKeyWordType == SHORT_NAME_KEYWORD) ? "UID=" : "LogonID=");

            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat( (char *)szConnStrOut, pKeyword, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, pConnectProps->szUser, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen(pKeyword);
            actualOutputLen += strlen(pConnectProps->szUser);
            actualOutputLen += strlen(";");
        }

        if(pConnectProps->szPassword[0])
        {
            pKeyword = (char *)((pConnectProps->iPasswordKeyWordType == SHORT_NAME_KEYWORD) ? "PWD=" : "Password=");

            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat( (char *)szConnStrOut, pKeyword, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, pConnectProps->szPassword, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen(pKeyword);
            actualOutputLen += strlen(pConnectProps->szPassword);
            actualOutputLen += strlen(";");
        }

        if(pConnectProps->szHost[0])
        {
            pKeyword = (char *)((pConnectProps->iHostNameKeyWordType == SHORT_NAME_KEYWORD) ? "Server=" : "HostName=");

            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat( (char *)szConnStrOut, pKeyword, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, pConnectProps->szHost, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen(pKeyword);
            actualOutputLen += strlen(pConnectProps->szHost);
            actualOutputLen += strlen(";");
        }

        if(pConnectProps->szPort[0])
        {
            pKeyword = (char *)((pConnectProps->iPortKeyWordType == SHORT_NAME_KEYWORD) ? "Port=" : "PortNumber=");

            if(szConnStrOut && (cbConnStrOut > 0))
            {
                strncat( (char *)szConnStrOut, pKeyword, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, pConnectProps->szPort, (cbConnStrOut - strlen((char *)szConnStrOut)-1));
                strncat( (char *)szConnStrOut, ";", (cbConnStrOut - strlen((char *)szConnStrOut)-1));
            }

            actualOutputLen += strlen(pKeyword);
            actualOutputLen += strlen(pConnectProps->szPort);
            actualOutputLen += strlen(";");
        }
       
        if(pcbConnStrOut)
            *pcbConnStrOut = (short) actualOutputLen;

        if(szConnStrOut && (cbConnStrOut > 0))
        {
            if(actualOutputLen > strlen((char *)szConnStrOut))
                rc = SQL_SUCCESS_WITH_INFO;
        }
        else
        if(actualOutputLen > 0)
            rc = SQL_SUCCESS_WITH_INFO;
    }

error: 

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLDriverConnect.
//
SQLRETURN SQL_API SQLDriverConnectW(SQLHDBC             phdbc,
                                    SQLHWND             hwnd,
                                    SQLWCHAR*            wszConnStrIn,
                                    SQLSMALLINT         cchConnStrIn,
                                    SQLWCHAR*            wszConnStrOut,
                                    SQLSMALLINT         cchConnStrOut,
                                    SQLSMALLINT*        pcchConnStrOut,
                                    SQLUSMALLINT        hDriverCompletion)
{
    SQLRETURN rc;
    char *szConnStrIn;
    char *szConnStrOut;
    size_t len;
    
    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDriverConnectW(FUNC_CALL, 0, phdbc, hwnd, wszConnStrIn, cchConnStrIn, wszConnStrOut, cchConnStrOut, pcchConnStrOut, hDriverCompletion);

    len = calculate_utf8_len(wszConnStrIn, cchConnStrIn);

    szConnStrIn = (char *)((len > 0) ? rs_calloc(sizeof(char),len + 1) : NULL);

    if(wszConnStrOut != NULL)
    {
        if(cchConnStrOut >= 0)
            szConnStrOut = (char *)rs_calloc(sizeof(char),cchConnStrOut + 1);
        else
            szConnStrOut = NULL;
    }
    else
        szConnStrOut = NULL;

    wchar_to_utf8(wszConnStrIn, cchConnStrIn, szConnStrIn, len);

    rc = RS_CONN_INFO::RS_SQLDriverConnect(phdbc, hwnd, (SQLCHAR *)szConnStrIn, cchConnStrIn, (SQLCHAR *)szConnStrOut, cchConnStrOut, pcchConnStrOut, hDriverCompletion);

    if(SQL_SUCCEEDED(rc))
        utf8_to_wchar(szConnStrOut, cchConnStrOut, wszConnStrOut, cchConnStrOut);

    szConnStrIn = (char *)rs_free(szConnStrIn);
    szConnStrOut = (char *)rs_free(szConnStrOut);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDriverConnectW(FUNC_RETURN, rc, phdbc, hwnd, wszConnStrIn, cchConnStrIn, wszConnStrOut, cchConnStrOut, pcchConnStrOut, hDriverCompletion);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLBrowseConnect supports an iterative method of discovering and enumerating the attributes and attribute values required to connect 
// to a data source. Each call to SQLBrowseConnect returns successive levels of attributes and attribute values. 
// When all levels have been enumerated, a connection to the data source is completed and a complete connection string is returned by 
// SQLBrowseConnect. A return code of SQL_SUCCESS or SQL_SUCCESS_WITH_INFO indicates that all connection information has been specified 
// and the application is now connected to the data source.
//
SQLRETURN SQL_API SQLBrowseConnect(SQLHDBC          phdbc,
                                    SQLCHAR       *szConnStrIn,
                                    SQLSMALLINT   cbConnStrIn,
                                    SQLCHAR       *szConnStrOut,
                                    SQLSMALLINT   cbConnStrOut,
                                    SQLSMALLINT   *pcbConnStrOut)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBrowseConnect(FUNC_CALL, 0, phdbc, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOut, pcbConnStrOut);

    rc = RS_CONN_INFO::RS_SQLBrowseConnect(phdbc, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOut, pcbConnStrOut);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBrowseConnect(FUNC_RETURN, rc, phdbc, szConnStrIn, cbConnStrIn, szConnStrOut, cbConnStrOut, pcbConnStrOut);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLBrowseConnect and SQLBrowseConnectW.
//
SQLRETURN SQL_API RS_CONN_INFO::RS_SQLBrowseConnect(SQLHDBC          phdbc,
                                    SQLCHAR       *szConnStrIn,
                                    SQLSMALLINT   cbConnStrIn,
                                    SQLCHAR       *szConnStrOut,
                                    SQLSMALLINT   cbConnStrOut,
                                    SQLSMALLINT   *pcbConnStrOut)
{
    SQLRETURN   rc = SQL_SUCCESS;
    int  iNeedMoreData = 0;      
    char *pszBrowseConnectInStr = NULL;
    char *pConnStr;
    RS_CONNECT_PROPS_INFO *pConnectProps;
    size_t actualOutputLen = 0;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }
    else
    {
        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

        // Clear error list
        pConn->pErrorList = clearErrorList(pConn->pErrorList);

        if ((cbConnStrIn != SQL_NTS) && (cbConnStrIn < 0))
        {
            addError(&pConn->pErrorList,"HY000","Invalid connection string length", 0, NULL);
            rc = SQL_ERROR;
            goto error;
        }

        pszBrowseConnectInStr = rs_strdup((char *)szConnStrIn, cbConnStrIn);
        pConnectProps = pConn->pConnectProps;

        if(pConn->iBrowseIteration == 0)
        {
            pConn->resetConnectProps();
            pConn->setConnectStr(pszBrowseConnectInStr);
        }
        else
          pConn->appendConnectStr(pszBrowseConnectInStr);

        // Free it bcoz we already copied it.
        pszBrowseConnectInStr = (char *)rs_free(pszBrowseConnectInStr);

        (pConn->iBrowseIteration)++;

        pConnStr = pConn->getConnectStr();
        
        iNeedMoreData = RS_CONN_INFO::doesNeedMoreBrowseConnectStr(pConnStr, (char *)szConnStrOut, cbConnStrOut, pcbConnStrOut, pConn->iBrowseIteration, pConn);
        if(iNeedMoreData > 0)
        {
            rc = SQL_NEED_DATA;
            goto error;
        }
        else
        if(iNeedMoreData < 0)
        {
            addError(&pConn->pErrorList,"01S00","Invalid connection string attribute", 0, NULL);
            rc = SQL_ERROR;
            pConn->iBrowseIteration = 0;
            goto error;
        }
        else
            pConn->iBrowseIteration = 0;

        // Parse the input string for DSN
        pConn->parseConnectString(pConnStr, SQL_NTS, FALSE, TRUE);

        // Read reg using DSN
        pConn->readMoreConnectPropsFromRegistry(TRUE);

        // Parse the input string for all
        pConn->parseConnectString(pConnStr, SQL_NTS, TRUE, FALSE);

        if (pConnectProps->szDSN[0] != '\0')
        {
            pConn->iInternal = TRUE;
            rc = RS_CONN_INFO::RS_SQLConnect(phdbc, (SQLCHAR *)(pConnectProps->szDSN), SQL_NTS, (SQLCHAR *)(pConnectProps->szUser), SQL_NTS, (SQLCHAR *)(pConnectProps->szPassword), SQL_NTS);
            pConn->iInternal = FALSE;
        }
        else
        { 
            /* DSN less connection 
            */

			// Check for AuthProfile
			rc = pConn->readAuthProfile(TRUE);
			if (rc == SQL_ERROR)
			{
				goto error;
			}

            if(pConnectProps->szHost[0] == '\0'
                || pConnectProps->szPort[0] == '\0'
                || pConnectProps->szDatabase[0] == '\0'
                || pConnectProps->szUser[0] == '\0')
            {
                addError(&pConn->pErrorList,"HY000", "Required keyword(s) HOST/PORT/Database/UID does not found in connection string", 0, NULL);
                rc = SQL_ERROR;
            }
            else
            {
                rc = doConnection(pConn);
            }
        }

        if(rc != SQL_SUCCESS) 
            goto error;

        if(pConnStr)
            actualOutputLen = strlen(pConnStr);

        if(pcbConnStrOut)
            *pcbConnStrOut = (short)actualOutputLen;

        if(szConnStrOut && (cbConnStrOut > 0))
        {
            *szConnStrOut = '\0';

            strncpy((char *)szConnStrOut,pConnStr,cbConnStrOut-1);
        }

        if(szConnStrOut && (cbConnStrOut > 0))
        {
            if(actualOutputLen > strlen((char *)szConnStrOut))
                rc = SQL_SUCCESS_WITH_INFO;
        }
        else
        if(actualOutputLen > 0)
            rc = SQL_SUCCESS_WITH_INFO;

    }

error: 

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLBrowseConnect.
//
SQLRETURN SQL_API SQLBrowseConnectW(SQLHDBC     phdbc,
                                    SQLWCHAR*    wszConnStrIn,
                                    SQLSMALLINT  cchConnStrIn,
                                    SQLWCHAR*     wszConnStrOut,
                                    SQLSMALLINT   cchConnStrOut,
                                    SQLSMALLINT*  pcchConnStrOut)
{
    SQLRETURN rc;
    char *szConnStrIn;
    char *szConnStrOut;
    size_t len;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBrowseConnectW(FUNC_CALL, 0, phdbc, wszConnStrIn, cchConnStrIn, wszConnStrOut, cchConnStrOut, pcchConnStrOut);

    len = calculate_utf8_len(wszConnStrIn, cchConnStrIn);

    szConnStrIn = (char *)((len > 0) ? rs_calloc(sizeof(char), len + 1) : NULL);

    if(wszConnStrOut != NULL)
    {
        if(cchConnStrOut >= 0)
            szConnStrOut = (char *)rs_calloc(sizeof(char), cchConnStrOut + 1);
        else
            szConnStrOut = NULL;
    }
    else
        szConnStrOut = NULL;

    wchar_to_utf8(wszConnStrIn, cchConnStrIn, szConnStrIn, len);

    rc = RS_CONN_INFO::RS_SQLBrowseConnect(phdbc, (SQLCHAR *)szConnStrIn, cchConnStrIn, (SQLCHAR *)szConnStrOut, cchConnStrOut, pcchConnStrOut);

    if(SQL_SUCCEEDED(rc) || rc == SQL_NEED_DATA)
        utf8_to_wchar(szConnStrOut, cchConnStrOut, wszConnStrOut, cchConnStrOut);

    szConnStrIn = (char *)rs_free(szConnStrIn);
    szConnStrOut = (char *)rs_free(szConnStrOut);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBrowseConnectW(FUNC_RETURN, rc, phdbc, wszConnStrIn, cchConnStrIn, wszConnStrOut, cchConnStrOut, pcchConnStrOut);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLDisconnect closes the connection associated with a specific connection handle.
//
SQLRETURN  SQL_API SQLDisconnect(SQLHDBC phdbc)
{
    SQLRETURN rc = SQL_SUCCESS;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDisconnect(FUNC_CALL, 0, phdbc);

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }
    else
    {
        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

        // Clear error list
        pConn->pErrorList = clearErrorList(pConn->pErrorList);

        if(pConn->isConnectionOpen())
        {
            libpqDisconnect(pConn);

            pConn->iStatus = RS_CLOSE_CONNECTION;
        }
    }

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDisconnect(FUNC_RETURN, rc, phdbc);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLFreeConnect has been replaced by SQLFreeHandle. 
//
SQLRETURN  SQL_API SQLFreeConnect(SQLHDBC phdbc)
{
    SQLRETURN rc;
    RS_ENV_INFO *pEnv = (phdbc) ? ((RS_CONN_INFO *)phdbc)->phenv : NULL;

    beginApiMutex(pEnv, NULL);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeConnect(FUNC_CALL, 0, phdbc);

    rc = RS_CONN_INFO::RS_SQLFreeConnect(phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeConnect(FUNC_RETURN, rc, phdbc);

    endApiMutex(pEnv, NULL);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLFreeConnect and SQLFreeHandle.
//
SQLRETURN  SQL_API RS_CONN_INFO::RS_SQLFreeConnect(SQLHDBC phdbc)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }
    else
    {
        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
        RS_STMT_INFO *curr;

        // Clear error list
        pConn->pErrorList = clearErrorList(pConn->pErrorList);

        // close/free statement
        curr = pConn->phstmtHead;
        while(curr != NULL)
        {
            RS_STMT_INFO *next = curr->pNext;

            RS_STMT_INFO::RS_SQLFreeStmt(curr, SQL_DROP, TRUE);

            curr = next;
        }

        libpqFreeConnect(pConn);

        // Remove from HENV list
        removeConnection(pConn);

        // Reset values, in case application use it again
        pConn->phenv=NULL;

        // Free connection props
        if(pConn->pConnectProps != NULL)
        {
            pConn->pConnectProps->pConnectStr = (char *)rs_free(pConn->pConnectProps->pConnectStr);
            pConn->pConnectProps->pInitializationString = (char *)rs_free(pConn->pConnectProps->pInitializationString);
            if(pConn->pConnectProps->pIamProps)
            {
              pConn->pConnectProps->pIamProps->pszJwt = (char *)rs_free(pConn->pConnectProps->pIamProps->pszJwt);
              pConn->pConnectProps->pIamProps = (RS_IAM_CONN_PROPS_INFO *)rs_free(pConn->pConnectProps->pIamProps);
            }

            if(pConn->pConnectProps->pHttpsProps)
              pConn->pConnectProps->pHttpsProps = (RS_PROXY_CONN_PROPS_INFO *)rs_free(pConn->pConnectProps->pHttpsProps);

			if (pConn->pConnectProps->pTcpProxyProps)
				pConn->pConnectProps->pTcpProxyProps = (RS_TCP_PROXY_CONN_PROPS_INFO *)rs_free(pConn->pConnectProps->pTcpProxyProps);

            delete pConn->pConnectProps;
            pConn->pConnectProps = NULL;
        }

        // Free connection attr
        if(pConn->pConnAttr != NULL)
        {
            pConn->pConnAttr->pCurrentCatalog = (char *)rs_free(pConn->pConnAttr->pCurrentCatalog);
            pConn->pConnAttr->pTraceFile = (char *)rs_free(pConn->pConnAttr->pTraceFile);
            pConn->pConnAttr->pTranslateLib = (char *)rs_free(pConn->pConnAttr->pTranslateLib);

            delete pConn->pConnAttr;
            pConn->pConnAttr = NULL;
        }

        // Release cmd buffer
        releasePaStrBuf(pConn->pCmdBuf);
        pConn->pCmdBuf = (struct _RS_STR_BUF *)rs_free(pConn->pCmdBuf);

        // Release explicit HDESC
        pConn->phdescHead = releaseExplicitDescs(pConn->phdescHead);

        // Delete API mutex
        rsDestroyMutex(pConn->hApiMutex);
        pConn->hApiMutex = NULL;

        // Delete sem
        rsDestroySem(pConn->hSemMultiStmt);
        pConn->hSemMultiStmt = NULL;

        // Free connection
        if (pConn != NULL) {
          delete pConn;
          pConn = NULL;
          phdbc = NULL;
        }
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLFreeEnv has been replaced by SQLFreeHandle.
//
SQLRETURN  SQL_API SQLFreeEnv(SQLHENV phenv)
{
    SQLRETURN rc;

    beginApiMutex(NULL, NULL);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeEnv(FUNC_CALL, 0, phenv);

    rc = RS_ENV_INFO::RS_SQLFreeEnv(phenv);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeEnv(FUNC_RETURN, rc, phenv);

    endApiMutex(NULL, NULL);

#if defined LINUX 
    sharedObjectDetach();
#endif

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLFreeEnv and SQLFreeHandle.
//
SQLRETURN  SQL_API RS_ENV_INFO::RS_SQLFreeEnv(SQLHENV phenv)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(!VALID_HENV(phenv))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }
    else
    {
        RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
        RS_CONN_INFO *curr;

        // Clear error list
        pEnv->pErrorList = clearErrorList(pEnv->pErrorList);

        // Disconnect/free connection
        curr = pEnv->phdbcHead;
        while(curr != NULL)
        {
            RS_CONN_INFO *next = curr->pNext;

            SQLDisconnect(curr);
            SQLFreeConnect(curr);

            curr = next;
        }

        // Free HENV mutex
        rsDestroyMutex(pEnv->hApiMutex);
        pEnv->hApiMutex = NULL;

        // Free env
        if (pEnv->pEnvAttr != NULL) {
          delete pEnv->pEnvAttr;
          pEnv->pEnvAttr = NULL;
        }

        if (pEnv != NULL) {
          delete pEnv;
          pEnv = NULL;
          phenv = NULL;
        }
    }

//    dumpMemLeak();

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLAllocHandle allocates an environment, connection, statement, or descriptor handle.
//
SQLRETURN  SQL_API SQLAllocHandle(SQLSMALLINT hHandleType,
                                    SQLHANDLE pInputHandle, 
                                    SQLHANDLE *ppOutputHandle)
{
    SQLRETURN       rc;
    
#if defined LINUX 
    if(hHandleType == SQL_HANDLE_ENV)
        sharedObjectAttach();
#endif

    if(hHandleType == SQL_HANDLE_ENV)
        beginApiMutex(NULL, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        beginApiMutex(pInputHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_STMT)
        beginApiMutex(NULL, pInputHandle);
    else
    if(hHandleType == SQL_HANDLE_DESC)
        beginApiMutex(NULL, pInputHandle);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocHandle(FUNC_CALL, 0, hHandleType, pInputHandle, ppOutputHandle);

    rc = RS_SQLAllocHandle(hHandleType, pInputHandle, ppOutputHandle);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocHandle(FUNC_RETURN, rc, hHandleType, pInputHandle, ppOutputHandle);

    if(hHandleType == SQL_HANDLE_ENV)
        endApiMutex(NULL, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        endApiMutex(pInputHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_STMT)
        endApiMutex(NULL, pInputHandle);
    else
    if(hHandleType == SQL_HANDLE_DESC)
        endApiMutex(NULL, pInputHandle);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Call appropriate allocation function depend on handle type.
//
SQLRETURN  SQL_API RS_SQLAllocHandle(SQLSMALLINT hHandleType,
                                    SQLHANDLE pInputHandle, 
                                    SQLHANDLE *ppOutputHandle)
{
    SQLRETURN       rc;
    
    if(hHandleType == SQL_HANDLE_ENV)
        rc = RS_ENV_INFO::RS_SQLAllocEnv(ppOutputHandle);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        rc = RS_ENV_INFO::RS_SQLAllocConnect(pInputHandle,ppOutputHandle);
    else
    if(hHandleType == SQL_HANDLE_STMT)
        rc = RS_CONN_INFO::RS_SQLAllocStmt(pInputHandle,ppOutputHandle);
    else
    if(hHandleType == SQL_HANDLE_DESC)
        rc = RS_DESC_INFO::RS_SQLAllocDesc(pInputHandle,ppOutputHandle);
    else
        rc = SQL_ERROR;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLFreeHandle frees resources associated with a specific environment, connection, statement, or descriptor handle.
//
SQLRETURN  SQL_API SQLFreeHandle(SQLSMALLINT hHandleType, 
                                 SQLHANDLE pHandle)
{
    SQLRETURN       rc;
    RS_ENV_INFO *pEnv = NULL;
    RS_CONN_INFO  *pConn = NULL;

    if(hHandleType == SQL_HANDLE_ENV)
        beginApiMutex(NULL, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
    {
        pEnv = (pHandle) ? ((RS_CONN_INFO  *)pHandle)->phenv : NULL;
        beginApiMutex(pEnv, NULL);
    }
    else
    if(hHandleType == SQL_HANDLE_STMT)
    {
        pConn = (pHandle) ? ((RS_STMT_INFO  *)pHandle)->phdbc : NULL;
        beginApiMutex(NULL, pConn);
    }
    else
    if(hHandleType == SQL_HANDLE_DESC)
    {
        pConn = (pHandle) ? ((RS_DESC_INFO  *)pHandle)->phdbc : NULL;
        beginApiMutex(NULL, pConn);
    }

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeHandle(FUNC_CALL, 0, hHandleType, pHandle);

    rc = RS_SQLFreeHandle(hHandleType, pHandle);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeHandle(FUNC_RETURN, rc, hHandleType, pHandle);

    if(hHandleType == SQL_HANDLE_ENV)
        endApiMutex(NULL, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
    {
        endApiMutex(pEnv, NULL);
    }
    else
    if(hHandleType == SQL_HANDLE_STMT)
    {
        endApiMutex(NULL, pConn);
    }
    else
    if(hHandleType == SQL_HANDLE_DESC)
    {
        endApiMutex(NULL, pConn);
    }

#if defined LINUX 
    if(hHandleType == SQL_HANDLE_ENV)
        sharedObjectDetach();
#endif

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Free the handle depending upon the handle type.
//
SQLRETURN  SQL_API RS_SQLFreeHandle(SQLSMALLINT hHandleType, 
                                    SQLHANDLE pHandle)
{
    SQLRETURN       rc;
 
    if(hHandleType == SQL_HANDLE_ENV)
        rc = RS_ENV_INFO::RS_SQLFreeEnv(pHandle);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        rc = RS_CONN_INFO::RS_SQLFreeConnect(pHandle);
    else
    if(hHandleType == SQL_HANDLE_STMT)
        rc = RS_STMT_INFO::RS_SQLFreeStmt(pHandle,SQL_DROP, FALSE);
    else
    if(hHandleType == SQL_HANDLE_DESC)
        rc = RS_DESC_INFO::RS_SQLFreeDesc(pHandle);
    else
        rc = SQL_ERROR;

    return rc;
}

/*====================================================================================================================================================*/


//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.x function SQLAllocStmt has been replaced by SQLAllocHandle. 
//
SQLRETURN  SQL_API SQLAllocStmt(SQLHDBC phdbc,
                                SQLHSTMT *pphstmt)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocStmt(FUNC_CALL, 0, phdbc, pphstmt);

    rc = RS_CONN_INFO::RS_SQLAllocStmt(phdbc, pphstmt);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLAllocStmt(FUNC_RETURN, rc, phdbc, pphstmt);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLAllocStmt and SQLAllocHandle.
//
SQLRETURN  SQL_API RS_CONN_INFO::RS_SQLAllocStmt(SQLHDBC phdbc,
                                    SQLHSTMT *pphstmt)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
    RS_STMT_INFO *pStmt = NULL;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    if(pphstmt == NULL)
    {
        rc = SQL_ERROR;
        goto error;
    }
    else
    {
        pStmt = (RS_STMT_INFO *)new RS_STMT_INFO((RS_CONN_INFO *)phdbc);

        if(pStmt != NULL)
        {
            // Initialize HSTMT
            pStmt->pAPD = allocateDesc(pConn, RS_APD, TRUE);
            pStmt->pIPD = allocateDesc(pConn, RS_IPD, TRUE);
            pStmt->pARD = allocateDesc(pConn, RS_ARD, TRUE);
            pStmt->pIRD = allocateDesc(pConn, RS_IRD, TRUE);

            pStmt->pStmtAttr = (RS_STMT_ATTR_INFO *)new RS_STMT_ATTR_INFO(pConn, pStmt);
            pStmt->pCmdBuf =   (struct _RS_STR_BUF *)rs_calloc(1, sizeof(RS_STR_BUF));

            if(!pStmt->pAPD
                || !pStmt->pIPD 
                || !pStmt->pARD
                || !pStmt->pIRD
                || !pStmt->pStmtAttr
                || !pStmt->pCmdBuf)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY001", "Memory allocation error", 0, NULL);
                goto error;
            }

            pStmt->pCscStatementContext = createCscStatementContext(pStmt, RS_ResultHandlerCallbackFunc, pConn->pgConn, TRUE);


            // Put HSTMT in front in HDBC list
            addStatement(pConn, pStmt);
            
            *pphstmt = pStmt;
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pConn->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }
    }

    return rc;

error:

    // Cleanup
    if(pStmt != NULL)
    {
        // Free statement attr
      if (pStmt->pStmtAttr != NULL) {
        delete pStmt->pStmtAttr;
        pStmt->pStmtAttr = NULL;
      }

        // Free descriptor
        pStmt->pAPD = releaseDescriptor(pStmt->pAPD, TRUE);
        pStmt->pIPD = releaseDescriptor(pStmt->pIPD, TRUE);
        pStmt->pARD = releaseDescriptor(pStmt->pARD, TRUE);
        pStmt->pIRD = releaseDescriptor(pStmt->pIRD, TRUE);

        // Release cmd buffer
        releasePaStrBuf(pStmt->pCmdBuf);
        pStmt->pCmdBuf = (struct _RS_STR_BUF *)rs_free(pStmt->pCmdBuf);

        // Free statement
        if(pStmt != NULL) {
          delete pStmt;
          pStmt = NULL;
        }
    }

    // Error condition.
    if(pphstmt != NULL)
        *pphstmt = SQL_NULL_HSTMT;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLFreeStmt stops processing associated with a specific statement, closes any open cursors associated with the statement, 
// discards pending results, frees all resources associated with the statement handle.
//
SQLRETURN  SQL_API SQLFreeStmt(SQLHSTMT phstmt,
                               SQLUSMALLINT uhOption)
{
    SQLRETURN rc;
    RS_CONN_INFO  *pConn = (phstmt) ? ((RS_STMT_INFO *)phstmt)->phdbc : NULL;

    beginApiMutex(NULL, pConn);

    rc = RS_STMT_INFO::RS_SQLFreeStmt(phstmt, uhOption, FALSE);

    endApiMutex(NULL, pConn);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLFreeStmt and SQLFreeHandle.
//
SQLRETURN  SQL_API RS_STMT_INFO::RS_SQLFreeStmt(SQLHSTMT phstmt,
                                    SQLUSMALLINT uhOption,
                                    int iInternalFlag)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeStmt(FUNC_CALL, 0, phstmt, uhOption);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    // Free option
    switch (uhOption) 
    {
        case SQL_DROP: 
        {
            // Free execution thread, if any
            waitAndFreeExecThread(pStmt, (iInternalFlag) ? FALSE : TRUE);

            // Release prepared stmt in the server
            libpqExecuteDeallocateCommand(pStmt, TRUE, TRUE);

            // Release prepare stmt related resources
            releasePrepares(pStmt);

            // Do any cleanup for libpq
            pStmt->InternalSQLCloseCursor();

            // Release cols binding
            clearBindColList(pStmt->pStmtAttr->pARD);

            // Release params binding
            clearBindParamList(pStmt);

            // Remove from HDBC list
            removeStatement(pStmt);

            // Reset values, in case application use it again
            pStmt->phdbc=NULL;
            pStmt->iStatus = RS_DROP_STMT;

            // Free statement attr
            if (pStmt->pStmtAttr != NULL) {
              delete pStmt->pStmtAttr;
              pStmt->pStmtAttr = NULL;
            }

            // Free descriptor
            pStmt->pAPD = releaseDescriptor(pStmt->pAPD, TRUE);
            pStmt->pIPD = releaseDescriptor(pStmt->pIPD, TRUE);
            pStmt->pARD = releaseDescriptor(pStmt->pARD, TRUE);
            pStmt->pIRD = releaseDescriptor(pStmt->pIRD, TRUE);

            // Release cmd buffer
            releasePaStrBuf(pStmt->pCmdBuf);
            pStmt->pCmdBuf = (struct _RS_STR_BUF *)rs_free(pStmt->pCmdBuf);
            setParamMarkerCount(pStmt, 0);

            // Release CSC statement context
            pStmt->pCscStatementContext = releaseCscStatementContext(pStmt->pCscStatementContext);

            // Release COPY command info
            if(pStmt->pCopyCmd)
            {
                pStmt->pCopyCmd->pszLocalFileName = (char *)rs_free(pStmt->pCopyCmd->pszLocalFileName);
                delete pStmt->pCopyCmd;
                pStmt->pCopyCmd = NULL;
            }

            // Release UNLOAD command info
            if(pStmt->pUnloadCmd)
            {
                pStmt->pUnloadCmd->pszLocalOutFileName = (char *)rs_free(pStmt->pUnloadCmd->pszLocalOutFileName);
                delete pStmt->pUnloadCmd;
                pStmt->pUnloadCmd = NULL;
            }

            // Release user insert command, if any.
            pStmt->pszUserInsertCmd = (char *)rs_free(pStmt->pszUserInsertCmd);

            // Release last batch multi-insert command, if any.
            releasePaStrBuf(pStmt->pszLastBatchMultiInsertCmd);
            pStmt->pszLastBatchMultiInsertCmd = (struct _RS_STR_BUF *)rs_free(pStmt->pszLastBatchMultiInsertCmd);

            // Free statement
            if (pStmt != NULL) {
              delete pStmt;
              pStmt = NULL;
              phstmt = NULL;
            }

            break;
        }

        case SQL_CLOSE: 
        {
            // Close the cursor
          pStmt->InternalSQLCloseCursor();
            break;
        }

        case SQL_UNBIND: 
        {
            // Release cols binding
            clearBindColList(pStmt->pStmtAttr->pARD);

            break;
        }

        case SQL_RESET_PARAMS:  
        {
            // Release params binding
            clearBindParamList(pStmt);

            break;
        }

        default: break;
    } // Switch

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFreeStmt(FUNC_RETURN, rc, phstmt, uhOption);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLAllocDesc and SQLAllocHandle.
//
SQLRETURN  SQL_API RS_DESC_INFO::RS_SQLAllocDesc(SQLHDBC phdbc,
                                    SQLHDESC *pphdesc)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;


    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    if(pphdesc == NULL)
    {
        rc = SQL_ERROR;
        goto error;
    }
    else
    {
        RS_DESC_INFO *pDesc = allocateDesc(pConn, RS_UNKNOWN_DESC_TYPE, FALSE);

        if(pDesc != NULL)
        {
            // Put HDESC in front in HDBC list
            addDescriptor(pConn, pDesc);
            
            *pphdesc = pDesc;
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pConn->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }
    }

    return rc;

error:

    // Error condition.
    if(pphdesc != NULL)
        *pphdesc = SQL_NULL_HDESC;


    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLFreeDesc and SQLFreeHandle.
//
SQLRETURN  SQL_API RS_DESC_INFO::RS_SQLFreeDesc(SQLHDESC phdesc)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;

    if(!VALID_HDESC(phdesc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Remove from HDBC list
    removeDescriptor(pDesc);

    // Free descriptor
    pDesc = releaseDescriptor(pDesc, FALSE);

error:

    return rc;
}

/*====================================================================================================================================================*/

int RS_CONN_INFO::readAuthProfile(int append)
{
	RS_CONN_INFO *pConn = this;
	int rc = SQL_SUCCESS;

	if (pConn->pConnectProps->pIamProps
		&& pConn->pConnectProps->pIamProps->szAuthProfile[0] != '\0')
	{
		try
		{
			char *pAuthProfileConnStr;
			RS_IAM_CONN_PROPS_INFO *pIamProps = pConn->pConnectProps->pIamProps;

			// Copy some DB Properties required for IAM
			rs_strncpy(pIamProps->szHost, pConnectProps->szHost, sizeof(pIamProps->szHost));

			pAuthProfileConnStr = RsIamEntry::ReadAuthProfile(pConn->pConnectProps->isIAMAuth,
				pIamProps,
				pConn->pConnectProps->pHttpsProps);
			if (pAuthProfileConnStr && *pAuthProfileConnStr != '\0')
			{
				// Parse the input string for all
				pConn->parseConnectString(pAuthProfileConnStr, SQL_NTS, append, FALSE);
			}

			if (pAuthProfileConnStr)
				free(pAuthProfileConnStr);
		}
		catch (RsErrorException& ex)
		{
			// Add error and return SQL_ERROR
			addError(&pConn->pErrorList, "HY000", ex.getErrorMessage(), 0, pConn);
			rc = SQL_ERROR;
			return rc;
		}
		catch (...)
		{
			addError(&pConn->pErrorList, "HY000", "IAM Unknown Error", 0, pConn);
			rc = SQL_ERROR;
			return rc;
		}

	}

	return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Parse the connection string and put attribute and value in the connection object.
// Return TRUE if connection string is valid otherwise FALSE.
//
int RS_CONN_INFO::parseConnectString(char *szConnStrIn, size_t cbConnStrIn, int append,int onlyDSN)
{
    RS_CONN_INFO *pConn = this;
    int valid = TRUE;
    RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;
    RS_CONN_ATTR_INFO *pConnAttr = pConn->pConnAttr;
    RS_IAM_CONN_PROPS_INFO *pIamProps = pConnectProps->pIamProps;
    RS_PROXY_CONN_PROPS_INFO *pHttpsProps = pConnectProps->pHttpsProps;
	RS_TCP_PROXY_CONN_PROPS_INFO *pTcpProxyProps = pConnectProps->pTcpProxyProps;

    if(!pConnectProps->pIamProps){
      pConnectProps->pIamProps = (RS_IAM_CONN_PROPS_INFO *)rs_calloc(1, sizeof(RS_IAM_CONN_PROPS_INFO));
      pIamProps = pConnectProps->pIamProps;
    }
    if(!pConnectProps->pHttpsProps){
      pConnectProps->pHttpsProps = (RS_PROXY_CONN_PROPS_INFO *)rs_calloc(1,sizeof(RS_PROXY_CONN_PROPS_INFO));
      pHttpsProps = pConnectProps->pHttpsProps;
    }
	if (!pConnectProps->pTcpProxyProps) {
      pConnectProps->pTcpProxyProps = (RS_TCP_PROXY_CONN_PROPS_INFO *)rs_calloc(
          1, sizeof(RS_TCP_PROXY_CONN_PROPS_INFO));
		pTcpProxyProps = pConnectProps->pTcpProxyProps;
	}

    // Internal lambda function applied to each extracted key-value
    auto processKeyVal = [&](const char *pname, const char *pval) {
      if (onlyDSN) {
        // We are looking for only DSN first.
        if (_stricmp(pname, RS_DSN) == 0) {
          if (pConnectProps->szDSN[0] == '\0') {
                            strncpy(pConnectProps->szDSN, pval, MAX_IDEN_LEN - 1);
                            pConnectProps->szDSN[MAX_IDEN_LEN - 1] = '\0';
          }
        }
      } else {
        /*
        * Store the value, if same param is not repeated.
        */
        if (_stricmp(pname, RS_HOST_NAME) == 0 ||
            _stricmp(pname, RS_HOST) == 0 || _stricmp(pname, RS_SERVER) == 0) {
          pConnectProps->iHostNameKeyWordType =
              (_stricmp(pname, RS_HOST_NAME) == 0) ? LONG_NAME_KEYWORD
                                                   : SHORT_NAME_KEYWORD;
                if (CAN_OVERRIDE_DSN || pConnectProps->szHost[0] == '\0') {
                    strncpy(pConnectProps->szHost, pval, MAX_IDEN_LEN - 1);
                    pConnectProps->szHost[MAX_IDEN_LEN - 1] = '\0';
                }
            } else if (_stricmp(pname, RS_PORT_NUMBER) == 0 ||
                        _stricmp(pname, RS_PORT) == 0) {
                pConnectProps->iPortKeyWordType =
                    (_stricmp(pname, RS_PORT_NUMBER) == 0)
                        ? LONG_NAME_KEYWORD
                        : SHORT_NAME_KEYWORD;
                if (CAN_OVERRIDE_DSN || pConnectProps->szPort[0] == '\0') {
                    strncpy(pConnectProps->szPort, pval, MAX_IDEN_LEN - 1);
                    pConnectProps->szPort[MAX_IDEN_LEN - 1] = '\0';
                }
            } else if (_stricmp(pname, RS_DATABASE) == 0 ||
                        _stricmp(pname, RS_DB) == 0) {
                pConnectProps->iDatabaseKeyWordType =
                    (_stricmp(pname, RS_DATABASE) == 0)
                        ? LONG_NAME_KEYWORD
                        : SHORT_NAME_KEYWORD;
                if (CAN_OVERRIDE_DSN ||
                    pConnectProps->szDatabase[0] == '\0') {
                    strncpy(pConnectProps->szDatabase, pval,
                            MAX_IDEN_LEN - 1);
                    pConnectProps->szDatabase[MAX_IDEN_LEN - 1] = '\0';
                }
            } else if (_stricmp(pname, RS_LOGON_ID) == 0 ||
                        _stricmp(pname, RS_UID) == 0) {
                pConnectProps->iUserKeyWordType =
                    (_stricmp(pname, RS_LOGON_ID) == 0)
                        ? LONG_NAME_KEYWORD
                        : SHORT_NAME_KEYWORD;
                if (CAN_OVERRIDE_DSN || pConnectProps->szUser[0] == '\0') {
                    strncpy(pConnectProps->szUser, pval, MAX_IDEN_LEN - 1);
                    pConnectProps->szUser[MAX_IDEN_LEN - 1] = '\0';
                }
            } else if (_stricmp(pname, RS_PASSWORD) == 0 ||
                        _stricmp(pname, RS_PWD) == 0) {
                pConnectProps->iPasswordKeyWordType =
                    (_stricmp(pname, RS_PASSWORD) == 0)
                        ? LONG_NAME_KEYWORD
                        : SHORT_NAME_KEYWORD;
                if (CAN_OVERRIDE_DSN ||
                    pConnectProps->szPassword[0] == '\0') {
                    strncpy(pConnectProps->szPassword, pval,
                            MAX_IDEN_LEN - 1);
                    pConnectProps->szPassword[MAX_IDEN_LEN - 1] = '\0';
                }
            } else if (_stricmp(pname, RS_DSN) == 0) {
                if (CAN_OVERRIDE_DSN || pConnectProps->szDSN[0] == '\0') {
                    strncpy(pConnectProps->szDSN, pval, MAX_IDEN_LEN - 1);
                    pConnectProps->szDSN[MAX_IDEN_LEN - 1] = '\0';
                }
            } else if (_stricmp(pname, RS_DRIVER) == 0) {
                if (CAN_OVERRIDE_DSN ||
                    pConnectProps->szDriver[0] == '\0') {
                    strncpy(pConnectProps->szDriver, pval,
                            MAX_IDEN_LEN - 1);
                    pConnectProps->szDriver[MAX_IDEN_LEN - 1] = '\0';
                }

						// Read DSN-less connection info from amazon.redshiftodbc.ini
						readCscOptionsForDsnlessConnection(pConnectProps);
        } else if (_stricmp(pname, RS_LOGIN_TIMEOUT) == 0 ||
                   _stricmp(pname, "LT") == 0) {
          sscanf(pval, "%d", &pConnAttr->iLoginTimeout);
        } else if (_stricmp(pname, RS_APPLICATION_USING_THREADS) == 0 ||
                 _stricmp(pname, "AUT") == 0) {
          sscanf(pval, "%d", &pConnectProps->iApplicationUsingThreads);
        } else if (_stricmp(pname, RS_FETCH_REF_CURSOR) == 0 ||
                   _stricmp(pname, "FRC") == 0) {
          sscanf(pval, "%d", &pConnectProps->iFetchRefCursor);
        } else if (_stricmp(pname, RS_TRANSACTION_ERROR_BEHAVIOR) == 0 ||
                   _stricmp(pname, "TEB") == 0) {
          sscanf(pval, "%d", &pConnectProps->iTransactionErrorBehavior);
        } else if (_stricmp(pname, RS_CONNECTION_RETRY_COUNT) == 0 ||
                   _stricmp(pname, "CRC") == 0) {
          sscanf(pval, "%d", &pConnectProps->iConnectionRetryCount);
        } else if (_stricmp(pname, RS_CONNECTION_RETRY_DELAY) == 0 ||
                   _stricmp(pname, "CRD") == 0) {
          sscanf(pval, "%d", &pConnectProps->iConnectionRetryDelay);
        } else if (_stricmp(pname, RS_QUERY_TIMEOUT) == 0 ||
                   _stricmp(pname, "QT") == 0) {
          sscanf(pval, "%d", &pConnectProps->iQueryTimeout);
        } else if (_stricmp(pname, RS_CLIENT_PROTOCOL_VERSION) == 0) {
						sscanf(pval, "%d", &pConnectProps->iClientProtocolVersion);
        } else if (_stricmp(pname, RS_INITIALIZATION_STRING) == 0 ||
                   _stricmp(pname, "IS") == 0) {
          if (pConnectProps->pInitializationString == NULL)
                            pConnectProps->pInitializationString = rs_strdup(pval, SQL_NTS);
        } else if (_stricmp(pname, RS_TRACE) == 0) {
          sscanf(pval, "%d", &pConnAttr->iTrace);

          if (pConnAttr->iTrace == SQL_OPT_TRACE_ON)
                            pConnectProps->iTraceLevel = LOG_LEVEL_DEBUG;
          else if (pConnAttr->iTrace == SQL_OPT_TRACE_OFF)
                            pConnectProps->iTraceLevel = LOG_LEVEL_OFF;
        } else if (_stricmp(pname, RS_TRACE_FILE) == 0) {
                        pConnAttr->pTraceFile = (char *)rs_free(pConnAttr->pTraceFile);
          pConnAttr->pTraceFile = rs_strdup(pval, SQL_NTS);
        } else if (_stricmp(pname, RS_TRACE_LEVEL) == 0) {
          sscanf(pval, "%d", &pConnectProps->iTraceLevel);

          if (pConnectProps->iTraceLevel == LOG_LEVEL_DEBUG)
                            pConnAttr->iTrace = SQL_OPT_TRACE_ON;
          else if (pConnectProps->iTraceLevel == LOG_LEVEL_OFF)
                            pConnAttr->iTrace = SQL_OPT_TRACE_OFF;
        } else if (_stricmp(pname, RS_CSC_ENABLE) == 0) {
          sscanf(pval, "%d", &pConnectProps->iCscEnable);
          if ((pConnectProps->iCscEnable) && (pConnectProps->iCscEnable != 1))
                            pConnectProps->iCscEnable = 0;
        } else if (_stricmp(pname, RS_CSC_MAX_FILE_SIZE) == 0) {
          sscanf(pval, "%lld", &pConnectProps->llCscMaxFileSize);
        } else if (_stricmp(pname, RS_CSC_PATH) == 0) {
          if (pval) {
            strncpy(pConnectProps->szCscPath, pval, MAX_PATH - 1);
                            pConnectProps->szCscPath[MAX_PATH - 1] = '\0';
                        }
        } else if (_stricmp(pname, RS_CSC_THRESHOLD) == 0) {
          sscanf(pval, "%lld", &pConnectProps->llCscThreshold);
        } else if (_stricmp(pname, RS_ENCRYPTION_METHOD) == 0 ||
                   _stricmp(pname, "EM") == 0) {
          sscanf(pval, "%d", &pConnectProps->iEncryptionMethod);
          if (pConnectProps->iEncryptionMethod == 0)
            rs_strncpy(pConnectProps->szSslMode, "disable",
                       sizeof(pConnectProps->szSslMode));
        } else if (_stricmp(pname, RS_SSL_MODE) == 0) {
          if (pval) strncpy(pConnectProps->szSslMode, pval, MAX_IDEN_LEN);
        } else if (_stricmp(pname, RS_VALIDATE_SERVER_CERTIFICATE) == 0 ||
                   _stricmp(pname, "VSC") == 0) {
          sscanf(pval, "%d", &pConnectProps->iValidateServerCertificate);
        } else if (_stricmp(pname, RS_TRUST_STORE) == 0 ||
                   _stricmp(pname, "TS") == 0) {
          if (pval) {
            strncpy(pConnectProps->szTrustStore, pval, MAX_PATH - 1);
                            pConnectProps->szTrustStore[MAX_PATH - 1] = '\0';
                        }
        } else if (_stricmp(pname, RS_KERBEROS_SERVICE_NAME) == 0 ||
                   _stricmp(pname, "KSN") == 0) {
          if (pval) {
            strncpy(pConnectProps->szKerberosServiceName, pval,
                    MAX_IDEN_LEN - 1);
                            pConnectProps->szKerberosServiceName[MAX_IDEN_LEN - 1] = '\0';
                        }
        } else if (_stricmp(pname, RS_STREAMING_CURSOR_ROWS) == 0 ||
                   _stricmp(pname, "SCR") == 0) {
          sscanf(pval, "%d", &pConnectProps->iStreamingCursorRows);
          if (pConnectProps->iStreamingCursorRows < 0)
                          pConnectProps->iStreamingCursorRows = 0;
        } else if (_stricmp(pname, RS_DATABASE_METADATA_CURRENT_DB_ONLY) == 0) {
						bool bVal = convertToBoolVal(pval);
						pConnectProps->iDatabaseMetadataCurrentDbOnly = (bVal) ? 1 : 0;
        } else if (_stricmp(pname, RS_READ_ONLY) == 0) {
						bool bVal = convertToBoolVal(pval);
						pConnectProps->iReadOnly = (bVal) ? 1 : 0;
        } else if (_stricmp(pname, RS_KEEP_ALIVE) == 0) {
          if (pval) {
							strncpy(pConnectProps->szKeepAlive, pval, MAX_NUMBER_BUF_LEN - 1);
							pConnectProps->szKeepAlive[MAX_NUMBER_BUF_LEN - 1] = '\0';
						}
        } else if (_stricmp(pname, RS_APPLICATION_NAME) == 0) {
          if (pval) {
            rs_strncpy(pConnAttr->szApplicationName, pval,
                       sizeof(pConnAttr->szApplicationName));
					}
        } else if (_stricmp(pname, RS_KEEP_ALIVE_IDLE) == 0) {
          if (pval) {
            strncpy(pConnectProps->szKeepAliveIdle, pval,
                    MAX_NUMBER_BUF_LEN - 1);
							pConnectProps->szKeepAliveIdle[MAX_NUMBER_BUF_LEN - 1] = '\0';
						}
        } else if (_stricmp(pname, RS_KEEP_ALIVE_COUNT) == 0) {
          if (pval) {
            strncpy(pConnectProps->szKeepAliveCount, pval,
                    MAX_NUMBER_BUF_LEN - 1);
							pConnectProps->szKeepAliveCount[MAX_NUMBER_BUF_LEN - 1] = '\0';
						}
        } else if (_stricmp(pname, RS_KEEP_ALIVE_INTERVAL) == 0) {
          if (pval) {
            strncpy(pConnectProps->szKeepAliveInterval, pval,
                    MAX_NUMBER_BUF_LEN - 1);
							pConnectProps->szKeepAliveInterval[MAX_NUMBER_BUF_LEN - 1] = '\0';
						}
        } else if (_stricmp(pname, RS_MIN_TLS) == 0) {
          if (pval) {
							strncpy(pConnectProps->szMinTLS, pval, MAX_NUMBER_BUF_LEN - 1);
							pConnectProps->szMinTLS[MAX_NUMBER_BUF_LEN - 1] = '\0';
						}
        } else if (_stricmp(pname, RS_IAM) == 0) {
                        pConnectProps->isIAMAuth = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_HTTPS_PROXY_HOST) == 0) {
          rs_strncpy(pHttpsProps->szHttpsHost, pval,
                     sizeof(pHttpsProps->szHttpsHost));
        } else if (_stricmp(pname, RS_HTTPS_PROXY_PORT) == 0) {
          sscanf(pval, "%d", &(pHttpsProps->iHttpsPort));
        } else if (_stricmp(pname, RS_HTTPS_PROXY_USER_NAME) == 0) {
          rs_strncpy(pHttpsProps->szHttpsUser, pval,
                     sizeof(pHttpsProps->szHttpsUser));
        } else if (_stricmp(pname, RS_HTTPS_PROXY_PASSWORD) == 0) {
          rs_strncpy(pHttpsProps->szHttpsPassword, pval,
                     sizeof(pHttpsProps->szHttpsPassword));
        } else if (_stricmp(pname, RS_IDP_USE_HTTPS_PROXY) == 0) {
                      pHttpsProps->isUseProxyForIdp = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_AUTH_TYPE) == 0) {
          rs_strncpy(pIamProps->szAuthType, pval,
                     sizeof(pIamProps->szAuthType));
        } else if (_stricmp(pname, RS_CLUSTER_ID) == 0) {
          rs_strncpy(pIamProps->szClusterId, pval,
                     sizeof(pIamProps->szClusterId));
        } else if (_stricmp(pname, RS_REGION) == 0) {
          rs_strncpy(pIamProps->szRegion, pval, sizeof(pIamProps->szRegion));
        } else if (_stricmp(pname, RS_END_POINT_URL) == 0) {
          rs_strncpy(pIamProps->szEndpointUrl, pval,
                     sizeof(pIamProps->szEndpointUrl));
        } else if (_stricmp(pname, RS_IAM_STS_ENDPOINT_URL) == 0) {
          rs_strncpy(pIamProps->szStsEndpointUrl, pval,
                     sizeof(pIamProps->szStsEndpointUrl));
        } else if (_stricmp(pname, RS_DB_USER) == 0) {
          rs_strncpy(pIamProps->szDbUser, pval, sizeof(pIamProps->szDbUser));
        } else if (_stricmp(pname, RS_DB_GROUPS) == 0) {
          rs_strncpy(pIamProps->szDbGroups, pval,
                     sizeof(pIamProps->szDbGroups));
        } else if (_stricmp(pname, RS_DB_GROUPS_FILTER) == 0) {
          rs_strncpy(pIamProps->szDbGroupsFilter, pval,
                     sizeof(pIamProps->szDbGroupsFilter));
        } else if (_stricmp(pname, RS_AUTO_CREATE) == 0) {
                      pIamProps->isAutoCreate = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_FORCE_LOWER_CASE) == 0) {
                      pIamProps->isForceLowercase = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_IDP_RESPONSE_TIMEOUT) == 0) {
          sscanf(pval, "%ld", &(pIamProps->lIdpResponseTimeout));
        } else if (_stricmp(pname, RS_IAM_DURATION) == 0) {
          sscanf(pval, "%ld", &(pIamProps->lIAMDuration));
        } else if (_stricmp(pname, RS_ACCESS_KEY_ID) == 0) {
          rs_strncpy(pIamProps->szAccessKeyID, pval,
                     sizeof(pIamProps->szAccessKeyID));
        } else if (_stricmp(pname, RS_SECRET_ACCESS_KEY) == 0) {
          rs_strncpy(pIamProps->szSecretAccessKey, pval,
                     sizeof(pIamProps->szSecretAccessKey));
        } else if (_stricmp(pname, RS_SESSION_TOKEN) == 0) {
          rs_strncpy(pIamProps->szSessionToken, pval,
                     sizeof(pIamProps->szSessionToken));
        } else if (_stricmp(pname, RS_PROFILE) == 0) {
          rs_strncpy(pIamProps->szProfile, pval, sizeof(pIamProps->szProfile));
        } else if (_stricmp(pname, RS_INSTANCE_PROFILE) == 0) {
                      pIamProps->isInstanceProfile = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_PLUGIN_NAME) == 0) {
          rs_strncpy(pIamProps->szPluginName, pval,
                     sizeof(pIamProps->szPluginName));
        } else if (_stricmp(pname, RS_PREFERRED_ROLE) == 0) {
          rs_strncpy(pIamProps->szPreferredRole, pval,
                     sizeof(pIamProps->szPreferredRole));
        } else if (_stricmp(pname, RS_IDP_HOST) == 0) {
          rs_strncpy(pIamProps->szIdpHost, pval, sizeof(pIamProps->szIdpHost));
        } else if (_stricmp(pname, RS_LOGIN_TO_RP) == 0) {
          rs_strncpy(pIamProps->szLoginToRp, pval,
                     sizeof(pIamProps->szLoginToRp));
        } else if (_stricmp(pname, RS_IDP_TENANT) == 0) {
          rs_strncpy(pIamProps->szIdpTenant, pval,
                     sizeof(pIamProps->szIdpTenant));
        } else if (_stricmp(pname, RS_CLIENT_ID) == 0) {
          rs_strncpy(pIamProps->szClientId, pval,
                     sizeof(pIamProps->szClientId));
        } else if (_stricmp(pname, RS_SCOPE) == 0) {
						rs_strncpy(pIamProps->szScope, pval, sizeof(pIamProps->szScope));
        } else if (_stricmp(pname, RS_CLIENT_SECRET) == 0) {
          rs_strncpy(pIamProps->szClientSecret, pval,
                     sizeof(pIamProps->szClientSecret));
        } else if (_stricmp(pname, RS_LOGIN_URL) == 0) {
          rs_strncpy(pIamProps->szLoginUrl, pval,
                     sizeof(pIamProps->szLoginUrl));
        } else if (_stricmp(pname, RS_PARTNER_SPID) == 0) {
          rs_strncpy(pIamProps->szPartnerSpid, pval,
                     sizeof(pIamProps->szPartnerSpid));
        } else if (_stricmp(pname, RS_APP_ID) == 0) {
          rs_strncpy(pIamProps->szAppId, pval, sizeof(pIamProps->szAppId));
        } else if (_stricmp(pname, RS_APP_NAME) == 0) {
          rs_strncpy(pIamProps->szAppName, pval, sizeof(pIamProps->szAppName));
        } else if (_stricmp(pname, RS_WEB_IDENTITY_TOKEN) == 0) {
					  int len = strlen(pval) + 1;
          pIamProps->pszJwt = (char *)rs_calloc(sizeof(char), len);

          rs_strncpy(pIamProps->pszJwt, pval, len);
        } else if (_stricmp(pname, RS_NATIVE_KEY_PROVIDER_NAME) == 0) {
          rs_strncpy(pConnectProps->szProviderName, pval, MAX_IDEN_LEN);
        } else if (_stricmp(pname, RS_ROLE_ARN) == 0) {
          rs_strncpy(pIamProps->szRoleArn, pval, sizeof(pIamProps->szRoleArn));
        } else if (_stricmp(pname, RS_ROLE_SESSION_NAME) == 0) {
          rs_strncpy(pIamProps->szRoleSessionName, pval,
                     sizeof(pIamProps->szRoleSessionName));
        } else if (_stricmp(pname, RS_SSL_INSECURE) == 0) {
          pIamProps->isSslInsecure = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_GROUP_FEDERATION) == 0) {
          pIamProps->isGroupFederation = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_DISABLE_CACHE) == 0) {
          pIamProps->isDisableCache = convertToBoolVal(pval);
        } else if (_stricmp(pname, RS_IDP_PORT) == 0) {
          sscanf(pval, "%d", &(pIamProps->iIdpPort));
        } else if (_stricmp(pname, RS_LISTEN_PORT) == 0) {
          sscanf(pval, "%ld", &(pIamProps->lListenPort));
        } else if (_stricmp(pname, RS_DURATION) == 0) {
          sscanf(pval, "%ld", &(pIamProps->lDuration));
        } else if (_stricmp(pname, RS_TCP_PROXY_HOST) == 0) {
          rs_strncpy(pTcpProxyProps->szHost, pval,
                     sizeof(pTcpProxyProps->szHost));
        } else if (_stricmp(pname, RS_TCP_PROXY_PORT) == 0) {
          rs_strncpy(pTcpProxyProps->szPort, pval,
                     sizeof(pTcpProxyProps->szPort));
        } else if (_stricmp(pname, RS_TCP_PROXY_USER_NAME) == 0) {
          rs_strncpy(pTcpProxyProps->szUser, pval,
                     sizeof(pTcpProxyProps->szUser));
        } else if (_stricmp(pname, RS_TCP_PROXY_PASSWORD) == 0) {
          rs_strncpy(pTcpProxyProps->szPassword, pval,
                     sizeof(pTcpProxyProps->szPassword));
        } else if (_stricmp(pname, RS_IAM_AUTH_PROFILE) == 0) {
          rs_strncpy(pIamProps->szAuthProfile, pval,
                     sizeof(pIamProps->szAuthProfile));
        } else if (_stricmp(pname, RS_IAM_STS_CONNECTION_TIMEOUT) == 0) {
          sscanf(pval, "%d", &(pIamProps->iStsConnectionTimeout));
        } else if (_stricmp(pname, RS_BASIC_AUTH_TOKEN) == 0) {
          rs_strncpy(pIamProps->szBasicAuthToken, pval,
                     sizeof(pIamProps->szBasicAuthToken));
        } else if (_stricmp(pname, RS_TOKEN_TYPE) == 0) {
          rs_strncpy(pIamProps->szTokenType, pval,
                     sizeof(pIamProps->szTokenType));
        } else if (_stricmp(pname, RS_START_URL) == 0) {
          rs_strncpy(pIamProps->szStartUrl, pval,
                     sizeof(pIamProps->szStartUrl));
        } else if (_stricmp(pname, RS_IDC_REGION) == 0) {
          rs_strncpy(pIamProps->szIdcRegion, pval,
                     sizeof(pIamProps->szIdcRegion));
        } else if (_stricmp(pname, RS_IDC_RESPONSE_TIMEOUT) == 0) {
          sscanf(pval, "%ld", &(pIamProps->lIdcResponseTimeout));
        } else if (_stricmp(pname, RS_IDC_CLIENT_DISPLAY_NAME) == 0) {
          rs_strncpy(pIamProps->szIdcClientDisplayName, pval,
                     sizeof(pIamProps->szIdcClientDisplayName));
        } else if (_stricmp(pname, RS_IDENTITY_NAMESPACE) == 0) {
          rs_strncpy(pIamProps->szIdentityNamespace, pval,
                     sizeof(pIamProps->szIdentityNamespace));
        } else if (_stricmp(pname, RS_STRING_TYPE) == 0) {
          if (pval)
            strncpy(pConnectProps->szStringType, pval,
                    sizeof(pConnectProps->szStringType));
        }
#ifdef WIN32
        else if (_stricmp(pname, RS_KERBEROS_API) == 0 ||
                 _stricmp(pname, "KSA") == 0) {
          if (pval) {
            strncpy(pConnectProps->szKerberosAPI, pval, MAX_IDEN_LEN - 1);
            pConnectProps->szKerberosAPI[MAX_IDEN_LEN - 1] = '\0';
                    }
                    }
#endif
        else {
          if (append) {
            // Add into connect string pointer
            pConn->appendConnectAttribueStr(pname, pval);
					}
                    }
                    }
    };  // end of processKeyVal function

    std::string connStr = cbConnStrIn == SQL_NTS
                              ? std::string(szConnStrIn)
                              : std::string(szConnStrIn, cbConnStrIn);
    auto connectionSettingsMap = parseConnectionString(connStr);
    for (auto &kv : connectionSettingsMap) {
      processKeyVal(kv.first.c_str(), kv.second.c_str());
    }

    return valid;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset connection properties to reuse for next connection.
//
void RS_CONN_INFO::resetConnectProps()
{
    RS_CONNECT_PROPS_INFO *pConnectProps = this->pConnectProps;
    RS_CONN_ATTR_INFO *pConnAttr = this->pConnAttr;

    pConnectProps->szHost[0] = '\0';
    pConnectProps->iHostNameKeyWordType = SHORT_NAME_KEYWORD;
    pConnectProps->szPort[0] = '\0';
    pConnectProps->iPortKeyWordType = SHORT_NAME_KEYWORD;
    pConnectProps->szDatabase[0] = '\0';
    pConnectProps->iDatabaseKeyWordType = SHORT_NAME_KEYWORD;
    pConnectProps->szUser[0] = '\0';
    pConnectProps->iUserKeyWordType = SHORT_NAME_KEYWORD;
    pConnectProps->szPassword[0] = '\0';
    pConnectProps->iPasswordKeyWordType = SHORT_NAME_KEYWORD;
    pConnectProps->szDSN[0] = '\0';
    pConnectProps->szDriver[0] = '\0';

    if(pConnectProps->pConnectStr)
    {
        pConnectProps->pConnectStr = (char *)rs_free(pConnectProps->pConnectStr);
        pConnectProps->cbConnectStr = 0;
    }

    if(pConnAttr != NULL)
    {
        if(pConnAttr->pCurrentCatalog != NULL) {
            strncpy(pConnectProps->szDatabase, pConnAttr->pCurrentCatalog, MAX_IDEN_LEN-1);
            pConnectProps->szDatabase[MAX_IDEN_LEN-1] = '\0';
        }
    }

    // Advance properties from DSN
//    pConnectProps->iEnableDescribeParam = 1;
//    pConnectProps->iExtendedColumnMetaData = 1;
    pConnectProps->iApplicationUsingThreads = 1;
    pConnectProps->iFetchRefCursor = 1;
    pConnectProps->iTransactionErrorBehavior = 1;
    pConnectProps->iConnectionRetryCount = 0;
    pConnectProps->iConnectionRetryDelay = 3;
    pConnectProps->iQueryTimeout = 0;
	pConnectProps->iClientProtocolVersion = -1;
	pConnectProps->pInitializationString = (char *)rs_free(pConnectProps->pInitializationString);

    pConnectProps->iTraceLevel = DEFAULT_TRACE_LEVEL;

    // Default CSC options
    pConnectProps->iCscEnable = FALSE;
    pConnectProps->llCscMaxFileSize = (4 * 1024);
    pConnectProps->szCscPath[0] = '\0';
    pConnectProps->llCscThreshold = 1;

    // Default SSL options
    pConnectProps->iEncryptionMethod = 1; // verify-ca
    pConnectProps->iValidateServerCertificate = 1;
//    pConnectProps->szHostNameInCertificate[0] = '\0';
    pConnectProps->szTrustStore[0] = '\0';

    // Multi-INSERT command conversion
    pConnectProps->iMultiInsertCmdConvertEnable = 1; // Default is ON

    // Default KSN
	pConnectProps->szKerberosServiceName[0] = '\0';

    // Default KSA
	pConnectProps->szKerberosAPI[0] = '\0';
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the connection string.
//
void RS_CONN_INFO::setConnectStr(char *szInitStr)
{
    RS_CONN_INFO *pConn = this;

    pConn->pConnectProps->pConnectStr = (char *)rs_free(pConn->pConnectProps->pConnectStr);

    if(szInitStr)
    {
        size_t len = strlen(szInitStr);
        
        len = (len >= INITIAL_CONNECT_STR_LEN) ? len + 1 : INITIAL_CONNECT_STR_LEN;

        pConn->pConnectProps->pConnectStr = (char *)rs_malloc(len);
        if(pConn->pConnectProps->pConnectStr)
        {
            pConn->pConnectProps->cbConnectStr = len;
            rs_strncpy(pConn->pConnectProps->pConnectStr, szInitStr, len);
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Append connection string.
//
void RS_CONN_INFO::appendConnectStr(char *szInitStr)
{
    RS_CONN_INFO *pConn = this;

    if(szInitStr)
    {
        size_t len = strlen(szInitStr);
        size_t len1 =  (pConn->pConnectProps->pConnectStr) ? strlen( pConn->pConnectProps->pConnectStr) : 0;
        
        if((len + len1 + 1) >= pConn->pConnectProps->cbConnectStr) // +1 for ';'
        {
            char *temp;
            len = (len + len1 + 2); // ';' and '\0'
            temp = (char *)rs_malloc(len);

            if(pConn->pConnectProps->pConnectStr)
            {
                rs_strncpy(temp,pConn->pConnectProps->pConnectStr,len);
                pConn->pConnectProps->pConnectStr = (char *)rs_free(pConn->pConnectProps->pConnectStr);
            }
            else
                temp[0] = '\0';

            pConn->pConnectProps->pConnectStr = temp;
            pConn->pConnectProps->cbConnectStr = len;
            temp = NULL;
        }


        if(pConn->pConnectProps->pConnectStr[0] != '\0' 
            && len1 > 0 
            && pConn->pConnectProps->pConnectStr[len1 - 1] != ';')
        {
            strncat(pConn->pConnectProps->pConnectStr,";", pConn->pConnectProps->cbConnectStr - strlen(pConn->pConnectProps->pConnectStr));
        }

        strncat(pConn->pConnectProps->pConnectStr,szInitStr, pConn->pConnectProps->cbConnectStr - strlen(pConn->pConnectProps->pConnectStr));
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Append given attribute and value in the connection string.
//
void RS_CONN_INFO::appendConnectAttribueStr(const char *szName, const char *szVal)
{
    RS_CONN_INFO *pConn = this;

    if(szName && szVal)
    {
        size_t len = strlen(szName) + strlen(szVal);
        size_t len1 =  (pConn->pConnectProps->pConnectStr) ? strlen( pConn->pConnectProps->pConnectStr) : 0;
        
        if((len + len1 + 2) >= pConn->pConnectProps->cbConnectStr) // +2 for ';' and '='
        {
            char *temp;
            len = (len + len1 + 3); // ';', '=' and '\0'
            temp = (char *)rs_malloc(len);

            if(pConn->pConnectProps->pConnectStr)
            {
                rs_strncpy(temp,pConn->pConnectProps->pConnectStr,len);
                pConn->pConnectProps->pConnectStr = (char *)rs_free(pConn->pConnectProps->pConnectStr);
            }
            else
                temp[0] = '\0';

            pConn->pConnectProps->pConnectStr = temp;
            pConn->pConnectProps->cbConnectStr = len;
            temp = NULL;
        }

        if(pConn->pConnectProps->pConnectStr[0] != '\0' 
            && len1 > 0 
            && pConn->pConnectProps->pConnectStr[len1 - 1] != ';')
        {
            strncat(pConn->pConnectProps->pConnectStr,";", pConn->pConnectProps->cbConnectStr - strlen(pConn->pConnectProps->pConnectStr));
        }

        strncat(pConn->pConnectProps->pConnectStr,szName, pConn->pConnectProps->cbConnectStr - strlen(pConn->pConnectProps->pConnectStr));
        strncat(pConn->pConnectProps->pConnectStr,"=", pConn->pConnectProps->cbConnectStr - strlen(pConn->pConnectProps->pConnectStr));
        strncat(pConn->pConnectProps->pConnectStr,szVal, pConn->pConnectProps->cbConnectStr - strlen(pConn->pConnectProps->pConnectStr));
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get connection string.
//
char *RS_CONN_INFO::getConnectStr()
{
    return this->pConnectProps->pConnectStr;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check for any needed connection option for browse connect.
//
// Returns 0, if no more connect options required, 
//        > 0, if more connect options required, 
//        < 0, means error
int RS_CONN_INFO::doesNeedMoreBrowseConnectStr(char *_szConnStrIn,char *szConnStrOut, SWORD cbConnStrOut, SWORD *pcbConnStrOut,
                                        int iBrowseIteration, RS_CONN_INFO *pConn)
{
    int iNeedMoreData = 0;
    char *pKeyword, *pDrvKeyword, *pDsnKeyword;
    int  iUpdated = 0;
    size_t cbBrowseConnectOutStr = INITIAL_CONNECT_STR_LEN;
    char *pszBrowseConnectOutStr = (char *)rs_malloc(cbBrowseConnectOutStr);
    char *szConnStrIn = NULL;
    RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;
    char tempBuf[MAX_IDEN_LEN];
    char tempBuf2[MAX_IDEN_LEN];

    char *szRequiredConnKeywords[MAX_REQUIRED_CONNECT_KEYWORDS] =
        {
            "Database=", 
            "HostName=", 
            "PortNumber=", 
            "LogonID=", 
            NULL
        };

    char *szRequiredConnKeywordsSynonyms[MAX_REQUIRED_CONNECT_KEYWORDS] =
        {
            "DB=", 
            "Server=",
            "PORT=", 
            "UID=", 
            NULL
        };

    char *szOptionalConnKeywords[MAX_OPTIONAL_CONNECT_KEYWORDS] =
        {
            "Password=",
            NULL
        };

    char *szOptionalConnKeywordsSynonyms[MAX_OPTIONAL_CONNECT_KEYWORDS] =
        {
            "PWD=",
            NULL
        };

    int i;
    
    pszBrowseConnectOutStr[0] = '\0';

    if(_szConnStrIn && *_szConnStrIn != '\0')
    {
        szConnStrIn = rs_strdup(_szConnStrIn, SQL_NTS);

        pDrvKeyword = stristr(szConnStrIn,"Driver");
        pDsnKeyword = stristr(szConnStrIn,"DSN");

        if(pDrvKeyword || pDsnKeyword)
        {
            if (pDsnKeyword) {
              // Parse the input string for DSN
              pConn->parseConnectString(szConnStrIn, SQL_NTS, FALSE, TRUE);
            }

            /* Required keywords */
            for(i = 0;i < MAX_REQUIRED_CONNECT_KEYWORDS;i++)
            {
                if(szRequiredConnKeywords[i] == NULL)
                    break;

                pKeyword = stristr(szConnStrIn,szRequiredConnKeywords[i]);
                if(!pKeyword)
                    pKeyword = stristr(szConnStrIn,szRequiredConnKeywordsSynonyms[i]);

                if(!pKeyword && pDsnKeyword) {
                  tempBuf[0] = '\0';

                  rs_strncpy(tempBuf2, szRequiredConnKeywords[i], sizeof(tempBuf2));
                  tempBuf2[strlen(tempBuf2) -1] = '\0'; // Remove "="
                  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, tempBuf2, "", tempBuf, MAX_IDEN_LEN, ODBC_INI);

                  if (tempBuf[0] == '\0') {
                    rs_strncpy(tempBuf2, szRequiredConnKeywordsSynonyms[i],sizeof(tempBuf2));
                    tempBuf2[strlen(tempBuf2) -1] = '\0'; // Remove "="
                    RS_SQLGetPrivateProfileString(pConnectProps->szDSN, tempBuf2, "", tempBuf, MAX_IDEN_LEN, ODBC_INI);
                  }

                  if (tempBuf[0] != '\0')
                    pKeyword = pDsnKeyword; // DSN contains required keyword
                  else
                    pKeyword = NULL;
                } // DSN


                if(!pKeyword)
                {
                    pszBrowseConnectOutStr = appendStr(pszBrowseConnectOutStr, &cbBrowseConnectOutStr, szRequiredConnKeywords[i]);
                    pszBrowseConnectOutStr = appendStr(pszBrowseConnectOutStr, &cbBrowseConnectOutStr,"?;");
                    iUpdated = 1;
                }
            } // Loop

            /* Optional keywords */
            if(iBrowseIteration == 1)
            {
                for(i = 0;i < MAX_OPTIONAL_CONNECT_KEYWORDS;i++)
                {
                    if(szOptionalConnKeywords[i] == NULL)
                        break;

                    pKeyword = stristr(szConnStrIn,szOptionalConnKeywords[i]);
                    if(!pKeyword)
                        pKeyword = stristr(szConnStrIn,szOptionalConnKeywordsSynonyms[i]);

                    if(!pKeyword && pDsnKeyword) {
                      tempBuf[0] = '\0';

                      rs_strncpy(tempBuf2, szOptionalConnKeywords[i],sizeof(tempBuf2));
                      tempBuf2[strlen(tempBuf2) -1] = '\0'; // Remove "="
                      RS_SQLGetPrivateProfileString(pConnectProps->szDSN, tempBuf2, "", tempBuf, MAX_IDEN_LEN, ODBC_INI);

                      if (tempBuf[0] == '\0') {
                        rs_strncpy(tempBuf2, szOptionalConnKeywordsSynonyms[i],sizeof(tempBuf2));
                        tempBuf2[strlen(tempBuf2) -1] = '\0'; // Remove "="
                        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, tempBuf2, "", tempBuf, MAX_IDEN_LEN, ODBC_INI);
                      }

                      if (tempBuf[0] != '\0')
                        pKeyword = pDsnKeyword; // DSN contains required keyword
                      else
                        pKeyword = NULL;
                    } // DSN


                    if(!pKeyword)
                    {
                        pszBrowseConnectOutStr = appendStr(pszBrowseConnectOutStr, &cbBrowseConnectOutStr,"*");
                        pszBrowseConnectOutStr = appendStr(pszBrowseConnectOutStr, &cbBrowseConnectOutStr, szOptionalConnKeywords[i]);
                        pszBrowseConnectOutStr = appendStr(pszBrowseConnectOutStr, &cbBrowseConnectOutStr,"?;");
                        iUpdated = 1;
                    }
                } // Loop
            } // Iteration 1
        }
        else
            iNeedMoreData = -1;

        // Free mem
        szConnStrIn = (char *)rs_free(szConnStrIn);
    }
    else
        iNeedMoreData = -1;

    if(iUpdated)
    {
        iNeedMoreData = 1;

        if(szConnStrOut)
        {
            strncpy(szConnStrOut,pszBrowseConnectOutStr,cbConnStrOut - 1);
            if(pcbConnStrOut)
                *pcbConnStrOut = (SWORD)strlen((char *)szConnStrOut);
        }
        else
        {
            if (pcbConnStrOut)
                *pcbConnStrOut = (SWORD)strlen((char *)pszBrowseConnectOutStr);
        }
    }

    pszBrowseConnectOutStr = (char *)rs_free(pszBrowseConnectOutStr);

    return iNeedMoreData;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if connection open otherwise FALSE.
//
int RS_CONN_INFO::isConnectionOpen()
{
    return (this->iStatus == RS_OPEN_CONNECTION);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if connection dead otherwise FALSE.
//
int RS_CONN_INFO::isConnectionDead()
{
  RS_CONN_INFO *pConn = this;
	int rc = pConn->isConnectionOpen();

	if(rc)
	{
		PGconn * pgConn = pConn->pgConn; // libpq connection handle.

		if(pgConn)
		{
			if(libpqConnectionStatus(pConn) == CONNECTION_BAD)
				rc = 1; // Message loop detected bad connection or 10053 socket error occurs.
			else
				rc = 0; // Connection is live
		}
		else
			rc = 0; // Consider connection is live
	}
	else
		rc = 1; // Close means dead

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read connection properties from registry or odbc.ini using given DSN.
//
void RS_CONN_INFO::readMoreConnectPropsFromRegistry(int readUser)
{
    RS_CONN_INFO *pConn = this;
    RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;
    RS_CONN_ATTR_INFO *pConnAttr = pConn->pConnAttr;
    int len;
    char temp[MAX_IAM_BUF_VAL];
	bool bVal;

    if(pConnectProps->szDSN[0] != '\0')
    {
        RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_IAM, &(pConnectProps->isIAMAuth));

        if(pConnectProps->isIAMAuth && pConnectProps->szUser[0] == '\0')
          readUser = TRUE;


        if(pConnectProps->szHost[0] == '\0') {
            RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_HOST_NAME, "", pConnectProps->szHost, MAX_IDEN_LEN, ODBC_INI);
            if(pConnectProps->szHost[0] == '\0') {
              RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_HOST, "", pConnectProps->szHost, MAX_IDEN_LEN, ODBC_INI);
              if(pConnectProps->szHost[0] == '\0')
                RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_SERVER, "", pConnectProps->szHost, MAX_IDEN_LEN, ODBC_INI);
            }
        }

        if(pConnectProps->szPort[0] == '\0') {
            RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PORT_NUMBER, "", pConnectProps->szPort, MAX_IDEN_LEN, ODBC_INI);
            if(pConnectProps->szPort[0] == '\0')
                RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PORT, "", pConnectProps->szPort, MAX_IDEN_LEN, ODBC_INI);

			if (pConnectProps->szPort[0] == '\0')
				strncpy(pConnectProps->szPort, DEFAULT_PORT, sizeof(pConnectProps->szPort));
        }

        if(pConnectProps->szDatabase[0] == '\0')
            RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_DATABASE, "", pConnectProps->szDatabase, MAX_IDEN_LEN, ODBC_INI);

        if(readUser)
        {
            if(pConnectProps->szUser[0] == '\0') {
                RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_LOGON_ID, "", pConnectProps->szUser, MAX_IDEN_LEN, ODBC_INI);
                if(pConnectProps->szUser[0] == '\0') {
                  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_UID, "", pConnectProps->szUser, MAX_IDEN_LEN, ODBC_INI);
                  if(pConnectProps->szUser[0] == '\0') {
                    RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_USER, "", pConnectProps->szUser, MAX_IDEN_LEN, ODBC_INI);
                  }
                }
            }

            if(pConnectProps->szPassword[0] == '\0') {
              RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PASSWORD, "", pConnectProps->szPassword, MAX_IDEN_LEN, ODBC_INI);
#ifdef WIN32
			  if (pConnectProps->szPassword[0] != '\0')
			  {
				  // Decrypt the password
				  char *pwd = (char *)decode64Password((const char *)(pConnectProps->szPassword), strlen(pConnectProps->szPassword));
				  if (pwd)
				  {
					  strncpy(pConnectProps->szPassword, pwd, sizeof(pConnectProps->szPassword));
					  free(pwd);
				  }

			  }
#endif
              if(pConnectProps->szPassword[0] == '\0') {
                RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PWD, "", pConnectProps->szPassword, MAX_IDEN_LEN, ODBC_INI);
              }
            }
        }

        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_LOGIN_TIMEOUT, &(pConnAttr->iLoginTimeout));
//        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_ENABLE_DESCRIBE_PARAM, &(pConnectProps->iEnableDescribeParam));
//        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_EXTENDED_COLUMN_METADATA, &(pConnectProps->iExtendedColumnMetaData));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_APPLICATION_USING_THREADS, &(pConnectProps->iApplicationUsingThreads));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_FETCH_REF_CURSOR, &(pConnectProps->iFetchRefCursor));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_TRANSACTION_ERROR_BEHAVIOR, &(pConnectProps->iTransactionErrorBehavior));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_CONNECTION_RETRY_COUNT, &(pConnectProps->iConnectionRetryCount));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_CONNECTION_RETRY_DELAY, &(pConnectProps->iConnectionRetryDelay));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_QUERY_TIMEOUT, &(pConnectProps->iQueryTimeout));
		RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_CLIENT_PROTOCOL_VERSION, &(pConnectProps->iClientProtocolVersion));
		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_STRING_TYPE, pConnectProps->szStringType, pConnectProps->szStringType, sizeof(pConnectProps->szStringType), ODBC_INI);

        if(pConnectProps->pInitializationString == NULL)
        {
            len = ON_CONNECT_CMD_MAX_LEN;

            if(len > 0)
            {
                pConnectProps->pInitializationString = (char *)rs_calloc(1, len + 1);
                if(pConnectProps->pInitializationString)
                {
                    RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_INITIALIZATION_STRING, "", pConnectProps->pInitializationString, len, ODBC_INI);
                }
            }
        }

        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_TRACE, &(pConnAttr->iTrace));
        if(pConnAttr->iTrace == SQL_OPT_TRACE_ON)
            pConnectProps->iTraceLevel = LOG_LEVEL_DEBUG;
        else
        if(pConnAttr->iTrace == SQL_OPT_TRACE_OFF)
            pConnectProps->iTraceLevel = LOG_LEVEL_OFF;

        if(pConnAttr->pTraceFile == NULL || pConnAttr->pTraceFile[0] == '\0')
        {
            pConnAttr->pTraceFile = (char *)rs_free(pConnAttr->pTraceFile);
            pConnAttr->pTraceFile = (char *)rs_calloc(1, MAX_PATH + 1);

            RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TRACE_FILE, "", pConnAttr->pTraceFile, MAX_PATH, ODBC_INI);
        }

        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_TRACE_LEVEL, &(pConnectProps->iTraceLevel));
        if(pConnectProps->iTraceLevel == LOG_LEVEL_DEBUG)
            pConnAttr->iTrace = SQL_OPT_TRACE_ON;
        else
        if(pConnectProps->iTraceLevel == LOG_LEVEL_OFF)
            pConnAttr->iTrace = SQL_OPT_TRACE_OFF;

        // Read CSC related parameters
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_CSC_ENABLE, &(pConnectProps->iCscEnable));
        if((pConnectProps->iCscEnable) && (pConnectProps->iCscEnable != 1))
            pConnectProps->iCscEnable = 0;
        RS_CONN_INFO::readLongLongValFromDsn(pConnectProps->szDSN, RS_CSC_THRESHOLD, &(pConnectProps->llCscThreshold));
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_CSC_PATH, "", pConnectProps->szCscPath, MAX_PATH, ODBC_INI);
        RS_CONN_INFO::readLongLongValFromDsn(pConnectProps->szDSN, RS_CSC_MAX_FILE_SIZE, &(pConnectProps->llCscMaxFileSize));

        // Read SSL related parameters
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_SSL_MODE, "", pConnectProps->szSslMode, MAX_IDEN_LEN, ODBC_INI);
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_ENCRYPTION_METHOD, &(pConnectProps->iEncryptionMethod));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_VALIDATE_SERVER_CERTIFICATE, &(pConnectProps->iValidateServerCertificate));
//        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_HOST_NAME_IN_CERTIFICATE, "", pConnectProps->szHostNameInCertificate, MAX_IDEN_LEN, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TRUST_STORE, "", pConnectProps->szTrustStore, MAX_PATH, ODBC_INI);

        // Read Multi-Insert command conversion
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_MULTI_INSERT_CMD_CONVERT_ENABLE, &(pConnectProps->iMultiInsertCmdConvertEnable));

        // Read KSN
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_KERBEROS_SERVICE_NAME, "", pConnectProps->szKerberosServiceName, MAX_IDEN_LEN, ODBC_INI);

#ifdef WIN32
        // Read KSA
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_KERBEROS_API, "", pConnectProps->szKerberosAPI, MAX_IDEN_LEN, ODBC_INI);
#endif

      // Read Streaming Cursor Rows
      RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_STREAMING_CURSOR_ROWS, &(pConnectProps->iStreamingCursorRows));
      if(pConnectProps->iCscEnable)
        pConnectProps->iStreamingCursorRows = 0;
      else
      if(pConnectProps->iStreamingCursorRows < 0)
        pConnectProps->iStreamingCursorRows = 0;

	  // Read current db only or multiple db
	  bVal = (pConnectProps->iDatabaseMetadataCurrentDbOnly == 1);
	  RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_DATABASE_METADATA_CURRENT_DB_ONLY, &bVal);
	  pConnectProps->iDatabaseMetadataCurrentDbOnly = (bVal) ? 1 : 0;

	  // Read READ ONLY session
	  bVal = (pConnectProps->iReadOnly == 1);
	  RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_READ_ONLY, &bVal);
	  pConnectProps->iReadOnly = (bVal) ? 1 : 0;

	  // Read Application name
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_APPLICATION_NAME, "", pConnAttr->szApplicationName, sizeof(pConnAttr->szApplicationName), ODBC_INI);

	  // Read Keep alive values
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_KEEP_ALIVE, "1", pConnectProps->szKeepAlive, MAX_NUMBER_BUF_LEN, ODBC_INI);
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_KEEP_ALIVE_COUNT, "", pConnectProps->szKeepAliveCount, MAX_NUMBER_BUF_LEN, ODBC_INI);
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_KEEP_ALIVE_IDLE, "", pConnectProps->szKeepAliveIdle, MAX_NUMBER_BUF_LEN, ODBC_INI);
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_KEEP_ALIVE_INTERVAL, "", pConnectProps->szKeepAliveInterval, MAX_NUMBER_BUF_LEN, ODBC_INI);

	  // Min TLS
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_MIN_TLS, "", pConnectProps->szMinTLS, MAX_NUMBER_BUF_LEN, ODBC_INI);

      char pluginName[MAX_IDEN_LEN];
      RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PLUGIN_NAME, "", pluginName, MAX_IDEN_LEN, ODBC_INI);

      if(_stricmp(pluginName, PLUGIN_IDP_TOKEN_AUTH) == 0 || _stricmp(pluginName, PLUGIN_BROWSER_IDC_AUTH) == 0) {
        pConnectProps->isNativeAuth = true;
      }

      // Read IAM props
      if(pConnectProps->isIAMAuth || pConnectProps->isNativeAuth)
        readIamConnectPropsFromRegistry();

      // Read HTTPS Proxy settings
      RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_HTTPS_PROXY_HOST, "", temp, MAX_IAM_BUF_VAL, ODBC_INI);
      if(temp[0] != '\0') {
        pConnectProps->pHttpsProps = (RS_PROXY_CONN_PROPS_INFO *)rs_calloc(1,sizeof(RS_PROXY_CONN_PROPS_INFO));

        RS_PROXY_CONN_PROPS_INFO *pHttpsProps = pConnectProps->pHttpsProps;
        rs_strncpy(pHttpsProps->szHttpsHost,temp,sizeof(pHttpsProps->szHttpsHost));
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_HTTPS_PROXY_PORT, &(pHttpsProps->iHttpsPort));
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_HTTPS_PROXY_USER_NAME, "", pHttpsProps->szHttpsUser, MAX_IDEN_LEN, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_HTTPS_PROXY_PASSWORD, "", pHttpsProps->szHttpsPassword, MAX_IDEN_LEN, ODBC_INI);

        RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_IDP_USE_HTTPS_PROXY, &(pHttpsProps->isUseProxyForIdp));
      }

	  // Read TCP Proxy settings
	  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TCP_PROXY_HOST, "", temp, MAX_IAM_BUF_VAL, ODBC_INI);
	  if (temp[0] != '\0') {
		  pConnectProps->pTcpProxyProps = (RS_TCP_PROXY_CONN_PROPS_INFO *)rs_calloc(1, sizeof(RS_TCP_PROXY_CONN_PROPS_INFO));

		  RS_TCP_PROXY_CONN_PROPS_INFO *pTcpProxyProps = pConnectProps->pTcpProxyProps;
		  rs_strncpy(pTcpProxyProps->szHost, temp,sizeof(pTcpProxyProps->szHost));
		  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TCP_PROXY_PORT, "", pTcpProxyProps->szPort, MAX_IDEN_LEN, ODBC_INI);
		  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TCP_PROXY_USER_NAME, "", pTcpProxyProps->szUser, MAX_IDEN_LEN, ODBC_INI);
		  RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TCP_PROXY_PASSWORD, "", pTcpProxyProps->szPassword, MAX_IDEN_LEN, ODBC_INI);
	  }


    } // DSN is not empty
}

/*====================================================================================================================================================*/

void RS_CONN_INFO::readIamConnectPropsFromRegistry()
{
    RS_CONN_INFO *pConn = this;
    RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;
    int len;

    if(pConnectProps->szDSN[0] != '\0')
    {
      // Allocate memory
      pConnectProps->pIamProps = (RS_IAM_CONN_PROPS_INFO *) rs_calloc(1,sizeof(RS_IAM_CONN_PROPS_INFO));
      if(pConnectProps->pIamProps)
      {
        RS_IAM_CONN_PROPS_INFO *pIamProps = pConnectProps->pIamProps;
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_AUTH_TYPE, "", pIamProps->szAuthType, MAX_IDEN_LEN, ODBC_INI);

        // Common properties
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_CLUSTER_ID, "", pIamProps->szClusterId, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_REGION, "", pIamProps->szRegion, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_END_POINT_URL, "", pIamProps->szEndpointUrl, MAX_IAM_BUF_VAL, ODBC_INI);
		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IAM_STS_ENDPOINT_URL, "", pIamProps->szStsEndpointUrl, MAX_IAM_BUF_VAL, ODBC_INI);
		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_DB_USER, "", pIamProps->szDbUser, MAX_IDEN_LEN, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_DB_GROUPS, "", pIamProps->szDbGroups, MAX_IAM_DBGROUPS_LEN, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_DB_GROUPS_FILTER, "", pIamProps->szDbGroupsFilter, MAX_IAM_DBGROUPS_LEN, ODBC_INI);

        RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_AUTO_CREATE, &(pIamProps->isAutoCreate));
        RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_FORCE_LOWER_CASE, &(pIamProps->isForceLowercase));

        RS_CONN_INFO::readLongValFromDsn(pConnectProps->szDSN, RS_IDP_RESPONSE_TIMEOUT, &(pIamProps->lIdpResponseTimeout));

        pIamProps->lIAMDuration = 900;
        RS_CONN_INFO::readLongValFromDsn(pConnectProps->szDSN, RS_IAM_DURATION, &(pIamProps->lIAMDuration));

        // Read all IAM related properties
        // Read accesskey, secret key and session tokens
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_ACCESS_KEY_ID, "", pIamProps->szAccessKeyID, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_SECRET_ACCESS_KEY, "", pIamProps->szSecretAccessKey, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_SESSION_TOKEN, "", pIamProps->szSessionToken, MAX_IAM_SESSION_TOKEN_LEN, ODBC_INI);

        // Read Profile name and is instance profile
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PROFILE, "", pIamProps->szProfile, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN,RS_INSTANCE_PROFILE, &(pIamProps->isInstanceProfile));

        // Read plugin_name and all other related params to plugin
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PLUGIN_NAME, "", pIamProps->szPluginName, MAX_IDEN_LEN, ODBC_INI);

        // Common properties of any plugin
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PREFERRED_ROLE, "", pIamProps->szPreferredRole, MAX_IAM_BUF_VAL, ODBC_INI);

        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IDP_HOST, "", pIamProps->szIdpHost, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_IDP_PORT, &(pIamProps->iIdpPort));
        RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_SSL_INSECURE, &(pIamProps->isSslInsecure));
		RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_GROUP_FEDERATION, &(pIamProps->isGroupFederation));
		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_LOGIN_TO_RP, "", pIamProps->szLoginToRp, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IDP_TENANT, "", pIamProps->szIdpTenant, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_CLIENT_ID, "", pIamProps->szClientId, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_CLIENT_SECRET, "", pIamProps->szClientSecret, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_LOGIN_URL, "", pIamProps->szLoginUrl, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_CONN_INFO::readLongValFromDsn(pConnectProps->szDSN, RS_LISTEN_PORT, &(pIamProps->lListenPort));
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_PARTNER_SPID, "", pIamProps->szPartnerSpid, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_APP_ID, "", pIamProps->szAppId, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_APP_NAME, "", pIamProps->szAppName, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_BASIC_AUTH_TOKEN, "", pIamProps->szBasicAuthToken, MAX_BASIC_AUTH_TOKEN_LEN, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_START_URL, "", pIamProps->szStartUrl, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IDC_REGION, "", pIamProps->szIdcRegion, MAX_IDEN_LEN, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IDENTITY_NAMESPACE, "", pIamProps->szIdentityNamespace, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_TOKEN_TYPE, "", pIamProps->szTokenType, MAX_IDEN_LEN, ODBC_INI);
        RS_CONN_INFO::readLongValFromDsn(pConnectProps->szDSN, RS_IDC_RESPONSE_TIMEOUT, &(pIamProps->lIdcResponseTimeout));
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IDC_CLIENT_DISPLAY_NAME, "", pIamProps->szIdcClientDisplayName, MAX_IAM_BUF_VAL, ODBC_INI);

		RS_CONN_INFO::readBoolValFromDsn(pConnectProps->szDSN, RS_DISABLE_CACHE, &(pIamProps->isDisableCache));

        if(_stricmp(pIamProps->szPluginName,IAM_PLUGIN_JWT) == 0 || _stricmp(pIamProps->szPluginName, JWT_IAM_AUTH_PLUGIN) == 0)
        {
          pIamProps->pszJwt = (char *)rs_calloc(sizeof(char),MAX_IAM_JWT_LEN);
          RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_WEB_IDENTITY_TOKEN, "", pIamProps->pszJwt, MAX_IAM_JWT_LEN, ODBC_INI);
        }

		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_SCOPE, "", pIamProps->szScope, MAX_IAM_BUF_VAL, ODBC_INI);


		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_NATIVE_KEY_PROVIDER_NAME, "", pConnectProps->szProviderName, MAX_IDEN_LEN, ODBC_INI);

        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_ROLE_ARN, "", pIamProps->szRoleArn, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_CONN_INFO::readLongValFromDsn(pConnectProps->szDSN, RS_DURATION, &(pIamProps->lDuration));
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_ROLE_SESSION_NAME, "", pIamProps->szRoleSessionName, MAX_IAM_BUF_VAL, ODBC_INI);
		RS_CONN_INFO::readIntValFromDsn(pConnectProps->szDSN, RS_IAM_STS_CONNECTION_TIMEOUT, &(pIamProps->iStsConnectionTimeout));

        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IAM_CA_PATH, "", pIamProps->szCaPath, MAX_IAM_BUF_VAL, ODBC_INI);
        RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IAM_CA_FILE, "", pIamProps->szCaFile, MAX_IAM_BUF_VAL, ODBC_INI);

		RS_SQLGetPrivateProfileString(pConnectProps->szDSN, RS_IAM_AUTH_PROFILE, "", pIamProps->szAuthProfile, MAX_IAM_BUF_VAL, ODBC_INI);

#ifndef WIN32
        char *driverPath = getDriverPath();
        if (driverPath != NULL && *driverPath != '\0')
        {
          snprintf(pIamProps->szCaFile,sizeof(pIamProps->szCaFile),"%s%c%s",driverPath, PATH_SEPARATOR_CHAR, REDSHIFT_ROOT_CERT_FILE);
        }
		if (driverPath)
			free(driverPath);
#endif

      }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read integer value from registry or odbc.ini using given DSN.
//
void RS_CONN_INFO::readIntValFromDsn(char *szDSN, char *pKey, int *piVal)
{
    char szTempNumBuf[MAX_NUMBER_BUF_LEN];

    snprintf(szTempNumBuf,sizeof(szTempNumBuf), "%d",*piVal);
#ifdef WIN32
    SQLGetPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
#if defined LINUX 
    RsIni::getPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
    sscanf(szTempNumBuf,"%d",piVal);
}

/*====================================================================================================================================================*/

void RS_CONN_INFO::readLongValFromDsn(char *szDSN, char *pKey, long *plVal)
{
    char szTempNumBuf[MAX_NUMBER_BUF_LEN];

    snprintf(szTempNumBuf,sizeof(MAX_NUMBER_BUF_LEN), "%ld",*plVal);
#ifdef WIN32
    SQLGetPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
#if defined LINUX 
    RsIni::getPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
    sscanf(szTempNumBuf,"%ld",plVal);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read long long value from registry or odbc.ini using given DSN.
//
void RS_CONN_INFO::readLongLongValFromDsn(char *szDSN, char *pKey, long long *pllVal)
{
    char szTempNumBuf[MAX_NUMBER_BUF_LEN];

    snprintf(szTempNumBuf,sizeof(szTempNumBuf), "%lld",*pllVal);
#ifdef WIN32
    SQLGetPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
#if defined LINUX 
    RsIni::getPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
    sscanf(szTempNumBuf,"%lld",pllVal);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read boolean value from registry or odbc.ini using given DSN.
//
void RS_CONN_INFO::readBoolValFromDsn(char *szDSN, char *pKey, bool *pbVal)
{
    char szTempNumBuf[MAX_NUMBER_BUF_LEN];

    snprintf(szTempNumBuf,sizeof(szTempNumBuf), "%s",(*pbVal == true) ? "true" : "false");
#ifdef WIN32
    SQLGetPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
#if defined LINUX 
    RsIni::getPrivateProfileString(szDSN, pKey, szTempNumBuf, szTempNumBuf, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif
    *pbVal = convertToBoolVal(szTempNumBuf);
}

/*====================================================================================================================================================*/

bool RS_CONN_INFO::convertToBoolVal(const char *pVal)
{
  return (_stricmp(pVal,"TRUE") == 0
            || _stricmp(pVal,"1") == 0);

}

/*====================================================================================================================================================*/

static void copyCommonConnectionProperties(RS_IAM_CONN_PROPS_INFO* pIamProps, RS_CONNECT_PROPS_INFO* pConnectProps) {
    rs_strncpy(pIamProps->szSslMode, pConnectProps->szSslMode, sizeof(pIamProps->szSslMode));
    rs_strncpy(pIamProps->szHost, pConnectProps->szHost, sizeof(pIamProps->szHost));
    rs_strncpy(pIamProps->szPort, pConnectProps->szPort, sizeof(pIamProps->szPort));
    rs_strncpy(pIamProps->szDatabase, pConnectProps->szDatabase, sizeof(pIamProps->szDatabase));
}


static void invokeNativePluginAuthentication(RS_CONN_INFO* pConn, bool isIAMAuth) { 
    RsIamEntry::NativePluginAuthentication(isIAMAuth,
        pConn->pConnectProps->pIamProps,
        pConn->pConnectProps->pHttpsProps,
        pConn->iamSettings);
}

static void copyIdpToken(RS_IAM_CONN_PROPS_INFO* pIamProps, const std::string& web_identity_token) {
    const char* pval = web_identity_token.c_str();
    int len = strlen(pval) + 1;
    pIamProps->pszJwt = (char*)rs_calloc(sizeof(char), len);
    rs_strncpy(pIamProps->pszJwt, pval, len);
}

static SQLRETURN handleFederatedNonIamConnection(RS_CONN_INFO* pConn) {
    
    RS_CONNECT_PROPS_INFO* pConnectProps = pConn->pConnectProps;
    RS_IAM_CONN_PROPS_INFO* pIamProps = pConn->pConnectProps->pIamProps;

    if (pIamProps && (pIamProps->szPluginName[0] != '\0') &&
        (_stricmp(pIamProps->szPluginName, PLUGIN_IDP_TOKEN_AUTH) == 0 ||
         _stricmp(pIamProps->szPluginName, PLUGIN_BROWSER_IDC_AUTH) == 0)) {
        copyCommonConnectionProperties(pIamProps, pConnectProps);
        pConnectProps->isNativeAuth = true;

        try {
            invokeNativePluginAuthentication(pConn, false);
            rs_strncpy(pConnectProps->szIdpType, RS_IDP_TYPE_AWS_IDC, MAX_IDEN_LEN);

            copyIdpToken(pIamProps, pConn->iamSettings.m_web_identity_token);
            return SQL_SUCCESS;
        }
        catch (RsErrorException& ex) {
            addError(&pConn->pErrorList, "HY000", ex.getErrorMessage(), 0, pConn);
            return SQL_ERROR;
        }
        catch (...) {
            addError(&pConn->pErrorList, "HY000", "IAM Unknown Error", 0, pConn);
            return SQL_ERROR;
        }
    }
}

/*====================================================================================================================================================*/

SQLRETURN RS_CONN_INFO::doConnection(RS_CONN_INFO *pConn) {

  SQLRETURN rc = SQL_SUCCESS;
  RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;
  bool isNativeAuth = false;

  // Check for IAM connection
  if(pConnectProps->isIAMAuth) {

	// Check for Redahift Native Auth
    RS_IAM_CONN_PROPS_INFO *pIamProps =  pConn->pConnectProps->pIamProps;

	if (pIamProps->szPluginName[0] != '\0'
		&& _stricmp(pIamProps->szPluginName, IAM_PLUGIN_JWT) == 0)
	{
		rs_strncpy(pConnectProps->szIdpType, "AzureAD", MAX_IDEN_LEN);
		isNativeAuth = true;
	} // Redshift native auth
	else
	{
		// Copy some DB Properties required for IAM
		rs_strncpy(pIamProps->szUser, pConnectProps->szUser,sizeof(pIamProps->szUser));
		rs_strncpy(pIamProps->szPassword, pConnectProps->szPassword,sizeof(pIamProps->szPassword));
		copyCommonConnectionProperties(pIamProps, pConnectProps);

		// Do IAM authentication and get derived user name and the temp db password
		try
		{
			if (pIamProps->szPluginName[0] != '\0'
				&& _stricmp(pIamProps->szPluginName, IAM_PLUGIN_BROWSER_AZURE_OAUTH2) == 0)
			{
				isNativeAuth = true;
				invokeNativePluginAuthentication(pConn, true);
				rs_strncpy(pConnectProps->szIdpType, "AzureAD", MAX_IDEN_LEN);
			} // Redshift native auth
			else
			{
				RsIamEntry::IamAuthentication(pConn->pConnectProps->isIAMAuth,
					pConn->pConnectProps->pIamProps,
					pConn->pConnectProps->pHttpsProps,
					pConn->iamSettings);
			}
		}
		catch (RsErrorException& ex)
		{
			// Add error and return SQL_ERROR
			addError(&pConn->pErrorList, "HY000", ex.getErrorMessage(), 0, pConn);
			rc = SQL_ERROR;
			return rc;
		}
		catch (...)
		{
			addError(&pConn->pErrorList, "HY000", "IAM Unknown Error", 0, pConn);
			rc = SQL_ERROR;
			return rc;
		}

		if (!isNativeAuth)
		{
			// Copy user name, password, host, and port from IAM
			rs_strncpy(pConnectProps->szUser, pConn->iamSettings.m_username.c_str(), sizeof(pConnectProps->szUser));
			rs_strncpy(pConnectProps->szPassword, pConn->iamSettings.m_password.c_str(), sizeof(pConnectProps->szPassword));

			if (!(pConn->iamSettings.m_host.empty())) {
				rs_strncpy(pConnectProps->szHost, pConn->iamSettings.m_host.c_str(), sizeof(pConnectProps->szHost));
			}

			if (pConn->iamSettings.m_port != 0)
				snprintf(pConnectProps->szPort, sizeof(pConnectProps->szPort), "%d", pConn->iamSettings.m_port);
		}
		else
		{
			copyIdpToken(pIamProps, pConn->iamSettings.m_web_identity_token);
		}
	} // Non Redshift Native Auth

  } // IAM
  else {
    rc = handleFederatedNonIamConnection(pConn);
    if(rc == SQL_ERROR) {
        return rc;
    }
  }

  rc = libpqConnect(pConn);

  return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read registry or odbc.ini using given section name and key name.
//
int RS_SQLGetPrivateProfileString(const char *pSectionName, char *pKey, char *pDflt, char *pReturn, int iSize, char *pFile)
{
#ifdef WIN32
    return SQLGetPrivateProfileString(pSectionName, pKey, pDflt, pReturn, iSize, pFile);
#endif
#if defined LINUX 
    return RsIni::getPrivateProfileString(pSectionName, pKey, pDflt, pReturn, iSize, pFile);
#endif
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read Redshift ini file using given section name and key name.
//
int RS_GetPrivateProfileString(const char *pSectionName, const char *pKey, const char *pDflt, char *pReturn, int iSize, char *pFile)
{
#ifdef WIN32
	return GetPrivateProfileString(pSectionName, pKey, pDflt, pReturn, iSize, pFile);
#endif
#if defined LINUX 
	return RsIni::getPrivateProfileStringWithFullPath(pSectionName, pKey, pDflt, pReturn, iSize, pFile);
#endif
}


