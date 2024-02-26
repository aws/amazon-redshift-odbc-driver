/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsexecute.h"
#include "rstransaction.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLExecDirect.
//
SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT   phstmt,
                                    SQLWCHAR* pwCmd,
                                    SQLINTEGER  cchLen)
{
    SQLRETURN rc;
    char *szCmd;
    size_t len;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecDirectW(FUNC_CALL, 0, phstmt, pwCmd, cchLen);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    if(!(pStmt->pExecThread))
    {
        // Clear error list
        pStmt->pErrorList = clearErrorList(pStmt->pErrorList);
    }

    // Release previously allocated buf, if any
    releasePaStrBuf(pStmt->pCmdBuf);
    setParamMarkerCount(pStmt,0);
    pStmt->iFunctionCall = FALSE;

    if(pwCmd == NULL)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }
    {
      /*
        Note:
        Based on the current value of SQL_WCHART_CONVERT macro,
        SQLWCHAR is typedefed to unsigned short(16 bytes in all
        platforms). This is a hotfix. It is recommended to move
        wchar16_to_utf8_str() to wchar_to_utf8() in the future to
        a) adjust its behavior for supporting a situation when
        SQL_WCHART_CONVERT is typedefed to wchar_t. b) Let all unicode
        conversions use the same bugfixed method.
       */
      std::string utf8;
      len = wchar16_to_utf8_str(pwCmd, cchLen, utf8);
      szCmd = (char *)checkLenAndAllocatePaStrBuf(len, pStmt->pCmdBuf);
      memcpy(szCmd, utf8.c_str(), len);  // szCmd is already null terminated
    }

    if(szCmd)
    {
        // Look for INSERT command with array binding, which can convert into Multi INSERT
        char *pLastBatchMultiInsertCmd = NULL;
        char *pszMultiInsertCmd = parseForMultiInsertCommand(pStmt, szCmd, SQL_NTS, FALSE, &pLastBatchMultiInsertCmd);

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

    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCmd, SQL_NTS, TRUE, FALSE, TRUE, TRUE);

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecDirectW(FUNC_RETURN, rc, phstmt, pwCmd, cchLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLExecDirect executes a preparable statement, using the current values of the parameter marker variables 
// if any parameters exist in the statement. SQLExecDirect is the fastest way to submit an SQL statement for 
// one-time execution.
//
SQLRETURN  SQL_API SQLExecDirect(SQLHSTMT phstmt,
                                    SQLCHAR* pCmd,
                                    SQLINTEGER cbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecDirect(FUNC_CALL, 0, phstmt, pCmd, cbLen);

    rc = RsExecute::RS_SQLExecDirect(phstmt, pCmd, cbLen, FALSE, FALSE, FALSE, TRUE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecDirect(FUNC_RETURN, rc, phstmt, pCmd, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLExecDirect and SQLExecDirectW.
//

SQLRETURN  SQL_API RsExecute::RS_SQLExecDirect(SQLHSTMT phstmt,
                                    SQLCHAR* pCmd,
                                    SQLINTEGER cbLen,
                                    int iInternal,
                                    int executePrepared,
                                    int iSQLExecDirectW,
                                    int iLockRequired)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char *pszCmd = NULL;
    char *pszMultiInsertCmd = NULL;
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
            if(!iPrepare)
                return rc;
            else
            {
                // Follow through for execute after prepare.
                rc = SQL_SUCCESS;
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

    if(pCmd == NULL && !executePrepared)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    // Check for COPY command in current execution
    rc = checkForCopyExecution(pStmt);
    if(rc == SQL_ERROR)
        goto error;

    if(executePrepared)
    {
        int iNoOfParams = (pStmt->pPrepareHead) ? getNumberOfParams(pStmt) : getParamMarkerCount(pStmt);
        int iNoOfBindParams = countBindParams(pStmt->pStmtAttr->pAPD->pDescRecHead);

        if(iNoOfParams > iNoOfBindParams)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY000", "Number of parameters in query is more than number of bind parameters.", 0, NULL);
            goto error; 
        }
    }

	if(libpqDoesAnyOtherStreamingCursorOpen(pStmt, TRUE))
	{
		// Error
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid streaming cursor state", 0, NULL);
        goto error; 
	}

    // Release/reset previously executed stmt info
    makeItReadyForNewQueryExecution(pStmt, executePrepared, FALSE, !iSQLExecDirectW);

    // Set implicit cursor name
    if(pStmt->szCursorName[0] == '\0' && !executePrepared)
        snprintf(pStmt->szCursorName,sizeof(pStmt->szCursorName),"%s%p",IMPLICIT_CURSOR_NAME_PREFIX, phstmt);

    if(!executePrepared)
    {
        if(!iInternal)
        {
            // Release previously allocated buf, if any
            releasePaStrBuf(pStmt->pCmdBuf);
            setParamMarkerCount(pStmt,0);

            // Look for INSERT command with array binding, which can convert into Multi INSERT
            pszMultiInsertCmd = parseForMultiInsertCommand(pStmt, (char *)pCmd, cbLen, FALSE, &pLastBatchMultiInsertCmd);

            if(!pszMultiInsertCmd)
            {
                pszCmd = (char *)checkReplaceParamMarkerAndODBCEscapeClause(pStmt, (char *)pCmd, cbLen, pStmt->pCmdBuf, TRUE);

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
                pszCmd = (char *)checkReplaceParamMarkerAndODBCEscapeClause(pStmt, pszMultiInsertCmd, SQL_NTS, pStmt->pCmdBuf, TRUE);
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

            // Is it called from SQLExecDirectW
            if(iSQLExecDirectW)
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
    }
    else
    {
        pszCmd = NULL;

        // Check for INSERT and ARRAY binding, which can happen after SQLPrepare call
        if((pStmt->iMultiInsert == 0) && (pStmt->pszUserInsertCmd != NULL))
        {
            // Is it array binding?
            RS_DESC_HEADER &pAPDDescHeader = pStmt->pStmtAttr->pAPD->pDescHeader;

            if(pAPDDescHeader.valid)
            {
                // Bind array/single value
                long lArraySize = pAPDDescHeader.lArraySize <= 0 ? 1 : pAPDDescHeader.lArraySize;
                int  iArrayBinding = (lArraySize > 1);

                if(iArrayBinding)
                {
                    // Re-prepare the statement
                    rc = pStmt->InternalSQLPrepare((SQLCHAR *)(pStmt->pszUserInsertCmd), SQL_NTS, FALSE, FALSE, TRUE, FALSE);
                    if(rc == SQL_ERROR)
                        goto error;

                    // Free the user command, if we converted INSERT to multi-INSERT
                    if(pStmt->iMultiInsert > 0)
                        pStmt->pszUserInsertCmd = (char *)rs_free(pStmt->pszUserInsertCmd);
                }
            }
        }
    }

    rc = libpqExecuteDirectOrPrepared(pStmt, pszCmd, executePrepared);

    if(rc == SQL_ERROR)
        goto error;

    pStmt->iStatus =  (rc == SQL_NEED_DATA) ? RS_EXECUTE_STMT_NEED_DATA : RS_EXECUTE_STMT;

	if(pConn && (pConn->pConnectProps->iStreamingCursorRows > 0) && iApiLocked)
	{
		endApiMutex(NULL, pConn);
	}

    return rc;

error:

	if(pConn && (pConn->pConnectProps->iStreamingCursorRows > 0) && iApiLocked)
	{
		endApiMutex(NULL, pConn);
	}

    pszMultiInsertCmd = (char *)rs_free(pszMultiInsertCmd);

    if(pStmt)
    {
        // Look for Rollback on error
        RS_CONN_INFO *pConn = pStmt->phdbc;

        if((pConn->pConnAttr->iAutoCommit == SQL_AUTOCOMMIT_OFF)
            && (pConn->pConnectProps->iTransactionErrorBehavior == 1)
            && !libpqIsTransactionIdle(pConn))
        {
            // Issue rollback command
          RsTransaction::RS_SQLTransact(NULL, (SQLHANDLE)pConn, SQL_ROLLBACK);
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLExecute executes a prepared statement, using the current values of the parameter marker variables if 
// any parameter markers exist in the statement.
//
SQLRETURN  SQL_API SQLExecute(SQLHSTMT phstmt)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecute(FUNC_CALL, 0, phstmt);

    rc = RsExecute::RS_SQLExecDirect(phstmt, NULL, 0, FALSE, TRUE, FALSE, TRUE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecute(FUNC_RETURN, rc, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLParamData is used together with SQLPutData to supply parameter data at statement execution time.
//
SQLRETURN  SQL_API SQLParamData(SQLHSTMT phstmt, SQLPOINTER *ppValue)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLParamData(FUNC_CALL, 0, phstmt, ppValue);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Is thread running?
    if(pStmt->pExecThread)
    {
        rc = checkExecutingThread(pStmt);
        if(rc == SQL_STILL_EXECUTING)
            return rc;
        else
        {
            if(pStmt->pExecThread->iExecFromParamData)
            {
                pStmt->pExecThread->iExecFromParamData = 0;
                resetAndReleaseDataAtExec(pStmt);
                waitAndFreeExecThread(pStmt, FALSE);
                return rc;
            }
            else
                rc = SQL_SUCCESS;
        }
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(pStmt->pCopyCmd && (pStmt->pCopyCmd->iCopyCmdType == COPY_STDIN))
    {
        int endOfCopy = (ppValue == NULL) || (*ppValue == NULL) || (_stricmp((char *)(*ppValue),"end") == 0);

        if(endOfCopy)
        {
            // Copy END 
            if(pStmt->pCopyCmd->iCopyStatus == COPY_IN_BUFFER)
            {
                pStmt->pCopyCmd->iCopyStatus = COPY_IN_ENDED;
                rc = libpqCopyEnd(pStmt, TRUE, NULL);
            }
            else
            if(pStmt->pCopyCmd->iCopyStatus == COPY_IN_STREAM)
            {
                pStmt->pCopyCmd->iCopyStatus = COPY_IN_ENDED;
            }
            else
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY010", "COPY function sequence error", 0, NULL);
                goto error; 
            } 
        }
        else
        {
            // First call after execute for COPY. unix ODBC DM was giving Function seq. error without it.
            rc = SQL_NEED_DATA;
        }
    }
    else
    {
        if(ppValue == NULL)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
            goto error; 
        }

        if(pStmt->pAPDRecDataAtExec)
        {
            if(pStmt->pAPDRecDataAtExec->pDataAtExec == NULL)
            {
                *ppValue = pStmt->pAPDRecDataAtExec->pValue;
                rc = SQL_NEED_DATA;
            }
            else
            {
                // Move to next data-at-exec
                if(needDataAtExec(pStmt, pStmt->pStmtAttr->pAPD->pDescRecHead,pStmt->lParamProcessedDataAtExec,pStmt->iExecutePreparedDataAtExec))
                {
                    *ppValue = pStmt->pAPDRecDataAtExec->pValue;
                    rc = SQL_NEED_DATA;
                }
                else
                {
                    if(pStmt->pExecThread)
                        pStmt->pExecThread->iExecFromParamData = 1;

                    // If all data-at-exec done then execute it.
                    rc = libpqExecuteDirectOrPrepared(pStmt, pStmt->pszCmdDataAtExec, pStmt->iExecutePreparedDataAtExec);

                    if(rc != SQL_STILL_EXECUTING)
                        resetAndReleaseDataAtExec(pStmt);

                    if(rc == SQL_ERROR)
                        goto error;

                    pStmt->iStatus =  RS_EXECUTE_STMT;

                }
            }
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY010", "Function sequence error", 0, NULL);
            goto error; 
        }
    } // !COPY

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLParamData(FUNC_RETURN, rc, phstmt, ppValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLPutData allows an application to send data for a parameter or column to the driver at statement execution 
// time.
//
SQLRETURN  SQL_API SQLPutData(SQLHSTMT phstmt,
                                SQLPOINTER pData, 
                                SQLLEN cbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPutData(FUNC_CALL, 0, phstmt, pData, cbLen);

    // Check the handle
    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Is thread running?
    if(pStmt->pExecThread)
    {
        rc = checkExecutingThread(pStmt);
        if(rc == SQL_STILL_EXECUTING)
            return rc;
        else
            rc = SQL_SUCCESS;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    // Is it for COPY FROM STDIN?
    if(pStmt->pCopyCmd && (pStmt->pCopyCmd->iCopyCmdType == COPY_STDIN))
    {
        // Copy buffer if pData != NULL && cbLen > 0
        if(pData && (cbLen != COPY_BUF_AS_FILE_STREAM))
        {
            cbLen = (INT_LEN(cbLen) == SQL_NTS) ? strlen((char *)pData) : cbLen;
        }

        if(pData && (cbLen > 0))
        {
            // COPY buffer
            pStmt->pCopyCmd->iCopyStatus = COPY_IN_BUFFER;
            rc = libpqCopyBuffer(pStmt, (char *)pData, (int)cbLen, TRUE);
        }
        else
        if(pData && (cbLen == COPY_BUF_AS_FILE_STREAM))
        {
            // Copy as File stream if pData != NULL && cbLen == -99.
            pStmt->pCopyCmd->iCopyStatus = COPY_IN_STREAM;
            rc = copyFromLocalFile(pStmt,(FILE *)pData, TRUE);
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY010", "Invalid buffer pointer or buffer length for COPY", 0, NULL);
            goto error; 
        }
    }
    else
    {
        if(pStmt->pAPDRecDataAtExec)
        {
            RS_DESC_REC *pDescRec = pStmt->pAPDRecDataAtExec;

            if(pDescRec->pDataAtExec == NULL)
                pDescRec->pDataAtExec = allocateAndSetDataAtExec((char *)pData, (long) cbLen);
            else
                pDescRec->pDataAtExec = appendDataAtExec(pDescRec->pDataAtExec, (char *)pData, (long) cbLen);
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY010", "Function sequence error", 0, NULL);
            goto error; 
        }
    }

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPutData(FUNC_RETURN, rc, phstmt, pData, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLCancel cancels the processing on a statement.
//
SQLRETURN  SQL_API SQLCancel(SQLHSTMT phstmt)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLCancel(FUNC_CALL, 0, phstmt);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    rc = libpqCancelQuery(pStmt);

    if(rc == SQL_SUCCESS)
        pStmt->iStatus = RS_CANCEL_STMT;

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLCancel(FUNC_RETURN, rc, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLBulkOperations performs bulk insertions and bulk bookmark operations, including update, delete, 
// and fetch by bookmark.
//
SQLRETURN   SQL_API SQLBulkOperations(SQLHSTMT   phstmt, SQLSMALLINT hOperation)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBulkOperations(FUNC_CALL, 0, phstmt, hOperation);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    switch(hOperation)
    {
        case SQL_ADD:
        case SQL_UPDATE_BY_BOOKMARK:
        case SQL_DELETE_BY_BOOKMARK:
        case SQL_FETCH_BY_BOOKMARK:
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented", 0, NULL);
            goto error; 
        }

        default:
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HYC92", "Invalid attribute identifier", 0, NULL);
            goto error; 
        }
    } // Switch

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBulkOperations(FUNC_RETURN, rc, phstmt, hOperation);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLNativeSql returns the SQL string as modified by the driver. SQLNativeSql does not execute the SQL statement.
//
SQLRETURN SQL_API SQLNativeSql(SQLHDBC   phdbc,
                                SQLCHAR*    szSqlStrIn,
                                SQLINTEGER    cbSqlStrIn,
                                SQLCHAR*    szSqlStrOut,
                                SQLINTEGER  cbSqlStrOut,
                                SQLINTEGER  *pcbSqlStrOut)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNativeSql(FUNC_CALL, 0, phdbc, szSqlStrIn, cbSqlStrIn, szSqlStrOut, cbSqlStrOut, pcbSqlStrOut);

    rc = RsExecute::RS_SQLNativeSql(phdbc, szSqlStrIn, cbSqlStrIn, szSqlStrOut, cbSqlStrOut, pcbSqlStrOut, FALSE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNativeSql(FUNC_RETURN, rc, phdbc, szSqlStrIn, cbSqlStrIn, szSqlStrOut, cbSqlStrOut, pcbSqlStrOut);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLNativeSql and SQLNativeSqlW.
//
SQLRETURN SQL_API RsExecute::RS_SQLNativeSql(SQLHDBC   phdbc,
                                    SQLCHAR*    szSqlStrIn,
                                    SQLINTEGER    cbSqlStrIn,
                                    SQLCHAR*    szSqlStrOut,
                                    SQLINTEGER  cbSqlStrOut,
                                    SQLINTEGER  *pcbSqlStrOut,
                                    int iInternal)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
    char *pszCmd;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    if(szSqlStrIn == NULL)
    {
        rc = SQL_ERROR;
        addError(&pConn->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    if(!iInternal)
    {
        // Release previously allocated buf, if any
        releasePaStrBuf(pConn->pCmdBuf);

        pszCmd = (char *)checkReplaceParamMarkerAndODBCEscapeClause(NULL,(char *)szSqlStrIn, cbSqlStrIn,pConn->pCmdBuf, FALSE);
    }
    else
        pszCmd = pConn->pCmdBuf->pBuf;

    rc = copyStrDataLargeLen(szSqlStrIn ? pszCmd : (char *)szSqlStrIn,
                                szSqlStrIn ? SQL_NTS : cbSqlStrIn, 
                                (char *)szSqlStrOut, cbSqlStrOut, pcbSqlStrOut);


    if(!iInternal)
    {
        // Release cmd allocated buf, if any
        releasePaStrBuf(pConn->pCmdBuf);
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLNativeSql.
//
SQLRETURN SQL_API SQLNativeSqlW(SQLHDBC      phdbc,
                                SQLWCHAR*    wszSqlStrIn,
                                SQLINTEGER   cchSqlStrIn,
                                SQLWCHAR*    wszSqlStrOut,
                                SQLINTEGER   cchSqlStrOut,
                                SQLINTEGER*  pcchSqlStrOut)
{
    SQLRETURN rc;
    char *szCmd;
    char *szSqlStrOut = NULL;
    size_t len;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
    
    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNativeSqlW(FUNC_CALL, 0, phdbc, wszSqlStrIn, cchSqlStrIn, wszSqlStrOut, cchSqlStrOut, pcchSqlStrOut);

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    // Release previously allocated buf, if any
    releasePaStrBuf(pConn->pCmdBuf);

    if(wszSqlStrIn == NULL)
    {
        rc = SQL_ERROR;
        addError(&pConn->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    len = calculate_utf8_len(wszSqlStrIn, cchSqlStrIn);
    szCmd = (char *)checkLenAndAllocatePaStrBuf(len, pConn->pCmdBuf);

    if((wszSqlStrOut != NULL) && (cchSqlStrOut >= 0))
        szSqlStrOut = (char *)rs_calloc(sizeof(char),cchSqlStrOut + 1);

    wchar_to_utf8(wszSqlStrIn, cchSqlStrIn, szCmd, len);

    if(szCmd)
    {
        int numOfODBCEscapeClauses = countODBCEscapeClauses(NULL,pConn->pCmdBuf->pBuf, SQL_NTS);

        if(numOfODBCEscapeClauses > 0)
        {
            char *pTempCmd = rs_strdup(pConn->pCmdBuf->pBuf, SQL_NTS);

            releasePaStrBuf(pConn->pCmdBuf);
            szCmd = (char *)replaceParamMarkerAndODBCEscapeClause(NULL, pTempCmd, SQL_NTS, pConn->pCmdBuf, 0, numOfODBCEscapeClauses);
            pTempCmd = (char *)rs_free(pTempCmd);
        }
    }

    rc = RsExecute::RS_SQLNativeSql(phdbc, (SQLCHAR *)szCmd, SQL_NTS, (SQLCHAR *)szSqlStrOut, cchSqlStrOut, pcchSqlStrOut, TRUE);

    if(SQL_SUCCEEDED(rc))
        utf8_to_wchar(szSqlStrOut, cchSqlStrOut, wszSqlStrOut, cchSqlStrOut);

error:

    // Release cmd allocated buf, if any
    releasePaStrBuf(pConn->pCmdBuf);

    szSqlStrOut = (char *)rs_free(szSqlStrOut);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNativeSqlW(FUNC_RETURN, rc, phdbc, wszSqlStrIn, cchSqlStrIn, wszSqlStrOut, cchSqlStrOut, pcchSqlStrOut);

    endApiMutex(NULL, phdbc);

    return rc;
}

   
