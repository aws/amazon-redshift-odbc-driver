/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsparameter.h"
#include "rsoptions.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLBindParameter binds a buffer to a parameter marker in an SQL statement.
//
SQLRETURN SQL_API SQLBindParameter(SQLHSTMT               phstmt,
                                    SQLUSMALLINT       hParam,
                                    SQLSMALLINT        hInOutType,
                                    SQLSMALLINT        hType,
                                    SQLSMALLINT        hSQLType,
                                    SQLULEN            iColSize,
                                    SQLSMALLINT        hScale,
                                    SQLPOINTER         pValue,
                                    SQLLEN             cbLen,
                                    SQLLEN             *pcbLenInd)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBindParameter(FUNC_CALL, 0, phstmt, hParam, hInOutType, hType, hSQLType, iColSize, hScale, pValue, cbLen, pcbLenInd);

    rc = RsParameter::RS_SQLBindParameter(phstmt, hParam, hInOutType, hType, hSQLType, iColSize, hScale, pValue, cbLen, pcbLenInd);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBindParameter(FUNC_RETURN, rc, phstmt, hParam, hInOutType, hType, hSQLType, iColSize, hScale, pValue, cbLen, pcbLenInd);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLBindParameter, SQLSetParam and SQLBindParam.
//
SQLRETURN SQL_API RsParameter::RS_SQLBindParameter(SQLHSTMT     phstmt,
                                    SQLUSMALLINT       hParam,
                                    SQLSMALLINT        hInOutType,
                                    SQLSMALLINT        hType,
                                    SQLSMALLINT        hSQLType,
                                    SQLULEN            iColSize,
                                    SQLSMALLINT        hScale,
                                    SQLPOINTER         pValue,
                                    SQLLEN             cbLen,
                                    SQLLEN             *pcbLenInd)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_DESC_REC *pDescRec;
    
    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if((short)hParam < 0)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid parameter number", 0, NULL);
        goto error; 
    }

/*  TODO: See OUTPUT param is supported or not in this driver.
    if(hInOutType == SQL_PARAM_OUTPUT)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY105", "Invalid parameter type", 0, NULL);
        goto error; 
    } */

    if(pValue == NULL && pcbLenInd == NULL)
    {
        // Unbind it
        releaseDescriptorRecByNum(pStmt->pStmtAttr->pAPD, hParam);
    }
    else
    {
		int iNewDecRec;

        // Find if binding already exist, then it's re-bind.
        pDescRec = checkAndAddDescRec(pStmt->pStmtAttr->pAPD, hParam, FALSE,&iNewDecRec);
        if(pDescRec == NULL)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }

        if(pDescRec)
        {
            // Store app values
            pDescRec->hRecNumber    = hParam;
            pDescRec->hInOutType  = hInOutType;
            pDescRec->hType        = hType;
            pDescRec->hParamSQLType    = hSQLType;
            pDescRec->iSize    = (int)iColSize;
            pDescRec->hScale        = hScale;
            pDescRec->pValue        = pValue;
            pDescRec->cbLen        = cbLen;
            pDescRec->pcbLenInd    = pcbLenInd;

            // For ARRAY binding we need C Type. If it's Default or unsupported then it may return 0, so we need to fall back on SQL type.
            pDescRec->iOctetLen = getOctetLenUsingCType(hType, (int)cbLen);
            if(pDescRec->iOctetLen == 0)
                pDescRec->iOctetLen = getOctetLen(hSQLType, (int)cbLen, 0);

			// Keep number of OUT only params
			if (iNewDecRec)
			{
				if (hInOutType == SQL_PARAM_OUTPUT)
					pStmt->iNumOfOutOnlyParams++;
				else
				if (hInOutType == SQL_PARAM_INPUT_OUTPUT)
					pStmt->iNumOfInOutOnlyParams++;
			}
        }
    }

error:

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLNumParams returns the number of parameters in an SQL statement.
//
SQLRETURN SQL_API SQLNumParams(SQLHSTMT  phstmt,
                                SQLSMALLINT   *pParamCount)
{
    SQLRETURN rc ;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNumParams(FUNC_CALL, 0, phstmt, pParamCount);

    rc = RsParameter::RS_SQLNumParams(phstmt, pParamCount);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLNumParams(FUNC_RETURN, rc, phstmt, pParamCount);

    return rc;
}

/*====================================================================================================================================================*/

SQLRETURN SQL_API RsParameter::RS_SQLNumParams(SQLHSTMT  phstmt,
                                                SQLSMALLINT   *pParamCount)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_PREPARE_INFO *pPrepare;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(pParamCount == NULL)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    *pParamCount = 0;

    if(pStmt->iStatus != RS_PREPARE_STMT)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY010", "Function sequence error", 0, NULL);
        goto error; 
    }

    pPrepare = pStmt->pPrepareHead;

    if(pPrepare)
        *pParamCount = (SQLSMALLINT) getNumberOfParams(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLDescribeParam returns the description of a parameter marker associated with a prepared SQL statement.
//
SQLRETURN SQL_API SQLDescribeParam(SQLHSTMT         phstmt,
                                    SQLUSMALLINT    hParam,
                                    SQLSMALLINT     *pDataType,
                                    SQLULEN         *pParamSize,
                                    SQLSMALLINT     *pDecimalDigits,
                                    SQLSMALLINT     *pNullable)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDescribeParam(FUNC_CALL, 0, phstmt, hParam, pDataType, pParamSize, pDecimalDigits, pNullable);

    rc = RsParameter::RS_SQLDescribeParam(phstmt, hParam, pDataType, pParamSize, pDecimalDigits, pNullable);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLDescribeParam(FUNC_RETURN, rc, phstmt, hParam, pDataType, pParamSize, pDecimalDigits, pNullable);

    return rc;
}

