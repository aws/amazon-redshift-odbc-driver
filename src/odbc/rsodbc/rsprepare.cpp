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
    SQLRETURN rc = SQL_SUCCESS;
    char *szCmd = NULL;
    size_t copiedChars = 0, len = 0;
    std::string u8Str;
    char *pLastBatchMultiInsertCmd = NULL;

    if (IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrepareW(FUNC_CALL, 0, phstmt, pwCmd, cchLen);
    auto exitLog = make_scope_exit([&]() noexcept {
        if (IS_TRACE_LEVEL_API_CALL()) {
            TraceSQLPrepareW(FUNC_RETURN, rc, phstmt, pwCmd, cchLen);
        }
    });

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    // Release previously allocated buf, if any
    releasePaStrBuf(pStmt->pCmdBuf);
    setParamMarkerCount(pStmt,0);
    pStmt->iFunctionCall = FALSE;
    pStmt->resetMultiInsertInfo();

    // --- Argument validation ---
    if (pwCmd == NULL) {
        addError(&pStmt->pErrorList, "HY009", "Invalid use of null pointer", 0, NULL);
        RS_LOG_ERROR("SQLPrepareW", "Invalid use of null pointer");
        rc = SQL_ERROR;
        return rc;
    }
    if (cchLen < 0 && cchLen != SQL_NTS) {
        addError(&pStmt->pErrorList, "HY090", "Invalid string or buffer length", 0, NULL);
        RS_LOG_ERROR("SQLPrepareW", "Invalid string or buffer length");
        rc = SQL_ERROR;
        return rc;
    }
    if (cchLen == 0) {
        addError(&pStmt->pErrorList, "HY000", "Empty statement text.", 0, NULL);
        RS_LOG_ERROR("SQLPrepareW", "Empty statement text.");
        rc = SQL_ERROR;
        return rc;
    }

    // --- Unicode → UTF-8 conversion ---
    copiedChars = sqlwchar_to_utf8_str(pwCmd, cchLen, u8Str);

    // Empty after conversion is also an error
    if (copiedChars == 0) {
        addError(&pStmt->pErrorList, "HY000", "Empty statement text.", 0, NULL);
        RS_LOG_ERROR("SQLPrepareW", "Empty statement text.");
        rc = SQL_ERROR;
        return rc;
    }

    // Allocate buffer for SQL text (below will ensure +1 for NUL)
    szCmd = (char *)checkLenAndAllocatePaStrBuf(copiedChars, pStmt->pCmdBuf);
    if (!szCmd) {
        addError(&pStmt->pErrorList, "HY001", "Memory allocation error", 0, NULL);
        rc = SQL_ERROR;
        return rc;
    }
    memcpy(szCmd, u8Str.data(), copiedChars);
    szCmd[copiedChars] = '\0';

    // Multi-insert transform
    {
        // Look for INSERT command with array binding, which can convert into Multi INSERT
        char *pszMultiInsertCmd = parseForMultiInsertCommand(
            pStmt, szCmd, SQL_NTS, &pLastBatchMultiInsertCmd);

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
                if (rc == SQL_ERROR) {
                    return rc;
                }
            }
        }
    }

    // Replace param markers / ODBC escapes if present
    if (szCmd && *szCmd) {
        int numPM = countParamMarkers(pStmt->pCmdBuf->pBuf, SQL_NTS);
        int numEsc = countODBCEscapeClauses(pStmt, pStmt->pCmdBuf->pBuf, SQL_NTS);
        setParamMarkerCount(pStmt, numPM);

        if (numPM > 0 || numEsc > 0) {
            char *tmp = rs_strdup(pStmt->pCmdBuf->pBuf, SQL_NTS);
            releasePaStrBuf(pStmt->pCmdBuf);
            szCmd = (char *)replaceParamMarkerAndODBCEscapeClause(pStmt, tmp, SQL_NTS,
                                                                  pStmt->pCmdBuf, numPM, numEsc);
            tmp = (char *)rs_free(tmp);
        }
    }

    // Final prepare (guard against accidental empties)
    if (!szCmd || !*szCmd) {
        addError(&pStmt->pErrorList, "HY000", "Empty statement text.", 0, NULL);
        rc = SQL_ERROR;
        return rc;
    }
    rc = RsPrepare::RS_SQLPrepare(phstmt, (SQLCHAR*)szCmd, (SQLINTEGER)strlen(szCmd),
                                  TRUE, TRUE, FALSE, TRUE);

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
    char szCursorName[MAX_IDEN_LEN] = {0};


    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetCursorNameW(FUNC_CALL, 0, phstmt, pwCursorName, cchLen);
    if (pwCursorName == NULL) {
        RS_LOG_ERROR("SQLSetCursorNameW",
                     "Invalid cursor name. NULL pointer provided.");
        addError(&pStmt->pErrorList, "HY090",
                 "Invalid string or buffer length", 0, NULL);
        return SQL_ERROR;
    }
    std::string utf8Str;
    size_t copiedChars = sqlwchar_to_utf8_str(pwCursorName, cchLen, utf8Str);
    if (copiedChars > 0 && copiedChars < MAX_IDEN_LEN) {
        memcpy(szCursorName, utf8Str.data(), copiedChars);
        szCursorName[copiedChars] = '\0';

        rc = RsPrepare::RS_SQLSetCursorName(phstmt, (SQLCHAR *)szCursorName,
                                            copiedChars);
    } else {
        RS_LOG_ERROR("SQLSetCursorNameW",
                     "Invalid cursor name. Conversion to UTF-8 failed. copiedChars:%d/%d",
                     copiedChars, MAX_IDEN_LEN);
        addError(&pStmt->pErrorList, "HY090",
                 "Invalid string or buffer length", 0, NULL);
        rc = SQL_ERROR;
    }

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
        RS_LOG_ERROR("RS_SQLSetCursorName", "Invalid use of null pointer");
        goto error; 
    }

    if((cbLen < 0) && (cbLen != SQL_NTS))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY090", "Invalid string or buffer length", 0, NULL);
        RS_LOG_ERROR("RS_SQLSetCursorName", "Invalid string or buffer length");
        goto error; 
    }

    szCursorName = (char *)makeNullTerminatedStr((char *)pCursorName, cbLen, NULL);

    if(!szCursorName)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, NULL);
        RS_LOG_ERROR("RS_SQLSetCursorName", "Memory allocation error");
        goto error; 
    }

    if(pStmt->iStatus == RS_ALLOCATE_STMT
            || pStmt->iStatus == RS_CLOSE_STMT)
    {
        if(_strnicmp(szCursorName, IMPLICIT_CURSOR_NAME_PREFIX, strlen(IMPLICIT_CURSOR_NAME_PREFIX)) == 0)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"34000", "Invalid cursor name", 0, NULL);
            RS_LOG_ERROR("RS_SQLSetCursorName", "Invalid cursor name");
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
                RS_LOG_ERROR("RS_SQLSetCursorName", "Duplicate cursor name");
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
        RS_LOG_ERROR("RS_SQLSetCursorName", "Invalid cursor state");
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
    char pCursorName[MAX_IDEN_LEN] = {0};

    if (IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetCursorNameW(FUNC_CALL, 0, phstmt, pwCursorName, cchLen, pcchLen);


    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }

    // Ask core to produce the cursor name in UTF-8.
    rc = RsPrepare::RS_SQLGetCursorName(
        phstmt,
        reinterpret_cast<SQLCHAR*>(pCursorName),
        MAX_IDEN_LEN,
        /* pcchLen out is irrelevant here; we re-compute wide length below */ 
        nullptr
    );

    size_t totalCharsNeeded = 0;   // logical length in chars (no terminator)
    size_t copiedChars      = 0;   // chars actually copied (no terminator)

    if (SQL_SUCCEEDED(rc) && pCursorName[0] != '\0') {
        // Convert UTF-8 -> client SQLWCHAR width into a temp buffer.
        SQLWCHAR *tempBuffer = nullptr;
        const size_t srcLenBytes = std::strlen(pCursorName);
        totalCharsNeeded = utf8_to_sqlwchar_alloc(
            pCursorName, srcLenBytes, &tempBuffer
        );

        if (tempBuffer) {
            // Capacity from client: cchLen (chars, incl. terminator).
            const size_t capChars =
                (cchLen > 0) ? static_cast<size_t>(cchLen) : 0;

            if (pwCursorName && capChars > 0) {
                // Copy + terminate using shared helpers.
                // Reports truncation via return code and copiedChars output.
                rc = copySqlwForClient(
                    /* dst         */ pwCursorName,
                    /* src         */ tempBuffer,
                    /* needed chars*/ totalCharsNeeded,
                    /* cchLen      */ capChars,
                    /* pcbLen(bytes)*/ nullptr,
                    /* copiedChars */ &copiedChars,
                    /* charSize    */ sizeofSQLWCHAR()
                );
            } else {
                // No buffer or zero capacity: report "would need" size.
                rc = SQL_SUCCESS_WITH_INFO;
                copiedChars = 0;

                // If caller passed a non-null buffer with zero cap, ensure
                // it is at least a valid empty string.
                if (pwCursorName) {
                    setFirstSqlwcharNull(pwCursorName);
                }
            }

            // Per ODBC, pcchLen returns required length in *characters*
            // (not including the terminator), regardless of truncation.
            if (pcchLen) {
                *pcchLen = static_cast<SQLSMALLINT>(totalCharsNeeded);
            }

            rs_free(tempBuffer);
        } else {
            // Conversion failed.
            rc = SQL_ERROR;
            if (pcchLen) *pcchLen = 0;
            if (pwCursorName) setFirstSqlwcharNull(pwCursorName);
        }
    } else {
        // No name available. Return empty and length = 0.
        if (pcchLen) {
            *pcchLen = 0;
        }
    }

    if (IS_TRACE_LEVEL_API_CALL()) {
        TraceSQLGetCursorNameW(
            FUNC_RETURN, rc, phstmt, pwCursorName, copiedChars, pcchLen
        );
    }

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
        RS_LOG_ERROR("RS_SQLGetCursorName", "Invalid use of null pointer");
        goto error; 
    }

    if((cbLen < 0) && (cbLen != SQL_NTS))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY090", "Invalid string or buffer length", 0, NULL);
        RS_LOG_ERROR("RS_SQLGetCursorName", "Invalid string or buffer length");
        goto error; 
    }

    if(pStmt->szCursorName[0] == '\0')
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY015", "No cursor name available", 0, NULL);
        RS_LOG_ERROR("RS_SQLGetCursorName", "No cursor name available");
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
        pszMultiInsertCmd = parseForMultiInsertCommand(
            pStmt, (char *)pCmd, cbLen, &pLastBatchMultiInsertCmd);

        if(!pszMultiInsertCmd)
        {
            pszCmd = (char *)checkReplaceParamMarkerAndODBCEscapeClause(pStmt,(char *)pCmd, cbLen, pStmt->pCmdBuf, TRUE);
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

