/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsoptions.h"
#include "rsdesc.h"
#include "rstransaction.h"
#include <map>
#include <set>
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLSetConnectOption has been replaced by SQLSetConnectAttr. 
//
SQLRETURN  SQL_API SQLSetConnectOption(SQLHDBC phdbc,
                                       SQLUSMALLINT hOption, 
                                       SQLULEN pValue)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectOption(FUNC_CALL, 0, phdbc, hOption, pValue);

    rc = RsOptions::RS_SQLSetConnectOption(phdbc, hOption, pValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectOption(FUNC_RETURN, rc, phdbc, hOption, pValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLSetConnectOption and SQLSetConnectOptionW.
//
SQLRETURN  SQL_API RsOptions::RS_SQLSetConnectOption(SQLHDBC phdbc,
                                            SQLUSMALLINT hOption, 
                                            SQLULEN Value)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLINTEGER    iAttribute;

    switch (hOption) 
    {
        case SQL_ACCESS_MODE: iAttribute = SQL_ATTR_ACCESS_MODE; break;
        case SQL_AUTOCOMMIT: iAttribute = SQL_ATTR_AUTOCOMMIT; break;
        case SQL_CURRENT_QUALIFIER: iAttribute = SQL_ATTR_CURRENT_CATALOG; break;
        case SQL_LOGIN_TIMEOUT: iAttribute = SQL_ATTR_LOGIN_TIMEOUT; break;
        case SQL_ODBC_CURSORS: iAttribute = SQL_ATTR_ODBC_CURSORS; break;
        case SQL_PACKET_SIZE: iAttribute = SQL_ATTR_PACKET_SIZE; break;
        case SQL_QUERY_TIMEOUT: iAttribute = SQL_ATTR_QUERY_TIMEOUT; break;
        case SQL_QUIET_MODE: iAttribute = SQL_ATTR_QUIET_MODE; break;
        case SQL_OPT_TRACE:  iAttribute = SQL_ATTR_TRACE; break;
        case SQL_OPT_TRACEFILE:  iAttribute = SQL_ATTR_TRACEFILE; break;
        case SQL_TRANSLATE_DLL: iAttribute = SQL_ATTR_TRANSLATE_LIB; break;
        case SQL_TRANSLATE_OPTION: iAttribute = SQL_ATTR_TRANSLATE_OPTION;  break;
        case SQL_TXN_ISOLATION: iAttribute = SQL_ATTR_TXN_ISOLATION; break;
        default: rc = checkHdbcHandleAndAddError(phdbc,SQL_ERROR,"HYC00", "Optional feature not implemented::RS_SQLSetConnectOption"); goto error;
    } // Switch

     rc = RsOptions::RS_SQLSetConnectAttr(phdbc, iAttribute, (SQLPOINTER) Value, SQL_NTS);

error:

     return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLSetConnectOption.
//
SQLRETURN SQL_API SQLSetConnectOptionW(SQLHDBC            phdbc,
                                        SQLUSMALLINT      hOption,
                                        SQLULEN           pwValue)
{
    SQLRETURN rc = SQL_SUCCESS;
    char *szValue = NULL;
    size_t len;

    // Check valid HDBC
    if(!VALID_HDBC(phdbc))
    {
        return SQL_INVALID_HANDLE;
    }

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectOptionW(FUNC_CALL, 0, phdbc, hOption, pwValue);

    if(hOption == SQL_CURRENT_QUALIFIER
        || hOption == SQL_OPT_TRACEFILE
        || hOption == SQL_TRANSLATE_DLL)
    {
        len = sqlwchar_to_utf8_alloc((SQLWCHAR *)pwValue, SQL_NTS, &szValue);
        if(!szValue)
        {
            RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
            addError(&pConn->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            return SQL_ERROR;
        }
    }

    rc = RsOptions::RS_SQLSetConnectOption(phdbc, hOption, (szValue) ? (SQLULEN) szValue : pwValue);

    szValue = (char *)rs_free(szValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectOptionW(FUNC_RETURN, rc, phdbc, hOption, pwValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetConnectOption.
//
SQLRETURN SQL_API SQLGetConnectOptionW(SQLHDBC   phdbc,
                                        SQLUSMALLINT    hOption,
                                        SQLPOINTER      pwValue)
{
    SQLRETURN rc = SQL_SUCCESS;
    char *szValue = NULL;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectOptionW(FUNC_CALL, 0, phdbc, hOption, pwValue);

    if(hOption == SQL_CURRENT_QUALIFIER
        || hOption == SQL_OPT_TRACEFILE
        || hOption == SQL_TRANSLATE_DLL)
    {
        if(pwValue != NULL)
        {
            szValue = (char *)rs_calloc(sizeof(char), MAX_TEMP_BUF_LEN);
        }
        else
            szValue = NULL;
    }

    rc = RsOptions::RS_SQLGetConnectOption(phdbc, hOption, (szValue) ? (SQLPOINTER) szValue : pwValue);

    if(SQL_SUCCEEDED(rc))
    {
        if(szValue)
            utf8_to_sqlwchar_str(szValue, SQL_NTS, (SQLWCHAR *)pwValue, strlen(szValue) + 1);
    }

    szValue = (char *)rs_free(szValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectOptionW(FUNC_RETURN, rc, phdbc, hOption, pwValue);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.x function SQLGetConnectOption has been replaced by SQLGetConnectAttr.
//
SQLRETURN  SQL_API SQLGetConnectOption(SQLHDBC phdbc,
                                        SQLUSMALLINT hOption, 
                                        SQLPOINTER pValue)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectOption(FUNC_CALL, 0, phdbc, hOption, pValue);

    rc = RsOptions::RS_SQLGetConnectOption(phdbc, hOption, pValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectOption(FUNC_RETURN, rc, phdbc, hOption, pValue);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetConnectOption and SQLGetConnectOptionW.
//
SQLRETURN  SQL_API RsOptions::RS_SQLGetConnectOption(SQLHDBC phdbc,
                                        SQLUSMALLINT hOption, 
                                        SQLPOINTER pValue)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLINTEGER    iAttribute;

    switch (hOption) 
    {
        case SQL_ACCESS_MODE: iAttribute = SQL_ATTR_ACCESS_MODE; break;
        case SQL_AUTOCOMMIT: iAttribute = SQL_ATTR_AUTOCOMMIT; break;
        case SQL_CURRENT_QUALIFIER: iAttribute = SQL_ATTR_CURRENT_CATALOG; break;
        case SQL_LOGIN_TIMEOUT: iAttribute = SQL_ATTR_LOGIN_TIMEOUT; break;
        case SQL_ODBC_CURSORS: iAttribute = SQL_ATTR_ODBC_CURSORS; break;
        case SQL_PACKET_SIZE: iAttribute = SQL_ATTR_PACKET_SIZE; break;
        case SQL_QUERY_TIMEOUT: iAttribute = SQL_ATTR_QUERY_TIMEOUT; break;
        case SQL_QUIET_MODE: iAttribute = SQL_ATTR_QUIET_MODE; break;
        case SQL_OPT_TRACE:  iAttribute = SQL_ATTR_TRACE; break;
        case SQL_OPT_TRACEFILE:  iAttribute = SQL_ATTR_TRACEFILE; break;
        case SQL_TRANSLATE_DLL: iAttribute = SQL_ATTR_TRANSLATE_LIB; break;
        case SQL_TRANSLATE_OPTION: iAttribute = SQL_ATTR_TRANSLATE_OPTION;  break;
        case SQL_TXN_ISOLATION: iAttribute = SQL_ATTR_TXN_ISOLATION; break;
        case SQL_ATTR_CONNECTION_DEAD: iAttribute = SQL_ATTR_CONNECTION_DEAD; break;
        default: rc = checkHdbcHandleAndAddError(phdbc,SQL_ERROR,"HYC00", "Optional feature not implemented::RS_SQLGetConnectOption"); goto error;
    } // Switch

     rc = RsOptions::RS_SQLGetConnectAttr(phdbc, iAttribute, pValue, SQL_MAX_OPTION_STRING_LENGTH, NULL);

error:

     return rc;
}

/*====================================================================================================================================================*/

/*
Search in supported attributes If the need be, also search in mappings
between supported attributes Verify that you find something (else error
out), and then send it to the main implementation(RS_SQLGetStmtAttr). Note:
If nothing found in the RS_SQLGetStmtAttr, it will fail in the switch
case.then you'd better go back to your supported list and fix things there
in order to error out early.
*/

SQLRETURN getSupportedAttribute(SQLINTEGER &iAttribute, SQLUSMALLINT hOption) {
    // The following sets are as per UnixOdbc's sql.h and sqlext.h. 
    // They should be consistent with the MS version.

    static const std::map<SQLINTEGER, SQLINTEGER> supportedAttributeMappings = {
        {SQL_ROWSET_SIZE, SQL_ATTR_ROW_ARRAY_SIZE},
    };

    static const std::set<SQLINTEGER> supportedAttributes = {
        SQL_ASYNC_ENABLE, // sqlext 14
        SQL_CONCURRENCY,  // sqlext 7
        SQL_CURSOR_TYPE,  // sqlext 6
        SQL_KEYSET_SIZE,  // sqlext 8
        /* statement attributes for ODBC 3.0 */
        SQL_ATTR_APP_ROW_DESC,          // sql 10010
        SQL_ATTR_APP_PARAM_DESC,        // sql 10011
        SQL_ATTR_IMP_ROW_DESC,          // sql, get 10012
        SQL_ATTR_IMP_PARAM_DESC,        // sql, get 10013
        SQL_ATTR_CURSOR_SCROLLABLE,     // sql, get (-1)
        SQL_ATTR_CURSOR_SENSITIVITY,    // sql, get (-2)
        SQL_ATTR_ASYNC_ENABLE,          // SQL_ASYNC_ENABLE 4
        SQL_ATTR_CONCURRENCY,           // SQL_CONCURRENCY 7
        SQL_ATTR_CURSOR_TYPE,           // SQL_CURSOR_TYPE 6
        SQL_ATTR_KEYSET_SIZE,           // SQL_KEYSET_SIZE 8
        SQL_ATTR_MAX_LENGTH,            // SQL_MAX_LENGTH 3
        SQL_ATTR_MAX_ROWS,              // SQL_MAX_ROWS 1
        SQL_ATTR_NOSCAN,                // SQL_NOSCAN 2
        SQL_ATTR_QUERY_TIMEOUT,         // SQL_QUERY_TIMEOUT 0
        SQL_ATTR_RETRIEVE_DATA,         // SQL_RETRIEVE_DATA 11
        SQL_ATTR_ROW_ARRAY_SIZE,        // SQL_ROWSET_SIZE 27, 9 (map)
        SQL_ATTR_ROW_BIND_TYPE,         // SQL_BIND_TYPE 5
        SQL_ATTR_ROW_NUMBER,            // SQL_ROW_NUMBER, get 14
        SQL_ATTR_SIMULATE_CURSOR,       // SQL_SIMULATE_CURSOR 10
        SQL_ATTR_USE_BOOKMARKS,         // SQL_USE_BOOKMARKS 12
        SQL_ATTR_FETCH_BOOKMARK_PTR,    //  sqlext 16
        SQL_ATTR_PARAM_BIND_OFFSET_PTR, // sqlext 17
        SQL_ATTR_PARAM_BIND_TYPE,       // sqlext 18
        SQL_ATTR_PARAM_OPERATION_PTR,   // sqlext 19
        SQL_ATTR_PARAM_STATUS_PTR,      // sqlext 20
        SQL_ATTR_PARAMS_PROCESSED_PTR,  // sqlext 21
        SQL_ATTR_PARAMSET_SIZE,         // sqlext 22
        SQL_ATTR_ROW_BIND_OFFSET_PTR,   // sqlext 23
        SQL_ATTR_ROW_OPERATION_PTR,     // sqlext 24
        SQL_ATTR_ROW_STATUS_PTR,        // sqlext 25
        SQL_ATTR_ROWS_FETCHED_PTR       // sqlext 26
    };

    auto it = supportedAttributes.find(hOption);
    if (it != supportedAttributes.end()) {
        iAttribute = *it;
    } else {
        auto itm = supportedAttributeMappings.find(hOption);
        if (itm != supportedAttributeMappings.end()) {
            iAttribute = itm->second;
        } else {
            return SQL_ERROR;
        }
    }
    return SQL_SUCCESS;
}

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLSetStmtOption has been replaced by SQLSetStmtAttr.
//
SQLRETURN  SQL_API SQLSetStmtOption(SQLHSTMT phstmt,
                                    SQLUSMALLINT hOption, 
                                    SQLULEN Value)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetStmtOption(FUNC_CALL, 0, phstmt, hOption, Value);

    rc = RsOptions::RS_SQLSetStmtOption(phstmt, hOption, Value);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetStmtOption(FUNC_RETURN, rc, phstmt, hOption, Value);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLSetStmtOption and SQLSetScrollOptions.
//
SQLRETURN  SQL_API RsOptions::RS_SQLSetStmtOption(SQLHSTMT phstmt,
                                        SQLUSMALLINT hOption, 
                                        SQLULEN Value)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLINTEGER    iAttribute;

    if (SQL_ERROR == getSupportedAttribute(iAttribute, hOption)) {
        rc = checkHstmtHandleAndAddError(
            phstmt, SQL_ERROR, "HYC00",
            "Optional feature not implemented:RS_SQLSetStmtOption");
        goto error;
    }
    rc = RsOptions::RS_SQLSetStmtAttr(phstmt, iAttribute, (SQLPOINTER) Value, SQL_NTS);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLGetStmtOption has been replaced by SQLGetStmtAttr.
//
SQLRETURN  SQL_API SQLGetStmtOption(SQLHSTMT phstmt,
                                    SQLUSMALLINT hOption, 
                                    SQLPOINTER pValue)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLINTEGER    iAttribute;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetStmtOption(FUNC_CALL, 0, phstmt, hOption, pValue);

    if (SQL_ERROR == getSupportedAttribute(iAttribute, hOption)) {
        rc = checkHstmtHandleAndAddError(
            phstmt, SQL_ERROR, "HYC00",
            "Optional feature not implemented:SQLGetStmtOption");
        goto error;
    }

    rc = RsOptions::RS_SQLGetStmtAttr(phstmt, iAttribute, pValue, SQL_MAX_OPTION_STRING_LENGTH, NULL);

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetStmtOption(FUNC_RETURN, rc, phstmt, hOption, pValue);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// In ODBC 3.x, the ODBC 2.0 function SQLSetScrollOptions has been replaced by calls to SQLGetInfo and SQLSetStmtAttr.
//
SQLRETURN SQL_API SQLSetScrollOptions(SQLHSTMT phstmt,
                                        SQLUSMALLINT       hConcurrency,
                                        SQLLEN             iKeysetSize,
                                        SQLUSMALLINT       hRowsetSize)
{
    SQLRETURN rc = SQL_SUCCESS;
    int iCursorType;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetScrollOptions(FUNC_CALL, 0, phstmt, hConcurrency, iKeysetSize, hRowsetSize);

    if(iKeysetSize > hRowsetSize)
    {
        iCursorType = SQL_CURSOR_KEYSET_DRIVEN;
    }
    else
    {
        switch(iKeysetSize)
        {
            case SQL_SCROLL_FORWARD_ONLY: iCursorType = SQL_CURSOR_FORWARD_ONLY; break;
            case SQL_SCROLL_STATIC: iCursorType = SQL_CURSOR_STATIC; break;
            case SQL_SCROLL_KEYSET_DRIVEN: iCursorType = SQL_CURSOR_KEYSET_DRIVEN; break;
            case SQL_SCROLL_DYNAMIC: iCursorType = SQL_CURSOR_DYNAMIC; break;
            default: rc = checkHstmtHandleAndAddError(phstmt,SQL_ERROR,"HYC00", "Optional feature not implemented:SQLSetScrollOptions"); goto error;
        }
    }

    rc = RsOptions::RS_SQLSetStmtOption(phstmt, SQL_CONCURRENCY, hConcurrency);
    if(rc == SQL_ERROR)
        goto error;

    rc = RsOptions::RS_SQLSetStmtOption(phstmt, SQL_CURSOR_TYPE, iCursorType);
    if(rc == SQL_ERROR)
        goto error;

    if(iKeysetSize > 0)
    {
        rc = RsOptions::RS_SQLSetStmtOption(phstmt, SQL_KEYSET_SIZE, iKeysetSize);
        if(rc == SQL_ERROR)
            goto error;
    }

    rc = RsOptions::RS_SQLSetStmtOption(phstmt, SQL_ROWSET_SIZE, hRowsetSize);
    if(rc == SQL_ERROR)
        goto error;

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetScrollOptions(FUNC_RETURN, rc, phstmt, hConcurrency, iKeysetSize, hRowsetSize);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetConnectAttr returns the current setting of a connection attribute.
//
SQLRETURN  SQL_API SQLGetConnectAttr(SQLHDBC phdbc,
                                       SQLINTEGER    iAttribute, 
                                       SQLPOINTER    pValue,
                                       SQLINTEGER    cbLen, 
                                       SQLINTEGER  *pcbLen)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectAttr(FUNC_CALL, 0, phdbc, iAttribute, pValue, cbLen, pcbLen);

    rc = RsOptions::RS_SQLGetConnectAttr(phdbc, iAttribute, pValue, cbLen, pcbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectAttr(FUNC_RETURN, rc, phdbc, iAttribute, pValue, cbLen, pcbLen);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetConnectAttr and SQLGetConnectAttrW.
//
SQLRETURN  SQL_API RsOptions::RS_SQLGetConnectAttr(SQLHDBC phdbc,
                                       SQLINTEGER    iAttribute, 
                                       SQLPOINTER    pValue,
                                       SQLINTEGER    cbLen, 
                                       SQLINTEGER  *pcbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    int *piVal = (int *)pValue;
    void **ppVal = (void **)pValue;
    RS_CONN_ATTR_INFO *pConnAttr;
    RS_CONNECT_PROPS_INFO *pConnectProps;

    // Check valid HDBC
    if(!VALID_HDBC(phdbc))
    {
        return SQL_INVALID_HANDLE;
    }
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    // String attrs: ValuePtr may be NULL only if StringLengthPtr is provided.
    if (RsOptions::isStrConnectAttr(iAttribute)) {
        if (!pValue && !pcbLen) {
            // For string attributes: pValue can be NULL only when querying
            // buffer size, which requires a valid StringLengthPtr
            // https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetconnectattr-function
            addError(&pConn->pErrorList, "HY009", "Invalid use of null pointer",
                     0, NULL);
            RS_LOG_ERROR(
                "CONNATTR",
                "String attr %d: ValuePtr and StringLengthPtr both NULL",
                iAttribute);
            return SQL_ERROR;
        }
        // Validate BufferLength only when we will actually write into ValuePtr.
        if (pValue && cbLen < 0) {
            addError(&pConn->pErrorList, "HY090",
                     "Invalid string or buffer length", 0, NULL);
            RS_LOG_ERROR("CONNATTR",
                         "Invalid buffer length for string attr %d: %d",
                         iAttribute, cbLen);
            return SQL_ERROR;
        }
    } else {
        // Non-string attrs: ValuePtr must not be NULL; 
        // BufferLength is ignored by spec.
        if (!pValue) {
            addError(&pConn->pErrorList, "HY009", "Invalid use of null pointer",
                     0, NULL);
            RS_LOG_ERROR("CONNATTR", "Non-string attr %d: NULL ValuePtr",
                         iAttribute);
            return SQL_ERROR;
        }
        // Do NOT validate cbLen here (ignored).
    }

    pConnAttr = pConn->pConnAttr;
    pConnectProps = pConn->pConnectProps;

    // Attribute operation
    switch(iAttribute)
    {
        case SQL_ATTR_ACCESS_MODE:
        {
            *piVal = pConnAttr->iAccessMode;
            break;
        }

        case SQL_ATTR_ASYNC_ENABLE:
        {
            *piVal = pConnAttr->iAsyncEnable;
            break;
        }

        case SQL_ATTR_AUTO_IPD:
        {
            *piVal = pConnAttr->iAutoIPD;
            break;
        }

        case SQL_ATTR_AUTOCOMMIT:
        {
            *piVal = pConnAttr->iAutoCommit;
            break;
        }

        case SQL_ATTR_CONNECTION_DEAD:
        {
            *piVal = pConn->isConnectionDead() ? SQL_CD_TRUE : SQL_CD_FALSE;
            break;
        }
        
        case SQL_ATTR_CONNECTION_TIMEOUT:
        {
            *piVal = pConnAttr->iConnectionTimeout;
            break;
        }

        case SQL_ATTR_CURRENT_CATALOG:
        {
            rc = copyStrDataLargeLen((pConnAttr->pCurrentCatalog) ? pConnAttr->pCurrentCatalog : pConn->pConnectProps->szDatabase, SQL_NTS,
                                    (char *)pValue, cbLen, pcbLen);

            break;
        }

        case SQL_ATTR_LOGIN_TIMEOUT:
        {
            *piVal = pConnAttr->iLoginTimeout;
            break;
        }

        case SQL_ATTR_METADATA_ID:
        {
            *piVal = pConnAttr->iMetaDataId;
            break;
        }

        case SQL_ATTR_ODBC_CURSORS:
        {
            *piVal = pConnAttr->iOdbcCursors;
            break;
        }

        case SQL_ATTR_PACKET_SIZE:
        {
            *piVal = pConnAttr->iPacketSize;
            break;
        }

        case SQL_ATTR_QUIET_MODE:
        {
            *ppVal = pConnAttr->hQuietMode;
            break;
        }

        case SQL_ATTR_TRACE:
        {
            *piVal = pConnAttr->iTrace;
            break;
        }

        case SQL_ATTR_TRACEFILE:
        {
            rc = copyStrDataLargeLen(pConnAttr->pTraceFile, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_ATTR_TRANSLATE_LIB:
        {
            rc = copyStrDataLargeLen(pConnAttr->pTranslateLib, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_ATTR_TRANSLATE_OPTION:
        {
            *piVal = pConnAttr->iTranslateOption;
            break;
        }

        case SQL_ATTR_TXN_ISOLATION:
        {
            *piVal = pConnAttr->iTxnIsolation;
            break;
        }

        case SQL_ATTR_IGNORE_UNICODE_FUNCTIONS: // DD DM Linux
        {
            *piVal = 1;
            break;
        }
        case SQL_ATTR_DRIVER_UNICODE_TYPE:
        case SQL_ATTR_APP_WCHAR_TYPE:   // DD DM specific attribute.
        case SQL_ATTR_APP_UNICODE_TYPE: // iODBC specific attribute for UTF-32
                                        // apps
        {
            *piVal = get_app_unicode_type();
            if (*piVal != SQL_DD_CP_UTF16 && *piVal != SQL_DD_CP_UTF32) {
                RS_LOG_ERROR("CONNATTR", "Invalid app unicode type:%d", *piVal);
                rc = SQL_ERROR;
                addError(&pConn->pErrorList, "HY000", "Invalid unicode state.", 0, NULL);
                return rc;
            }
            break;
        }

        case SQL_ATTR_QUERY_TIMEOUT:
        {
            *piVal = pConnectProps->iQueryTimeout;
            break;
        }

        default:
        {
            rc = SQL_ERROR;
            addError(&pConn->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLGetConnectAttr", 0, NULL);
            return rc;
        }

    } // Switch

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetStmtAttr returns the current setting of a statement attribute.
//
SQLRETURN  SQL_API SQLGetStmtAttr(SQLHSTMT        phstmt,
                                   SQLINTEGER    iAttribute, 
                                   SQLPOINTER    pValue,
                                   SQLINTEGER    cbLen, 
                                   SQLINTEGER  *pcbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetStmtAttr(FUNC_CALL, 0, phstmt, iAttribute, pValue, cbLen, pcbLen);

    rc = RsOptions::RS_SQLGetStmtAttr(phstmt, iAttribute, pValue, cbLen, pcbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetStmtAttr(FUNC_RETURN, rc, phstmt, iAttribute, pValue, cbLen, pcbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetStmtAttr and SQLGetStmtAttrW.
//
SQLRETURN  SQL_API RsOptions::RS_SQLGetStmtAttr(SQLHSTMT        phstmt,
                                        SQLINTEGER    iAttribute, 
                                        SQLPOINTER    pValue,
                                        SQLINTEGER    cbLen, 
                                        SQLINTEGER  *pcbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    int *piVal = (int *)pValue;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_STMT_ATTR_INFO *pStmtAttr;

    // Validate the handle
    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!pValue)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Output buffer is NULL", 0, NULL);
        goto error;
    }

    pStmtAttr = pStmt->pStmtAttr;

    switch(iAttribute)
    {
        case SQL_ATTR_APP_PARAM_DESC:
        {
            *(void **) pValue = pStmtAttr->pAPD;
            if(pcbLen)
                *pcbLen = sizeof(void *);
            break;
        }

        case SQL_ATTR_APP_ROW_DESC:
        {
            *(void **) pValue = pStmtAttr->pARD;
            if(pcbLen)
                *pcbLen = sizeof(void *);
            break;
        }

        case SQL_ATTR_ASYNC_ENABLE:
        {
            *piVal = pStmtAttr->iAsyncEnable;
            break;
        }

        case SQL_ATTR_CONCURRENCY:
        {
            *piVal = pStmtAttr->iConcurrency;
            break;
        }

        case SQL_ATTR_CURSOR_SCROLLABLE:
        {
            *piVal = pStmtAttr->iCursorScrollable;
            break;
        }

        case SQL_ATTR_CURSOR_SENSITIVITY:
        {
            *piVal = pStmtAttr->iCursorSensitivity;
            break;
        }

        case SQL_ATTR_CURSOR_TYPE:
        {
            *piVal = pStmtAttr->iCursorType;
            break;
        }

        case SQL_ATTR_FETCH_BOOKMARK_PTR:
        {
            *(void **)pValue = pStmtAttr->pFetchBookmarkPtr;
            if(pcbLen)
                *pcbLen = sizeof(void *);
            break;
        }

        case SQL_ATTR_IMP_PARAM_DESC:
        {
            *(void **)pValue = pStmt->pIPD;
            if(pcbLen)
                *pcbLen = sizeof(void *);
            break;
        }

        case SQL_ATTR_IMP_ROW_DESC:
        {
            *(void **)pValue = pStmt->pIRD;
            if(pcbLen)
                *pcbLen = sizeof(void *);

            break;
        }

        case SQL_ATTR_KEYSET_SIZE:
        {
            *piVal = pStmtAttr->iKeysetSize;
            break;
        }

        case SQL_ATTR_MAX_LENGTH:
        {
            *piVal = pStmtAttr->iMaxLength;
            break;
        }

        case SQL_ATTR_MAX_ROWS:
        {
            *piVal = pStmtAttr->iMaxRows;
            break;
        }

        case SQL_ATTR_METADATA_ID:
        {
            *piVal = pStmtAttr->iMetaDataId;
            break;
        }

        case SQL_ATTR_NOSCAN:
        {
            *piVal = pStmtAttr->iNoScan;
            break;
        }

        case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pAPD, 0, SQL_DESC_BIND_OFFSET_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_PARAM_BIND_TYPE:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pAPD, 0, SQL_DESC_BIND_TYPE, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_PARAM_OPERATION_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pAPD, 0, SQL_DESC_ARRAY_STATUS_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_PARAM_STATUS_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmt->pIPD, 0, SQL_DESC_ARRAY_STATUS_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_PARAMS_PROCESSED_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmt->pIPD, 0, SQL_DESC_ROWS_PROCESSED_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_PARAMSET_SIZE:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmt->pAPD, 0, SQL_DESC_ARRAY_SIZE, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_QUERY_TIMEOUT:
        {
            *piVal = pStmtAttr->iQueryTimeout;
            break;
        }

        case SQL_ATTR_RETRIEVE_DATA:
        {
            *piVal = pStmtAttr->iRetrieveData;
            break;
        }

        case SQL_ATTR_ROW_ARRAY_SIZE:
        case SQL_ROWSET_SIZE:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pARD, 0, SQL_DESC_ARRAY_SIZE, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_ROW_BIND_OFFSET_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pARD, 0, SQL_DESC_BIND_OFFSET_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_ROW_BIND_TYPE:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pARD, 0, SQL_DESC_BIND_TYPE, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_ROW_NUMBER:
        {
            *piVal = pStmtAttr->iRowNumber;
            break;
        }

        case SQL_ATTR_ROW_OPERATION_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmtAttr->pARD, 0, SQL_DESC_ARRAY_STATUS_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_ROW_STATUS_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmt->pIRD, 0, SQL_DESC_ARRAY_STATUS_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_ROWS_FETCHED_PTR:
        {
            rc = RsDesc::RS_SQLGetDescField(pStmt->pIRD, 0, SQL_DESC_ROWS_PROCESSED_PTR, pValue, cbLen, pcbLen, TRUE);
            break;
        }

        case SQL_ATTR_SIMULATE_CURSOR:
        {
            *piVal = pStmtAttr->iSimulateCursor;
            break;
        }

        case SQL_ATTR_USE_BOOKMARKS:
        {
            *piVal = pStmtAttr->iUseBookmark;
            break;
        }

        case SQL_ATTR_ENABLE_AUTO_IPD:
        default:
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLGetStmtAttr", 0, NULL);
            goto error;
        }
    } // Switch

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSetConnectAttr sets attributes that govern aspects of connections.
//
SQLRETURN  SQL_API SQLSetConnectAttr(SQLHDBC phdbc,
                                       SQLINTEGER    iAttribute, 
                                       SQLPOINTER    pValue,
                                       SQLINTEGER    cbLen)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectAttr(FUNC_CALL, 0, phdbc, iAttribute, pValue, cbLen);

    rc = RsOptions::RS_SQLSetConnectAttr(phdbc, iAttribute, pValue, cbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectAttr(FUNC_RETURN, rc, phdbc, iAttribute, pValue, cbLen);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for  SQLSetConnectAttr and SQLSetConnectAttrW.
//
SQLRETURN  SQL_API RsOptions::RS_SQLSetConnectAttr(SQLHDBC phdbc,
                                           SQLINTEGER    iAttribute, 
                                           SQLPOINTER    pValue,
                                           SQLINTEGER    cbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_ATTR_INFO *pConnAttr;
    RS_CONNECT_PROPS_INFO *pConnectProps;

    int iVal = (int)(long)pValue;
    RS_CONN_INFO *pConn = nullptr;
    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }
    pConn = (RS_CONN_INFO *)phdbc;

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    pConnAttr = pConn->pConnAttr;
    pConnectProps = pConn->pConnectProps;

    switch(iAttribute)
    {
        case SQL_ATTR_ACCESS_MODE:
        {
            // Check valid values
            if(iVal != SQL_MODE_READ_ONLY && iVal != SQL_MODE_READ_WRITE)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pConnAttr->iAccessMode = iVal;

            if (pConn->iStatus == RS_OPEN_CONNECTION) {
                const std::string query = (iVal == SQL_MODE_READ_ONLY)
                                              ? "SET READONLY=1"
                                              : "SET READONLY=0";
                rc = onConnectExecute(pConn, (char *)query.c_str());
            }

            break;
        }

        case SQL_ATTR_ASYNC_ENABLE:
        {
            // Check valid values
            if(iVal != SQL_ASYNC_ENABLE_OFF && iVal != SQL_ASYNC_ENABLE_ON)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pConnAttr->iAsyncEnable = iVal;

            break;
        }

        case SQL_ATTR_AUTOCOMMIT:
        {
            // Check valid values
            if(iVal != SQL_TRUE && iVal != SQL_FALSE)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            if(pConn->isConnectionOpen())
            {
                if(!pConnAttr->iAutoCommit && iVal)
                {
                    // Commit the changes
                  RsTransaction::RS_SQLTransact(NULL, phdbc, SQL_COMMIT);
                }
            }

            pConnAttr->iAutoCommit = iVal;

            break;
        }

        case SQL_ATTR_CONNECTION_TIMEOUT:
        {
            // This is for login as well as any execution on the connection.
            // Right now we are using only for login timeout. 
            // When we support actual query timeout, we will use it.
            pConnAttr->iConnectionTimeout = iVal;
			if(pConnAttr->iLoginTimeout == 0)
				pConnAttr->iLoginTimeout = pConnAttr->iConnectionTimeout;

            break;
        }

        case SQL_ATTR_CURRENT_CATALOG:
        {
            if(pValue == NULL)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
                goto error;
            }

            if(pConn->isConnectionOpen())
            {
                rc = SQL_ERROR;
                addError(
                    &pConn->pErrorList, "HY011",
                    "Attribute cannot be set now: SQL_ATTR_CURRENT_CATALOG", 0,
                    NULL);
                goto error;
            }
            else
            {
                // Free previously allocated buffer, if any.
                pConnAttr->pCurrentCatalog = (char *)rs_free(pConnAttr->pCurrentCatalog);

                // Allocate and copy new value
                pConnAttr->pCurrentCatalog = rs_strdup((char *)pValue, cbLen);
            }

            break;
        }

        case SQL_ATTR_LOGIN_TIMEOUT:
        {
            pConnAttr->iLoginTimeout = iVal;
            break;
        }

        case SQL_ATTR_METADATA_ID:
        {
            if(iVal != SQL_TRUE && iVal != SQL_FALSE)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pConnAttr->iMetaDataId = iVal;

            break;
        }

        case SQL_ATTR_ODBC_CURSORS:
        {
            if(iVal != SQL_CUR_USE_IF_NEEDED 
                && iVal != SQL_CUR_USE_ODBC
                && iVal != SQL_CUR_USE_DRIVER)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pConnAttr->iOdbcCursors = iVal;

            break;
        }

        case SQL_ATTR_PACKET_SIZE:
        {
            if(pConn->isConnectionOpen())
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList, "HY011",
                         "Attribute cannot be set now: SQL_ATTR_PACKET_SIZE", 0,
                         NULL);
                goto error;
            }

            pConnAttr->iPacketSize = iVal;
            break;
        }

        case SQL_ATTR_QUIET_MODE:
        {
            pConnAttr->hQuietMode = pValue;
            break;
        }

        case SQL_ATTR_TRACE:
        {
            if(iVal != SQL_OPT_TRACE_OFF && iVal != SQL_OPT_TRACE_ON)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pConnAttr->iTrace = iVal;

            if(pConnAttr->iTrace == SQL_OPT_TRACE_ON)
                pConnectProps->iTraceLevel = LOG_LEVEL_DEBUG;
            else
            if(pConnAttr->iTrace == SQL_OPT_TRACE_OFF)
                pConnectProps->iTraceLevel = LOG_LEVEL_OFF;

            break;
        }

        case SQL_ATTR_TRACEFILE:
        {
            if(pValue == NULL)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
                goto error;
            }

            pConnAttr->pTraceFile = (char *)rs_free(pConnAttr->pTraceFile);
            pConnAttr->pTraceFile = rs_strdup((char *)pValue, cbLen);

            break;
        }

        case SQL_ATTR_TRANSLATE_LIB:
        {
            if(pValue == NULL)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
                goto error;
            }

            pConnAttr->pTranslateLib = (char *)rs_free(pConnAttr->pTranslateLib);
            pConnAttr->pTranslateLib = rs_strdup((char *)pValue, cbLen);

            break;
        }

        case SQL_ATTR_TRANSLATE_OPTION:
        {
            pConnAttr->iTranslateOption = iVal;

            break;
        }

        case SQL_ATTR_TXN_ISOLATION:
        {
            if(pConn->isConnectionOpen())
            {
                // Is connection has active transaction?
                if(!libpqIsTransactionIdle(pConn))
                {
                    rc = SQL_ERROR;
                    addError(
                        &pConn->pErrorList, "HY011",
                        "Attribute cannot be set now: SQL_ATTR_TXN_ISOLATION",
                        0, NULL);
                    goto error;
                }

                if(iVal != SQL_TXN_READ_UNCOMMITTED 
                    && iVal != SQL_TXN_READ_COMMITTED 
                    && iVal != SQL_TXN_REPEATABLE_READ
                    && iVal != SQL_TXN_SERIALIZABLE)
                {
                    rc = SQL_ERROR;
                    addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                    goto error;
                }

                if(iVal != SQL_TXN_SERIALIZABLE)
                {
                    iVal = SQL_TXN_SERIALIZABLE;
                    rc = SQL_SUCCESS_WITH_INFO;
                    addError(&pConn->pErrorList,"01S02", "Option value changed", 0, NULL);
                }
            } 

            pConnAttr->iTxnIsolation = iVal;

            break;
        }
        case SQL_ATTR_DRIVER_UNICODE_TYPE:
        case SQL_ATTR_APP_UNICODE_TYPE:
        case SQL_ATTR_APP_WCHAR_TYPE: {
            int v = iVal;
            if (v != SQL_DD_CP_UTF16 && v != SQL_DD_CP_UTF32) {
                rc = SQL_ERROR;
                RS_LOG_ERROR("CONNATTR", "Invalid unicode type:%d", v);
                addError(&pConn->pErrorList, "HY024",
                         "Invalid Unicode type for CON attribute", 0, NULL);
                goto error;
            }
            // affects future DBCs / default behavior
            set_process_unicode_type(v);
            break;
        }

        case SQL_ATTR_QUERY_TIMEOUT:
        {
            if(iVal < 0)
            {
                rc = SQL_ERROR;
                addError(&pConn->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            } 

            pConnectProps->iQueryTimeout = iVal;
            break;
        }

        case SQL_ATTR_ANSI_APP:
        /*
        Since we can handle both unicode as well as ANSI, we'll return SQL_ERROR as per
        https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-drivers?view=sql-server-ver16
        */
            rc = SQL_ERROR;
            break;
        default:
        {
            rc = SQL_ERROR;
            addError(&pConn->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLSetConnectAttr-2", 0, NULL);
            goto error;
        }

    } // Switch

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSetStmtAttr sets attributes related to a statement.
//
SQLRETURN  SQL_API SQLSetStmtAttr(SQLHSTMT    phstmt,
                                   SQLINTEGER    iAttribute, 
                                   SQLPOINTER    pValue,
                                   SQLINTEGER    cbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetStmtAttr(FUNC_CALL, 0, phstmt, iAttribute, pValue, cbLen);

    rc = RsOptions::RS_SQLSetStmtAttr(phstmt, iAttribute, pValue, cbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetStmtAttr(FUNC_RETURN, rc, phstmt, iAttribute, pValue, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLSetStmtAttr and SQLSetStmtAttrW.
//
SQLRETURN  SQL_API RsOptions::RS_SQLSetStmtAttr(SQLHSTMT    phstmt,
                                       SQLINTEGER    iAttribute, 
                                       SQLPOINTER    pValue,
                                       SQLINTEGER    cbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    RS_STMT_ATTR_INFO *pStmtAttr;
    int iVal = (int)(long)pValue;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    pStmtAttr = pStmt->pStmtAttr;

    switch(iAttribute)
    {
        case SQL_ATTR_APP_PARAM_DESC:
        {
            if(pValue)
            {
                pStmtAttr->pAPD = (RS_DESC_INFO *) pValue;
                pStmtAttr->pAPD->iType = RS_APD;
            }
            else
                pStmtAttr->pAPD = pStmt->pAPD;

            break;
        }

        case SQL_ATTR_APP_ROW_DESC:
        {
            if(pValue)
            {
                pStmtAttr->pARD = (RS_DESC_INFO *) pValue;
                pStmtAttr->pARD->iType = RS_ARD;
            }
            else
                pStmtAttr->pARD = pStmt->pARD;

            break;
        }

        case SQL_ATTR_ASYNC_ENABLE:
        {
            // Check valid values
            if(iVal != SQL_ASYNC_ENABLE_OFF
                && iVal != SQL_ASYNC_ENABLE_ON)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pStmtAttr->iAsyncEnable = iVal;
            break;
        }

        case SQL_ATTR_CONCURRENCY:
        {
            if(iVal != SQL_CONCUR_READ_ONLY 
                && iVal != SQL_CONCUR_LOCK
                && iVal != SQL_CONCUR_ROWVER
                && iVal != SQL_CONCUR_VALUES)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            if(iVal == SQL_CONCUR_LOCK
                || iVal == SQL_CONCUR_ROWVER)
            {
                rc = SQL_SUCCESS_WITH_INFO;
                addError(&pStmt->pErrorList,"01S02", "Option value changed", 0, NULL);
                goto error;
            }

            pStmtAttr->iConcurrency = iVal;
            break;
        }

        case SQL_ATTR_CURSOR_SCROLLABLE:
        {
            if(iVal != SQL_NONSCROLLABLE 
                && iVal != SQL_SCROLLABLE)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pStmtAttr->iCursorScrollable = iVal;
            break;
        }

        case SQL_ATTR_CURSOR_SENSITIVITY:
        {
            if(iVal != SQL_UNSPECIFIED 
                && iVal != SQL_INSENSITIVE
                && iVal != SQL_SENSITIVE)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            if(iVal == SQL_SENSITIVE)
            {
                rc = SQL_SUCCESS_WITH_INFO;
                addError(&pStmt->pErrorList,"01S02", "Option value changed", 0, NULL);
                goto error;
            }

            pStmtAttr->iCursorSensitivity = iVal;
            break;
        }

        case SQL_ATTR_CURSOR_TYPE:
        {
            if(iVal != SQL_CURSOR_FORWARD_ONLY 
                && iVal != SQL_CURSOR_KEYSET_DRIVEN
                && iVal != SQL_CURSOR_DYNAMIC
                && iVal != SQL_CURSOR_STATIC)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            if(iVal == SQL_CURSOR_KEYSET_DRIVEN
                || iVal == SQL_CURSOR_DYNAMIC)
            {
                rc = SQL_SUCCESS_WITH_INFO;
                addError(&pStmt->pErrorList,"01S02", "Option value changed", 0, NULL);
                goto error;
            }

            pStmtAttr->iCursorType = iVal;
            break;
        }

        case SQL_ATTR_FETCH_BOOKMARK_PTR:
        {
            pStmtAttr->pFetchBookmarkPtr = pValue;
            break;
        }

        case SQL_ATTR_KEYSET_SIZE:
        {
            if(iVal != 0)
            {
                rc = SQL_SUCCESS_WITH_INFO;
                addError(&pStmt->pErrorList,"01S02", "Option value changed", 0, NULL);
                goto error;
            }

            pStmtAttr->iKeysetSize = iVal;
            break;
        }

        case SQL_ATTR_MAX_LENGTH:
        {
            pStmtAttr->iMaxLength = iVal;
            break;
        }

        case SQL_ATTR_MAX_ROWS:
        {
            pStmtAttr->iMaxRows = iVal;
            break;
        }

        case SQL_ATTR_METADATA_ID:
        {
            if(iVal != SQL_TRUE && iVal != SQL_FALSE)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pStmtAttr->iMetaDataId = iVal;

            break;
        }

        case SQL_ATTR_NOSCAN:
        {
            if(iVal != SQL_NOSCAN_OFF 
                && iVal != SQL_NOSCAN_ON)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pStmtAttr->iNoScan = iVal;
            break;
        }

        case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pAPD,0,SQL_DESC_BIND_OFFSET_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_PARAM_BIND_TYPE:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pAPD,0,SQL_DESC_BIND_TYPE,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_PARAM_OPERATION_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pAPD,0,SQL_DESC_ARRAY_STATUS_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_PARAM_STATUS_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmt->pIPD,0,SQL_DESC_ARRAY_STATUS_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_PARAMS_PROCESSED_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmt->pIPD,0,SQL_DESC_ROWS_PROCESSED_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_PARAMSET_SIZE:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pAPD,0,SQL_DESC_ARRAY_SIZE,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_QUERY_TIMEOUT:
        {
            if(iVal < 0)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            } 

            pStmtAttr->iQueryTimeout = iVal;
            break;
        }

        case SQL_ATTR_RETRIEVE_DATA:
        {
            if(iVal != SQL_RD_OFF
                && iVal != SQL_RD_ON)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pStmtAttr->iRetrieveData = iVal;
            break;
        }

        case SQL_ATTR_ROW_ARRAY_SIZE:
        case SQL_ROWSET_SIZE:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pARD,0,SQL_DESC_ARRAY_SIZE,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_ROW_BIND_OFFSET_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pARD,0,SQL_DESC_BIND_OFFSET_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_ROW_BIND_TYPE:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pARD,0,SQL_DESC_BIND_TYPE,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_ROW_OPERATION_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmtAttr->pARD,0,SQL_DESC_ARRAY_STATUS_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_ROW_STATUS_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmt->pIRD,0,SQL_DESC_ARRAY_STATUS_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_ROWS_FETCHED_PTR:
        {
            rc = RsDesc::RS_SQLSetDescField(pStmt->pIRD,0,SQL_DESC_ROWS_PROCESSED_PTR,pValue,cbLen,TRUE);
            break;
        }

        case SQL_ATTR_SIMULATE_CURSOR:
        {
            if(iVal != SQL_SC_NON_UNIQUE
                && iVal != SQL_SC_TRY_UNIQUE
                && iVal != SQL_SC_UNIQUE)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            if(iVal != SQL_SC_NON_UNIQUE)
            {
                rc = SQL_SUCCESS_WITH_INFO;
                addError(&pStmt->pErrorList,"01S02", "Option value changed", 0, NULL);
                goto error;
            }

            pStmtAttr->iSimulateCursor = iVal;
            break;
        }

        case SQL_ATTR_USE_BOOKMARKS:
        {
            if(iVal != SQL_UB_OFF
                && iVal != SQL_UB_ON)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                goto error;
            }

            pStmtAttr->iUseBookmark = iVal;
            break;
        }

        case SQL_ATTR_ENABLE_AUTO_IPD:
        default:
        {
            rc = SQL_ERROR;
            std::string err =
                "Optional feature not implemented in SQLSetStmtAttr:" +
                std::to_string(iAttribute);
            addError(&pStmt->pErrorList, "HYC00", err.data(), 0, NULL);
            goto error;
        }
    } // Switch

error:

    return rc;
}

/*====================================================================================================================================================*/

// ============================================================================
// SQLGetConnectAttrW : Unicode version of SQLGetConnectAttr.
// ============================================================================

SQLRETURN  SQL_API SQLGetConnectAttrW(SQLHDBC        phdbc,
                                       SQLINTEGER    iAttribute, 
                                       SQLPOINTER    pwValue,
                                       SQLINTEGER    cbLen, 
                                       SQLINTEGER  *pcbLen)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
    beginApiMutex(NULL, phdbc);

    if (IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetConnectAttrW(FUNC_CALL, 0, phdbc, iAttribute, pwValue, cbLen, pcbLen);

    bool isStr = RsOptions::isStrConnectAttr(iAttribute);
    size_t copiedChars = 0;

    auto on_exit = make_scope_exit([&]() noexcept {
        if (IS_TRACE_LEVEL_API_CALL()) {
            TraceSQLGetConnectAttrW(FUNC_RETURN, rc, phdbc, iAttribute, pwValue,
                                    copiedChars * sizeofSQLWCHAR(), pcbLen);
        }
        endApiMutex(NULL, phdbc);
    });
    // Early check to have parity with non-wide version.
    // Validate BufferLength only if a destination buffer is supplied.
    if (isStr && pwValue != nullptr) {
        if (cbLen < 0) {
            rc = SQL_ERROR;
            RS_LOG_ERROR("CONNATTR", "Invalid buffer length: %d", cbLen);
            addError(&pConn->pErrorList, "HY090",
                     "Invalid string or buffer length", 0, NULL);
            return rc;
        }
        // For W APIs: BufferLength must be a multiple of sizeof(SQLWCHAR)
        if ((cbLen % (SQLINTEGER)sizeofSQLWCHAR()) != 0) {
            rc = SQL_ERROR;
            RS_LOG_ERROR("CONNATTR", "Unaligned BufferLength for Unicode: %d",
                         cbLen);
            addError(&pConn->pErrorList, "HY090",
                     "Invalid string or buffer length (must be multiple of "
                     "wide char size)",
                     0, NULL);
            return rc;
        }
    }

    if (!isStr) {
        // Non-string attributes: pass through unchanged.
        return RsOptions::RS_SQLGetConnectAttr(phdbc, iAttribute, pwValue,
                                               cbLen, pcbLen);
    }

    // -------- STRING ATTRIBUTE GUARDS (per ODBC) --------
    // If caller passed no buffer AND no length pointer -> HY009 (invalid null
    // pointer).
    if (pwValue == nullptr && pcbLen == nullptr) {
        // Post HY009
        addError(&pConn->pErrorList, "HY009", "Invalid use of null pointer", 0,
                 NULL);
        return SQL_ERROR;
    }

    // Discover required UTF-8 length (bytes excl. NUL) from driver state.
    SQLINTEGER utf8BytesRequired = 0;
    rc = RsOptions::RS_SQLGetConnectAttr(phdbc, iAttribute, nullptr, 0,
                                         &utf8BytesRequired);
    if (!SQL_SUCCEEDED(rc)) {
        return rc;
    }

    // Fetch UTF-8 value locally (don’t touch caller buffer yet).
    std::string utf8;
    utf8.resize(
        static_cast<size_t>((std::max<SQLINTEGER>)(0, utf8BytesRequired)) + 1,
        '\0');
    SQLINTEGER utf8BytesReturned = 0;

    rc = RsOptions::RS_SQLGetConnectAttr(
        phdbc, iAttribute, utf8.data(),
        static_cast<SQLINTEGER>(utf8.size()), // include space for NUL
        &utf8BytesReturned);
    if (!SQL_SUCCEEDED(rc)) {
        return rc;
    }

    utf8BytesReturned =
        (utf8BytesReturned < 0)
            ? 0
            : (std::min<SQLINTEGER>)(utf8.size(), utf8BytesReturned);

    size_t usedUtf8 = static_cast<size_t>(utf8BytesReturned);
    if (usedUtf8 && utf8[usedUtf8 - 1] == '\0') {
        --usedUtf8;
    }

    // Compute required wide code units (excluding NULL) without writing.
    const size_t totalCharsNeeded =
        utf8_to_sqlwchar_strlen(utf8.data(), static_cast<int>(usedUtf8),
                                /*auto-detect*/ -1);
    // bytes needed (excluding NULL) for W path
    const SQLINTEGER requiredBytes =
        static_cast<SQLINTEGER>(totalCharsNeeded * sizeofSQLWCHAR());
    // No capacity if caller gave no buffer or zero/negative cbLen
    const bool noCapacity = (pwValue == nullptr) || (cbLen <= 0);

    // --- Spec-compliant guard rails ---
    if (noCapacity && pcbLen == nullptr) {
        // Invalid use of null pointer: nowhere to write and nowhere to report
        // length
        addError(&pConn->pErrorList, "HY009", "Invalid use of null pointer", 0,
                 NULL);
        return SQL_ERROR;
    }

    // If cbLen <= 0, there is no capacity to write into pwValue (even if DM
    // passed a dummy pointer).
    if (cbLen <= 0 || pwValue == nullptr) {
        if (pcbLen) {
            *pcbLen = requiredBytes;
            // Length query
            return SQL_SUCCESS;
        } else {
            // Both NULL cases are handled above. This is just defensive
            // fallback
            addError(&pConn->pErrorList, "HY009", "Invalid use of null pointer",
                     0, NULL);
            return SQL_ERROR;
        }
    }

    // We have capacity (>0) and a non-NULL buffer: convert and copy.
    const size_t capWide = static_cast<size_t>(cbLen) / sizeofSQLWCHAR();
    copiedChars = utf8_to_sqlwchar_str(
        utf8.data(), usedUtf8, static_cast<SQLWCHAR *>(pwValue), capWide);

    // Ensure NUL termination if any capacity exists.
    if (capWide > 0) {
        const size_t term =
            (copiedChars < capWide) ? copiedChars : (capWide - 1);
        setNthSqlwcharNull(static_cast<SQLWCHAR *>(pwValue), term);
    }
    if (pcbLen)
        *pcbLen = requiredBytes;

    if (copiedChars < totalCharsNeeded) {
        // Truncation occurred
        addError(&pConn->pErrorList, "01004", "String data, right truncated", 0,
                 NULL);
        return SQL_SUCCESS_WITH_INFO;
    }

    return SQL_SUCCESS;
}

/*====================================================================================================================================================*/

// ============================================================================
// SQLSetConnectAttrW : Unicode version of SQLSetConnectAttr.
// ============================================================================

SQLRETURN  SQL_API SQLSetConnectAttrW(SQLHDBC    phdbc,
                                       SQLINTEGER    iAttribute, 
                                       SQLPOINTER    pwValue,
                                       SQLINTEGER    cbLen)
{
    /*
    Note:
    Consider adding beginApiMutex(NULL, phdbc) and endApiMutex(NULL, phdbc)
    */
    SQLRETURN rc = SQL_SUCCESS;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetConnectAttrW(FUNC_CALL, 0, phdbc, iAttribute, pwValue, cbLen);

    // Common epilogue: trace + unlock, always runs.
    auto on_exit = make_scope_exit([&]() noexcept {
        if (IS_TRACE_LEVEL_API_CALL())
            TraceSQLSetConnectAttrW(FUNC_RETURN, rc, phdbc, iAttribute, pwValue,
                                    cbLen);
    });

    if ((iAttribute == SQL_ATTR_CURRENT_CATALOG ||
         iAttribute == SQL_ATTR_TRACEFILE ||
         iAttribute == SQL_ATTR_TRANSLATE_LIB) &&
        pwValue) {
        // cchLen is in client code units; if SQL_NTS, pass SQL_NTS through
        const int cchLen = (cbLen != SQL_NTS)
                               ? static_cast<int>(cbLen / sizeofSQLWCHAR())
                               : SQL_NTS;

        std::string utf8Str;
        // Autodetects UTF-16/32 under the hood; returns BYTES (excl. NUL)
        const size_t outBytes = sqlwchar_to_utf8_str(
            static_cast<const SQLWCHAR *>(pwValue), cchLen, utf8Str);
        rc = RsOptions::RS_SQLSetConnectAttr(
            phdbc, iAttribute, utf8Str.data(),
            static_cast<SQLINTEGER>(outBytes));
    } else {

        // Non-string attributes or null value: forward unchanged.
        rc = RsOptions::RS_SQLSetConnectAttr(phdbc, iAttribute, pwValue, cbLen);
    }
    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetStmtAttr.
//
SQLRETURN SQL_API SQLGetStmtAttrW(SQLHSTMT  phstmt,
                                    SQLINTEGER   iAttribute,
                                    SQLPOINTER   pwValue,
                                    SQLINTEGER   cbLen,
                                    SQLINTEGER   *pcbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetStmtAttrW(FUNC_CALL, 0, phstmt, iAttribute, pwValue, cbLen, pcbLen);

    // All options are integers.
    rc = RsOptions::RS_SQLGetStmtAttr(phstmt,iAttribute,pwValue,cbLen,pcbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetStmtAttrW(FUNC_RETURN, rc, phstmt, iAttribute, pwValue, cbLen, pcbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLSetStmtAttr.
//
SQLRETURN  SQL_API SQLSetStmtAttrW(SQLHSTMT    phstmt,
                                   SQLINTEGER    iAttribute, 
                                   SQLPOINTER    pwValue,
                                   SQLINTEGER    cbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetStmtAttrW(FUNC_CALL, 0, phstmt, iAttribute, pwValue, cbLen);

    rc = RsOptions::RS_SQLSetStmtAttr(phstmt,iAttribute,pwValue,cbLen);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetStmtAttrW(FUNC_RETURN, rc, phstmt, iAttribute, pwValue, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSetEnvAttr sets attributes that govern aspects of environments.
//
SQLRETURN  SQL_API SQLSetEnvAttr(SQLHENV phenv,
                                   SQLINTEGER    iAttribute, 
                                   SQLPOINTER    pValue,
                                   SQLINTEGER    cbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    int iVal = (int)(long)pValue;
    RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
    RS_ENV_ATTR_INFO *pEnvAttr;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetEnvAttr(FUNC_CALL, 0, phenv, iAttribute, pValue, cbLen);

    if(!VALID_HENV(phenv))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pEnv->pErrorList = clearErrorList(pEnv->pErrorList);
    pEnvAttr = pEnv->pEnvAttr;

    switch(iAttribute)
    {
        case SQL_ATTR_CONNECTION_POOLING:
        {
            switch(iVal)
            {
                case SQL_CP_OFF:
                case SQL_CP_ONE_PER_DRIVER:
                case SQL_CP_ONE_PER_HENV:
                {
                    pEnvAttr->iConnectionPooling = iVal;
                    break;
                }

                default:
                {
                    rc = SQL_ERROR;
                    addError(&pEnv->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                    goto error; 
                }
            }

            break;
        }

        case SQL_ATTR_CP_MATCH:
        {
            switch(iVal)
            {
                case SQL_CP_STRICT_MATCH:
                case SQL_CP_RELAXED_MATCH:
                {
                    pEnvAttr->iConnectionPoolingMatch = iVal;
                    break;
                }

                default:
                {
                    rc = SQL_ERROR;
                    addError(&pEnv->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                    goto error; 
                }
            }

            break;
        }

        case SQL_ATTR_ODBC_VERSION:
        {
            switch(iVal)
            {
                case SQL_OV_ODBC2:
                case SQL_OV_ODBC3:
                {
                    pEnvAttr->iOdbcVersion = iVal;
                    break;
                }

                default:
                {
                    rc = SQL_ERROR;
                    addError(&pEnv->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                    goto error; 
                }
            }

            break;
        }

        case SQL_ATTR_OUTPUT_NTS:
        {
            switch(iVal)
            {
                case SQL_TRUE:
                {
                    pEnvAttr->iOutputNts = iVal;
                    break;
                }

                case SQL_FALSE:
                {
                    rc = SQL_ERROR;
                    addError(&pEnv->pErrorList,"HYC00", "Optional feature not implemented:SQLSetEnvAttr", 0, NULL);
                    goto error; 
                }

                default:
                {
                    rc = SQL_ERROR;
                    addError(&pEnv->pErrorList,"HY024", "Invalid attribute value", 0, NULL);
                    goto error; 
                }
            }
            break;
        }

        case SQL_ATTR_APP_UNICODE_TYPE:
        case SQL_ATTR_APP_WCHAR_TYPE:
        case SQL_ATTR_DRIVER_UNICODE_TYPE: {
            int v = iVal;
            if (v != SQL_DD_CP_UTF16 && v != SQL_DD_CP_UTF32) {
                rc = SQL_ERROR;
                addError(&pEnv->pErrorList, "HY024",
                         "Invalid Unicode type for ENV attribute", 0, NULL);
                RS_LOG_ERROR("RS_SQLSetEnvAttr",
                             "Invalid Unicode type for ENV attribute");
                goto error;
            }
             // affects future DBCs / default behavior
            set_process_unicode_type(v);
            break;
        }

        default:
        {
            rc = SQL_ERROR;
            addError(&pEnv->pErrorList,"HY092", "Invalid attribute/option identifier", 0, NULL);
            goto error; 
        }
    } // Switch

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetEnvAttr(FUNC_RETURN, rc, phenv, iAttribute, pValue, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetEnvAttr returns the current setting of an environment attribute.
//
SQLRETURN  SQL_API SQLGetEnvAttr(SQLHENV phenv,
                                   SQLINTEGER    iAttribute, 
                                   SQLPOINTER    pValue,
                                   SQLINTEGER    cbLen, 
                                   SQLINTEGER    *pcbLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    int *piVal = (int *)pValue;
    RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
    RS_ENV_ATTR_INFO *pEnvAttr;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetEnvAttr(FUNC_CALL, 0, phenv, iAttribute, pValue, cbLen, pcbLen);

    if(!VALID_HENV(phenv))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pEnv->pErrorList = clearErrorList(pEnv->pErrorList);

    if(!pValue)
    {
        rc = SQL_ERROR;
        addError(&pEnv->pErrorList,"HY000", "Output buffer is NULL", 0, NULL);
        goto error; 
    }

    pEnvAttr = pEnv->pEnvAttr;

    switch(iAttribute)
    {
        case SQL_ATTR_CONNECTION_POOLING:
        {
            *piVal = pEnvAttr->iConnectionPooling;
            break;
        }

        case SQL_ATTR_CP_MATCH:
        {
            *piVal = pEnvAttr->iConnectionPoolingMatch;
            break;
        }

        case SQL_ATTR_ODBC_VERSION:
        {
            *piVal = pEnvAttr->iOdbcVersion;
            break;
        }

        case SQL_ATTR_OUTPUT_NTS:
        {
            *piVal = pEnvAttr->iOutputNts;
            break;
        }

        case SQL_ATTR_APP_UNICODE_TYPE:
        case SQL_ATTR_APP_WCHAR_TYPE:
        case SQL_ATTR_DRIVER_UNICODE_TYPE: {
            *piVal = get_app_unicode_type();
            if (*piVal != SQL_DD_CP_UTF16 && *piVal != SQL_DD_CP_UTF32) {
                rc = SQL_ERROR;
                addError(&pEnv->pErrorList, "HY000",
                         "Invalid Unicode type for ENV attribute", 0, NULL);
                RS_LOG_ERROR("RS_SQLGetEnvAttr",
                             "Invalid Unicode type for ENV attribute");
                goto error;
            }
            break;
        }

        default:
        {
            rc = SQL_ERROR;
            addError(&pEnv->pErrorList,"HY092", "Invalid attribute/option identifier", 0, NULL);
            goto error; 
        }
    } // Switch

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetEnvAttr(FUNC_RETURN, rc, phenv, iAttribute, pValue, cbLen, pcbLen);

    return rc;
}

/*====================================================================================================================================================*/
// Checks if the input is a string-type connection attribute
//
bool RsOptions::isStrConnectAttr(SQLINTEGER iAttribute) {
    return iAttribute == SQL_ATTR_CURRENT_CATALOG
        || iAttribute == SQL_ATTR_TRACEFILE
        || iAttribute == SQL_ATTR_TRANSLATE_LIB;
}