/*====================================================================================================================================================*/

SQLRETURN SQL_API RsParameter::RS_SQLDescribeParam(SQLHSTMT         phstmt,
                                                    SQLUSMALLINT    hParam,
                                                    SQLSMALLINT     *pDataType,
                                                    SQLULEN         *pParamSize,
                                                    SQLSMALLINT     *pDecimalDigits,
                                                    SQLSMALLINT     *pNullable)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_PREPARE_INFO *pPrepare;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);
    pPrepare = pStmt->pPrepareHead;

    if(pPrepare)
    {
        if(pPrepare->iNumberOfParams && pStmt->pIPD->pDescRecHead)
        {
            int iNumberOfParams =  getNumberOfParams(pStmt);

            if(hParam > 0 && hParam <= iNumberOfParams)
            {
                RS_DESC_REC *pDescRec = &pStmt->pIPD->pDescRecHead[hParam - 1];

                if(pDataType)
                    *pDataType = pDescRec->hType;

                if(pParamSize)
                    *pParamSize = pDescRec->iSize;

                if(pDecimalDigits)
                    *pDecimalDigits = pDescRec->hScale;

                if(pNullable)
                    *pNullable = pDescRec->hNullable;

                // Convert to ODBC2 type, if needed
                CONVERT_TO_ODBC2_SQL_DATE_TYPES(pStmt, pDataType);
            }
            else
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY000", "Invalid parameter number", 0, NULL);
                goto error; 
            }
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"07005", "No parameters found", 0, NULL);
            goto error; 
        }
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "No prepared statement found", 0, NULL);
        goto error; 
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// The ODBC 2.0 function SQLParamOptions has been replaced in ODBC 3.x by calls to SQLSetStmtAttr.
//
SQLRETURN SQL_API SQLParamOptions(SQLHSTMT phstmt, 
                                  SQLULEN  iCrow,
                                  SQLULEN  *piRow)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLParamOptions(FUNC_CALL, 0, phstmt, iCrow, piRow);

    rc = RsParameter::RS_SQLParamOptions(phstmt, iCrow, piRow);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLParamOptions(FUNC_RETURN, rc, phstmt, iCrow, piRow);

    return rc;
}

/*====================================================================================================================================================*/

SQLRETURN SQL_API RsParameter::RS_SQLParamOptions(SQLHSTMT phstmt,
                                  SQLULEN  iCrow,
                                  SQLULEN  *piRow)
{
    SQLRETURN rc;

    // Param set size
    rc = RsOptions::RS_SQLSetStmtAttr(phstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) iCrow, 0);

    if(rc == SQL_SUCCESS)
    {
        // Actual processed param size
        if(piRow)
            *piRow = 0;

        rc = RsOptions::RS_SQLSetStmtAttr(phstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, piRow, 0);
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 2.0, the ODBC 1.0 function SQLSetParam has been replaced by SQLBindParameter. 
//
SQLRETURN  SQL_API SQLSetParam(SQLHSTMT phstmt, 
                               SQLUSMALLINT hParam, 
                                SQLSMALLINT hValType,
                                SQLSMALLINT hParamType, 
                                SQLULEN iLengthPrecision,
                                SQLSMALLINT hParamScale, 
                                SQLPOINTER pParamVal,
                                SQLLEN *piStrLen_or_Ind)
{
    SQLRETURN rc;
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetParam(FUNC_CALL, 0, phstmt, hParam, hValType, hParamType, iLengthPrecision, hParamScale, pParamVal, piStrLen_or_Ind);

    rc = RsParameter::RS_SQLBindParameter(phstmt, hParam, SQL_PARAM_INPUT_OUTPUT, hValType, hParamType, iLengthPrecision, hParamScale,
                                        pParamVal, SQL_SETPARAM_VALUE_MAX,    piStrLen_or_Ind);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetParam(FUNC_RETURN, rc, phstmt, hParam, hValType, hParamType, iLengthPrecision, hParamScale, pParamVal, piStrLen_or_Ind);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.0, function SQLBindParam has been replaced by SQLBindParameter. 
//
SQLRETURN  SQL_API SQLBindParam(SQLHSTMT phstmt,
                                SQLUSMALLINT hParam, 
                                SQLSMALLINT hValType,
                                SQLSMALLINT hParamType, 
                                SQLULEN        iLengthPrecision,
                                SQLSMALLINT hParamScale, 
                                SQLPOINTER  pParamVal,
                                SQLLEN        *piStrLen_or_Ind)
{
    SQLRETURN rc;
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBindParam(FUNC_CALL, 0, phstmt, hParam, hValType, hParamType, iLengthPrecision, hParamScale, pParamVal, piStrLen_or_Ind);

    rc = RsParameter::RS_SQLBindParameter(phstmt, hParam, SQL_PARAM_INPUT, hValType, hParamType, iLengthPrecision, hParamScale,
                                          pParamVal, SQL_SETPARAM_VALUE_MAX,    piStrLen_or_Ind); // Bufferlength

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLBindParam(FUNC_RETURN, rc, phstmt, hParam, hValType, hParamType, iLengthPrecision, hParamScale, pParamVal, piStrLen_or_Ind);

    return rc;
}


