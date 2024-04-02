/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsdrvinfo.h"

static int doesFunctionSupport(UWORD iFunction);
static SQLRETURN strInfoResponse(char *pSrc, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen, int *piRetType);
static SQLRETURN shortInfoResponse(SQLUSMALLINT hData, SQLUSMALLINT *phBuf, SQLSMALLINT *pcbLen, int *piRetType);
static SQLRETURN integerInfoResponse(SQLUINTEGER iData, SQLUINTEGER *phBuf, SQLSMALLINT *pcbLen, int *piRetType);

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetInfo.
//
SQLRETURN SQL_API SQLGetInfoW(SQLHDBC   phdbc,
                                SQLUSMALLINT    hInfoType,
                                SQLPOINTER        pwInfoValue,
                                SQLSMALLINT     cbLen,
                                SQLSMALLINT*    pcbLen)
{
    SQLRETURN rc;
    SQLPOINTER pInfoValue = NULL;
    int iRetType;
    int allocFlag = FALSE;
    SQLSMALLINT cchLen = cbLen/sizeof(WCHAR);

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetInfoW(FUNC_CALL, 0, phdbc, hInfoType, pwInfoValue, cbLen, pcbLen);

    switch(hInfoType)
    {
        case SQL_ACCESSIBLE_PROCEDURES:
        case SQL_ACCESSIBLE_TABLES:
        case SQL_CATALOG_NAME:
        case SQL_DATA_SOURCE_READ_ONLY:
        case SQL_ODBC_SQL_OPT_IEF: /* SQL_INTEGRITY */
        case SQL_COLLATION_SEQ:
        case SQL_COLUMN_ALIAS:
        case SQL_DESCRIBE_PARAMETER:
        case SQL_EXPRESSIONS_IN_ORDERBY:
        case SQL_LIKE_ESCAPE_CLAUSE:
        case SQL_DATA_SOURCE_NAME:
        case SQL_DATABASE_NAME:
        case SQL_DBMS_NAME:
        case SQL_DBMS_VER:
        case SQL_DM_VER:
        case SQL_DRIVER_NAME:
        case SQL_DRIVER_ODBC_VER:
        case SQL_DRIVER_VER:
        case SQL_IDENTIFIER_QUOTE_CHAR:
        case SQL_KEYWORDS:
        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
        case SQL_ROW_UPDATES:
        case SQL_QUALIFIER_NAME_SEPARATOR:     /* SQL_CATALOG_NAME_SEPARATOR */
        case SQL_QUALIFIER_TERM:             /*  SQL_CATALOG_TERM */
        case SQL_SPECIAL_CHARACTERS:
        case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
        case SQL_MULT_RESULT_SETS:
        case SQL_MULTIPLE_ACTIVE_TXN:
        case SQL_NEED_LONG_DATA_LEN:
        case SQL_OUTER_JOINS:
        case SQL_PROCEDURES:
        case SQL_OWNER_TERM:                /* SQL_SCHEMA_TERM  */
        case SQL_PROCEDURE_TERM:
        case SQL_SEARCH_PATTERN_ESCAPE:
        case SQL_SERVER_NAME:
        case SQL_TABLE_TERM:
        case SQL_USER_NAME:
        case SQL_XOPEN_CLI_YEAR:
        {
            if(pwInfoValue != NULL)
            {
                if(cbLen >= 0)
                {
                    pInfoValue = rs_calloc(sizeof(char), cchLen + 1);
                    allocFlag = TRUE;
                }
            }

            break;
        }

        default:
        {
            // int/short.
            pInfoValue = pwInfoValue;
            break;
        }
    }

    rc = RsDrvInfo::RS_SQLGetInfo(phdbc, hInfoType, pInfoValue, cchLen, pcbLen, &iRetType);

    if(SQL_SUCCEEDED(rc))
    {
        if(iRetType == SQL_C_CHAR)
        {
            if(pwInfoValue)
                utf8_to_wchar((char *)pInfoValue, SQL_NTS, (WCHAR *)pwInfoValue, cchLen);

            if(pcbLen)
                *pcbLen = (*pcbLen) * sizeof(WCHAR);
        }
    }

    if(allocFlag && pInfoValue)
        pInfoValue = rs_free(pInfoValue);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetInfoW(FUNC_RETURN, rc, phdbc, hInfoType, pwInfoValue, cbLen, pcbLen);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetInfo returns general information about the driver and data source associated with a connection.
//
SQLRETURN  SQL_API SQLGetInfo(SQLHDBC phdbc,
                                SQLUSMALLINT hInfoType, 
                                SQLPOINTER pInfoValue,
                                SQLSMALLINT cbLen, 
                                SQLSMALLINT *pcbLen)
{
    SQLRETURN rc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetInfo(FUNC_CALL, 0, phdbc, hInfoType, pInfoValue, cbLen, pcbLen);

    rc = RsDrvInfo::RS_SQLGetInfo(phdbc, hInfoType, pInfoValue, cbLen, pcbLen, NULL);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetInfo(FUNC_RETURN, rc, phdbc, hInfoType, pInfoValue, cbLen, pcbLen);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetInfo and SQLGetInfoW.
//
SQLRETURN  SQL_API RsDrvInfo::RS_SQLGetInfo(SQLHDBC phdbc,
                                    SQLUSMALLINT hInfoType, 
                                    SQLPOINTER pInfoValue,
                                    SQLSMALLINT cbLen, 
                                    SQLSMALLINT *pcbLen,
                                    int *piRetType)
{
    SQLRETURN rc = SQL_SUCCESS;
    char *pVal = (char *) pInfoValue;
    SQLUSMALLINT *phVal = (SQLUSMALLINT *)pInfoValue;
    SQLUINTEGER     *piVal = (SQLUINTEGER *)pInfoValue;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;
    SQLUINTEGER iVal;
    SQLUSMALLINT hVal;
    char *pTemp;
    char szSrvrBuf[MAX_TEMP_BUF_LEN];

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error; 
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    if(cbLen < 0)
    {
        rc = SQL_ERROR;
        addError(&pConn->pErrorList,"HY090", "Invalid string or buffer length", 0, NULL);
        goto error; 
    }

    switch (hInfoType) 
    {
        case SQL_ACCESSIBLE_PROCEDURES:
        case SQL_ACCESSIBLE_TABLES:
        case SQL_DATA_SOURCE_READ_ONLY:
        case SQL_ODBC_SQL_OPT_IEF: /* SQL_INTEGRITY */
        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
        case SQL_ROW_UPDATES:
        {
            rc = strInfoResponse("N", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_COLLATION_SEQ:
        {
            rc = strInfoResponse("", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_SPECIAL_CHARACTERS:
        {
            rc = strInfoResponse("_", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_QUALIFIER_NAME_SEPARATOR: /* SQL_CATALOG_NAME_SEPARATOR */
        {
            rc = strInfoResponse(".", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_QUALIFIER_TERM:           /* SQL_CATALOG_TERM */
        {
            rc = strInfoResponse("database", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_CATALOG_NAME:
        case SQL_COLUMN_ALIAS:
        case SQL_DESCRIBE_PARAMETER:
        case SQL_EXPRESSIONS_IN_ORDERBY:
        case SQL_LIKE_ESCAPE_CLAUSE:
        case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
        case SQL_MULT_RESULT_SETS:
        case SQL_MULTIPLE_ACTIVE_TXN:
        case SQL_NEED_LONG_DATA_LEN:
        case SQL_OUTER_JOINS:
        case SQL_PROCEDURES:
        {
            rc = strInfoResponse("Y", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_ACTIVE_CONNECTIONS: /* SQL_MAX_DRIVER_CONNECTIONS */
        case SQL_ACTIVE_ENVIRONMENTS:
        case SQL_MAX_COLUMNS_IN_GROUP_BY:
        case SQL_MAX_COLUMNS_IN_INDEX:
        case SQL_MAX_COLUMNS_IN_ORDER_BY:
        case SQL_MAX_COLUMNS_IN_SELECT:
        case SQL_MAX_COLUMNS_IN_TABLE:
        case SQL_MAX_TABLES_IN_SELECT:
        {
            rc = shortInfoResponse(0, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_QUALIFIER_LOCATION: /* SQL_CATALOG_LOCATION */
        {
            iVal= SQL_CL_START;
            rc = shortInfoResponse(iVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_ACTIVE_STATEMENTS: /* SQL_MAX_CONCURRENT_ACTIVITIES */
        case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
        {
            rc = shortInfoResponse(1, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_CREATE_ASSERTION:
        case SQL_CREATE_CHARACTER_SET:
        case SQL_CREATE_COLLATION:
        case SQL_CREATE_TRANSLATION:
        case SQL_DROP_ASSERTION:
        case SQL_DROP_CHARACTER_SET:
        case SQL_DROP_COLLATION:
        case SQL_DROP_TRANSLATION:
        case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
        case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
        case SQL_INDEX_KEYWORDS:
        case SQL_INFO_SCHEMA_VIEWS:
        case SQL_KEYSET_CURSOR_ATTRIBUTES1:
        case SQL_KEYSET_CURSOR_ATTRIBUTES2:
        case SQL_MAX_BINARY_LITERAL_LEN:
        case SQL_MAX_CHAR_LITERAL_LEN:
        case SQL_MAX_ROW_SIZE:
        case SQL_MAX_STATEMENT_LEN:
        case SQL_STATIC_SENSITIVITY:
        case SQL_TIMEDATE_ADD_INTERVALS:
        case SQL_TIMEDATE_DIFF_INTERVALS:
        {
            rc = integerInfoResponse(0, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_QUALIFIER_USAGE: /* SQL_CATALOG_USAGE */
        {
            iVal = SQL_CU_DML_STATEMENTS;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_AGGREGATE_FUNCTIONS:
        {
            iVal = SQL_AF_ALL 
                    | SQL_AF_AVG 
                    | SQL_AF_COUNT 
                    | SQL_AF_DISTINCT 
                    | SQL_AF_MAX 
                    | SQL_AF_MIN 
                    | SQL_AF_SUM ;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_ALTER_DOMAIN:
        {
            iVal = 0;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_ALTER_TABLE:
        {
            iVal = SQL_AT_ADD_COLUMN |
                SQL_AT_DROP_COLUMN |
                SQL_AT_ADD_CONSTRAINT |
                SQL_AT_ADD_COLUMN_SINGLE |
                SQL_AT_DROP_COLUMN_CASCADE |
                SQL_AT_DROP_COLUMN_RESTRICT |
                SQL_AT_ADD_TABLE_CONSTRAINT |
                SQL_AT_DROP_TABLE_CONSTRAINT_CASCADE |
                SQL_AT_DROP_TABLE_CONSTRAINT_RESTRICT |
                SQL_AT_CONSTRAINT_INITIALLY_DEFERRED |
                SQL_AT_CONSTRAINT_INITIALLY_IMMEDIATE |
                SQL_AT_CONSTRAINT_DEFERRABLE;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_ASYNC_MODE:
        {
            iVal = SQL_AM_STATEMENT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_BATCH_ROW_COUNT:
        {
            iVal = SQL_BRC_EXPLICIT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_BATCH_SUPPORT:
        {
            iVal = SQL_BS_SELECT_EXPLICIT
                    | SQL_BS_ROW_COUNT_EXPLICIT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_BOOKMARK_PERSISTENCE:
        {
            iVal = SQL_BP_DROP;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONCAT_NULL_BEHAVIOR:
        {
            hVal = SQL_CB_NULL;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_CHAR:
        case SQL_CONVERT_LONGVARCHAR:
        case SQL_CONVERT_VARCHAR:
        {
            iVal = SQL_CVT_CHAR
                    | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT
                    | SQL_CVT_REAL | SQL_CVT_DOUBLE | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BIT
                    | SQL_CVT_TINYINT | SQL_CVT_BIGINT | SQL_CVT_DATE | SQL_CVT_TIME | SQL_CVT_TIMESTAMP;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_BIGINT:
        case SQL_CONVERT_INTEGER:
        case SQL_CONVERT_NUMERIC:
        case SQL_CONVERT_REAL:
        case SQL_CONVERT_SMALLINT:
        case SQL_CONVERT_TINYINT:
        {
            iVal = SQL_CVT_CHAR
                    | SQL_CVT_NUMERIC | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT
                    | SQL_CVT_REAL | SQL_CVT_DOUBLE | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR
                    | SQL_CVT_TINYINT |  SQL_CVT_BIT | SQL_CVT_BIGINT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_BIT:
        {
            iVal = SQL_CVT_CHAR | SQL_CVT_INTEGER
                | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BIT | SQL_CVT_BIGINT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_DATE:
        {
            iVal = SQL_CVT_CHAR | SQL_CVT_VARCHAR
                | SQL_CVT_LONGVARCHAR | SQL_CVT_DATE | SQL_CVT_TIMESTAMP;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_TIME:
        {
            iVal = SQL_CVT_CHAR | SQL_CVT_VARCHAR
                | SQL_CVT_LONGVARCHAR | SQL_CVT_TIME;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_TIMESTAMP:
        {
            iVal = SQL_CVT_CHAR | SQL_CVT_VARCHAR
                | SQL_CVT_LONGVARCHAR | SQL_CVT_DATE | SQL_CVT_TIME | SQL_CVT_TIMESTAMP;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }


        case SQL_CONVERT_DECIMAL:
        case SQL_CONVERT_DOUBLE:
        case SQL_CONVERT_FLOAT:
        {
            iVal = SQL_CVT_CHAR | SQL_CVT_NUMERIC
                | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_FLOAT | SQL_CVT_REAL
                | SQL_CVT_DOUBLE | SQL_CVT_VARCHAR | SQL_CVT_LONGVARCHAR | SQL_CVT_BIGINT;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_BINARY:
        case SQL_CONVERT_INTERVAL_YEAR_MONTH:
        case SQL_CONVERT_INTERVAL_DAY_TIME:
        case SQL_CONVERT_LONGVARBINARY:
        case SQL_CONVERT_VARBINARY:
        {
            iVal = 0;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CONVERT_WCHAR:
        case SQL_CONVERT_WLONGVARCHAR:
        case SQL_CONVERT_WVARCHAR:
        {
            iVal =  SQL_CVT_NUMERIC
                | SQL_CVT_DECIMAL | SQL_CVT_INTEGER | SQL_CVT_SMALLINT | SQL_CVT_REAL | SQL_CVT_DOUBLE
                | SQL_CVT_BINARY | SQL_CVT_BIT | SQL_CVT_BIGINT | SQL_CVT_DATE | SQL_CVT_TIME
                | SQL_CVT_TIMESTAMP | SQL_CVT_WCHAR | SQL_CVT_WLONGVARCHAR | SQL_CVT_WVARCHAR;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }


        case SQL_CONVERT_FUNCTIONS:
        {
            iVal = SQL_FN_CVT_CONVERT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CORRELATION_NAME:
        {
            hVal = SQL_CN_ANY;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_CREATE_DOMAIN:
        {
            iVal = 0;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CREATE_SCHEMA:
        {
            iVal = SQL_CS_CREATE_SCHEMA
                    | SQL_CS_AUTHORIZATION;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CREATE_TABLE:
        {
            iVal = SQL_CT_CREATE_TABLE
                     | SQL_CT_COMMIT_PRESERVE | SQL_CT_COMMIT_DELETE | SQL_CT_GLOBAL_TEMPORARY
                     | SQL_CT_LOCAL_TEMPORARY | SQL_CT_COLUMN_CONSTRAINT | SQL_CT_COLUMN_DEFAULT
                     | SQL_CT_TABLE_CONSTRAINT | SQL_CT_CONSTRAINT_NAME_DEFINITION;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CREATE_VIEW:
        {
            iVal = SQL_CV_CREATE_VIEW;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_CURSOR_COMMIT_BEHAVIOR:
        case SQL_CURSOR_ROLLBACK_BEHAVIOR:
        {
            hVal = SQL_CB_PRESERVE;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_CURSOR_SENSITIVITY:
        {
            iVal = SQL_INSENSITIVE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DATA_SOURCE_NAME:
        {
            rc = strInfoResponse((char *)((pConn->pConnectProps) ? pConn->pConnectProps->szDSN : ""),
                                    pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DATABASE_NAME:
        {
            rc = strInfoResponse((char *)((pConn->pConnectProps) ? pConn->pConnectProps->szDatabase : ""),
                                    pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DATETIME_LITERALS:
        {
            iVal = SQL_DL_SQL92_DATE
                   | SQL_DL_SQL92_TIME
                   | SQL_DL_SQL92_TIMESTAMP
                   | SQL_DL_SQL92_INTERVAL_YEAR
                   | SQL_DL_SQL92_INTERVAL_MONTH
                   | SQL_DL_SQL92_INTERVAL_DAY
                   | SQL_DL_SQL92_INTERVAL_HOUR
                   | SQL_DL_SQL92_INTERVAL_MINUTE
                   | SQL_DL_SQL92_INTERVAL_SECOND
                   | SQL_DL_SQL92_INTERVAL_YEAR_TO_MONTH
                   | SQL_DL_SQL92_INTERVAL_DAY_TO_HOUR
                   | SQL_DL_SQL92_INTERVAL_DAY_TO_MINUTE
                   | SQL_DL_SQL92_INTERVAL_DAY_TO_SECOND
                   | SQL_DL_SQL92_INTERVAL_HOUR_TO_MINUTE
                   | SQL_DL_SQL92_INTERVAL_HOUR_TO_SECOND
                   | SQL_DL_SQL92_INTERVAL_MINUTE_TO_SECOND;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DBMS_NAME:
        {
            rc = strInfoResponse("Redshift", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DBMS_VER:
        {
            //  Server version in the form of ##.##.####
            pTemp = libpqParameterStatus(pConn,"padb_version"); // server_version
            rc = strInfoResponse(pTemp, pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DDL_INDEX:
        {
/*            iVal = SQL_DI_CREATE_INDEX
                    | SQL_DI_DROP_INDEX; */
            iVal = 0;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DEFAULT_TXN_ISOLATION:
        {
            iVal = SQL_TXN_SERIALIZABLE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DM_VER:
        {
            rc = strInfoResponse("", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DRIVER_NAME:
        {
            char fullPathDllName[MAX_PATH + _MAX_FNAME];
            char fname[_MAX_FNAME];
            char ext[_MAX_EXT];
            char driverName[_MAX_FNAME + _MAX_EXT];

             fullPathDllName[0] = '\0';
             fname[0] = '\0';
             ext[0] = '\0';
             driverName[0] = '\0';

#ifdef WIN32
             GetModuleFileName(gRsGlobalVars.hModule,fullPathDllName, sizeof(fullPathDllName));
             _splitpath(fullPathDllName, NULL, NULL, fname, ext);
             snprintf(driverName,sizeof(driverName),"%s%s",fname,ext);
#endif
#if defined LINUX 
             snprintf(driverName, sizeof(driverName), DRIVER_SO_NAME);
#endif

            rc = strInfoResponse(driverName, pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DRIVER_ODBC_VER:
        {
            // ##.##
            rc = strInfoResponse("03.52", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DRIVER_VER:
        {
            // ##.##.#### format.
            rc = strInfoResponse(ODBC_DRIVER_VERSION, pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_DROP_DOMAIN:
        {
            iVal = 0;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DROP_SCHEMA:
        {
            iVal = SQL_DS_DROP_SCHEMA
                    | SQL_DS_RESTRICT | SQL_DS_CASCADE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DROP_TABLE:
        {
            iVal = SQL_DT_DROP_TABLE
                    | SQL_DT_RESTRICT | SQL_DT_CASCADE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_DROP_VIEW:
        {
            iVal = SQL_DV_DROP_VIEW
                    | SQL_DV_RESTRICT | SQL_DV_CASCADE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_FETCH_DIRECTION:
        {
            iVal = SQL_FD_FETCH_NEXT
                    | SQL_FD_FETCH_FIRST | SQL_FD_FETCH_LAST | SQL_FD_FETCH_PRIOR | SQL_FD_FETCH_ABSOLUTE
                    | SQL_FD_FETCH_RELATIVE | SQL_FD_FETCH_BOOKMARK;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_FILE_USAGE:
        {
            hVal = SQL_FILE_NOT_SUPPORTED;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
        {
            iVal = SQL_CA1_NEXT | SQL_CA1_BULK_ADD;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
        {
            iVal = SQL_CA2_READ_ONLY_CONCURRENCY | SQL_CA2_MAX_ROWS_SELECT | SQL_CA2_CRC_EXACT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_GETDATA_EXTENSIONS:
        {
            iVal = SQL_GD_ANY_COLUMN
                    | SQL_GD_ANY_ORDER | SQL_GD_BOUND;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_GROUP_BY:
        {
            hVal = SQL_GB_GROUP_BY_EQUALS_SELECT;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_IDENTIFIER_CASE:
        {
            hVal = SQL_IC_LOWER;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_IDENTIFIER_QUOTE_CHAR:
        {
            rc = strInfoResponse("\"", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_INSERT_STATEMENT:
        {
            iVal = SQL_IS_INSERT_LITERALS
                    | SQL_IS_INSERT_SEARCHED | SQL_IS_SELECT_INTO;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_KEYWORDS:
        {
            char *pKeywords = "ANALYSE,ANALYZE,ARRAY,ASYMMETRIC,BINARY,CURRENT_ROLE,DO,FREEZE,ILIKE,ISNULL,LIMIT,LOCALTIME,LOCALTIMESTAMP,NEW,NOTNULL,OFF,OFFSET,OLD,PLACING,SIMILAR,SYMMETRIC,VERBOSE";
            rc = strInfoResponse(pKeywords, pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_LOCK_TYPES:
        {
            iVal = SQL_LCK_NO_CHANGE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_MAX_QUALIFIER_NAME_LEN: /* SQL_MAX_CATALOG_NAME_LEN */
        case SQL_MAX_COLUMN_NAME_LEN:
        case SQL_MAX_CURSOR_NAME_LEN:
        case SQL_MAX_IDENTIFIER_LEN:
        case SQL_MAX_PROCEDURE_NAME_LEN:
        case SQL_MAX_OWNER_NAME_LEN: /* SQL_MAX_SCHEMA_NAME_LEN */
        case SQL_MAX_TABLE_NAME_LEN:
        case SQL_MAX_USER_NAME_LEN:
        {
            hVal = MAX_IDEN_LEN - 2; // 127
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_MAX_INDEX_SIZE:
        {
            iVal = 0;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_NON_NULLABLE_COLUMNS:
        {
            hVal = SQL_NNC_NON_NULL;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_NULL_COLLATION:
        {
            hVal = SQL_NC_HIGH;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_NUMERIC_FUNCTIONS:
        {
            iVal = SQL_FN_NUM_ABS
                     | SQL_FN_NUM_ACOS 
                     | SQL_FN_NUM_ASIN 
                     | SQL_FN_NUM_ATAN 
                     | SQL_FN_NUM_ATAN2
                     | SQL_FN_NUM_CEILING 
                     | SQL_FN_NUM_COS 
                     | SQL_FN_NUM_COT 
                     | SQL_FN_NUM_DEGREES
                     | SQL_FN_NUM_EXP 
                     | SQL_FN_NUM_FLOOR
                     | SQL_FN_NUM_LOG 
                     | SQL_FN_NUM_LOG10
                     | SQL_FN_NUM_MOD 
                     | SQL_FN_NUM_PI 
                     | SQL_FN_NUM_POWER 
                     | SQL_FN_NUM_RADIANS
                     | SQL_FN_NUM_ROUND 
                     | SQL_FN_NUM_SIGN 
                     | SQL_FN_NUM_SIN 
                     | SQL_FN_NUM_SQRT
                     | SQL_FN_NUM_TAN  
                     | SQL_FN_NUM_TRUNCATE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_ODBC_API_CONFORMANCE:
        {
            hVal = SQL_OAC_LEVEL2;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_ODBC_INTERFACE_CONFORMANCE:
        {
            iVal = SQL_OIC_CORE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_ODBC_SAG_CLI_CONFORMANCE:
        {
            hVal = SQL_OSCC_COMPLIANT;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_ODBC_SQL_CONFORMANCE:
        {
            hVal = SQL_OSC_CORE;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_OJ_CAPABILITIES:
        {
            iVal = SQL_OJ_LEFT
                     | SQL_OJ_RIGHT | SQL_OJ_FULL | SQL_OJ_NESTED | SQL_OJ_NOT_ORDERED | SQL_OJ_INNER
                     | SQL_OJ_ALL_COMPARISON_OPS;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_OWNER_TERM: /* SQL_SCHEMA_TERM */
        {
            rc = strInfoResponse("schema", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_OWNER_USAGE: /* SQL_SCHEMA_USAGE */
        {
            iVal = SQL_OU_DML_STATEMENTS
                     | SQL_OU_PROCEDURE_INVOCATION | SQL_OU_TABLE_DEFINITION | SQL_OU_INDEX_DEFINITION
                     | SQL_OU_PRIVILEGE_DEFINITION;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_PARAM_ARRAY_ROW_COUNTS:
        {
            iVal = SQL_PARC_NO_BATCH;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_PARAM_ARRAY_SELECTS:
        {
            iVal = SQL_PAS_NO_SELECT;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_POS_OPERATIONS:
        {
            iVal = SQL_POS_POSITION
                    | SQL_POS_REFRESH | SQL_POS_UPDATE | SQL_POS_DELETE | SQL_POS_ADD;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_POSITIONED_STATEMENTS:
        {
            iVal = SQL_PS_POSITIONED_DELETE | SQL_PS_POSITIONED_UPDATE | SQL_PS_SELECT_FOR_UPDATE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_PROCEDURE_TERM:
        {
            rc = strInfoResponse("procedure", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_QUOTED_IDENTIFIER_CASE:
        {
            if(pConn->isConnectionOpen())
            {
                getGucVariableVal(pConn, "downcase_delimited_identifier",szSrvrBuf,sizeof(szSrvrBuf));

                if(_stricmp(szSrvrBuf,"off") == 0)
                    hVal = SQL_IC_SENSITIVE;
                else
                    hVal = SQL_IC_LOWER;
            }
            else
                hVal = SQL_IC_LOWER;

            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_SCROLL_OPTIONS:
        {
            iVal = SQL_SO_FORWARD_ONLY
                    | SQL_SO_STATIC;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SCROLL_CONCURRENCY:
        {
            iVal = SQL_SCCO_READ_ONLY
                    | SQL_SCCO_OPT_VALUES;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SEARCH_PATTERN_ESCAPE:
        {
            rc = strInfoResponse("\\", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_SERVER_NAME:
        {
            rc = strInfoResponse((char *)((pConn->pConnectProps) ? pConn->pConnectProps->szHost : ""),
                                    pVal, cbLen, pcbLen, piRetType);

            break;
        }

        case SQL_SQL_CONFORMANCE:
        {
            iVal = SQL_SC_SQL92_ENTRY;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_DATETIME_FUNCTIONS:
        {
            iVal = SQL_SDF_CURRENT_DATE | SQL_SDF_CURRENT_TIME | SQL_SDF_CURRENT_TIMESTAMP;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_FOREIGN_KEY_DELETE_RULE:
        {
            iVal = SQL_SFKD_CASCADE | SQL_SFKD_NO_ACTION | SQL_SFKD_SET_DEFAULT | SQL_SFKD_SET_NULL;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:
        {
            iVal = SQL_SFKU_CASCADE | SQL_SFKU_NO_ACTION | SQL_SFKU_SET_DEFAULT | SQL_SFKU_SET_NULL;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_GRANT:
        {
            iVal = SQL_SG_WITH_GRANT_OPTION
                     | SQL_SG_DELETE_TABLE | SQL_SG_INSERT_TABLE | SQL_SG_INSERT_COLUMN | SQL_SG_REFERENCES_TABLE
                     | SQL_SG_SELECT_TABLE | SQL_SG_UPDATE_TABLE | SQL_SG_UPDATE_COLUMN;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
        {
            iVal = SQL_SNVF_BIT_LENGTH | SQL_SNVF_CHAR_LENGTH | SQL_SNVF_CHARACTER_LENGTH | SQL_SNVF_EXTRACT
                    | SQL_SNVF_OCTET_LENGTH | SQL_SNVF_POSITION;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_PREDICATES:
        {
            iVal = SQL_SP_EXISTS
                     | SQL_SP_ISNOTNULL | SQL_SP_ISNULL | SQL_SP_OVERLAPS | SQL_SP_UNIQUE | SQL_SP_LIKE
                     | SQL_SP_IN | SQL_SP_BETWEEN | SQL_SP_COMPARISON | SQL_SP_QUANTIFIED_COMPARISON;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
        {
            iVal = SQL_SRJO_CROSS_JOIN | SQL_SRJO_EXCEPT_JOIN | SQL_SRJO_FULL_OUTER_JOIN | SQL_SRJO_INNER_JOIN
                     | SQL_SRJO_INTERSECT_JOIN | SQL_SRJO_LEFT_OUTER_JOIN | SQL_SRJO_NATURAL_JOIN
                     | SQL_SRJO_RIGHT_OUTER_JOIN | SQL_SRJO_UNION_JOIN;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_REVOKE:
        {
            iVal = SQL_SR_GRANT_OPTION_FOR
                     | SQL_SR_CASCADE | SQL_SR_RESTRICT | SQL_SR_DELETE_TABLE | SQL_SR_INSERT_TABLE
                     | SQL_SR_REFERENCES_TABLE | SQL_SR_SELECT_TABLE | SQL_SR_UPDATE_TABLE | SQL_SR_UPDATE_COLUMN;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_ROW_VALUE_CONSTRUCTOR:
        {
            iVal = SQL_SRVC_VALUE_EXPRESSION | SQL_SRVC_NULL;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_STRING_FUNCTIONS:
        {
            iVal = SQL_SSF_CONVERT
                     | SQL_SSF_LOWER | SQL_SSF_UPPER | SQL_SSF_SUBSTRING | SQL_SSF_TRANSLATE
                     | SQL_SSF_TRIM_BOTH | SQL_SSF_TRIM_LEADING | SQL_SSF_TRIM_TRAILING;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SQL92_VALUE_EXPRESSIONS:
        {
            iVal = SQL_SVE_CASE
                    | SQL_SVE_CAST | SQL_SVE_COALESCE | SQL_SVE_NULLIF;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_STANDARD_CLI_CONFORMANCE:
        {
            iVal = SQL_SCC_ISO92_CLI;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_STATIC_CURSOR_ATTRIBUTES1:
        {
            iVal = SQL_CA1_NEXT
                     | SQL_CA1_ABSOLUTE | SQL_CA1_RELATIVE | SQL_CA1_BOOKMARK | SQL_CA1_LOCK_NO_CHANGE
                     | SQL_CA1_POS_POSITION | SQL_CA1_POS_UPDATE | SQL_CA1_POS_DELETE | SQL_CA1_POS_REFRESH
                     | SQL_CA1_POSITIONED_UPDATE | SQL_CA1_POSITIONED_DELETE | SQL_CA1_SELECT_FOR_UPDATE
                     | SQL_CA1_BULK_ADD;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_STATIC_CURSOR_ATTRIBUTES2:
        {
            iVal = SQL_CA2_READ_ONLY_CONCURRENCY | SQL_CA2_OPT_VALUES_CONCURRENCY | SQL_CA2_CRC_EXACT
                    | SQL_CA2_SIMULATE_TRY_UNIQUE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_STRING_FUNCTIONS:
        {
            iVal =  SQL_FN_STR_ASCII
                     | SQL_FN_STR_BIT_LENGTH
                     | SQL_FN_STR_CHAR
                     | SQL_FN_STR_CHAR_LENGTH 
                     | SQL_FN_STR_CHARACTER_LENGTH 
                     | SQL_FN_STR_CONCAT
                     | SQL_FN_STR_LCASE
                     | SQL_FN_STR_LENGTH
                     | SQL_FN_STR_LOCATE_2 
                     | SQL_FN_STR_LTRIM 
                     | SQL_FN_STR_OCTET_LENGTH
                     | SQL_FN_STR_POSITION
                     | SQL_FN_STR_REPEAT
                     | SQL_FN_STR_REPLACE 
                     | SQL_FN_STR_RTRIM 
                     | SQL_FN_STR_SUBSTRING 
                     | SQL_FN_STR_UCASE;

            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SUBQUERIES:
        {
            iVal = SQL_SQ_COMPARISON
                    | SQL_SQ_EXISTS | SQL_SQ_IN | SQL_SQ_QUANTIFIED | SQL_SQ_CORRELATED_SUBQUERIES;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_SYSTEM_FUNCTIONS:
        {
            iVal = SQL_FN_SYS_DBNAME
                    | SQL_FN_SYS_IFNULL
                    | SQL_FN_SYS_USERNAME;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_TABLE_TERM:
        {
            rc = strInfoResponse("table", pVal, cbLen, pcbLen, piRetType);
            break;
        }

        case SQL_TIMEDATE_FUNCTIONS:
        {
            iVal =  SQL_FN_TD_CURRENT_DATE
                     | SQL_FN_TD_CURRENT_TIME
                     | SQL_FN_TD_CURRENT_TIMESTAMP
                     | SQL_FN_TD_CURDATE  
                     | SQL_FN_TD_CURTIME 
                     | SQL_FN_TD_EXTRACT
                     | SQL_FN_TD_NOW;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_TXN_CAPABLE:
        {
            hVal = SQL_TC_ALL;
            rc = shortInfoResponse(hVal, phVal, pcbLen, piRetType);
            break;
        }

        case SQL_TXN_ISOLATION_OPTION:
        {
            iVal = SQL_TXN_READ_UNCOMMITTED | SQL_TXN_READ_COMMITTED | SQL_TXN_REPEATABLE_READ
                    | SQL_TXN_SERIALIZABLE;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_UNION:
        {
            iVal = SQL_U_UNION | SQL_U_UNION_ALL;
            rc = integerInfoResponse(iVal, piVal, pcbLen, piRetType);
            break;
        }

        case SQL_USER_NAME:
        {
            rc = strInfoResponse((char *)((pConn->pConnectProps) ? pConn->pConnectProps->szUser : ""),
                                    pVal, cbLen, pcbLen, piRetType);

            break;
        }

        case SQL_XOPEN_CLI_YEAR:
        {
            rc = strInfoResponse("1995", pVal, cbLen, pcbLen, piRetType);

            break;
        }

        default: 
        {
            rc = SQL_ERROR;
            addError(&pConn->pErrorList,"HYC00", "Optional field not implemented.", 0, NULL);
            goto error; 
        }
     } // Switch

    // Add warning
    if(rc == SQL_SUCCESS_WITH_INFO)
        addWarning(&pConn->pErrorList,"01004", "String data, right truncated", 0, NULL);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if function supported otherwise FALSE.
//
static int doesFunctionSupport(UWORD iFunction)
{
    switch( iFunction )
    {
        case    SQL_API_SQLALLOCCONNECT:          
        case    SQL_API_SQLALLOCENV:
        case    SQL_API_SQLALLOCHANDLE:
        case    SQL_API_SQLALLOCSTMT:         
        case    SQL_API_SQLBINDCOL:
        case    SQL_API_SQLBINDPARAM:
        case    SQL_API_SQLBINDPARAMETER:
        case    SQL_API_SQLBROWSECONNECT: 
        case    SQL_API_SQLBULKOPERATIONS:
        case    SQL_API_SQLCANCEL: 
        case    SQL_API_SQLCLOSECURSOR:
        case    SQL_API_SQLCOLATTRIBUTE:
        case    SQL_API_SQLCOLUMNPRIVILEGES: 
        case    SQL_API_SQLCOLUMNS:   
        case    SQL_API_SQLCONNECT:  
        case    SQL_API_SQLCOPYDESC: 
        case    SQL_API_SQLDATASOURCES:      
        case    SQL_API_SQLDESCRIBECOL:  
        case    SQL_API_SQLDESCRIBEPARAM:
        case    SQL_API_SQLDISCONNECT:  
        case    SQL_API_SQLDRIVERCONNECT:    
        case    SQL_API_SQLDRIVERS:
        case    SQL_API_SQLENDTRAN:
        case    SQL_API_SQLERROR:            
        case    SQL_API_SQLEXECDIRECT:       
        case    SQL_API_SQLEXECUTE: 
        case    SQL_API_SQLEXTENDEDFETCH: 
        case    SQL_API_SQLFETCH:  
        case    SQL_API_SQLFETCHSCROLL: 
        case    SQL_API_SQLFOREIGNKEYS:      
        case    SQL_API_SQLFREECONNECT:      
        case    SQL_API_SQLFREEENV:  
        case    SQL_API_SQLFREEHANDLE:
        case    SQL_API_SQLFREESTMT:         
        case    SQL_API_SQLGETCONNECTATTR:
        case    SQL_API_SQLGETCONNECTOPTION: 
        case    SQL_API_SQLGETCURSORNAME:    
        case    SQL_API_SQLGETDATA:  
        case    SQL_API_SQLGETDESCFIELD:
        case    SQL_API_SQLGETDESCREC:
        case    SQL_API_SQLGETDIAGFIELD:
        case    SQL_API_SQLGETDIAGREC:
        case    SQL_API_SQLGETENVATTR:
        case    SQL_API_SQLGETFUNCTIONS:     
        case    SQL_API_SQLGETINFO: 
        case    SQL_API_SQLGETSTMTATTR:
        case    SQL_API_SQLGETSTMTOPTION:    
        case    SQL_API_SQLGETTYPEINFO: 
        case    SQL_API_SQLMORERESULTS: 
        case    SQL_API_SQLNATIVESQL:
        case    SQL_API_SQLNUMRESULTCOLS:    
        case    SQL_API_SQLNUMPARAMS:        
        case    SQL_API_SQLPARAMDATA:        
        case    SQL_API_SQLPARAMOPTIONS:     
        case    SQL_API_SQLPREPARE:          
        case    SQL_API_SQLPRIMARYKEYS:      
        case    SQL_API_SQLPROCEDURECOLUMNS: 
        case    SQL_API_SQLPROCEDURES:       
        case    SQL_API_SQLPUTDATA:          
        case    SQL_API_SQLROWCOUNT:  
        case    SQL_API_SQLSETCONNECTATTR:
        case    SQL_API_SQLSETCONNECTOPTION: 
        case    SQL_API_SQLSETCURSORNAME:    
        case    SQL_API_SQLSETDESCFIELD:
        case    SQL_API_SQLSETDESCREC:
        case    SQL_API_SQLSETENVATTR:
        case    SQL_API_SQLSETPARAM:         
        case    SQL_API_SQLSETPOS:           
        case    SQL_API_SQLSETSCROLLOPTIONS: 
        case    SQL_API_SQLSETSTMTATTR:
        case    SQL_API_SQLSETSTMTOPTION:    
        case    SQL_API_SQLSPECIALCOLUMNS:   
        case    SQL_API_SQLSTATISTICS:       
        case    SQL_API_SQLTABLES:           
        case    SQL_API_SQLTABLEPRIVILEGES: 
        case    SQL_API_SQLTRANSACT: 
        {
            return TRUE;
        }

        default: break;
    }

    return FALSE;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetFunctions returns information about whether a driver supports a specific ODBC function. 
// This function is implemented in the Driver Manager; it can also be implemented in drivers. 
// If a driver implements SQLGetFunctions, the Driver Manager calls the function in the driver. 
// Otherwise, it executes the function itself.
//
SQLRETURN  SQL_API SQLGetFunctions(SQLHDBC phdbc,
                                   SQLUSMALLINT uhFunctionId, 
                                   SQLUSMALLINT *puhSupported)
{
    SQLRETURN   rc = SQL_SUCCESS;
    int i;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    beginApiMutex(NULL, phdbc);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetFunctions(FUNC_CALL, 0, phdbc, uhFunctionId, puhSupported);

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    if(puhSupported == NULL)
    {
        addError(&pConn->pErrorList,"HY000","Output buffer is NULL", 0, NULL);
        rc = SQL_ERROR;
        goto error;
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    if(uhFunctionId == SQL_API_ODBC3_ALL_FUNCTIONS)
    {
        memset(puhSupported,'\0',SQL_API_ODBC3_ALL_FUNCTIONS_SIZE); 

        // ODBC 2.x
        for(i = SQL_ODBC2_API_START; i <= SQL_ODBC2_API_LAST; i++)
        {
            int iVal = doesFunctionSupport((UWORD) i);

            if(iVal == TRUE)
            {
                SET_SQL_FUNCTION_BIT(puhSupported,i);
            }
        }

        // ODBC 3.x
        for(i=SQL_ODBC3_API_START; i<= SQL_ODBC3_API_LAST; i++)
        {
            int iVal = doesFunctionSupport((UWORD) i);

            if(iVal == TRUE)
            {
                SET_SQL_FUNCTION_BIT(puhSupported,i);
            }
        }
    }
    else
    if(uhFunctionId == SQL_API_ALL_FUNCTIONS)
    {
        // ODBC 2.x
        for ( i=SQL_ODBC2_API_START; i<= SQL_ODBC2_API_LAST; i++)
            *(puhSupported+i) = doesFunctionSupport((UWORD) i);
    }
    else
        *(puhSupported) = doesFunctionSupport(uhFunctionId);

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetFunctions(FUNC_RETURN, rc, phdbc, uhFunctionId, puhSupported);

    endApiMutex(NULL, phdbc);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return string type information for SQLGetInfo. 
//
static SQLRETURN strInfoResponse(char *pSrc, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen, int *piRetType)
{
    SQLRETURN rc = SQL_SUCCESS;
    size_t len;

    if(piRetType)
        *piRetType = SQL_C_CHAR;

    if(pSrc)
    {
        len = strlen(pSrc);

        if(pDest)
        {
            if(len == 0)
            {
                *pDest = '\0';
            }
            else
            {
                if((short)len < cbLen)
                    rs_strncpy(pDest, pSrc, cbLen);
                else
                {
                    if(cbLen > 0)
                    {
                        strncpy(pDest, pSrc, cbLen - 1);
                        pDest[cbLen-1] = '\0';
                    }
                    else
                        *pDest = '\0';

                    rc = SQL_SUCCESS_WITH_INFO;
                }
            }
        }
        else
            rc = SQL_SUCCESS_WITH_INFO;
    }
    else
        len = 0;

    if(pcbLen)
        *pcbLen = (SQLSMALLINT)len;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return short type information for SQLGetInfo.
//
static SQLRETURN shortInfoResponse(SQLUSMALLINT hData, SQLUSMALLINT *phBuf, SQLSMALLINT *pcbLen, int *piRetType)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(piRetType)
        *piRetType = SQL_C_SHORT;

    if(phBuf)
        *phBuf = hData;

    if(pcbLen)
        *pcbLen = sizeof(SQLUSMALLINT);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return integer type information for SQLGetInfo. 
//
static SQLRETURN integerInfoResponse(SQLUINTEGER iData, SQLUINTEGER *phBuf, SQLSMALLINT *pcbLen, int *piRetType)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(piRetType)
        *piRetType = SQL_C_LONG;

    if(phBuf)
        *phBuf = iData;

    if(pcbLen)
        *pcbLen = sizeof(SQLUINTEGER);

    return rc;
}

