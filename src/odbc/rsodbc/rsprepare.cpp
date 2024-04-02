/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsprepare.h"
#include "rsmin.h"


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLPrepare prepares an SQL string for execution.
//
SQLRETURN  SQL_API SQLPrepare(SQLHSTMT phstmt,
                                SQLCHAR* pCmd,
                                SQLINTEGER cbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrepare(FUNC_CALL, 0, phstmt, pCmd, cbLen);

    rc = RsPrepare::RS_SQLPrepare(phstmt, pCmd, cbLen, FALSE, FALSE, FALSE, TRUE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrepare(FUNC_RETURN, rc, phstmt, pCmd, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLPrepare.
//
SQLRETURN SQL_API SQLPrepareW(SQLHSTMT  phstmt,
                                SQLWCHAR* pwCmd,
                                SQLINTEGER cchLen)
{
    SQLRETURN rc;
    char *szCmd;
    size_t len;
    std::string u8Str;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char *pLastBatchMultiInsertCmd = NULL;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrepareW(FUNC_CALL, 0, phstmt, pwCmd, cchLen);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    // Release previously allocated buf, if any
    releasePaStrBuf(pStmt->pCmdBuf);
    setParamMarkerCount(pStmt,0);
    pStmt->iFunctionCall = FALSE;

    pStmt->pszUserInsertCmd = (char *)rs_free(pStmt->pszUserInsertCmd);
    releasePaStrBuf(pStmt->pszLastBatchMultiInsertCmd);
    pStmt->pszLastBatchMultiInsertCmd = (struct _RS_STR_BUF *)rs_free(pStmt->pszLastBatchMultiInsertCmd);

    if(pwCmd == NULL)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }
    len = wchar16_to_utf8_str(pwCmd, cchLen, u8Str);
    szCmd = (char *)checkLenAndAllocatePaStrBuf(len, pStmt->pCmdBuf);
    memcpy(szCmd, u8Str.data(), len);
    szCmd[len] = '\0';

    if(szCmd)
    {
        // Look for INSERT command with array binding, which can convert into Multi INSERT
        char *pszMultiInsertCmd = parseForMultiInsertCommand(pStmt, szCmd, SQL_NTS, TRUE, &pLastBatchMultiInsertCmd);

        if(pszMultiInsertCmd)
        {
            len = strlen(pszMultiInsertCmd);
            szCmd = (char *)checkLenAndAllocatePaStrBuf(len, pStmt->pCmdBuf);
            if(szCmd)
                rs_strncpy(szCmd, pszMultiInsertCmd,len+1);

            pszMultiInsertCmd = (char *)rs_free(pszMultiInsertCmd);

            if(pLastBatchMultiInsertCmd)
            {
                rc = createLastBatchMultiInsertCmd(pStmt, pLastBatchMultiInsertCmd);
                pLastBatchMultiInsertCmd = NULL;
                if(rc == SQL_ERROR)
                    goto error;
            }
        }
    }

    if(szCmd)
    {
        int numOfParamMarkers = countParamMarkers(pStmt->pCmdBuf->pBuf, SQL_NTS);
        int numOfODBCEscapeClauses = countODBCEscapeClauses(pStmt,pStmt->pCmdBuf->pBuf, SQL_NTS);

        setParamMarkerCount(pStmt,numOfParamMarkers);

        if(numOfParamMarkers > 0 || numOfODBCEscapeClauses > 0)
        {
            char *pTempCmd = rs_strdup(pStmt->pCmdBuf->pBuf, SQL_NTS);

            releasePaStrBuf(pStmt->pCmdBuf);
            szCmd = (char *)replaceParamMarkerAndODBCEscapeClause(pStmt, pTempCmd, SQL_NTS, pStmt->pCmdBuf, numOfParamMarkers, numOfODBCEscapeClauses);
            pTempCmd = (char *)rs_free(pTempCmd);
        }
    }

    rc = RsPrepare::RS_SQLPrepare(phstmt, (SQLCHAR *)szCmd, SQL_NTS, TRUE, TRUE, FALSE, TRUE);

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrepareW(FUNC_RETURN, rc, phstmt, pwCmd, cchLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLSetCursorName.
//
SQLRETURN SQL_API SQLSetCursorNameW(SQLHSTMT  phstmt,
                                    SQLWCHAR*    pwCursorName,
                                    SQLSMALLINT cchLen)
{
    SQLRETURN rc;
    char szCursorName[MAX_IDEN_LEN];

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetCursorNameW(FUNC_CALL, 0, phstmt, pwCursorName, cchLen);

    wchar_to_utf8(pwCursorName, cchLen, szCursorName, MAX_IDEN_LEN);

    rc = RsPrepare::RS_SQLSetCursorName(phstmt, (SQLCHAR *)szCursorName, SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetCursorNameW(FUNC_RETURN, rc, phstmt, pwCursorName, cchLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSetCursorName associates a cursor name with an active statement. If an application does not call SQLSetCursorName, 
// the driver generates cursor names as needed for SQL statement processing.
//
SQLRETURN  SQL_API SQLSetCursorName(SQLHSTMT phstmt,
                                    SQLCHAR* pCursorName,
                                    SQLSMALLINT cbLen)
{
    SQLRETURN  rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetCursorName(FUNC_CALL, 0, phstmt, pCursorName, cbLen);

    rc = RsPrepare::RS_SQLSetCursorName(phstmt, pCursorName, cbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetCursorName(FUNC_RETURN, rc, phstmt, pCursorName, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLSetCursorName and SQLSetCursorNameW.
//
SQLRETURN  SQL_API RsPrepare::RS_SQLSetCursorName(SQLHSTMT phstmt,
                                        SQLCHAR* pCursorName,
                                        SQLSMALLINT cbLen)
{
    SQLRETURN  rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char *szCursorName = NULL;
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *pTempStmt;
    int min_len = 0;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!pCursorName)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    if((cbLen < 0) && (cbLen != SQL_NTS))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY090", "Invalid string or buffer length", 0, NULL);
        goto error; 
    }

    szCursorName = (char *)makeNullTerminatedStr((char *)pCursorName, cbLen, NULL);

    if(!szCursorName)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, NULL);
        goto error; 
    }

    if(pStmt->iStatus == RS_ALLOCATE_STMT
            || pStmt->iStatus == RS_CLOSE_STMT)
    {
        if(_strnicmp(szCursorName, IMPLICIT_CURSOR_NAME_PREFIX, strlen(IMPLICIT_CURSOR_NAME_PREFIX)) == 0)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"34000", "Invalid cursor name", 0, NULL);
            goto error; 
        }

        // Look for duplicate name
        pConn = pStmt->phdbc;

        // Statement loop
        for(pTempStmt = pConn->phstmtHead; pTempStmt != NULL; pTempStmt = pTempStmt->pNext)
        {
            if (pStmt == pTempStmt)
		continue;

            if(_stricmp(pTempStmt->szCursorName, szCursorName) == 0)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"3C000", "Duplicate cursor name", 0, NULL);
                goto error; 
            }
        } // Loop

        // Copy the cursor name
        min_len = redshift_min(strlen(szCursorName), MAX_IDEN_LEN - 1);
        strncpy(pStmt->szCursorName, szCursorName, min_len);
        pStmt->szCursorName[min_len] = '\0';
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"24000", "Invalid cursor state", 0, NULL);
        goto error; 
    }

error:

    if(szCursorName != (char *)pCursorName)
        szCursorName = (char *)rs_free(szCursorName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetCursorName.
//
SQLRETURN SQL_API SQLGetCursorNameW(SQLHSTMT  phstmt,
                                    SQLWCHAR*        pwCursorName,
                                    SQLSMALLINT     cchLen,
                                    SQLSMALLINT*    pcchLen)
{
    SQLRETURN rc;
    char *pCursorName = NULL;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetCursorNameW(FUNC_CALL, 0, phstmt, pwCursorName, cchLen, pcchLen);

    if((pwCursorName != NULL) && (cchLen >= 0))
        pCursorName = (char *)rs_calloc(sizeof(char),cchLen + 1);

    rc = RsPrepare::RS_SQLGetCursorName(phstmt, (SQLCHAR *)pCursorName, cchLen, pcchLen);

    if(SQL_SUCCEEDED(rc))
        utf8_to_wchar(pCursorName, cchLen, pwCursorName, cchLen);

    pCursorName = (char *)rs_free(pCursorName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetCursorNameW(FUNC_RETURN, rc, phstmt, pwCursorName, cchLen, pcchLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetCursorName returns the cursor name associated with a specified statement.
//
SQLRETURN  SQL_API SQLGetCursorName(SQLHSTMT phstmt,
                                    SQLCHAR *pCursorName,
                                    SQLSMALLINT cbLen,
                                    SQLSMALLINT *pcbLen)
{
    SQLRETURN  rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetCursorName(FUNC_CALL, 0, phstmt, pCursorName, cbLen, pcbLen);

    rc = RsPrepare::RS_SQLGetCursorName(phstmt, pCursorName, cbLen, pcbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetCursorName(FUNC_RETURN, rc, phstmt, pCursorName, cbLen, pcbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetCursorName and SQLGetCursorNameW.
//
SQLRETURN  SQL_API RsPrepare::RS_SQLGetCursorName(SQLHSTMT phstmt,
                                    SQLCHAR *pCursorName,
                                    SQLSMALLINT cbLen,
                                    SQLSMALLINT *pcbLen)
{
    SQLRETURN  rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!pCursorName)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    if((cbLen < 0) && (cbLen != SQL_NTS))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY090", "Invalid string or buffer length", 0, NULL);
        goto error; 
    }

    if(pStmt->szCursorName[0] == '\0')
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY015", "No cursor name available", 0, NULL);
        goto error; 
    }

    rc = copyStrDataSmallLen(pStmt->szCursorName, SQL_NTS, (char *)pCursorName, cbLen, pcbLen);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLCloseCursor closes a cursor that has been opened on a statement and discards pending results.
//
SQLRETURN  SQL_API SQLCloseCursor(SQLHSTMT phstmt)
{
    SQLRETURN  rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLCloseCursor(FUNC_CALL, 0, phstmt);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(pStmt->iStatus == RS_ALLOCATE_STMT
        || pStmt->iStatus == RS_CLOSE_STMT)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"24000", "Invalid cursor state", 0, NULL);
        goto error; 
    }

    rc = RsPrepare::RS_SQLCloseCursor((RS_STMT_INFO *)phstmt);

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLCloseCursor(FUNC_RETURN, rc, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

SQLRETURN  SQL_API RS_STMT_INFO::InternalSQLCloseCursor()
{
  return RsPrepare::RS_SQLCloseCursor(this);
}

//---------------------------------------------------------------------------------------------------------igarish
// Helper function for SQLCloseCursor and called internally from many places.
//
SQLRETURN  SQL_API RsPrepare::RS_SQLCloseCursor(RS_STMT_INFO *pStmt)
{
    SQLRETURN  rc = SQL_SUCCESS;

    releaseResults(pStmt);
    pStmt->iStatus = RS_CLOSE_STMT;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLPrepare and SQLPrepareW.
//

SQLRETURN  SQL_API RS_STMT_INFO::InternalSQLPrepare(
                                    SQLCHAR* pCmd,
                                    SQLINTEGER cbLen,
                                    int iInternal,
                                    int iSQLPrepareW,
                                    int iReprepareForMultiInsert,
                                    int iLockRequired)
{
  return RsPrepare::RS_SQLPrepare(this, pCmd, cbLen, iInternal, iSQLPrepareW, iReprepareForMultiInsert, iLockRequired);

}

SQLRETURN  SQL_API RsPrepare::RS_SQLPrepare(SQLHSTMT phstmt,
                                    SQLCHAR* pCmd,
                                    SQLINTEGER cbLen,
                                    int iInternal,
                                    int iSQLPrepareW,
                                    int iReprepareForMultiInsert,
									int iLockRequired)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char *pszCmd;
    char *pszMultiInsertCmd = NULL;
    char *pszUserInsertCmd = NULL;
    char *pLastBatchMultiInsertCmd = NULL;
	RS_CONN_INFO *pConn = NULL;
	int iApiLocked = FALSE;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Is thread running?
    if(pStmt->pExecThread)
    {
        int iPrepare = pStmt->pExecThread->iPrepare;

        rc = checkExecutingThread(pStmt);
        if(rc != SQL_STILL_EXECUTING)
        {
            waitAndFreeExecThread(pStmt, FALSE);
            if(iPrepare)
                return rc;
            else
            {
                // Follow through for next prepare.
            }
        }
        else
            return rc;
    }

	pConn = pStmt->phdbc;

	if(iLockRequired && pConn && (pConn->pConnectProps->iStreamingCursorRows > 0))
	{
		iApiLocked = TRUE;
		beginApiMutex(NULL, pConn);
	}

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    // Check for COPY command in current execution
    rc = checkForCopyExecution(pStmt);
    if(rc == SQL_ERROR)
        goto error;

    if(pCmd == NULL)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    // Save the flag before clean-up
    if(iSQLPrepareW)
    {
        pszUserInsertCmd = pStmt->pszUserInsertCmd;
        pStmt->pszUserInsertCmd = NULL;
    }

	if(libpqDoesAnyOtherStreamingCursorOpen(pStmt, TRUE))
	{
		// Error
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid streaming cursor state", 0, NULL);
        goto error; 
	}

    // Release/reset previously executed stmt info
    makeItReadyForNewQueryExecution(pStmt, FALSE, iReprepareForMultiInsert, !iSQLPrepareW);

    // Restore the flag after clean-up
    if(iSQLPrepareW)
    {
        pStmt->pszUserInsertCmd = pszUserInsertCmd;
        pszUserInsertCmd = NULL;
    }

    // Set implicit cursor name
    if(pStmt->szCursorName[0] == '\0')
        snprintf(pStmt->szCursorName,sizeof(pStmt->szCursorName),"%s%p",IMPLICIT_CURSOR_NAME_PREFIX, phstmt);

    if(!iInternal)
    {
        // Release previously allocated buf, if any
        releasePaStrBuf(pStmt->pCmdBuf);
        setParamMarkerCount(pStmt,0);

        // Look for INSERT command with array binding, which can convert into Multi INSERT
        pszMultiInsertCmd = parseForMultiInsertCommand(pStmt, (char *)pCmd, cbLen, TRUE, &pLastBatchMultiInsertCmd);

        if(!pszMultiInsertCmd)
        {
            pszCmd = (char *)checkReplaceParamMarkerAndODBCEscapeClause(pStmt,(char *)pCmd, cbLen, pStmt->pCmdBuf, TRUE);

            // Look for COPY command
            parseForCopyCommand(pStmt, pszCmd, SQL_NTS);

            if(!(pStmt->pCopyCmd))
            {
                // Look for UNLOAD command
                parseForUnloadCommand(pStmt, pszCmd, SQL_NTS);
            }
        }
        else
        {
            pszCmd = (char *)checkReplaceParamMarkerAndODBCEscapeClause(pStmt,(char *)pszMultiInsertCmd, SQL_NTS, pStmt->pCmdBuf, TRUE);
            pszMultiInsertCmd = (char *)rs_free(pszMultiInsertCmd);

            if(pLastBatchMultiInsertCmd)
            {
                rc = createLastBatchMultiInsertCmd(pStmt, pLastBatchMultiInsertCmd);
                pLastBatchMultiInsertCmd = NULL;
                if(rc == SQL_ERROR)
                    goto error;

            }
        }
    }
    else
    {
        pszCmd = pStmt->pCmdBuf->pBuf;

        // Is it called from SQLPrepareW?
        if(iSQLPrepareW)
        {
            if(!(pStmt->iMultiInsert))
            {
                // Look for COPY command
                parseForCopyCommand(pStmt, pszCmd, SQL_NTS);

                if(!(pStmt->pCopyCmd))
                {
                    // Look for UNLOAD command
                    parseForUnloadCommand(pStmt, pszCmd, SQL_NTS);
                }
            }
        }
    }

    rc = libpqPrepare(pStmt, pszCmd);

    if(rc == SQL_ERROR)
        goto error;

    pStmt->iStatus = RS_PREPARE_STMT;

error:

    pszMultiInsertCmd = (char *)rs_free(pszMultiInsertCmd);

	if(pConn && (pConn->pConnectProps->iStreamingCursorRows > 0) && iApiLocked)
	{
		endApiMutex(NULL, pConn);
	}

    return rc;
}

