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
#include "rsoptions.h"
#include "rsmin.h"

#ifdef __cplusplus
extern "C" {
#endif

int pgIsFileCreatedCsc(PGresult * pgResult);
int pgReadNextBatchOfRowsCsc(PGresult * res, int *piError);
int pgReadPreviousBatchOfRowsCsc(PGresult * res, int *piError);
int pgReadFirstBatchOfRowsCsc(PGresult * res, int *piError);
int pgGetFirstRowIndexCsc(PGresult * res);
int pgReadLastBatchOfRowsCsc(PGresult * res, int *piError);
int pgReadAbsoluteBatchOfRowsCsc(PGresult * res, int *piError, int *indexBuf);
int pgIsFirstBatchCsc(PGresult * pgResult);
int pgIsLastBatchCsc(PGresult * pgResult);
int pgGetExecuteNumberCsc(PGresult * pgResult);
void pgWaitForCscNextResult(void *_pCscStatementContext, PGresult * res, int resultExecutionNumber);

#ifdef __cplusplus
}
#endif


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLNumResultCols returns the number of columns in a result set.
//
// IRD: SQL_DESC_COUNT 
SQLRETURN  SQL_API SQLNumResultCols(SQLHSTMT phstmt,
                                    SQLSMALLINT *pColumnCount)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNumResultCols(FUNC_CALL, 0, phstmt, pColumnCount);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(pColumnCount == NULL)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    *pColumnCount = 0;
    pResult = pStmt->pResultHead;

    if(pResult)
        *pColumnCount = (SQLSMALLINT) pResult->iNumberOfCols;
    else 
    if(pStmt->pPrepareHead && pStmt->pPrepareHead->pResultForDescribeCol && pStmt->iStatus == RS_PREPARE_STMT)
        *pColumnCount = (SQLSMALLINT) pStmt->pPrepareHead->pResultForDescribeCol->iNumberOfCols;

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNumResultCols(FUNC_RETURN, rc, phstmt, pColumnCount);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLDescribeCol returns the result descriptor  column name,type, column size, decimal digits, and nullability  
// for one column in the result set. This information also is available in the fields of the IRD.
//
SQLRETURN  SQL_API SQLDescribeCol(SQLHSTMT phstmt,
                                   SQLUSMALLINT hCol, 
                                   SQLCHAR *pColName,
                                   SQLSMALLINT cbLen, 
                                   SQLSMALLINT *pcbLen,
                                   SQLSMALLINT *pDataType,  
                                   SQLULEN *pColSize,
                                   SQLSMALLINT *pDecimalDigits,  
                                   SQLSMALLINT *pNullable)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDescribeCol(FUNC_CALL, 0, phstmt, hCol, pColName, cbLen, pcbLen, pDataType, pColSize, pDecimalDigits, pNullable);

    rc = RS_STMT_INFO::RS_SQLDescribeCol(phstmt, hCol, pColName, cbLen, pcbLen, pDataType, pColSize, pDecimalDigits, pNullable);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDescribeCol(FUNC_RETURN, rc, phstmt, hCol, pColName, cbLen, pcbLen, pDataType, pColSize, pDecimalDigits, pNullable);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLDescribeCol and SQLDescribeColW.
