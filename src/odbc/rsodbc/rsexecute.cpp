/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsexecute.h"
#include "rstransaction.h"
#include "rsescapeclause.h"

bool isFixedLengthDataType(short sqlType);

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
    size_t copiedChars = 0, len = 0;
    RS_STMT_INFO *pStmt = nullptr;
    std::string utf8Str;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExecDirectW(FUNC_CALL, 0, phstmt, pwCmd, cchLen);

    auto exitLog = make_scope_exit([&]() noexcept {
        if (IS_TRACE_LEVEL_API_CALL()) {
            TraceSQLExecDirectW(FUNC_RETURN, rc, phstmt, pwCmd, copiedChars);
        }
    });

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }
    pStmt = (RS_STMT_INFO *)phstmt;

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
        return rc; 
    }

    copiedChars = sqlwchar_to_utf8_str(pwCmd, cchLen, utf8Str);
    if (copiedChars == 0) {
        rc = SQL_ERROR;
        std::string err =
            "Invalid SQL Statement: Unicode to UTF-8 conversion failed.";
        RS_LOG_ERROR("RSEXE", "%s", err.c_str());
        addError(&pStmt->pErrorList, "HY000", err.data(), 0, NULL);
        return rc;
    }
    szCmd = (char *)checkLenAndAllocatePaStrBuf(copiedChars + 1, pStmt->pCmdBuf);
    if (!szCmd) {
        rc = SQL_ERROR;
        RS_LOG_ERROR("RSEXE", "Memory allocation error");
        addError(&pStmt->pErrorList, "HY001", "Memory allocation error", 0, NULL);
        return rc;
    }
    memcpy(szCmd, utf8Str.data(), copiedChars);
    szCmd[copiedChars] = '\0';

    if(szCmd)
    {
        // Look for INSERT command with array binding, which can convert into Multi INSERT
        char *pLastBatchMultiInsertCmd = NULL;
        char *pszMultiInsertCmd = parseForMultiInsertCommand(pStmt, szCmd, SQL_NTS, &pLastBatchMultiInsertCmd);

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
                if(rc == SQL_ERROR) {
                    return rc;
                }
            }
        }
    }

    if(szCmd)
    {
        int numOfParamMarkers = ODBCEscapeClauseProcessor::countParamMarkers(
            pStmt->pCmdBuf->pBuf, SQL_NTS);
        int numOfODBCEscapeClauses =
            ODBCEscapeClauseProcessor::countODBCEscapeClauses(
                pStmt, pStmt->pCmdBuf->pBuf, SQL_NTS);

        setParamMarkerCount(pStmt,numOfParamMarkers);

        if(numOfParamMarkers > 0 || numOfODBCEscapeClauses > 0)
        {
            char *pTempCmd = rs_strdup(pStmt->pCmdBuf->pBuf, SQL_NTS);

            releasePaStrBuf(pStmt->pCmdBuf);
            szCmd = (char *)ODBCEscapeClauseProcessor::
                replaceParamMarkerAndODBCEscapeClause(
                    pStmt, pTempCmd, SQL_NTS, pStmt->pCmdBuf, numOfParamMarkers,
                    numOfODBCEscapeClauses);
            pTempCmd = (char *)rs_free(pTempCmd);
        }
    }

    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCmd, SQL_NTS, TRUE, FALSE, TRUE, TRUE);

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

bool RS_STMT_INFO::shouldRePrepareArrayBinding() {
    if (pStmtAttr == nullptr || pStmtAttr->pAPD == nullptr) {
        return false;
    }
    const RS_DESC_HEADER &pAPDDescHeader = pStmtAttr->pAPD->pDescHeader;
    if (!pAPDDescHeader.valid || pszUserInsertCmd == NULL) {
        return false;
    }
    return lArraySizeMultiInsert != pAPDDescHeader.lArraySize;
}

