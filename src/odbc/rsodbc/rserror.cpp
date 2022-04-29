/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rserror.h"
#include "rsmin.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLError returns error or status information. 
//
SQLRETURN  SQL_API SQLError(SQLHENV phenv,
                            SQLHDBC phdbc, 
                            SQLHSTMT phstmt,
                            SQLCHAR *pSqlState,  
                            SQLINTEGER *piNativeError,
                            SQLCHAR *pMessageText, 
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen)
{
    SQLRETURN rc;

    if(phstmt == NULL)
        beginApiMutex((phdbc) ? NULL : phenv, phdbc);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLError(FUNC_CALL, 0, phenv, phdbc, phstmt, pSqlState, piNativeError, pMessageText, cbLen, pcbLen);
    }

    rc = RsError::RS_SQLError(phenv, phdbc, phstmt, NULL, pSqlState, piNativeError, pMessageText, cbLen, pcbLen, 1, TRUE);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLError(FUNC_RETURN, rc, phenv, phdbc, phstmt, pSqlState, piNativeError, pMessageText, cbLen, pcbLen);
    }

    if(phstmt == NULL)
        endApiMutex((phdbc) ? NULL : phenv, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLError and SQLErrorW.
//
SQLRETURN  SQL_API RsError::RS_SQLError(SQLHENV phenv,
                            SQLHDBC phdbc, 
                            SQLHSTMT phstmt,
                            SQLHDESC phdesc,
                            SQLCHAR *pSqlstate,  
                            SQLINTEGER *piNativeError,
                            SQLCHAR *pMessageText, 
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen,
                            SQLSMALLINT recNumber,
                            int remove)


{
    SQLRETURN rc = SQL_SUCCESS;
    RS_ERROR_INFO *pError;
    RS_ENV_INFO *pEnv = NULL;
    
    // Check handle for getting error.
    if(phdesc != NULL)
    {
        RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;

        pEnv = (pDesc->phdbc) ? pDesc->phdbc->phenv : NULL;
        pError = getNextError(&(pDesc->pErrorList), recNumber, remove);
    }
    else
    if(phstmt != NULL)
    {
        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

        pEnv  = pStmt->phdbc->phenv;
        pError = getNextError(&(pStmt->pErrorList), recNumber, remove);
    }
    else
    if(phdbc != NULL)
    {
        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

        pEnv  = pConn->phenv;
        pError = getNextError(&(pConn->pErrorList), recNumber, remove);
    }
    else
    if(phenv != NULL)
    {
        pEnv = (RS_ENV_INFO *)phenv;

        pError = getNextError(&(pEnv->pErrorList), recNumber, remove);
    }
    else
        pError = NULL;

    // Do we have any error or warning?
    if(pError != NULL)
    {
        rc = copyStrDataSmallLen(pError->szErrMsg, SQL_NTS, (char *)pMessageText, cbLen, pcbLen);

        if(pSqlstate != NULL)
            rs_strncpy((char *)pSqlstate, pError->szSqlState, sizeof(pError->szSqlState));

        if(piNativeError != NULL)
            *piNativeError = pError->lNativeErrCode;

        mapToODBC2SqlState(pEnv,(char *)pSqlstate);

        if(remove) {
          delete pError;
          pError = NULL;
        }
    }
    else
    {
        rc = SQL_NO_DATA;

        if(pMessageText && cbLen > 0)
            *pMessageText = '\0';
        if(pcbLen)
            *pcbLen = 0;
        if(pSqlstate != NULL)
            pSqlstate[0] = '\0';
        if(piNativeError != NULL)
            *piNativeError = 0;
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLError.
//
SQLRETURN SQL_API SQLErrorW(SQLHENV     phenv,
                            SQLHDBC     phdbc,
                            SQLHSTMT    phstmt,
                            SQLWCHAR*    pwSqlState,
                            SQLINTEGER* piNativeError,
                            SQLWCHAR*    pwMessageText,
                            SQLSMALLINT  cchLen,
                            SQLSMALLINT* pcchLen)
{
    SQLRETURN rc;
    char szSqlState[MAX_SQL_STATE_LEN];
    char szErrMsg[MAX_ERR_MSG_LEN];
    SQLSMALLINT hLen = redshift_min(MAX_ERR_MSG_LEN,cchLen);

    if(phstmt == NULL)
        beginApiMutex((phdbc) ? NULL : phenv, phdbc);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLErrorW(FUNC_CALL, 0, phenv, phdbc, phstmt, pwSqlState, piNativeError, pwMessageText, cchLen, pcchLen);
    }

    szSqlState[MAX_SQL_STATE_LEN - 1] = '\0';
    if(hLen > 0)
        szErrMsg[hLen - 1] = '\0';
    else
        szErrMsg[MAX_ERR_MSG_LEN - 1] = '\0';
    rc = RsError::RS_SQLError(phenv, phdbc, phstmt, NULL, (SQLCHAR *)szSqlState, piNativeError, (SQLCHAR *)((pwMessageText) ? szErrMsg : NULL),
                    (pwMessageText) ? hLen : 0, pcchLen, 1, TRUE);

    if(SQL_SUCCEEDED(rc))
    {
        // Convert to unicode
        if(pwSqlState)
            utf8_to_wchar(szSqlState, MAX_SQL_STATE_LEN, pwSqlState, MAX_SQL_STATE_LEN);

        if(pwMessageText)
            utf8_to_wchar(szErrMsg, SQL_NTS, pwMessageText, cchLen);
    }

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLErrorW(FUNC_RETURN, rc, phenv, phdbc, phstmt, pwSqlState, piNativeError, pwMessageText, cchLen, pcchLen);
    }

    if(phstmt == NULL)
        endApiMutex((phdbc) ? NULL : phenv, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetDiagRec returns the current values of multiple fields of a diagnostic record that contains error, warning, 
// and status information. Unlike SQLGetDiagField, which returns one diagnostic field per call, SQLGetDiagRec 
// returns several commonly used fields of a diagnostic record, including the SQLSTATE, the native error code, and 
// the diagnostic message text.
//
SQLRETURN  SQL_API SQLGetDiagRec(SQLSMALLINT hHandleType, 
                                   SQLHANDLE pHandle,
                                   SQLSMALLINT hRecNumber, 
                                   SQLCHAR *pSqlState,
                                   SQLINTEGER *piNativeError, 
                                   SQLCHAR *pMessageText,
                                   SQLSMALLINT cbLen, 
                                   SQLSMALLINT *pcbLen)
{
    SQLRETURN rc;

    if(hHandleType == SQL_HANDLE_ENV)
        beginApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        beginApiMutex(NULL, pHandle);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagRec(FUNC_CALL, 0, hHandleType, pHandle, hRecNumber, pSqlState, piNativeError, pMessageText, cbLen, pcbLen);
    }

    rc = RsError::RS_SQLGetDiagRec(hHandleType, pHandle, hRecNumber, pSqlState, piNativeError, pMessageText, cbLen, pcbLen);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagRec(FUNC_RETURN, rc, hHandleType, pHandle, hRecNumber, pSqlState, piNativeError, pMessageText, cbLen, pcbLen);
    }

    if(hHandleType == SQL_HANDLE_ENV)
        endApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        endApiMutex(NULL, pHandle);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetDiagRec and SQLGetDiagRecW.
//
SQLRETURN  SQL_API RsError::RS_SQLGetDiagRec(SQLSMALLINT hHandleType,
                                   SQLHANDLE pHandle,
                                   SQLSMALLINT hRecNumber, 
                                   SQLCHAR *pSqlstate,
                                   SQLINTEGER *piNativeError, 
                                   SQLCHAR *pMessageText,
                                   SQLSMALLINT cbLen, 
                                   SQLSMALLINT *pcbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLHENV phenv = NULL;
    SQLHDBC phdbc = NULL; 
    SQLHSTMT phstmt = NULL;
    SQLHDESC phdesc = NULL;

    // Check for record number
    if(hRecNumber <= 0 || cbLen < 0)
        rc = SQL_ERROR;

    // Check for handle type
    if(hHandleType == SQL_HANDLE_ENV)
        phenv = pHandle;
    else
    if(hHandleType == SQL_HANDLE_DBC)
        phdbc = pHandle;
    else
    if(hHandleType == SQL_HANDLE_STMT)
        phstmt = pHandle;
    else
    if(hHandleType == SQL_HANDLE_DESC)
    {
        phdesc = pHandle;
    }
    else
    {
        // Invalid handle type
        rc = SQL_INVALID_HANDLE;
    }

    if(rc == SQL_SUCCESS)
    {
        if(phenv || phdbc || phstmt || phdesc)
        {
            rc = RsError::RS_SQLError(phenv, phdbc, phstmt, phdesc, pSqlstate, piNativeError, pMessageText, cbLen, pcbLen, hRecNumber, FALSE);
        }
        else
        {
            rc = SQL_INVALID_HANDLE;
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetDiagRec.
//
SQLRETURN SQL_API SQLGetDiagRecW(SQLSMALLINT    hHandleType,
                                    SQLHANDLE      pHandle,
                                    SQLSMALLINT    hRecNumber,
                                    SQLWCHAR       *pwSqlState,
                                    SQLINTEGER     *piNativeError,
                                    SQLWCHAR       *pwMessageText,
                                    SQLSMALLINT    cchLen,
                                    SQLSMALLINT    *pcchLen)
{
    SQLRETURN rc;
    char szSqlState[MAX_SQL_STATE_LEN];
    char szErrMsg[MAX_ERR_MSG_LEN];
    SQLSMALLINT hLen = redshift_min(MAX_ERR_MSG_LEN,cchLen);

    if(hHandleType == SQL_HANDLE_ENV)
        beginApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        beginApiMutex(NULL, pHandle);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagRecW(FUNC_CALL, 0, hHandleType, pHandle, hRecNumber, pwSqlState, piNativeError, pwMessageText, cchLen, pcchLen);
    }

    szSqlState[MAX_SQL_STATE_LEN - 1] = '\0';

    if(hLen > 0)
        szErrMsg[hLen - 1] = '\0';
    else
        szErrMsg[MAX_ERR_MSG_LEN - 1] = '\0';

    rc = RsError::RS_SQLGetDiagRec(hHandleType, pHandle, hRecNumber, (SQLCHAR *)szSqlState, piNativeError,  (SQLCHAR *)((pwMessageText) ? szErrMsg : NULL),
                            (pwMessageText) ? hLen : 0, pcchLen);

    if(SQL_SUCCEEDED(rc))
    {
        // Convert to unicode
        if(pwSqlState)
            utf8_to_wchar(szSqlState, MAX_SQL_STATE_LEN, pwSqlState, MAX_SQL_STATE_LEN);

        if(pwMessageText)
            utf8_to_wchar(szErrMsg, SQL_NTS, pwMessageText, cchLen);
    }

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagRecW(FUNC_RETURN, rc, hHandleType, pHandle, hRecNumber, pwSqlState, piNativeError, pwMessageText, cchLen, pcchLen);
    }

    if(hHandleType == SQL_HANDLE_ENV)
        endApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        endApiMutex(NULL, pHandle);

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetDiagField returns the current value of a field of a record of the diagnostic data structure 
// (associated with a specified handle) that contains error, warning, and status information.
//
SQLRETURN  SQL_API SQLGetDiagField(SQLSMALLINT hHandleType, 
                                    SQLHANDLE pHandle,
                                    SQLSMALLINT hRecNumber, 
                                    SQLSMALLINT hDiagIdentifier,
                                    SQLPOINTER  pDiagInfo, 
                                    SQLSMALLINT cbLen,
                                    SQLSMALLINT *pcbLen)
{
    SQLRETURN rc;

    if(hHandleType == SQL_HANDLE_ENV)
        beginApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        beginApiMutex(NULL, pHandle);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagField(FUNC_CALL, 0, hHandleType, pHandle, hRecNumber, hDiagIdentifier, pDiagInfo, cbLen, pcbLen);
    }

    rc = RsError::RS_SQLGetDiagField(hHandleType, pHandle, hRecNumber, hDiagIdentifier, pDiagInfo, cbLen, pcbLen, NULL);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagField(FUNC_RETURN, rc, hHandleType, pHandle, hRecNumber, hDiagIdentifier, pDiagInfo, cbLen, pcbLen);
    }

    if(hHandleType == SQL_HANDLE_ENV)
        endApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        endApiMutex(NULL, pHandle);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetDiagField and SQLGetDiagFieldW.
//
SQLRETURN  SQL_API RsError::RS_SQLGetDiagField(SQLSMALLINT HandleType,
                                        SQLHANDLE Handle,
                                        SQLSMALLINT hRecNumber, 
                                        SQLSMALLINT hDiagIdentifier,
                                        SQLPOINTER  pDiagInfo, 
                                        SQLSMALLINT cbLen,
                                        SQLSMALLINT *pcbLen,
                                        int *piRetType)
{
    SQLRETURN rc = SQL_SUCCESS;
    size_t     cbRetLen = -1;
    int         iRetType = SQL_C_CHAR;
    SQLHENV phenv = NULL;
    SQLHDBC phdbc = NULL; 
    SQLHSTMT phstmt = NULL;
    SQLHDESC phdesc = NULL;
    RS_CONN_INFO *pConn = NULL;
    RS_STMT_INFO *pStmt = NULL;
    RS_DESC_INFO *pDesc = NULL;
    int        iIsHeaderField;

    // Check handle type
    if(HandleType == SQL_HANDLE_ENV)
        phenv = Handle;
    else
    if(HandleType == SQL_HANDLE_DBC)
    {
        phdbc = Handle;
        pConn = (RS_CONN_INFO *)phdbc;
    }
    else
    if(HandleType == SQL_HANDLE_STMT)
    {
        phstmt = Handle;
        pStmt  = (RS_STMT_INFO *)phstmt;
        if(pStmt)
            pConn = pStmt->phdbc;
    }
    else
    if(HandleType == SQL_HANDLE_DESC)
    {
        phdesc = Handle;
        pDesc = (RS_DESC_INFO *)phdesc;
        if(pDesc)
            pConn = pDesc->phdbc;
    }
    else
    {
        rc = SQL_INVALID_HANDLE;
    }

    if(!phenv && !phdbc && !phstmt && !phdesc)
    {
        rc = (rc == SQL_SUCCESS) ? SQL_INVALID_HANDLE : rc;
    }

    if(rc == SQL_SUCCESS)
    {
        // Check for header field
        if(hDiagIdentifier == SQL_DIAG_CURSOR_ROW_COUNT
            || hDiagIdentifier == SQL_DIAG_DYNAMIC_FUNCTION
            || hDiagIdentifier == SQL_DIAG_DYNAMIC_FUNCTION_CODE
            || hDiagIdentifier == SQL_DIAG_RETURNCODE
            || hDiagIdentifier == SQL_DIAG_NUMBER
            || hDiagIdentifier == SQL_DIAG_ROW_COUNT)
            iIsHeaderField = TRUE;
        else
            iIsHeaderField = FALSE;

        // Header field ignores the record number
        if(!iIsHeaderField && hRecNumber <= 0)
        {
            rc = SQL_ERROR;
        }
    }

    if(rc == SQL_SUCCESS)
    {
        switch(hDiagIdentifier)
        {
            //
            // Header fields.
            //
            case SQL_DIAG_CURSOR_ROW_COUNT:
            {
                iRetType = SQL_C_LONG;

                if(HandleType == SQL_HANDLE_STMT && pStmt)
                {
                    if(pDiagInfo)
                    {
                        RS_RESULT_INFO *pResult = pStmt->pResultHead;

                        if(pResult && (pResult->iNumberOfCols > 0))
                            *(SQLINTEGER *)pDiagInfo = pResult->iNumberOfRowsInMem;
                        else
                            *(SQLINTEGER *)pDiagInfo = 0;
                    }
                }
                else
                    rc = SQL_ERROR;

                break;
            }

            case SQL_DIAG_DYNAMIC_FUNCTION:
            case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
            case SQL_DIAG_RETURNCODE:
            {
                rc = SQL_NO_DATA;
                break;
            }

            case SQL_DIAG_NUMBER:
            {
                iRetType = SQL_C_LONG;

                if(pDiagInfo)
                {
                    RS_ERROR_INFO *pErrorList;
                    
                    *(SQLINTEGER *)pDiagInfo = 0;

                    if(phstmt != NULL)
                    {
                        RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

                        pErrorList = pStmt->pErrorList;
                    }
                    else
                    if(phdbc != NULL)
                    {
                        RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

                        pErrorList = pConn->pErrorList;
                    }
                    else
                    if(phenv != NULL)
                    {
                        RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;

                        pErrorList = pEnv->pErrorList;
                    }
                    else
                    if(phdesc != NULL)
                    {
                        RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;

                        pErrorList = pDesc->pErrorList;
                    }
                    else
                        pErrorList = NULL;

                    if(pErrorList)
                        *(SQLINTEGER *)pDiagInfo = getTotalErrors(pErrorList);
                } 

                break;
            }

            case SQL_DIAG_ROW_COUNT:
            {
                iRetType = SQL_C_LONG;

                if(HandleType == SQL_HANDLE_STMT && pStmt)
                {
                    if(pDiagInfo)
                    {
                        RS_RESULT_INFO *pResult = pStmt->pResultHead;

                        if(pResult && (pResult->iNumberOfCols == 0))
                            *(SQLINTEGER *)pDiagInfo = pResult->lRowsUpdated;
                        else
                            *(SQLINTEGER *)pDiagInfo = 0;
                    }
                }
                else
                    rc = SQL_ERROR;

                break;
            }

            //
            // Record fields
            //
            case SQL_DIAG_CLASS_ORIGIN:
            case SQL_DIAG_SUBCLASS_ORIGIN:
            {
                char szSqlState[MAX_SQL_STATE_LEN];
                char *pVal = NULL;

                szSqlState[0] = '\0';
                RsError::RS_SQLError(phenv, phdbc, phstmt, phdesc, (SQLCHAR *)szSqlState, NULL , NULL, 0,
                                    NULL, hRecNumber, FALSE); 

                if(szSqlState[0] == 'I' && szSqlState[1] == 'M')
                    pVal = "ODBC 3.51";
                else
                    pVal = "ISO 9075";
            
                cbRetLen = (pVal) ? strlen(pVal) : 0;

                if(pDiagInfo &&(cbLen > (short) cbRetLen))
                {
                    if(pVal)
                        rs_strncpy((char *)pDiagInfo, pVal,cbLen);
                    else
                        *((char *) pDiagInfo) = '\0';
                }
                else
                {
                    if(pDiagInfo && cbLen > 0)
                    {
                        if(pVal)
                        {
                            strncpy((char *)pDiagInfo, pVal, cbLen - 1);
                            ((char *)pDiagInfo)[cbLen - 1] = '\0';
                        }
                        else
                            *((char *) pDiagInfo) = '\0';
                    }

                    rc = SQL_SUCCESS_WITH_INFO;
                }


                break;
            }

            case SQL_DIAG_CONNECTION_NAME:
            {
                cbRetLen = 0;

                if(pDiagInfo &&(cbLen > (short) cbRetLen))
                {
                    *((char *) pDiagInfo) = '\0';
                }
                else
                {
                    if(pDiagInfo && cbLen > 0)
                        *((char *) pDiagInfo) = '\0';

                    rc = SQL_SUCCESS_WITH_INFO;
                }

                break;
            }

            case SQL_DIAG_SERVER_NAME:
            {
                char *pVal = NULL;

                if(pConn)
                    pVal = (char *)((pConn->pConnectProps) ? pConn->pConnectProps->szDSN : "");

                cbRetLen = (pVal) ? strlen(pVal) : 0;

                if(pDiagInfo &&(cbLen > (short) cbRetLen))
                {
                    if(pVal)
                        rs_strncpy((char *)pDiagInfo, pVal,cbLen);
                    else
                        *((char *) pDiagInfo) = '\0';
                }
                else
                {
                    if(pDiagInfo && cbLen > 0)
                    {
                        if(pVal)
                        {
                            strncpy((char *)pDiagInfo, pVal, cbLen - 1);
                            ((char *)pDiagInfo)[cbLen - 1] = '\0';
                        }
                        else
                            *((char *) pDiagInfo) = '\0';
                    }

                    rc = SQL_SUCCESS_WITH_INFO;
                }

                break;
            }

            case SQL_DIAG_MESSAGE_TEXT:
            {
                cbRetLen = 0;
                rc = RsError::RS_SQLError(phenv, phdbc, phstmt, phdesc, NULL, NULL, (SQLCHAR *)pDiagInfo, cbLen,
                                    (SQLSMALLINT *)((pcbLen) ? pcbLen : (SQLSMALLINT *)(void *)(&cbRetLen)), hRecNumber, FALSE);

                cbRetLen = (pcbLen) ? *pcbLen : cbRetLen;

                break;
            }

            case SQL_DIAG_NATIVE:
            {
                iRetType = SQL_C_LONG;

                rc = RsError::RS_SQLError(phenv, phdbc, phstmt, phdesc, NULL, (SQLINTEGER *) pDiagInfo, NULL, 0,
                                    NULL, hRecNumber, FALSE); 

                break;
            }

            case SQL_DIAG_SQLSTATE:
            {
                cbRetLen = MAX_SQL_STATE_LEN - 1;

                rc = RsError::RS_SQLError(phenv, phdbc, phstmt, phdesc, (SQLCHAR *)pDiagInfo, NULL , NULL, 0,
                                    NULL, hRecNumber, FALSE); 

                if (rc == SQL_SUCCESS_WITH_INFO)  
                    rc = SQL_SUCCESS;

                break;
            }


            case SQL_DIAG_COLUMN_NUMBER:
            {
                iRetType = SQL_C_LONG;

                if(HandleType == SQL_HANDLE_STMT)
                {
                    if(pDiagInfo)
                        *(SQLINTEGER *)pDiagInfo = SQL_COLUMN_NUMBER_UNKNOWN;
                }
                else
                    rc = SQL_ERROR;

                break;
            }

            case SQL_DIAG_ROW_NUMBER:
            {
                iRetType = SQL_C_LONG;

                if(HandleType == SQL_HANDLE_STMT)
                {
                    if(pDiagInfo)
                        *(SQLINTEGER *)pDiagInfo = SQL_ROW_NUMBER_UNKNOWN;
                }
                else
                    rc = SQL_ERROR;

                break;
            }

            default:
            {
                rc = SQL_NO_DATA;
                break;
            }
        } // Diag Iden type switch

        // Check return type
        switch(iRetType)
        {
            case SQL_C_CHAR:
            {
                if (cbRetLen >= 0)
                {
                    if ((short)cbRetLen >= cbLen)
                    {
                        if (rc == SQL_SUCCESS)
                            rc = SQL_SUCCESS_WITH_INFO;

                        if (cbLen > 0)
                            ((char *) pDiagInfo)[cbLen - 1] = '\0';
                    }

                    if (pcbLen)  
                        *pcbLen = (SQLSMALLINT) cbRetLen;
                }
                break;
            }

            case SQL_C_LONG:
            {
                if(rc == SQL_SUCCESS_WITH_INFO)
                    rc = SQL_SUCCESS;

                if (pcbLen)  
                    *pcbLen = sizeof(SQLINTEGER);

                break;
            }
        }
    }

    if(piRetType)
        *piRetType = iRetType;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetDiagField.
//
SQLRETURN SQL_API SQLGetDiagFieldW(SQLSMALLINT     hHandleType,
                                    SQLHANDLE       pHandle,
                                    SQLSMALLINT     hRecNumber,
                                    SQLSMALLINT     hDiagIdentifier,
                                    SQLPOINTER      pwDiagInfo,
                                    SQLSMALLINT     cbLen,
                                    SQLSMALLINT     *pcbLen)
{
    SQLRETURN rc;
    SQLSMALLINT cchLen = cbLen/sizeof(WCHAR);
    int iIsCharDiagIdentifier = isCharDiagIdentifier(hDiagIdentifier);
    int iExtraBytes = (hDiagIdentifier == SQL_DIAG_SQLSTATE && (cbLen < (6 * sizeof(WCHAR)))) ? (6 * sizeof(WCHAR)) - cbLen : 0;
    SQLPOINTER *pDiagInfo = (SQLPOINTER *)((iIsCharDiagIdentifier && cbLen >= 0) ? rs_malloc(cchLen + iExtraBytes + 1) : NULL);
    int iRetType;

    if(hHandleType == SQL_HANDLE_ENV)
        beginApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        beginApiMutex(NULL, pHandle);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagFieldW(FUNC_CALL, 0, hHandleType, pHandle, hRecNumber, hDiagIdentifier, pwDiagInfo, cbLen, pcbLen);
    }

    rc = RsError::RS_SQLGetDiagField(hHandleType, pHandle, hRecNumber, hDiagIdentifier, (iIsCharDiagIdentifier) ? pDiagInfo : pwDiagInfo,
                            (iIsCharDiagIdentifier) ? cchLen : cbLen, pcbLen, &iRetType);

    if(SQL_SUCCEEDED(rc))
    {
        if(iRetType == SQL_C_CHAR && iIsCharDiagIdentifier)
        {
            // Convert to unicode
            if(pwDiagInfo)
                utf8_to_wchar((char *)pDiagInfo, cchLen, (WCHAR *)pwDiagInfo, cchLen);

            if(pcbLen)
                *pcbLen = *pcbLen * sizeof(WCHAR);
        }
    }

    pDiagInfo = (SQLPOINTER *)rs_free(pDiagInfo);

    if(IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        TraceSQLGetDiagFieldW(FUNC_RETURN, rc, hHandleType, pHandle, hRecNumber, hDiagIdentifier, pwDiagInfo, cbLen, pcbLen);
    }

    if(hHandleType == SQL_HANDLE_ENV)
        endApiMutex(pHandle, NULL);
    else
    if(hHandleType == SQL_HANDLE_DBC)
        endApiMutex(NULL, pHandle);

    return rc;
}