//
// IRD: SQL_DESC_NAME
// IRD: SQL_DESC_CONCISE_TYPE
// IRD: SQL_DESC_NULLABLE
SQLRETURN  SQL_API RS_STMT_INFO::RS_SQLDescribeCol(SQLHSTMT phstmt,
                                       SQLUSMALLINT hCol, 
                                       SQLCHAR *pColName,
                                       SQLSMALLINT cbLen, 
                                       SQLSMALLINT *pcbLen,
                                       SQLSMALLINT *pDataType,  
                                       SQLULEN *pColSize,
                                       SQLSMALLINT *pDecimalDigits,  
                                       SQLSMALLINT *pNullable)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;
    RS_DESC_REC *pDescRecHead = NULL;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);
    pResult = pStmt->pResultHead;
    if(!pResult)
    {
        if(pStmt->pPrepareHead && pStmt->pPrepareHead->pResultForDescribeCol && pStmt->iStatus == RS_PREPARE_STMT)
        {
            pResult = pStmt->pPrepareHead->pResultForDescribeCol;
            if(pResult)
                pDescRecHead = pResult->pIRDRecs;
        }
    }
    else
        pDescRecHead = pStmt->pIRD->pDescRecHead;

    if(pResult)
    {
        if(pResult->iNumberOfCols && pDescRecHead)
        {
            if(hCol > 0 && hCol <= pResult->iNumberOfCols)
            {
                RS_DESC_REC *pDescRec = &pDescRecHead[hCol - 1];

                rc = copyStrDataSmallLen(pDescRec->szName, SQL_NTS, (char *)pColName, cbLen, pcbLen);

                if(pDataType)
                    *pDataType = pDescRec->hType;

                if(pColSize)
                    *pColSize = getSize(pDescRec->hType, pDescRec->iSize);

                if(pDecimalDigits)
                    *pDecimalDigits = getScale(pDescRec->hType, pDescRec->hScale);

                if(pNullable)
                    *pNullable = pDescRec->hNullable;

                // Convert to ODBC2 type, if needed
                CONVERT_TO_ODBC2_SQL_DATE_TYPES(pStmt, pDataType);
            }
            else
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY000", "Invalid column number", 0, NULL);
                goto error; 
            }
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"07005", "No columns found", 0, NULL);
            goto error; 
        }
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "No result found", 0, NULL);
        goto error; 
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLDescribeCol.
//
SQLRETURN SQL_API SQLDescribeColW(SQLHSTMT            phstmt,
                                    SQLUSMALLINT    hCol,
                                    SQLWCHAR*        pwColName,
                                    SQLSMALLINT     cchLen,
                                    SQLSMALLINT*    pcchLen,
                                    SQLSMALLINT*    pDataType,
                                    SQLULEN*        pColSize,
                                    SQLSMALLINT*    pDecimalDigits,
                                    SQLSMALLINT*    pNullable)
{
    SQLRETURN rc;
    char szColName[MAX_IDEN_LEN] = {0};
    SQLSMALLINT hLen;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDescribeColW(FUNC_CALL, 0, phstmt, hCol, pwColName, cchLen, pcchLen, pDataType, pColSize, pDecimalDigits, pNullable);

    if(cchLen < 0)
        cchLen = 0;

    hLen = redshift_min(MAX_IDEN_LEN - 1, cchLen);
    rc = RS_STMT_INFO::RS_SQLDescribeCol(phstmt, hCol, (SQLCHAR *)((pwColName) ? szColName : NULL), (pwColName) ? hLen : 0,
                            pcchLen, pDataType, pColSize, pDecimalDigits, pNullable);

    if (SQL_SUCCEEDED(rc) && pwColName) {
        // Convert to unicode
      int strLen = char_utf8_to_wchar_utf16(szColName, cchLen, pwColName);
      *pcchLen = strLen;
      RS_LOG_TRACE("RSRES",
                   "cchLen=%d sizeof(SQLWCHAR):%d strLen=%d *pcchLen=%d",
                   (int)cchLen, sizeof(SQLWCHAR), strLen, *pcchLen);
    } else {
        RS_LOG_WARN("RSRES", "Possible failure:%d", SQL_SUCCEEDED(rc));
    }

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDescribeColW(FUNC_RETURN, rc, phstmt, hCol, pwColName, cchLen, pcchLen, pDataType, pColSize, pDecimalDigits, pNullable);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLColAttributes has been replaced by SQLColAttribute. 
//
SQLRETURN SQL_API SQLColAttributes(SQLHSTMT  phstmt,
                                    SQLUSMALLINT   hCol,
                                    SQLUSMALLINT   hOption,
                                    SQLPOINTER     pcValue,
                                    SQLSMALLINT    cbLen,
                                    SQLSMALLINT    *pcbLen,
                                    SQLLEN         *plValue)
{
    SQLRETURN rc;
    SQLUSMALLINT hFieldIdentifier = mapColAttributesToColAttributeIdentifier(hOption);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttributes(FUNC_CALL, 0, phstmt, hCol, hFieldIdentifier, pcValue, cbLen, pcbLen, plValue);

    rc = RS_STMT_INFO::RS_SQLColAttribute(phstmt, hCol, hFieldIdentifier, pcValue, cbLen, pcbLen, plValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttributes(FUNC_RETURN, rc, phstmt, hCol, hFieldIdentifier, pcValue, cbLen, pcbLen, plValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLColAttributes.
//
SQLRETURN SQL_API SQLColAttributesW(SQLHSTMT     phstmt,
                                    SQLUSMALLINT hCol, 
                                    SQLUSMALLINT hOption,
                                    SQLPOINTER   pwValue, 
                                    SQLSMALLINT  cbLen,
                                    SQLSMALLINT *pcbLen, 
                                    SQLLEN        *plValue)
{
    SQLRETURN rc;
    SQLUSMALLINT hFieldIdentifier = mapColAttributesToColAttributeIdentifier(hOption);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttributesW(FUNC_CALL, 0, phstmt, hCol, hFieldIdentifier, pwValue, cbLen, pcbLen, plValue);

    rc = RS_STMT_INFO::RS_SQLColAttributeW(phstmt, hCol, hFieldIdentifier, pwValue, cbLen, pcbLen, plValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttributesW(FUNC_RETURN, rc, phstmt, hCol, hFieldIdentifier, pwValue, cbLen, pcbLen, plValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLBindCol binds application data buffers to columns in the result set.
//
SQLRETURN  SQL_API SQLBindCol(SQLHSTMT phstmt,
                                SQLUSMALLINT hCol, 
                                SQLSMALLINT hType,
                                SQLPOINTER pValue, 
                                SQLLEN cbLen, 
                                SQLLEN *pcbLenInd)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_DESC_REC *pDescRec;
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBindCol(FUNC_CALL, 0, phstmt, hCol, hType, pValue, cbLen, pcbLenInd);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(hCol <= 0)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid column number", 0, NULL);
        goto error; 
    }

    if(pValue == NULL && pcbLenInd == NULL)
    {
        // Unbind it
        releaseDescriptorRecByNum(pStmt->pStmtAttr->pARD, hCol);
    }
    else
    {
        // Find if binding already exist, then it's re-bind.
        pDescRec = checkAndAddDescRec(pStmt->pStmtAttr->pARD, hCol, FALSE, NULL);
        if(pDescRec == NULL)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }

        if(pDescRec)
        {
            // Store app values
            pDescRec->hRecNumber = hCol;
            pDescRec->hType        = hType;
            pDescRec->pValue    = pValue;
            pDescRec->cbLen        = cbLen;
            pDescRec->pcbLenInd = pcbLenInd;

            pDescRec->iOctetLen = getOctetLenUsingCType(hType, (int)cbLen);
        }
    }

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBindCol(FUNC_RETURN, rc, phstmt, hCol, hType, pValue, cbLen, pcbLenInd);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLFetch fetches the next rowset of data from the result set and returns data for all bound columns.
//
// Page 652 for IRD/ARD.
SQLRETURN  SQL_API SQLFetch(SQLHSTMT phstmt)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFetch(FUNC_CALL, 0, phstmt);

    rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFetch(FUNC_RETURN, rc, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLMoreResults determines whether more results are available on a statement containing SELECT, UPDATE, INSERT, 
// or DELETE statements and, if so, initializes processing for those results.
//
SQLRETURN SQL_API SQLMoreResults(SQLHSTMT    phstmt)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLMoreResults(FUNC_CALL, 0, phstmt);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);
    pResult = pStmt->pResultHead;

    if(pResult)
    {
        int currentExecuteNumber = pgGetExecuteNumberCsc(pResult->pgResult);
		int iStreamingCursor = FALSE;

        // Wait for next result, if any. 
        pgWaitForCscNextResult(pStmt->pCscStatementContext,pResult->pgResult, currentExecuteNumber);

        // Move to next Result; 
        pStmt->pResultHead = pStmt->pResultHead->pNext;

        // Free current result
        releaseResult(pResult, TRUE, pStmt);

        // Get next result
        pResult = pStmt->pResultHead;

		if(!pResult
			&& pStmt->pCscStatementContext 
			&& isStreamingCursorMode(pStmt)
			&& !(libpqIsEndOfStreamingCursorQuery(pStmt))
		)
		{
			// Read Next Result from socket
			rc = libpqReadNextResultOfStreamingCursor(pStmt, pStmt->pCscStatementContext, pStmt->phdbc->pgConn, TRUE);
			if(rc == SQL_ERROR)
				goto error;

			iStreamingCursor = TRUE;

			// Get next result
			pResult = pStmt->pResultHead;
		}


        if(!pResult)
		{
            rc = SQL_NO_DATA_FOUND;
		}
        else
        {
			if(!iStreamingCursor)
			{
				// Copy IRD recs.
				copyIRDRecsFromResult(pStmt->pResultHead, pStmt->pIRD);
			}
        }
    }
    else
        rc = SQL_NO_DATA_FOUND;

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLMoreResults(FUNC_RETURN, rc, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLExtendedFetch fetches the specified rowset of data from the result set and returns data for all bound columns.
//
SQLRETURN SQL_API SQLExtendedFetch(SQLHSTMT  phstmt,
                                    SQLUSMALLINT    hFetchOrientation,
                                    SQLLEN          iFetchOffset,
                                    SQLULEN         *piRowCount,
                                    SQLUSMALLINT    *phRowStatus)
{
    SQLRETURN rc;
    SQLRETURN rc1;
    void *pSavRowsFetchedPtr = NULL;
    void *pSavRowStatusPtr = NULL;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExtendedFetch(FUNC_CALL, 0, phstmt, hFetchOrientation, iFetchOffset, piRowCount, phRowStatus);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    if(piRowCount)
    {
        rc = RsOptions::RS_SQLGetStmtAttr(phstmt, SQL_ATTR_ROWS_FETCHED_PTR,&pSavRowsFetchedPtr,sizeof(void *),NULL);

        if(rc != SQL_SUCCESS)
            goto error;

        *piRowCount = 0;

        rc = RsOptions::RS_SQLSetStmtAttr(phstmt, SQL_ATTR_ROWS_FETCHED_PTR, piRowCount, sizeof(void *));

        if(rc != SQL_SUCCESS)
            goto error;
    }

    if(phRowStatus)
    {
        rc = RsOptions::RS_SQLGetStmtAttr(phstmt, SQL_ATTR_ROW_STATUS_PTR,&pSavRowStatusPtr,sizeof(void *),NULL);

        if(rc != SQL_SUCCESS)
            goto error;

        rc = RsOptions::RS_SQLSetStmtAttr(phstmt, SQL_ATTR_ROW_STATUS_PTR, phRowStatus, sizeof(void *));

        if(rc != SQL_SUCCESS)
            goto error;
    }

    rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, hFetchOrientation, iFetchOffset);

    // Reset user pointers
    if(piRowCount)
    {
        rc1 = RsOptions::RS_SQLSetStmtAttr(phstmt, SQL_ATTR_ROWS_FETCHED_PTR, pSavRowsFetchedPtr, sizeof(void *));

        if(rc1 != SQL_SUCCESS)
        {
            rc = rc1;
            goto error;
        }
    }

    if(phRowStatus)
    {
        rc1 = RsOptions::RS_SQLSetStmtAttr(phstmt, SQL_ATTR_ROW_STATUS_PTR, pSavRowStatusPtr, sizeof(void *));

        if(rc1 != SQL_SUCCESS)
        {
            rc = rc1;
            goto error;
        }
    }

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLExtendedFetch(FUNC_RETURN, rc, phstmt, hFetchOrientation, iFetchOffset, piRowCount, phRowStatus);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetData retrieves data for a single column in the result set.
//
// SQL_ARD_TYPE
SQLRETURN  SQL_API SQLGetData(SQLHSTMT phstmt,
                                  SQLUSMALLINT hCol, 
                                  SQLSMALLINT hType,
                                  SQLPOINTER pValue, 
                                  SQLLEN cbLen,
                                  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetData(FUNC_CALL, 0, phstmt, hCol, hType, pValue, cbLen, pcbLenInd);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    rc = RS_STMT_INFO::RS_SQLGetData(pStmt, hCol, hType, pValue, cbLen, pcbLenInd, FALSE);

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetData(FUNC_RETURN, rc, phstmt, hCol, hType, pValue, cbLen, pcbLenInd);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetData and SQLFetchScroll.
//
SQLRETURN  SQL_API RS_STMT_INFO::RS_SQLGetData(RS_STMT_INFO *pStmt,
                                  SQLUSMALLINT hCol, 
                                  SQLSMALLINT hType,
                                  SQLPOINTER pValue, 
                                  SQLLEN cbLen,
                                  SQLLEN *pcbLenInd,
								  int iInternal)

{
    SQLRETURN rc = SQL_SUCCESS;
    RS_RESULT_INFO *pResult = pStmt->pResultHead;

    if(pcbLenInd)
        *pcbLenInd = 0L;

    if(pResult)
    {
        if(pResult->iNumberOfCols && pStmt->pIRD->pDescRecHead)
        {
			if(!iInternal && (hCol == pResult->iPrevhCol))
			{
				rc = SQL_NO_DATA;
				goto error;
			}

            if(hCol > 0 && hCol <= pResult->iNumberOfCols)
            {
                int  iDataLen;
				int format;
                char *pData = libpqGetData(pResult, hCol - 1, &iDataLen, &format);
                RS_DESC_REC *pDescRec = &pStmt->pIRD->pDescRecHead[hCol - 1];

                if(pcbLenInd)
                {
                    *pcbLenInd = 0;
                    *pcbLenInd = iDataLen;
                }

                if (pValue && pData && (iDataLen != SQL_NULL_DATA)) {
                  rc = convertSQLDataToCData(
                      pStmt, pData, iDataLen, pDescRec->hType, pValue, cbLen,
                      &(pResult->cbLenOffset), pcbLenInd, hType,
                      pDescRec->hRsSpecialType, format, pDescRec);
                } else if (iDataLen == SQL_NULL_DATA) {
                  if (pValue && (cbLen > 0)) *(char *)pValue = '\0';

                    if(!pcbLenInd)
                    {
                        rc = SQL_ERROR;
                        addError(&pStmt->pErrorList,"22002", "Indicator variable required but not supplied", 0, NULL);
                        goto error; 
                    }
                    else
                        *pcbLenInd = SQL_NULL_DATA;
                }

				if(!iInternal && (rc != SQL_SUCCESS_WITH_INFO))
					pResult->iPrevhCol = hCol;
            }
            else
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY000", "Invalid column number", 0, NULL);
                goto error; 
            }
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY000", "No columns found", 0, NULL);
            goto error; 
        }
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "No result found", 0, NULL);
        goto error; 
    }

error:

	if(pResult && !iInternal && (rc == SQL_ERROR))
		pResult->iPrevhCol = hCol;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLRowCount returns the number of rows affected by an UPDATE, INSERT, or DELETE statement.
//
// IRD: SQL_DIAG_ROW_COUNT
SQLRETURN  SQL_API SQLRowCount(SQLHSTMT phstmt, SQLLEN* pRowCount)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLRowCount(FUNC_CALL, 0, phstmt, pRowCount);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(pRowCount == NULL)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    *pRowCount = -1L;
    pResult = pStmt->pResultHead;

    // If it's INSERT/UPDATE/DELETE then return the count otherwise return -1.
    if(pResult && (pResult->iNumberOfCols == 0))
        *pRowCount = pResult->lRowsUpdated;
	else
	if (pResult && (pResult->iNumberOfCols > 0))
	{
		if (pStmt->pCscStatementContext
			&& isStreamingCursorMode(pStmt)
			&& (libpqIsEndOfStreamingCursor(pStmt))
			)
			*pRowCount = pResult->iNumberOfRowsInMem; 
		else
		if(!isStreamingCursorMode(pStmt))
			*pRowCount = pResult->iNumberOfRowsInMem; // All rows in memory
	}

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLRowCount(FUNC_RETURN, rc, phstmt, pRowCount);

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSetPos sets the cursor position in a rowset and allows an application to refresh data in the rowset or 
// to update or delete data in the result set.
//
SQLRETURN SQL_API SQLSetPos(SQLHSTMT  phstmt,
                            SQLSETPOSIROW  iRow,
                            SQLUSMALLINT   hOperation,
                            SQLUSMALLINT   hLockType)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetPos(FUNC_CALL, 0, phstmt, iRow, hOperation, hLockType);

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(hOperation == SQL_REFRESH
        || hOperation == SQL_UPDATE
        || hOperation == SQL_DELETE
        || hOperation != SQL_POSITION)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented", 0, NULL);
        goto error; 
    }
    else
    if(hOperation == SQL_POSITION && hLockType != SQL_LOCK_NO_CHANGE)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented", 0, NULL);
        goto error; 
    }
    else
    if(!isScrollableCursor(pStmt))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY106", "Cursor is forward only or non-scrollable", 0, NULL);
        goto error; 
    }

    // Position operation without lock change
    pResult = pStmt->pResultHead;

    if(pResult)
    {
        if((iRow > 0) && (iRow <= pResult->iNumberOfRowsInMem))
        {
            // Position the cursor at specified row.
            pResult->iCurRow = (int)(iRow - 1);
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY107", "Row value out of range", 0, NULL);
            goto error; 
        }
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"24000", "Invalid cursor state", 0, NULL);
        goto error; 
    }

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetPos(FUNC_RETURN, rc, phstmt, iRow, hOperation, hLockType);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLColAttribute.
//
SQLRETURN  SQL_API SQLColAttributeW(SQLHSTMT     phstmt,
                                    SQLUSMALLINT hCol, 
                                    SQLUSMALLINT hFieldIdentifier,
                                    SQLPOINTER   pwValue, 
                                    SQLSMALLINT  cbLen,
                                    SQLSMALLINT *pcbLen, 
                                    SQLLEN_POINTER       plValue)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttributeW(FUNC_CALL, 0, phstmt, hCol, hFieldIdentifier, pwValue, cbLen, pcbLen, plValue);

    rc = RS_STMT_INFO::RS_SQLColAttributeW(phstmt, hCol, hFieldIdentifier, pwValue, cbLen, pcbLen, plValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttributeW(FUNC_RETURN, rc, phstmt, hCol, hFieldIdentifier, pwValue, cbLen, pcbLen, plValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLColAttributeW and SQLColAttributesW.
//
SQLRETURN  SQL_API RS_STMT_INFO::RS_SQLColAttributeW(SQLHSTMT     phstmt,
                                        SQLUSMALLINT hCol, 
                                        SQLUSMALLINT hFieldIdentifier,
                                        SQLPOINTER   pwValue, 
                                        SQLSMALLINT  cbLen,
                                        SQLSMALLINT *pcbLen, 
                                        SQLLEN        *plValue)
{
    SQLRETURN rc;
    char *pValue = NULL;
    int strOption;
    SQLSMALLINT cchLen = 0;

    if(pcbLen)
        *pcbLen = 0;

    strOption = isStrFieldIdentifier(hFieldIdentifier);

    if(strOption)
    {
        if(cbLen < 0)
            cbLen = 0;

        cchLen = cbLen/sizeof(WCHAR);
        if(pwValue != NULL && cbLen >= 0)
            pValue = (char *)rs_calloc(sizeof(char), cchLen + 1);
    }

    rc = RS_STMT_INFO::RS_SQLColAttribute(phstmt, hCol, hFieldIdentifier, (strOption) ? pValue : pwValue,
                                                         (strOption) ? cchLen : cbLen, pcbLen, plValue);

    if(SQL_SUCCEEDED(rc))
    {
        if(strOption)
        {
            // Convert to unicode
            if(pwValue)
                utf8_to_wchar((char *)pValue, cchLen, (WCHAR *)pwValue, cchLen);

            if(pcbLen)
                *pcbLen = *pcbLen * sizeof(WCHAR);
        }
    }

    pValue = (char *)rs_free(pValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLColAttribute returns descriptor information for a column in a result set. Descriptor information is returned 
// as a character string, a descriptor-dependent value, or an integer value.
//
SQLRETURN  SQL_API SQLColAttribute(SQLHSTMT        phstmt,
                                   SQLUSMALLINT hCol, 
                                   SQLUSMALLINT hFieldIdentifier,
                                   SQLPOINTER    pcValue, 
                                   SQLSMALLINT    cbLen,
                                   SQLSMALLINT *pcbLen, 
                                   SQLLEN_POINTER   plValue)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttribute(FUNC_CALL, 0, phstmt, hCol, hFieldIdentifier, pcValue, cbLen, pcbLen, plValue);

    rc = RS_STMT_INFO::RS_SQLColAttribute(phstmt, hCol, hFieldIdentifier, pcValue, cbLen, pcbLen, plValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColAttribute(FUNC_RETURN, rc, phstmt, hCol, hFieldIdentifier, pcValue, cbLen, pcbLen, plValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLColAttribute and SQLColAttributeW.
//
SQLRETURN  SQL_API RS_STMT_INFO::RS_SQLColAttribute(SQLHSTMT        phstmt,
                                       SQLUSMALLINT hCol, 
                                       SQLUSMALLINT hFieldIdentifier,
                                       SQLPOINTER    pcValue, 
                                       SQLSMALLINT    cbLen,
                                       SQLSMALLINT *pcbLen, 
                                       SQLLEN       *plValue)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;
    int isCharIdentifier;
    RS_DESC_REC *pDescRecHead = NULL;

    // Check stmt handle
    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    // Check whether field identifier is char or integer type
    isCharIdentifier = isStrFieldIdentifier(hFieldIdentifier);

    // Is buffer valid?
    if((isCharIdentifier 
            && ((pcValue == NULL && pcbLen == NULL) || (cbLen < 0 && cbLen != SQL_NTS)))
        || (!isCharIdentifier && plValue == NULL))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY090", "Invalid string or buffer length", 0, NULL);
        goto error; 
    }
    
    pResult = pStmt->pResultHead;

    if(!pResult)
    {
        if(pStmt->pPrepareHead && pStmt->pPrepareHead->pResultForDescribeCol && pStmt->iStatus == RS_PREPARE_STMT)
        {
            pResult = pStmt->pPrepareHead->pResultForDescribeCol;
            if(pResult)
                pDescRecHead = pResult->pIRDRecs;
        }
    }
    else
        pDescRecHead = pStmt->pIRD->pDescRecHead;

    if(pResult)
    {
        if(hFieldIdentifier == SQL_DESC_COUNT)
        {
            *plValue = 0;
            *plValue = pResult->iNumberOfCols;
        }
        else
        {
            if(pResult->iNumberOfCols && pDescRecHead)
            {
                if(hCol > 0 && hCol <= pResult->iNumberOfCols)
                {
                    RS_DESC_REC *pDescRec = &pDescRecHead[hCol - 1];

                    switch(hFieldIdentifier)
                    {
                        case SQL_DESC_AUTO_UNIQUE_VALUE:
                        {
                            *plValue = pDescRec->cAutoInc;
                            break;
                        }

                        case SQL_DESC_BASE_COLUMN_NAME:
                        case SQL_DESC_LABEL:
                        case SQL_DESC_NAME:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szName, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_BASE_TABLE_NAME:
                        case SQL_DESC_TABLE_NAME:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szTableName, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_CASE_SENSITIVE:
                        {
                            *plValue = pDescRec->cCaseSensitive;
                            break;
                        }

                        case SQL_DESC_CATALOG_NAME:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szCatalogName, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_CONCISE_TYPE:
                        {
                            *plValue = getConciseType(pDescRec->hConciseType, pDescRec->hType);
                            break;
                        }

                        case SQL_DESC_TYPE:
                        {
                            *plValue = pDescRec->hType;

                            // Convert to ODBC2 type, if needed
                            CONVERT_TO_ODBC2_SQL_DATE_TYPES(pStmt, plValue);

                            break;
                        }

                        case SQL_DESC_DISPLAY_SIZE:
                        {
                            *plValue = pDescRec->iDisplaySize;
                            break;
                        }

                        case SQL_DESC_FIXED_PREC_SCALE:
                        {
                            *plValue = pDescRec->cFixedPrecScale;
                            break;
                        }

                        case SQL_DESC_LENGTH:
                        case SQL_COLUMN_LENGTH:
                        {
                            *plValue = getSize(pDescRec->hType, pDescRec->iSize);
                            break;
                        }

                        case SQL_DESC_LITERAL_PREFIX:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szLiteralPrefix, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_LITERAL_SUFFIX:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szLiteralSuffix, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_LOCAL_TYPE_NAME:
                        case SQL_DESC_TYPE_NAME:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szTypeName, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_NULLABLE:
                        {
                            *plValue = pDescRec->hNullable; 
                            break;
                        }

                        case SQL_DESC_NUM_PREC_RADIX:
                        {
                            *plValue = pDescRec->iNumPrecRadix;
                            break;
                        }

                        case SQL_DESC_OCTET_LENGTH:
                        {
                            *plValue = pDescRec->iOctetLen;
                            break;
                        }

                        case SQL_DESC_PRECISION:
                        case SQL_COLUMN_PRECISION:
                        {
                            *plValue = pDescRec->iPrecision;
                            break;
                        }

                        case SQL_DESC_SCALE:
                        case SQL_COLUMN_SCALE:
                        {
                            *plValue = getScale(pDescRec->hType, pDescRec->hScale);
                            break;
                        }

                        case SQL_DESC_SCHEMA_NAME:
                        {
                            rc = copyStrDataSmallLen(pDescRec->szSchemaName, SQL_NTS, (char *)pcValue, cbLen, pcbLen);
                            break;
                        }

                        case SQL_DESC_SEARCHABLE:
                        {
                            *plValue = pDescRec->iSearchable;
                            break;
                        }

                        case SQL_DESC_UNNAMED:
                        {
                            *plValue = pDescRec->iUnNamed;
                            break;
                        }

                        case SQL_DESC_UNSIGNED:
                        {
                            *plValue = pDescRec->cUnsigned;
                            break;
                        }

                        case SQL_DESC_UPDATABLE:
                        {
                            *plValue = pDescRec->iUpdatable;
                            break;
                        }

                        default:
                        {
                            rc = SQL_ERROR;
                            addError(&pStmt->pErrorList,"HY091", "Invalid descriptor field identifier", 0, NULL);
                            goto error; 
                        }
                    } // Switch
                }
                else
                {
                    rc = SQL_ERROR;
                    addError(&pStmt->pErrorList,"HY000", "Invalid column number", 0, NULL);
                    goto error; 
                }
            }
            else
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"07005", "No columns found", 0, NULL);
                goto error; 
            }
        } // SQL_DESC_COUNT
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "No result found", 0, NULL);
        goto error; 
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLFetchScroll fetches the specified rowset of data from the result set and returns data for all bound columns. 
// Rowsets can be specified at an absolute or relative position or by bookmark.
//
SQLRETURN  SQL_API SQLFetchScroll(SQLHSTMT phstmt, 
                                    SQLSMALLINT hFetchOrientation,
                                    SQLLEN iFetchOffset)

{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFetchScroll(FUNC_CALL, 0, phstmt, hFetchOrientation, iFetchOffset);

    rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, hFetchOrientation, iFetchOffset);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLFetchScroll(FUNC_RETURN, rc, phstmt, hFetchOrientation, iFetchOffset);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLFetchScroll, SQLFetch and SQLExtendedFetch.
//
SQLRETURN  SQL_API RS_STMT_INFO::RS_SQLFetchScroll(SQLHSTMT phstmt,
                                      SQLSMALLINT hFetchOrientation,
                                      SQLLEN iFetchOffset)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_RESULT_INFO *pResult;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!isScrollableCursor(pStmt)
        && (hFetchOrientation != SQL_FETCH_NEXT))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY106", "Fetch type out of range", 0, NULL);
        goto error; 
    }

    pResult = pStmt->pResultHead;

    if(pResult)
    {
        RS_DESC_HEADER &pIRDDescHeader = pStmt->pIRD->pDescHeader;
        RS_DESC_HEADER &pARDDescHeader = pStmt->pStmtAttr->pARD->pDescHeader;

        // Fetch array/single row
        long lRowsToFetch = (pARDDescHeader.lArraySize <= 0) ? 1 : pARDDescHeader.lArraySize;
        long lRowFetched = 0;
        int  iBlockCursor = (lRowsToFetch > 1);
        int  iBindOffset = (pARDDescHeader.plBindOffsetPtr) ? *(pARDDescHeader.plBindOffsetPtr) : 0;

		// Reset previous hCol for SQLGetData
		pResult->iPrevhCol = 0;

        // Loop for block cursor
        for(lRowFetched = 0; lRowFetched < lRowsToFetch; lRowFetched++)
        {
            // Find new position of the cursor
            switch(hFetchOrientation)
            {
                case SQL_FETCH_NEXT:
                {
                    int iMaxRowsReached;

                    // Do we need to read from CSC?
                    if((pResult->iCurRow + 1) >= pResult->iNumberOfRowsInMem)
                    {
                        iMaxRowsReached = ((pStmt->pStmtAttr->iMaxRows > 0) 
                                                && ((pResult->iCurRow + pResult->iRowOffset + 1) >= pStmt->pStmtAttr->iMaxRows));

                        if(!iMaxRowsReached)
                        {
                            // Check for client side cursor
                            if(pgIsFileCreatedCsc(pResult->pgResult))
                            {
                                int iCscError = 0;
                                int gotRowsFromCsc = pgReadNextBatchOfRowsCsc(pResult->pgResult,&iCscError);

                                if(gotRowsFromCsc)
                                {
                                    pResult->iRowOffset += pResult->iNumberOfRowsInMem; // We are discarding some data.
                                    pResult->iCurRow = -1; // We increment below
                                    // New #of rows in memory
                                    pResult->iNumberOfRowsInMem = PQntuples(pResult->pgResult);
                                }

                                if(iCscError)
                                {
                                    rc = SQL_ERROR;
                                    addError(&pStmt->pErrorList,"24000", "An I/O error occurred while reading client side cursor.", 0, NULL);
                                    goto error; 
                                }
                            }
							else
							if(pStmt->pCscStatementContext 
								&& isStreamingCursorMode(pStmt)
								&& !(libpqIsEndOfStreamingCursor(pStmt))
							)
							{
								int iNumberOfRowsInMem = pResult->iNumberOfRowsInMem;
                                int iError = 0;

								// Read more rows from socket
								libpqReadNextBatchOfStreamingRows(pStmt, pStmt->pCscStatementContext, pResult->pgResult, pStmt->phdbc->pgConn,&iError, FALSE);


								// Set pResult parameters
                                // New #of rows in memory
                                pResult->iNumberOfRowsInMem = PQntuples(pResult->pgResult);

								if(pResult->iNumberOfRowsInMem > 0)
								{
                                    pResult->iRowOffset += iNumberOfRowsInMem; // We are discarding some data.
                                    pResult->iCurRow = -1; // We increment below
								}

                                if(iError)
                                {
                                    rc = SQL_ERROR;
                                    addError(&pStmt->pErrorList,"24000", "An I/O error occurred while reading streaming cursor.", 0, NULL);
                                    goto error; 
                                }
							}
                        } // !Max limit
                    } // !Last row in memory

                    // In memory cursor 
                    if((pResult->iCurRow >= -1) && (pResult->iCurRow <= (pResult->iNumberOfRowsInMem - 1)))
                    {
                        iMaxRowsReached = ((pStmt->pStmtAttr->iMaxRows > 0) 
                                                && ((pResult->iCurRow + pResult->iRowOffset + 1) >= pStmt->pStmtAttr->iMaxRows));

                        if(!iMaxRowsReached)
                        {
                            (pResult->iCurRow)++;

                            if(pResult->iCurRow >= pResult->iNumberOfRowsInMem)
                                rc = SQL_NO_DATA;
                        }
                        else
                            rc = SQL_NO_DATA;
                    }
                    else
                        rc = SQL_NO_DATA;

                    break;
                }

                case SQL_FETCH_PRIOR:
                {
                    // Do we need to read from CSC?
                    if((pResult->iCurRow -1) < 0)
                    {
                        // Check for client side cursor
                        if(pgIsFileCreatedCsc(pResult->pgResult))
                        {
                            int iCscError = 0;
                            int gotRowsFromCsc = pgReadPreviousBatchOfRowsCsc(pResult->pgResult,&iCscError);

                            if(gotRowsFromCsc)
                            {
                                // New #of rows in memory
                                pResult->iNumberOfRowsInMem = PQntuples(pResult->pgResult);
                                pResult->iRowOffset -= pResult->iNumberOfRowsInMem; // Point to zeroth row in current batch w.r.t. overall result.
                                pResult->iCurRow = pResult->iNumberOfRowsInMem; // We decrement below
                            }

                            if(iCscError)
                            {
                                rc = SQL_ERROR;
                                addError(&pStmt->pErrorList,"24000", "An I/O error occurred while reading client side cursor.", 0, NULL);
                                goto error; 
                            }
                        }
                    }

                    // In memory cursor
                    if((pResult->iCurRow >= 0) && (pResult->iCurRow <= pResult->iNumberOfRowsInMem))
                    {
                        (pResult->iCurRow)--;
                        if(pResult->iCurRow <= -1)
                            rc = SQL_NO_DATA;
                    }
                    else
                        rc = SQL_NO_DATA;

                    break;

                }

                case SQL_FETCH_FIRST:
                {
                    if(pResult->iNumberOfRowsInMem > 0)
                    {
                        rc = RS_RESULT_INFO::setFetchAtFirstRow(pStmt, pResult);
                        if(rc == SQL_ERROR)
                            goto error;

                        // In memory cursor
                        pResult->iCurRow = 0;

                        if(iBlockCursor)
                            hFetchOrientation = SQL_FETCH_NEXT;
                    }
                    else
                        rc = SQL_NO_DATA;

                    break;
                }

                case SQL_FETCH_LAST:
                {
                    if(pResult->iNumberOfRowsInMem > 0)
                    {
                        rc = RS_RESULT_INFO::setFetchAtLastRow(pStmt, pResult);
                        if(rc == SQL_ERROR)
                            goto error;

                        // In memory cursor
                        if((pStmt->pStmtAttr->iMaxRows > 0) && (pStmt->pStmtAttr->iMaxRows < (pResult->iRowOffset + pResult->iNumberOfRowsInMem)))
                        {
                            pResult->iCurRow = (pStmt->pStmtAttr->iMaxRows > pResult->iRowOffset) 
                                                    ? pStmt->pStmtAttr->iMaxRows - pResult->iRowOffset - 1
                                                    : 0; // This shouldn't happen.
                        }
                        else
                            pResult->iCurRow = pResult->iNumberOfRowsInMem - 1;

                        if(iBlockCursor)
                            hFetchOrientation = SQL_FETCH_NEXT;
                    }
                    else
                        rc = SQL_NO_DATA;

                    break;
                }

                case SQL_FETCH_ABSOLUTE:
                {
                    rc = RS_RESULT_INFO::setAbsolute(pStmt, pResult, iFetchOffset);
                    if(rc == SQL_ERROR)
                        goto error;

                    if(iBlockCursor && rc == SQL_SUCCESS)
                        hFetchOrientation = SQL_FETCH_NEXT;

                    break;
                }

                case SQL_FETCH_RELATIVE:
                {
                    if(iFetchOffset == 0)
                    {
                        // Valid but no change in cursor position
                        if(pResult->iNumberOfRowsInMem == 0)
                        {
                            rc = SQL_NO_DATA;
                        }
                        else
                        if(RS_RESULT_INFO::isBeforeFirstRow(pStmt, pResult) || RS_RESULT_INFO::isAfterLastRow(pStmt, pResult)) // before first or after last
                        {
                            rc = SQL_NO_DATA;
                        }
                        else
                        {
                            rc = SQL_SUCCESS;
                        }
                    }
                    else
                    {
                        //have to add 1 since absolute expects a 1-based index
                        SQLLEN index = pResult->iCurRow + 1 + iFetchOffset;

                        // Check for client side cursor
                        if(pgIsFileCreatedCsc(pResult->pgResult))
                        {
                            // Index should be from row_offset.
                            index += pResult->iRowOffset;
                        }
                        
                        if (index < 0)
                        {
                            // Absolute -ve and relative -ve are different. Here -ve means before first row.

                            // Set at first in CSC
                            rc = RS_RESULT_INFO::setFetchAtFirstRow(pStmt, pResult);
                            if(rc == SQL_ERROR)
                                goto error;

                            // Before first
                            pResult->iCurRow = -1;
                            rc = SQL_NO_DATA;
                        }
                        else
                        {
                            rc = RS_RESULT_INFO::setAbsolute(pStmt, pResult, index);
                        }
                    }

                    if(rc == SQL_ERROR)
                        goto error;

                    if(iBlockCursor && rc == SQL_SUCCESS)
                        hFetchOrientation = SQL_FETCH_NEXT;

                    break;
                }

                case SQL_FETCH_BOOKMARK:
                {
                    rc = SQL_ERROR;
                    addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented", 0, NULL);
                    goto error; 
                }

                default:
                {
                    rc = SQL_ERROR;
                    addError(&pStmt->pErrorList,"HY106", "Fetch type out of range", 0, NULL);
                    goto error; 
                }

            } // Switch

            if(rc == SQL_SUCCESS)
            {
                if(pStmt->pStmtAttr->iRetrieveData == SQL_RD_ON)
                {
                    RS_DESC_REC *pDescRec;
                    SQLRETURN rc1;

                    // Put data in bind buffers, if any
                    for(pDescRec = pStmt->pStmtAttr->pARD->pDescRecHead; pDescRec != NULL; pDescRec = pDescRec->pNext)
                    {
                        int iValOffset = 0;
                        SQLLEN *pcbLenInd;

                        if(iBlockCursor)
                        {
                            if(pARDDescHeader.lBindType == SQL_BIND_BY_COLUMN)
                            {
                                // Column wise binding
                                iValOffset = pDescRec->iOctetLen;

                                if(!iValOffset)
                                {
                                    rc = SQL_ERROR;
                                    addError(&pStmt->pErrorList,"HY000", "Array element length is zero.", 0, NULL);
                                    goto error;
                                }

                                pcbLenInd  = (pDescRec->pcbLenInd) ? pDescRec->pcbLenInd + lRowFetched  : NULL;
                            }
                            else
                            {
                                // Row wise binding
                                iValOffset = pARDDescHeader.lBindType;

                                if(pDescRec->plOctetLen == NULL)
                                {
                                    // Same structure should have length indicator
                                    pcbLenInd  = (pDescRec->pcbLenInd) ? (SQLLEN *)(((char *)pDescRec->pcbLenInd) + (iValOffset * lRowFetched))  : NULL;
                                }
                                else
                                {
                                    // Different array should have length indicator
                                    pcbLenInd  = (pDescRec->pcbLenInd) ? pDescRec->pcbLenInd + lRowFetched  : NULL;
                                }
                            }
                        }
                        else
                            pcbLenInd = (pDescRec->pcbLenInd) ? pDescRec->pcbLenInd + lRowFetched  : NULL;

                        // Get data 
                        rc1 = RS_STMT_INFO::RS_SQLGetData(pStmt, pDescRec->hRecNumber, pDescRec->hType,
                                            (pDescRec->pValue) ? ((char *)pDescRec->pValue + (lRowFetched * iValOffset) + iBindOffset) : NULL, 
                                            pDescRec->cbLen, 
                                            (SQLLEN *)((char *)pcbLenInd + iBindOffset),
											TRUE);

                        if(rc1 == SQL_ERROR)
                        {
                            rc = SQL_ERROR;
                            break;
                        }
                    } // Column loop

                    // Put the fetch count
                    if(pIRDDescHeader.valid)
                    {
                        // Row count
                        if(pIRDDescHeader.plRowsProcessedPtr)
                            *(pIRDDescHeader.plRowsProcessedPtr) = lRowFetched + 1;

                        // Row status
                        if(pIRDDescHeader.phArrayStatusPtr)
                        {
                            short hRowStatus;

                            if(rc == SQL_SUCCESS)
                                hRowStatus = SQL_ROW_SUCCESS;
                            else
                            if(rc == SQL_SUCCESS_WITH_INFO)
                                hRowStatus = SQL_ROW_SUCCESS_WITH_INFO;
                            else
                            if(rc == SQL_ERROR)
                                hRowStatus = SQL_ROW_ERROR;
                            else
                                hRowStatus = SQL_ROW_ERROR;

                            *(pIRDDescHeader.phArrayStatusPtr + lRowFetched) = hRowStatus;
                        }
                    } 
                } // SQL_RD_ON
            } // Success
            else
            {
                if(rc == SQL_NO_DATA)
                {
                    // Put the row status of last row as SQL_ROW_NOROW
                    if(pStmt->pStmtAttr->iRetrieveData == SQL_RD_ON)
                    {
                        if (pIRDDescHeader.valid &&
                            pIRDDescHeader.phArrayStatusPtr) {
                            *(pIRDDescHeader.phArrayStatusPtr + lRowFetched) =
                                SQL_ROW_NOROW;
                        }
                    } // SQL_RD_ON
                }

                break; // SQL_NO_DATA
            }
        } // Rows loop

        // Check for last partial set of rows
        if(rc == SQL_NO_DATA)
        {
            if(lRowFetched > 0)
                rc = SQL_SUCCESS;
        } 
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"24000", "Invalid cursor state", 0, NULL);
        goto error; 
    }

error:

    return rc;
}


/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the cursor at first batch
//

SQLRETURN RS_RESULT_INFO::setFetchAtFirstRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(pResult->iNumberOfRowsInMem > 0)
    {
        // Check for client side cursor and set at the first batch
        if(pgIsFileCreatedCsc(pResult->pgResult))
        {
            int iCscError = 0;
            int gotRowsFromCsc = pgReadFirstBatchOfRowsCsc(pResult->pgResult,&iCscError);

            if(gotRowsFromCsc)
            {
                // New #of rows in memory
                pResult->iNumberOfRowsInMem = PQntuples(pResult->pgResult);
                pResult->iRowOffset = pgGetFirstRowIndexCsc(pResult->pgResult); 
                pResult->iCurRow = 0; 
            }

            if(iCscError)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"24000", "An I/O error occurred while reading client side cursor.", 0, NULL);
                goto error; 
            }
        }
    }