void RS_STMT_INFO::resetMultiInsertInfo() {
    pszUserInsertCmd = (char *)rs_free(pszUserInsertCmd);
    lArraySizeMultiInsert = 0;
    iMultiInsert = 0;
    // Release last batch multi-insert command, if any.
    releasePaStrBuf(pszLastBatchMultiInsertCmd);
    pszLastBatchMultiInsertCmd =
        (struct _RS_STR_BUF *)rs_free(pszLastBatchMultiInsertCmd);
    iLastBatchMultiInsert = 0;
}

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
            pszMultiInsertCmd = parseForMultiInsertCommand(
                pStmt, (char *)pCmd, cbLen, &pLastBatchMultiInsertCmd);

            if(!pszMultiInsertCmd)
            {
                pszCmd = (char *)ODBCEscapeClauseProcessor::
                    checkReplaceParamMarkerAndODBCEscapeClause(
                        pStmt, (char *)pCmd, cbLen, pStmt->pCmdBuf, TRUE);

            }
            else
            {
                pszCmd = (char *)ODBCEscapeClauseProcessor::
                    checkReplaceParamMarkerAndODBCEscapeClause(
                        pStmt, pszMultiInsertCmd, SQL_NTS, pStmt->pCmdBuf,
                        TRUE);
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
        }
    }
    else
    {
        pszCmd = NULL;

        // Check for INSERT and ARRAY binding, which can happen after SQLPrepare call
        if (pStmt->shouldRePrepareArrayBinding()) {
            RS_LOG_DEBUG("EXECUTE", "Re-prepare the statement...");
            // Re-prepare the statement
            rc = pStmt->InternalSQLPrepare((SQLCHAR *)(pStmt->pszUserInsertCmd),
                                           SQL_NTS, FALSE, FALSE, TRUE, FALSE);
            if (rc == SQL_ERROR)
                goto error;
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

    if(pStmt->pAPDRecDataAtExec)
    {
        RS_DESC_REC *pDescRec = pStmt->pAPDRecDataAtExec;

        // Check if SQLPutData has been called before for this parameter
        // and if the target SQL type is fixed-length
        if (pDescRec->pDataAtExec != NULL) {
            // Check if the parameter type supports piecewise data transfer
            // Only character and binary data types support data-at-execution in
            // pieces. All other types (numeric, date/time, etc.) should return
            // HY019
            if (isFixedLengthDataType(pDescRec->hParamSQLType)) {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList, "HY019",
                         "Non-character and non-binary data sent in pieces", 0,
                         NULL);
                goto error;
            }
        }

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
            addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented-SQLBulkOperations:hOperation ", 0, NULL);
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

        pszCmd = (char *)ODBCEscapeClauseProcessor::
            checkReplaceParamMarkerAndODBCEscapeClause(
                NULL, (char *)szSqlStrIn, cbSqlStrIn, pConn->pCmdBuf, FALSE);
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
    std::vector<char> szSqlStrOut;
    RS_CONN_INFO *pConn = nullptr;
    std::string utf8Str;
    size_t ansiTextLen = 0;
    size_t allocation = 0;
    size_t totalCharsNeeded = 0;
    size_t copiedChars = 0;
    bool hasInputChars = false;

    beginApiMutex(NULL, phdbc);

    if (IS_TRACE_LEVEL_API_CALL())
        TraceSQLNativeSqlW(FUNC_CALL, 0, phdbc, wszSqlStrIn, cchSqlStrIn,
                           wszSqlStrOut, cchSqlStrOut, pcchSqlStrOut);

    auto exitLog = make_scope_exit([&]() noexcept {
        // Release cmd allocated buf, if any
        if (pConn && pConn->pCmdBuf) {
            releasePaStrBuf(pConn->pCmdBuf);
        }

        if (IS_TRACE_LEVEL_API_CALL()) {
            TraceSQLNativeSqlW(FUNC_RETURN, rc, phdbc, wszSqlStrIn, cchSqlStrIn,
                               wszSqlStrOut, copiedChars, pcchSqlStrOut);
        }

        endApiMutex(NULL, phdbc);
    });

    if (!VALID_HDBC(phdbc)) {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }

    pConn = (RS_CONN_INFO *)phdbc;

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    // Release previously allocated buf, if any
    releasePaStrBuf(pConn->pCmdBuf);
    // Some Sanity checks
    if (wszSqlStrIn == NULL) {
        rc = SQL_ERROR;
        addError(&pConn->pErrorList, "HY009", "Invalid use of null pointer", 0,
                 NULL);
        RS_LOG_ERROR("RS_SQLNativeSqlW", "Invalid use of null pointer");
        return rc;
    }
    if (wszSqlStrOut && cchSqlStrOut < 0) {
        rc = SQL_ERROR;
        addError(&pConn->pErrorList, "HY090",
                 "Invalid buffer length specified (must be >= 0)", 0, NULL);
        RS_LOG_ERROR("RS_SQLNativeSqlW",
                     "Invalid buffer length specified (must be >= 0)");
        return rc;
    }

    // To determine whether the user really intended to pass some text.
    hasInputChars = wszSqlStrIn && cchSqlStrIn != 0 &&
                    (cchSqlStrIn > 0 || !isFirstSqlwcharNull(wszSqlStrIn));
    // Convert input Unicode to UTF-8
    ansiTextLen = sqlwchar_to_utf8_str(wszSqlStrIn, cchSqlStrIn, utf8Str);

    // If they claimed to have input chars, but conversion produced nothing,
    // treat it as malformed Unicode.
    if (hasInputChars && ansiTextLen == 0 && utf8Str.empty()) {
        addError(&pConn->pErrorList, "HY000",
                 "Invalid or malformed Unicode in input.", 0, NULL);
        RS_LOG_ERROR("RS_SQLNativeSqlW",
                     "Invalid or malformed Unicode in input.");
        rc = SQL_ERROR;
        return rc;
    }

    // In non trivial case, Allocate the maximum amount possible for query
    // length in redshift. This call to API may just be a SQL-Query lenth query
    allocation = ((wszSqlStrOut != NULL) && (cchSqlStrOut >= 0))
                     ? (ansiTextLen * 4 + 1)
                     : (ansiTextLen > 0 ? ansiTextLen * 4 + 1 : 1024);
    szSqlStrOut.resize(allocation, 0);

    // Handle empty string case - allocate at least 1 byte for null terminator
    szCmd = (char *)checkLenAndAllocatePaStrBuf(
        (ansiTextLen > 0) ? ansiTextLen : 1, pConn->pCmdBuf);

    if (szCmd) {
        if (ansiTextLen > 0) {
            memcpy(szCmd, utf8Str.c_str(), ansiTextLen);
        }
        szCmd[ansiTextLen] = '\0'; // Ensure null termination for empty string
        int numOfODBCEscapeClauses =
            ODBCEscapeClauseProcessor::countODBCEscapeClauses(
                NULL, pConn->pCmdBuf->pBuf, SQL_NTS);

        if (numOfODBCEscapeClauses > 0) {
            char *pTempCmd = rs_strdup(pConn->pCmdBuf->pBuf, SQL_NTS);

            releasePaStrBuf(pConn->pCmdBuf);
            szCmd = (char *)ODBCEscapeClauseProcessor::
                replaceParamMarkerAndODBCEscapeClause(NULL, pTempCmd, SQL_NTS,
                                                      pConn->pCmdBuf, 0,
                                                      numOfODBCEscapeClauses);
            pTempCmd = (char *)rs_free(pTempCmd);
        }
    }

    rc = RsExecute::RS_SQLNativeSql(phdbc, (SQLCHAR *)szCmd, SQL_NTS,
                                    (SQLCHAR *)szSqlStrOut.data(), szSqlStrOut.size(),
                                    pcchSqlStrOut, TRUE);
    if (SQL_SUCCEEDED(rc)) {
        if (!wszSqlStrOut) {
            // Length-only query: just compute Unicode length
            totalCharsNeeded = utf8_to_sqlwchar_strlen(szSqlStrOut.data(), SQL_NTS);
            copiedChars = 0;
        } else if (cchSqlStrOut == 0) {
            // Caller gave non-null buffer but size 0: report full length,
            // treat as truncation
            totalCharsNeeded = utf8_to_sqlwchar_strlen(szSqlStrOut.data(), SQL_NTS);
            copiedChars = 0;
            addError(&pConn->pErrorList, "01004",
                     "String data, right truncation.", 0, NULL);
            rc = SQL_SUCCESS_WITH_INFO;
        } else {
            // Normal scenario: convert UTF-8 -> SQLWCHAR into caller's buffer
            copiedChars =
                utf8_to_sqlwchar_str(szSqlStrOut.data(), SQL_NTS, wszSqlStrOut,
                                     cchSqlStrOut, &totalCharsNeeded);
        }

        if (pcchSqlStrOut) {
            *pcchSqlStrOut = (SQLINTEGER)totalCharsNeeded;
        }

        // Check for truncation in the "normal" case
        if (wszSqlStrOut && cchSqlStrOut > 0 &&
            totalCharsNeeded >= (size_t)cchSqlStrOut) {
            if (rc != SQL_SUCCESS_WITH_INFO) {
                // We have NOT already emitted this warning
                addError(&pConn->pErrorList, "01004",
                         "String data, right truncation.", 0, NULL);
            }
            rc = SQL_SUCCESS_WITH_INFO;
        }
    }

    return rc;
}

/**
 * isFixedLengthDataType
 *
 * Determines if the given SQL data type is a fixed-length type that does NOT
 * support piecewise data transfer via SQLPutData.
 *
 * According to ODBC specification, only character and binary data types support
 * data-at-execution in pieces. Fixed-length types (numeric, date/time, etc.)
 * must be provided in a single SQLPutData call. Attempting to send fixed-length
 * data in multiple pieces should result in SQLSTATE HY019 error.
 *
 * @param sqlType The SQL data type to check (e.g., SQL_INTEGER, SQL_CHAR, etc.)
 *
 * @return true if the type is fixed-length and does NOT support piecewise transfer
 *         false if the type is character/binary and DOES support piecewise transfer
 *
 * Supported piecewise types:
 *   - SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR (character types)
 *   - SQL_WCHAR, SQL_WVARCHAR, SQL_WLONGVARCHAR (wide character types)
 *   - SQL_BINARY, SQL_VARBINARY, SQL_LONGVARBINARY (binary types)
 *
 * All other types (SQL_INTEGER, SQL_FLOAT, SQL_DATE, etc.) are fixed-length
 * and return true.
 */
bool isFixedLengthDataType(short sqlType) {
    // Check if the sql type is character or binary data types
    // These types support data-at-execution in pieces
    if (sqlType == SQL_CHAR ||
        sqlType == SQL_VARCHAR ||
        sqlType == SQL_LONGVARCHAR ||
        sqlType == SQL_WCHAR ||
        sqlType == SQL_WVARCHAR ||
        sqlType == SQL_WLONGVARCHAR ||
        sqlType == SQL_BINARY ||
        sqlType == SQL_VARBINARY ||
        sqlType == SQL_LONGVARBINARY) {
            return false; // Variable-length type, supports piecewise transfer
    }
    return true; // Fixed-length type, does NOT support piecewise transfer
}