error:

    return rc;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the cursor at last batch
//

SQLRETURN RS_RESULT_INFO::setFetchAtLastRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult)
{
    SQLRETURN rc = SQL_SUCCESS;

    // Check for client side cursor
    if(pgIsFileCreatedCsc(pResult->pgResult))
    {
        int iCscError = 0;
        int gotRowsFromCsc = pgReadLastBatchOfRowsCsc(pResult->pgResult,&iCscError);

        if(gotRowsFromCsc)
        {
            // New #of rows in memory
            pResult->iNumberOfRowsInMem = PQntuples(pResult->pgResult);
            pResult->iRowOffset = pgGetFirstRowIndexCsc(pResult->pgResult); 
            pResult->iCurRow = pResult->iNumberOfRowsInMem - 1; 
        }

        if(iCscError)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"24000", "An I/O error occurred while reading client side cursor.", 0, NULL);
            goto error; 
        }
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set absolute row position in the result cursor.
//
SQLRETURN RS_RESULT_INFO::setAbsolute(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult, SQLLEN iFetchOffset)
{
    SQLRETURN rc = SQL_SUCCESS;

    // index is 1-based, but internally we use 0-based indices
    int internalIndex = 0;

    if(iFetchOffset == 0)
    {
        // Set at first in CSC
        rc = RS_RESULT_INFO::setFetchAtFirstRow(pStmt, pResult);
        if(rc == SQL_ERROR)
            goto error;

        // Before first
        pResult->iCurRow = -1;
        rc = SQL_NO_DATA;

        return rc;
    }
    
    // Check for client side cursor
    if(pgIsFileCreatedCsc(pResult->pgResult))
    {
        int iCscError = 0;
        int iNewIndex = (int) iFetchOffset;
        int gotRowsFromCsc = pgReadAbsoluteBatchOfRowsCsc(pResult->pgResult,&iCscError,&iNewIndex);

        if(gotRowsFromCsc)
        {
            // New #of rows in memory
            pResult->iNumberOfRowsInMem = PQntuples(pResult->pgResult);
            pResult->iRowOffset = pgGetFirstRowIndexCsc(pResult->pgResult); 
        }

        // Adjust the index in current memory batch
        iFetchOffset = iNewIndex;

        if(iCscError)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"24000", "An I/O error occurred while reading client side cursor.", 0, NULL);
            goto error; 
        }
    }

    if(iFetchOffset < 0)
    {
        //if iFetchOffset<0, count from the end of the result set, but check
        //to be sure that it is not beyond the first index
        if (iFetchOffset >= -(pResult->iNumberOfRowsInMem))
            internalIndex = (int)(pResult->iNumberOfRowsInMem + iFetchOffset);
        else
        {
            // Set at first in CSC
            rc = RS_RESULT_INFO::setFetchAtFirstRow(pStmt, pResult);
            if(rc == SQL_ERROR)
                goto error;

            // Before first
            pResult->iCurRow = -1;
            rc = SQL_NO_DATA;

            return rc;
        }
    }
    else
    {
        //must be the case that iFetchOffset>0,
        //find the correct place, assuming that
        //the index is not too large
        if (iFetchOffset <= pResult->iNumberOfRowsInMem)
            internalIndex = (int)(iFetchOffset - 1);
        else
        {
            // Set at last in CSC
            rc = RS_RESULT_INFO::setFetchAtLastRow(pStmt, pResult);
            if(rc == SQL_ERROR)
                goto error;

            // After last
            pResult->iCurRow = pResult->iNumberOfRowsInMem;
            rc = SQL_NO_DATA;

            return rc;
        }
    }

    // Set in memory index
    pResult->iCurRow = internalIndex;

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if it's before first row
//
int RS_RESULT_INFO::isBeforeFirstRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult)
{
    int rc = ((pResult->iRowOffset + pResult->iCurRow) < 0 && pResult->iNumberOfRowsInMem > 0);

    // Check for client side cursor
    if(rc && pgIsFileCreatedCsc(pResult->pgResult))
    {
        // Are we on first batch in file cursor? We already reach at before first in memory cursor.
        // So first batch in file and before first row in memory will conclude it's at before first row.
        rc = pgIsFirstBatchCsc(pResult->pgResult);
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if it's before first row
//
int RS_RESULT_INFO::isAfterLastRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult)
{
    int rows_size = pResult->iNumberOfRowsInMem;
    int rc = (pResult->iCurRow >= rows_size && rows_size > 0);
    
    // Check for client side cursor
    if(rc  && pgIsFileCreatedCsc(pResult->pgResult))
    {
        // Are we on last batch in file cursor? We already reach after last in memory cursor.
        // So last batch in file and after last row in memory will conclude it's at after last row.
        rc = pgIsLastBatchCsc(pResult->pgResult);
    }

    return rc;
}
