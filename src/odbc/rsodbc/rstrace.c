/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rstrace.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rslock.h"
#include "rserror.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set trace level and trace file info.
//
void setTraceLevelAndFile(int iTracelLevel, char *pTraceFile)
{
    getGlobalLogVars()->iTraceLevel = iTracelLevel;

    if(pTraceFile && *pTraceFile != '\0')
        rs_strncpy(getGlobalLogVars()->szTraceFile, pTraceFile, sizeof(getGlobalLogVars()->szTraceFile));
    else
    {
        DWORD dwRetVal = 0;
        char  szTempPath[MAX_PATH + 1];

        dwRetVal = GetTempPath(MAX_PATH, szTempPath); 
        if (dwRetVal > MAX_PATH || (dwRetVal == 0))
        {
            szTempPath[0] = '\0';
        }

        snprintf(getGlobalLogVars()->szTraceFile,sizeof(getGlobalLogVars()->szTraceFile),"%s%s%s", szTempPath, (szTempPath[0] != '\0') ? PATH_SEPARATOR : "", TRACE_FILE_NAME);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create trace file.
// Deprecated
//
void createTraceFile()
{
    return;
    if(IS_TRACE_ON())
    {
        if(getGlobalLogVars()->szTraceFile[0] != '\0')
        {
            getGlobalLogVars()->fpTrace = fopen(getGlobalLogVars()->szTraceFile, (IS_TRACE_LEVEL_DEBUG_APPEND()) ? "a+" : "w+");

            if(!getGlobalLogVars()->fpTrace)
            {
                // Try to create under TEMP directory
                if(strchr(getGlobalLogVars()->szTraceFile, PATH_SEPARATOR_CHAR) == NULL)
                {
                    DWORD dwRetVal = 0;
                    char  szTempPath[MAX_PATH + 1];
                    char  szFileName[MAX_PATH + 1];

                    rs_strncpy(szFileName, getGlobalLogVars()->szTraceFile,sizeof(szFileName));

                    dwRetVal = GetTempPath(MAX_PATH, szTempPath); 
                    if (dwRetVal > MAX_PATH || (dwRetVal == 0))
                    {
                        szTempPath[0] = '\0';
                    }

                    if(szTempPath[0] != '\0')
                    {
                        snprintf(szFileName,sizeof(szFileName),"%s%s%s", szTempPath, PATH_SEPARATOR, getGlobalLogVars()->szTraceFile);
                        rs_strncpy(getGlobalLogVars()->szTraceFile, szFileName, sizeof(getGlobalLogVars()->szTraceFile));

                        getGlobalLogVars()->fpTrace = fopen(getGlobalLogVars()->szTraceFile, (IS_TRACE_LEVEL_DEBUG_APPEND()) ? "a+" : "w+");
                    }
                }
            }

            if(!getGlobalLogVars()->fpTrace)
                getGlobalLogVars()->iTraceLevel = LOG_LEVEL_OFF;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get trace file handle.
//
FILE *getTraceFileHandle()
{
    // return getGlobalLogVars()->fpTrace;
    return NULL;
}

//---------------------------------------------------------------------------------------------------------igarish
// Get trace file name.
//
char *getTraceFileName()
{
    return getGlobalLogVars()->szTraceFile;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Close the trace file.
// Deprecated
//
void closeTraceFile()
{
    return;
    if(IS_TRACE_ON())
    {
        if(getGlobalLogVars()->fpTrace)
        {
            fclose(getGlobalLogVars()->fpTrace);
            getGlobalLogVars()->fpTrace = NULL;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceError(const char *fmt,...)
{
    WRITE_INTO_FILE(TRUE);
}

/*====================================================================================================================================================*/
void RsTrace::traceAPICall(const char *fmt,...)
{
    WRITE_INTO_FILE(TRUE);
}

/*====================================================================================================================================================*/

void RsTrace::traceArg(const char *fmt,...)
{
    WRITE_INTO_FILE(TRUE);
}

/*====================================================================================================================================================*/

void RsTrace::traceArgVal(const char *fmt,...)
{
    WRITE_INTO_FILE(FALSE);
}

/*====================================================================================================================================================*/

void RsTrace::traceHandle(const char *pArgName, SQLHANDLE handle)
{
    if (handle == NULL)    
        traceArg("\t%s=NULL",pArgName);
    else
    {
#ifdef WIN32
        traceArg("\t%s=0x%p",pArgName,handle);
#endif
#if defined LINUX 
        traceArg("\t%s=%p",pArgName,handle);
#endif
    }
}

/*====================================================================================================================================================*/

void RsTrace::tracePointer(const char *var, void *ptr)
{
    if (ptr == NULL)    
        traceArg("\t%s=NULL",var);
    else
    {
#ifdef WIN32
        traceArg("\t%s=0x%p",var,ptr);
#endif
#if defined LINUX 
        traceArg("\t%s=%p",var,ptr);
#endif
    }
}

/*====================================================================================================================================================*/

char* RsTrace::getRcString(SQLRETURN iRc)
{
    switch(iRc)
    {
        case SQL_SUCCESS:            return "SQL_SUCCESS";
        case SQL_SUCCESS_WITH_INFO: return("SQL_SUCCESS_WITH_INFO");
        case SQL_ERROR:                return("SQL_ERROR");
        case SQL_INVALID_HANDLE:    return("SQL_INVALID_HANDLE");
        case SQL_NO_DATA:           return("SQL_NO_DATA");
        case SQL_NEED_DATA:         return("SQL_NEED_DATA");
        case SQL_STILL_EXECUTING:   return("SQL_STILL_EXECUTING");
        default:                    return("");    
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceErrorList(SQLHENV phenv,SQLHDBC phdbc,SQLHSTMT phstmt,SQLHDESC phdesc)
{
    if(IS_TRACE_LEVEL_ERROR())
    {
        SQLRETURN rc;
        char errorMsg[MAX_ERR_MSG_LEN];
        char sqlState[MAX_SQL_STATE_LEN];
        SQLINTEGER iNativeError;
        int iRecNo = 1;

        do
        {
            errorMsg[0] = '\0';
            sqlState[0] = '\0';
            iNativeError = 0;

            rc = RsError::RS_SQLError(phenv, phdbc, phstmt, phdesc, (SQLCHAR *)sqlState, &iNativeError, (SQLCHAR *)errorMsg, MAX_ERR_MSG_LEN, NULL, iRecNo++, FALSE);

            if(SQL_SUCCEEDED(rc))
                traceError("SQLState:%s NativeCode:%d Error:%s",sqlState, iNativeError, errorMsg);

        }while(SQL_SUCCEEDED(rc));
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceHandleType(SQLSMALLINT hHandleType)
{
    switch(hHandleType) 
    {
        case SQL_HANDLE_ENV:
            traceArg("\thHandleType=SQL_HANDLE_ENV");
            break;
        case SQL_HANDLE_DBC :
            traceArg("\thHandleType=SQL_HANDLE_DBC");
            break;
        case SQL_HANDLE_STMT:
            traceArg("\thHandleType=SQL_HANDLE_STMT");
            break;
        case SQL_HANDLE_DESC:
            traceArg("\thHandleType=SQL_HANDLE_DESC");
            break;
        default:
            traceShortVal("hHandleType",hHandleType);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceClosingBracket()
{
    traceArg("\t )");
}

/*====================================================================================================================================================*/

void RsTrace::traceStrValWithSmallLen(const char *pArgName, const char *pVal, SQLSMALLINT cbLen)
{
    if(pVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        if (cbLen == SQL_NULL_DATA) 
            traceArg("\t%s=NULL_DATA",pArgName);
        else 
        if (cbLen == SQL_NTS) 
            traceArg("\t%s=%.*s",pArgName,TRACE_MAX_STR_VAL_LEN,pVal);
        else 
        if(cbLen > 0) 
        {
            traceArg("\t%s=%.*s",pArgName,(cbLen > TRACE_MAX_STR_VAL_LEN) ? TRACE_MAX_STR_VAL_LEN : cbLen,
                                    pVal);
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceStrValWithLargeLen(const char *pArgName, const char *pVal, SQLINTEGER cbLen)
{
    if(pVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        if (cbLen == SQL_NULL_DATA) 
            traceArg("\t%s=NULL_DATA",pArgName);
        else 
        if (cbLen == SQL_NTS) 
            traceArg("\t%s=%.*s",pArgName,TRACE_MAX_STR_VAL_LEN,pVal);
        else 
        if(cbLen > 0) 
        {
            traceArg("\t%s=%.*s",pArgName,(cbLen > TRACE_MAX_STR_VAL_LEN) ? TRACE_MAX_STR_VAL_LEN : cbLen,
                                    pVal);
        }
    }
}

/*====================================================================================================================================================*/

/*====================================================================================================================================================*/

void RsTrace::traceLongLongPtrVal(const char *pArgName, long long*pllVal)
{
    if(pllVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
        traceLongLongVal(pArgName,*pllVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceIntPtrVal(const char *pArgName, int *piVal)
{
    if(piVal == NULL)
        traceArg("\t%s=NULL",pArgName);
    else
        traceIntVal(pArgName,*piVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceLongPtrVal(const char *pArgName, long *plVal)
{
    if(plVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
        traceLongVal(pArgName,*plVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceShortPtrVal(const char *pArgName, short *phVal)
{
    if(phVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
        traceShortVal(pArgName,*phVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceFloatPtrVal(const char *pArgName, float *pfVal)
{
    if(pfVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
        traceFloatVal(pArgName,*pfVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceDoublePtrVal(const char *pArgName, double *pdVal)
{
    if(pdVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
        traceDoubleVal(pArgName,*pdVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceBitPtrVal(const char *pArgName, char *pbVal)
{
    if(pbVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
        traceBitVal(pArgName,*pbVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceDatePtrVal(const char *pArgName, DATE_STRUCT *pdtVal)
{
    if(pdtVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        traceArg("\t%s=%hd/%hd/%hd",pArgName,pdtVal->month,pdtVal->day,pdtVal->year);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceTimeStampPtrVal(const char *pArgName, TIMESTAMP_STRUCT *ptsVal)
{
    if(ptsVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        traceArg("\t%s=%hd/%hd/%hd %hd:%hd:%hd.%ld",pArgName,ptsVal->month,ptsVal->day,ptsVal->year,
                                                    ptsVal->hour, ptsVal->minute, ptsVal->second, 
                                                    ptsVal->fraction);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceTimePtrVal(const char *pArgName, TIME_STRUCT *ptVal)
{
    if(ptVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        traceArg("\t%s=%hd:%hd:%hd",pArgName, ptVal->hour, ptVal->minute, ptVal->second);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceNumericPtrVal(const char *pArgName, SQL_NUMERIC_STRUCT *pnVal)
{
    if(pnVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        int i;

        traceArgVal("\t%s=",pArgName);
        for(i = 0; i < SQL_MAX_NUMERIC_LEN;i++)
            traceArgVal(" %02X ", pnVal->val[i]);
        traceArgVal("\n");
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceStrSmallLen(const char *pArgName, SQLSMALLINT cbLen)
{
    if (cbLen == SQL_NULL_DATA) 
        traceArg("\t%s=%s",pArgName,"SQL_NULL_DATA");
    else 
    if (cbLen == SQL_NTS) 
        traceArg("\t%s=%s",pArgName,"SQL_NTS");
    else 
        traceShortVal(pArgName, cbLen);
}

/*====================================================================================================================================================*/

void RsTrace::traceStrLargeLen(const char *pArgName, SQLINTEGER cbLen)
{
    if (cbLen == SQL_NULL_DATA) 
        traceArg("\t%s=%s",pArgName,"SQL_NULL_DATA");
    else 
    if (cbLen == SQL_NTS) 
        traceArg("\t%s=%s",pArgName,"SQL_NTS");
    else 
        traceArg("\t%s=%ld",pArgName, cbLen);
}

/*====================================================================================================================================================*/

void RsTrace::traceStrOutSmallLen(const char *pArgName, SQLSMALLINT *pcbLen)
{
    if(pcbLen)
    {
        SQLSMALLINT cbLen = *pcbLen;

        if (cbLen == SQL_NULL_DATA) 
            traceArg("\t%s=%s",pArgName,"SQL_NULL_DATA");
        else 
        if (cbLen == SQL_NTS) 
            traceArg("\t%s=%s",pArgName,"SQL_NTS");
        else 
            traceShortVal(pArgName, cbLen);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceStrOutLargeLen(const char *pArgName, SQLINTEGER *pcbLen)
{
    if(pcbLen)
    {
        SQLINTEGER cbLen = *pcbLen;

        if (cbLen == SQL_NULL_DATA) 
            traceArg("\t%s=%s",pArgName,"SQL_NULL_DATA");
        else 
        if (cbLen == SQL_NTS) 
            traceArg("\t%s=%s",pArgName,"SQL_NTS");
        else 
            traceLongVal(pArgName, cbLen);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceDiagIdentifier(SQLSMALLINT hDiagIdentifier)
{
    switch(hDiagIdentifier) 
    {
        case SQL_DIAG_CURSOR_ROW_COUNT:
            traceArg("\thDiagIdentifier=SQL_DIAG_CURSOR_ROW_COUNT");
            break;
        case SQL_DIAG_DYNAMIC_FUNCTION:
            traceArg("\thDiagIdentifier=SQL_DIAG_DYNAMIC_FUNCTION");
            break;
        case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
            traceArg("\thDiagIdentifier=SQL_DIAG_DYNAMIC_FUNCTION_CODE");
            break;
        case SQL_DIAG_RETURNCODE:
            traceArg("\thDiagIdentifier=SQL_DIAG_RETURNCODE");
            break;
        case SQL_DIAG_NUMBER:
            traceArg("\thDiagIdentifier=SQL_DIAG_NUMBER");
            break;
        case SQL_DIAG_ROW_COUNT:
            traceArg("\thDiagIdentifier=SQL_DIAG_ROW_COUNT");
            break;
        case SQL_DIAG_CLASS_ORIGIN:
            traceArg("\thDiagIdentifier=SQL_DIAG_CLASS_ORIGIN");
            break;
        case SQL_DIAG_SUBCLASS_ORIGIN:
            traceArg("\thDiagIdentifier=SQL_DIAG_SUBCLASS_ORIGIN");
            break;
        case SQL_DIAG_CONNECTION_NAME:
            traceArg("\thDiagIdentifier=SQL_DIAG_CONNECTION_NAME");
            break;
        case SQL_DIAG_SERVER_NAME:
            traceArg("\thDiagIdentifier=SQL_DIAG_SERVER_NAME");
            break;
        case SQL_DIAG_MESSAGE_TEXT:
            traceArg("\thDiagIdentifier=SQL_DIAG_MESSAGE_TEXT");
            break;
        case SQL_DIAG_NATIVE:
            traceArg("\thDiagIdentifier=SQL_DIAG_NATIVE");
            break;
        case SQL_DIAG_SQLSTATE:
            traceArg("\thDiagIdentifier=SQL_DIAG_SQLSTATE");
            break;
        case SQL_DIAG_COLUMN_NUMBER:
            traceArg("\thDiagIdentifier=SQL_DIAG_COLUMN_NUMBER");
            break;
        case SQL_DIAG_ROW_NUMBER:
            traceArg("\thDiagIdentifier=SQL_DIAG_ROW_NUMBER");
            break;
        default:
            traceShortVal("hDiagIdentifier",hDiagIdentifier);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceDiagIdentifierOutput(SQLSMALLINT hDiagIdentifier,
                                SQLPOINTER  pDiagInfo, 
                                SQLSMALLINT cbLen,
                                int iUnicode)
{
    switch(hDiagIdentifier) 
    {
        case SQL_DIAG_CLASS_ORIGIN:
        case SQL_DIAG_SUBCLASS_ORIGIN:
        case SQL_DIAG_CONNECTION_NAME:
        case SQL_DIAG_SERVER_NAME:
        case SQL_DIAG_MESSAGE_TEXT:
        case SQL_DIAG_SQLSTATE:
        {
            if(iUnicode)
                traceWStrValWithSmallLen("*pwDiagInfo",(SQLWCHAR *)pDiagInfo, (cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
            else
                traceStrValWithSmallLen("*pDiagInfo",(char *)pDiagInfo,cbLen);

            break;
        }

        case SQL_DIAG_NATIVE:
        case SQL_DIAG_NUMBER:
        {
            traceIntPtrVal((char *)((iUnicode) ? "*pwDiagInfo" : "*pDiagInfo"), (int *)pDiagInfo);
            break;
        }

        default:
        {
            traceArg("\t%s=Couldn't trace it",(iUnicode) ? "*pwDiagInfo" : "*pDiagInfo");
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceGetInfoType(SQLUSMALLINT hInfoType)
{
    switch (hInfoType) 
    {
        case SQL_ACCESSIBLE_PROCEDURES: 
        {
            traceArg("\thInfoType=SQL_ACCESSIBLE_PROCEDURES");
            break;
        }

        case SQL_ACCESSIBLE_TABLES: 
        {
            traceArg("\thInfoType=SQL_ACCESSIBLE_TABLES");
            break;
        }

        case SQL_ACTIVE_ENVIRONMENTS:
        {
            traceArg("\thInfoType=SQL_ACTIVE_ENVIRONMENTS");
            break;
        }

        case SQL_AGGREGATE_FUNCTIONS:
        {
            traceArg("\thInfoType=SQL_AGGREGATE_FUNCTIONS");
            break;
        }

        case SQL_ALTER_DOMAIN:
        {
            traceArg("\thInfoType=SQL_ALTER_DOMAIN");
            break;
        }

        case SQL_ALTER_TABLE: 
        {
            traceArg("\thInfoType=SQL_ALTER_TABLE");
            break;
        }

        case SQL_ASYNC_MODE: 
        {
            traceArg("\thInfoType=SQL_ASYNC_MODE");
            break;
        }

        case SQL_BATCH_ROW_COUNT: 
        {
            traceArg("\thInfoType=SQL_BATCH_ROW_COUNT");
            break;
        }

        case SQL_BATCH_SUPPORT: 
        {
            traceArg("\thInfoType=SQL_BATCH_SUPPORT");
            break;
        }

        case SQL_BOOKMARK_PERSISTENCE: 
        {
            traceArg("\thInfoType=SQL_BOOKMARK_PERSISTENCE");
            break;
        }

        case SQL_QUALIFIER_LOCATION: 
        {
            traceArg("\thInfoType=SQL_QUALIFIER_LOCATION");
            break;
        }

        case SQL_CATALOG_NAME: 
        {
            traceArg("\thInfoType=SQL_CATALOG_NAME");
            break;
        }

        case SQL_QUALIFIER_NAME_SEPARATOR: 
        {
            traceArg("\thInfoType=SQL_QUALIFIER_NAME_SEPARATOR");
            break;
        }

        case SQL_QUALIFIER_TERM: 
        {
            traceArg("\thInfoType=SQL_QUALIFIER_TERM");
            break;
        }

        case SQL_QUALIFIER_USAGE: 
        {
            traceArg("\thInfoType=SQL_QUALIFIER_USAGE");
            break;
        }

        case SQL_COLLATION_SEQ: 
        {
            traceArg("\thInfoType=SQL_COLLATION_SEQ");
            break;
        }

        case SQL_COLUMN_ALIAS: 
        {
            traceArg("\thInfoType=SQL_COLUMN_ALIAS");
            break;
        }

        case SQL_CONCAT_NULL_BEHAVIOR: 
        {
            traceArg("\thInfoType=SQL_CONCAT_NULL_BEHAVIOR");
            break;
        }

        case SQL_CONVERT_BIGINT: 
        {
            traceArg("\thInfoType=SQL_CONVERT_BIGINT");
            break;
        }

        case SQL_CONVERT_BINARY: 
        {
            traceArg("\thInfoType=SQL_CONVERT_BINARY");
            break;
        }

        case SQL_CONVERT_BIT: 
        {
            traceArg("\thInfoType=SQL_CONVERT_BIT");
            break;
        }

        case SQL_CONVERT_CHAR: 
        {
            traceArg("\thInfoType=SQL_CONVERT_CHAR");
            break;
        }

        case SQL_CONVERT_DATE: 
        {
            traceArg("\thInfoType=SQL_CONVERT_DATE");
            break;
        }

        case SQL_CONVERT_DECIMAL: 
        {
            traceArg("\thInfoType=SQL_CONVERT_DECIMAL");
            break; 
        }

        case SQL_CONVERT_DOUBLE: 
        {
            traceArg("\thInfoType=SQL_CONVERT_DOUBLE");
            break;
        }

        case SQL_CONVERT_FLOAT: 
        {
            traceArg("\thInfoType=SQL_CONVERT_FLOAT");
            break;
        }

        case SQL_CONVERT_INTEGER: 
        {
            traceArg("\thInfoType=SQL_CONVERT_INTEGER");
            break;
        }

        case SQL_CONVERT_INTERVAL_YEAR_MONTH: 
        {
            traceArg("\thInfoType=SQL_CONVERT_INTERVAL_YEAR_MONTH");
            break;
        }

        case SQL_CONVERT_INTERVAL_DAY_TIME: 
        {
            traceArg("\thInfoType=SQL_CONVERT_INTERVAL_DAY_TIME");
            break;
        }

        case SQL_CONVERT_LONGVARBINARY: 
        {
            traceArg("\thInfoType=SQL_CONVERT_LONGVARBINARY");
            break; 
        }

        case SQL_CONVERT_LONGVARCHAR: 
        {
            traceArg("\thInfoType=SQL_CONVERT_LONGVARCHAR");
            break;
        }

        case SQL_CONVERT_NUMERIC: 
        {
            traceArg("\thInfoType=SQL_CONVERT_NUMERIC");
            break;
        }

        case SQL_CONVERT_REAL: 
        {
            traceArg("\thInfoType=SQL_CONVERT_REAL");
            break;
        }

        case SQL_CONVERT_SMALLINT: 
        {
            traceArg("\thInfoType=SQL_CONVERT_SMALLINT");
            break;
        }

        case SQL_CONVERT_TIME: 
        {
            traceArg("\thInfoType=SQL_CONVERT_TIME");
            break;
        }

        case SQL_CONVERT_TIMESTAMP: 
        {
            traceArg("\thInfoType=SQL_CONVERT_TIMESTAMP");
            break;
        }

        case SQL_CONVERT_TINYINT: 
        {
            traceArg("\thInfoType=SQL_CONVERT_TINYINT");
            break;
        }

        case SQL_CONVERT_VARBINARY: 
        {
            traceArg("\thInfoType=SQL_CONVERT_VARBINARY");
            break;
        }

        case SQL_CONVERT_VARCHAR: 
        {
            traceArg("\thInfoType=SQL_CONVERT_VARCHAR");
            break;
        }

        case SQL_CONVERT_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_CONVERT_FUNCTIONS");
            break;  
        }

        case SQL_CORRELATION_NAME: 
        {
            traceArg("\thInfoType=SQL_CORRELATION_NAME");
            break;
        }

        case SQL_CREATE_ASSERTION: 
        {
            traceArg("\thInfoType=SQL_CREATE_ASSERTION");
            break;
        }

        case SQL_CREATE_CHARACTER_SET: 
        {
            traceArg("\thInfoType=SQL_CREATE_CHARACTER_SET");
            break;
        }

        case SQL_CREATE_COLLATION: 
        {
            traceArg("\thInfoType=SQL_CREATE_COLLATION");
            break;
        }

        case SQL_CREATE_DOMAIN: 
        {
            traceArg("\thInfoType=SQL_CREATE_DOMAIN");
            break;
        }

        case SQL_CREATE_SCHEMA: 
        {
            traceArg("\thInfoType=SQL_CREATE_SCHEMA");
            break;
        }

        case SQL_CREATE_TABLE: 
        {
            traceArg("\thInfoType=SQL_CREATE_TABLE");
            break;
        }

        case SQL_CREATE_TRANSLATION: 
        {
            traceArg("\thInfoType=SQL_CREATE_TRANSLATION");
            break;
        }

        case SQL_CREATE_VIEW: 
        {
            traceArg("\thInfoType=SQL_CREATE_VIEW");
            break;
        }

        case SQL_CURSOR_COMMIT_BEHAVIOR: 
        {
            traceArg("\thInfoType=SQL_CURSOR_COMMIT_BEHAVIOR");
            break;
        }

        case SQL_CURSOR_ROLLBACK_BEHAVIOR: 
        {
            traceArg("\thInfoType=SQL_CURSOR_ROLLBACK_BEHAVIOR");
            break;
        }

        case SQL_CURSOR_SENSITIVITY: 
        {
            traceArg("\thInfoType=SQL_CURSOR_SENSITIVITY");
            break;
        }

        case SQL_DATA_SOURCE_NAME: 
        {
            traceArg("\thInfoType=SQL_DATA_SOURCE_NAME");
            break;
        }

        case SQL_DATA_SOURCE_READ_ONLY: 
        {
            traceArg("\thInfoType=SQL_DATA_SOURCE_READ_ONLY");
            break;
        }

        case SQL_DATABASE_NAME: 
        {
            traceArg("\thInfoType=SQL_DATABASE_NAME");
            break;
        }

        case SQL_DATETIME_LITERALS: 
        {
            traceArg("\thInfoType=SQL_DATETIME_LITERALS");
            break;
        }

        case SQL_DBMS_NAME: 
        {
            traceArg("\thInfoType=SQL_DBMS_NAME");
            break;
        }

        case SQL_DBMS_VER: 
        {
            traceArg("\thInfoType=SQL_DBMS_VER");
            break; 
        }

        case SQL_DDL_INDEX: 
        {
            traceArg("\thInfoType=SQL_DDL_INDEX");
            break; 
        }

        case SQL_DEFAULT_TXN_ISOLATION: 
        {
            traceArg("\thInfoType=SQL_DEFAULT_TXN_ISOLATION");
            break;
        }

        case SQL_DESCRIBE_PARAMETER: 
        {
            traceArg("\thInfoType=SQL_DESCRIBE_PARAMETER");
            break;
        }

        case SQL_DM_VER: 
        {
            traceArg("\thInfoType=SQL_DM_VER");
            break;  
        }

        case SQL_DRIVER_NAME: 
        {
            traceArg("\thInfoType=SQL_DRIVER_NAME");
            break;  
        }

        case SQL_DRIVER_ODBC_VER: 
        {
            traceArg("\thInfoType=SQL_DRIVER_ODBC_VER");
            break;
        }

        case SQL_DRIVER_VER: 
        {
            traceArg("\thInfoType=SQL_DRIVER_VER");
            break;  
        }

        case SQL_DROP_ASSERTION: 
        {
            traceArg("\thInfoType=SQL_DROP_ASSERTION");
            break;  
        }

        case SQL_DROP_CHARACTER_SET: 
        {
            traceArg("\thInfoType=SQL_DROP_CHARACTER_SET");
            break;  
        }

        case SQL_DROP_COLLATION: 
        {
            traceArg("\thInfoType=SQL_DROP_COLLATION");
            break;  
        }

        case SQL_DROP_DOMAIN: 
        {
            traceArg("\thInfoType=SQL_DROP_DOMAIN");
            break;  
        }

        case SQL_DROP_SCHEMA: 
        {
            traceArg("\thInfoType=SQL_DROP_SCHEMA");
            break;  
        }

        case SQL_DROP_TABLE: 
        {
            traceArg("\thInfoType=SQL_DROP_TABLE");
            break;  
        }

        case SQL_DROP_TRANSLATION: 
        {
            traceArg("\thInfoType=SQL_DROP_TRANSLATION");
            break;  
        }

        case SQL_DROP_VIEW: 
        {
            traceArg("\thInfoType=SQL_DROP_VIEW");
            break;  
        }

        case SQL_DYNAMIC_CURSOR_ATTRIBUTES1: 
        {
            traceArg("\thInfoType=SQL_DYNAMIC_CURSOR_ATTRIBUTES1");
            break;  
        }

        case SQL_DYNAMIC_CURSOR_ATTRIBUTES2: 
        {
            traceArg("\thInfoType=SQL_DYNAMIC_CURSOR_ATTRIBUTES2");
            break;  
        }

        case SQL_EXPRESSIONS_IN_ORDERBY: 
        {
            traceArg("\thInfoType=SQL_EXPRESSIONS_IN_ORDERBY");
            break;  
        }

        case SQL_FETCH_DIRECTION: 
        {
            traceArg("\thInfoType=SQL_FILE_USAGE");
            break;  
        }

        case SQL_FILE_USAGE: 
        {
            traceArg("\thInfoType=SQL_FILE_USAGE");
            break;  
        }

        case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1: 
        {
            traceArg("\thInfoType=SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1");
            break;  
        }

        case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2: 
        {
            traceArg("\thInfoType=SQL_DROP_TABLE");
            break;  
        }

        case SQL_GETDATA_EXTENSIONS: 
        {
            traceArg("\thInfoType=SQL_GETDATA_EXTENSIONS");
            break;  
        }

        case SQL_GROUP_BY: 
        {
            traceArg("\thInfoType=SQL_GROUP_BY");
            break;  
        }

        case SQL_IDENTIFIER_CASE: 
        {
            traceArg("\thInfoType=SQL_IDENTIFIER_CASE");
            break;  
        }

        case SQL_IDENTIFIER_QUOTE_CHAR: 
        {
            traceArg("\thInfoType=SQL_IDENTIFIER_QUOTE_CHAR");
            break;  
        }

        case SQL_INDEX_KEYWORDS: 
        {
            traceArg("\thInfoType=SQL_INDEX_KEYWORDS");
            break;  
        }

        case SQL_INFO_SCHEMA_VIEWS: 
        {
            traceArg("\thInfoType=SQL_INFO_SCHEMA_VIEWS");
            break;  
        }

        case SQL_INSERT_STATEMENT: 
        {
            traceArg("\thInfoType=SQL_INSERT_STATEMENT");
            break;  
        }

        case SQL_ODBC_SQL_OPT_IEF: 
        {
            traceArg("\thInfoType=SQL_ODBC_SQL_OPT_IEF");
            break;  
        }

        case SQL_KEYSET_CURSOR_ATTRIBUTES1: 
        {
            traceArg("\thInfoType=SQL_KEYSET_CURSOR_ATTRIBUTES1");
            break;  
        }

        case SQL_KEYSET_CURSOR_ATTRIBUTES2: 
        {
            traceArg("\thInfoType=SQL_KEYSET_CURSOR_ATTRIBUTES2");
            break;  
        }

        case SQL_KEYWORDS: 
        {
            traceArg("\thInfoType=SQL_KEYWORDS");
            break;  
        }

        case SQL_LIKE_ESCAPE_CLAUSE: 
        {
            traceArg("\thInfoType=SQL_LIKE_ESCAPE_CLAUSE");
            break;  
        }

        case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS: 
        {
            traceArg("\thInfoType=SQL_MAX_ASYNC_CONCURRENT_STATEMENTS");
            break;  
        }

        case SQL_MAX_BINARY_LITERAL_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_BINARY_LITERAL_LEN");
            break;  
        }

        case SQL_MAX_QUALIFIER_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_QUALIFIER_NAME_LEN");
            break;  
        }

        case SQL_MAX_CHAR_LITERAL_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_CHAR_LITERAL_LEN");
            break;  
        }

        case SQL_MAX_COLUMN_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_COLUMN_NAME_LEN");
            break;  
        }

        case SQL_MAX_COLUMNS_IN_GROUP_BY: 
        {
            traceArg("\thInfoType=SQL_MAX_COLUMNS_IN_GROUP_BY");
            break;  
        }

        case SQL_MAX_COLUMNS_IN_INDEX: 
        {
            traceArg("\thInfoType=SQL_MAX_COLUMNS_IN_INDEX");
            break;  
        }

        case SQL_MAX_COLUMNS_IN_ORDER_BY: 
        {
            traceArg("\thInfoType=SQL_MAX_COLUMNS_IN_ORDER_BY");
            break;  
        }

        case SQL_MAX_COLUMNS_IN_SELECT: 
        {
            traceArg("\thInfoType=SQL_MAX_COLUMNS_IN_SELECT");
            break;  
        }

        case SQL_MAX_COLUMNS_IN_TABLE: 
        {
            traceArg("\thInfoType=SQL_MAX_COLUMNS_IN_TABLE");
            break;  
        }

        case SQL_ACTIVE_STATEMENTS: 
        {
            traceArg("\thInfoType=SQL_ACTIVE_STATEMENTS");
            break;  
        }

        case SQL_MAX_CURSOR_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_CURSOR_NAME_LEN");
            break;  
        }

        case SQL_ACTIVE_CONNECTIONS: 
        {
            traceArg("\thInfoType=SQL_ACTIVE_CONNECTIONS");
            break;  
        }

        case SQL_MAX_IDENTIFIER_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_IDENTIFIER_LEN");
            break;  
        }

        case SQL_MAX_INDEX_SIZE: 
        {
            traceArg("\thInfoType=SQL_MAX_INDEX_SIZE");
            break;  
        }

        case SQL_MAX_PROCEDURE_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_PROCEDURE_NAME_LEN");
            break;  
        }

        case SQL_MAX_ROW_SIZE: 
        {
            traceArg("\thInfoType=SQL_MAX_ROW_SIZE");
            break;  
        }

        case SQL_MAX_ROW_SIZE_INCLUDES_LONG: 
        {
            traceArg("\thInfoType=SQL_MAX_ROW_SIZE_INCLUDES_LONG");
            break;  
        }

        case SQL_MAX_OWNER_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_OWNER_NAME_LEN");
            break;  
        }

        case SQL_MAX_STATEMENT_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_STATEMENT_LEN");
            break;  
        }

        case SQL_MAX_TABLE_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_TABLE_NAME_LEN");
            break;  
        }

        case SQL_MAX_TABLES_IN_SELECT: 
        {
            traceArg("\thInfoType=SQL_MAX_TABLES_IN_SELECT");
            break;  
        }

        case SQL_MAX_USER_NAME_LEN: 
        {
            traceArg("\thInfoType=SQL_MAX_USER_NAME_LEN");
            break;  
        }

        case SQL_MULT_RESULT_SETS: 
        {
            traceArg("\thInfoType=SQL_MULT_RESULT_SETS");
            break;  
        }

        case SQL_MULTIPLE_ACTIVE_TXN: 
        {
            traceArg("\thInfoType=SQL_MULTIPLE_ACTIVE_TXN");
            break;  
        }

        case SQL_NEED_LONG_DATA_LEN: 
        {
            traceArg("\thInfoType=SQL_NEED_LONG_DATA_LEN");
            break;  
        }

        case SQL_NON_NULLABLE_COLUMNS: 
        {
            traceArg("\thInfoType=SQL_NON_NULLABLE_COLUMNS");
            break;  
        }

        case SQL_NULL_COLLATION: 
        {
            traceArg("\thInfoType=SQL_NULL_COLLATION");
            break;  
        }

        case SQL_NUMERIC_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_NUMERIC_FUNCTIONS");
            break;  
        }

        case SQL_ODBC_API_CONFORMANCE: 
        {
            traceArg("\thInfoType=SQL_ODBC_API_CONFORMANCE");
            break;  
        }

        case SQL_ODBC_INTERFACE_CONFORMANCE: 
        {
            traceArg("\thInfoType=SQL_ODBC_INTERFACE_CONFORMANCE");
            break;  
        }

        case SQL_ODBC_SAG_CLI_CONFORMANCE: 
        {
            traceArg("\thInfoType=SQL_ODBC_SAG_CLI_CONFORMANCE");
            break;  
        }

        case SQL_ODBC_SQL_CONFORMANCE: 
        {
            traceArg("\thInfoType=SQL_ODBC_SQL_CONFORMANCE");
            break;  
        }

        case SQL_OJ_CAPABILITIES: 
        {
            traceArg("\thInfoType=SQL_OJ_CAPABILITIES");
            break;  
        }

        case SQL_ORDER_BY_COLUMNS_IN_SELECT: 
        {
            traceArg("\thInfoType=SQL_ORDER_BY_COLUMNS_IN_SELECT");
            break;  
        }

        case SQL_PARAM_ARRAY_ROW_COUNTS: 
        {
            traceArg("\thInfoType=SQL_PARAM_ARRAY_ROW_COUNTS");
            break;  
        }

        case SQL_PARAM_ARRAY_SELECTS: 
        {
            traceArg("\thInfoType=SQL_PARAM_ARRAY_SELECTS");
            break;  
        }

        case SQL_POS_OPERATIONS: 
        {
            traceArg("\thInfoType=SQL_POS_OPERATIONS");
            break;  
        }

        case SQL_POSITIONED_STATEMENTS: 
        {
            traceArg("\thInfoType=SQL_POSITIONED_STATEMENTS");
            break;  
        }

        case SQL_PROCEDURE_TERM: 
        {
            traceArg("\thInfoType=SQL_PROCEDURE_TERM");
            break;  
        }

        case SQL_PROCEDURES: 
        {
            traceArg("\thInfoType=SQL_PROCEDURES");
            break;  
        }

        case SQL_QUOTED_IDENTIFIER_CASE: 
        {
            traceArg("\thInfoType=SQL_QUOTED_IDENTIFIER_CASE");
            break;  
        }

        case SQL_ROW_UPDATES: 
        {
            traceArg("\thInfoType=SQL_ROW_UPDATES");
            break;  
        }

        case SQL_OWNER_TERM: 
        {
            traceArg("\thInfoType=SQL_OWNER_TERM");
            break;  
        }

        case SQL_OWNER_USAGE: 
        {
            traceArg("\thInfoType=SQL_OWNER_USAGE");
            break;  
        }

        case SQL_SCROLL_OPTIONS: 
        {
            traceArg("\thInfoType=SQL_SCROLL_OPTIONS");
            break;  
        }

        case SQL_SCROLL_CONCURRENCY: 
        {
            traceArg("\thInfoType=SQL_SCROLL_CONCURRENCY");
            break;  
        }

        case SQL_SEARCH_PATTERN_ESCAPE: 
        {
            traceArg("\thInfoType=SQL_SEARCH_PATTERN_ESCAPE");
            break;  
        }

        case SQL_SERVER_NAME: 
        {
            traceArg("\thInfoType=SQL_SERVER_NAME");
            break;  
        }

        case SQL_SPECIAL_CHARACTERS: 
        {
            traceArg("\thInfoType=SQL_SPECIAL_CHARACTERS");
            break;  
        }

        case SQL_SQL_CONFORMANCE: 
        {
            traceArg("\thInfoType=SQL_SQL_CONFORMANCE");
            break;  
        }

        case SQL_SQL92_DATETIME_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_SQL92_DATETIME_FUNCTIONS");
            break;  
        }

        case SQL_SQL92_FOREIGN_KEY_DELETE_RULE: 
        {
            traceArg("\thInfoType=SQL_SQL92_FOREIGN_KEY_DELETE_RULE");
            break;  
        }

        case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE: 
        {
            traceArg("\thInfoType=SQL_SQL92_FOREIGN_KEY_UPDATE_RULE");
            break;  
        }

        case SQL_SQL92_GRANT: 
        {
            traceArg("\thInfoType=SQL_SQL92_GRANT");
            break;  
        }

        case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_SQL92_NUMERIC_VALUE_FUNCTIONS");
            break;  
        }

        case SQL_SQL92_PREDICATES: 
        {
            traceArg("\thInfoType=SQL_SQL92_PREDICATES");
            break;  
        }

        case SQL_SQL92_RELATIONAL_JOIN_OPERATORS: 
        {
            traceArg("\thInfoType=SQL_SQL92_RELATIONAL_JOIN_OPERATORS");
            break;  
        }

        case SQL_SQL92_REVOKE: 
        {
            traceArg("\thInfoType=SQL_SQL92_REVOKE");
            break;  
        }

        case SQL_SQL92_ROW_VALUE_CONSTRUCTOR: 
        {
            traceArg("\thInfoType=SQL_SQL92_ROW_VALUE_CONSTRUCTOR");
            break;  
        }

        case SQL_SQL92_STRING_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_SQL92_STRING_FUNCTIONS");
            break;  
        }

        case SQL_SQL92_VALUE_EXPRESSIONS: 
        {
            traceArg("\thInfoType=SQL_SQL92_VALUE_EXPRESSIONS");
            break;  
        }

        case SQL_STANDARD_CLI_CONFORMANCE: 
        {
            traceArg("\thInfoType=SQL_STANDARD_CLI_CONFORMANCE");
            break;  
        }

        case SQL_STATIC_CURSOR_ATTRIBUTES1: 
        {
            traceArg("\thInfoType=SQL_STATIC_CURSOR_ATTRIBUTES1");
            break;  
        }

        case SQL_STATIC_CURSOR_ATTRIBUTES2: 
        {
            traceArg("\thInfoType=SQL_STATIC_CURSOR_ATTRIBUTES2");
            break;  
        }

        case SQL_STRING_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_STRING_FUNCTIONS");
            break;  
        }

        case SQL_SUBQUERIES: 
        {
            traceArg("\thInfoType=SQL_SUBQUERIES");
            break;  
        }

        case SQL_SYSTEM_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_SYSTEM_FUNCTIONS");
            break;  
        }

        case SQL_TABLE_TERM: 
        {
            traceArg("\thInfoType=SQL_TABLE_TERM");
            break;  
        }

        case SQL_TIMEDATE_ADD_INTERVALS: 
        {
            traceArg("\thInfoType=SQL_TIMEDATE_ADD_INTERVALS");
            break;  
        }

        case SQL_TIMEDATE_DIFF_INTERVALS: 
        {
            traceArg("\thInfoType=SQL_TIMEDATE_DIFF_INTERVALS");
            break;  
        }

        case SQL_TIMEDATE_FUNCTIONS: 
        {
            traceArg("\thInfoType=SQL_TIMEDATE_FUNCTIONS");
            break;  
        }

        case SQL_TXN_CAPABLE: 
        {
            traceArg("\thInfoType=SQL_TXN_CAPABLE");
            break;  
        }

        case SQL_TXN_ISOLATION_OPTION: 
        {
            traceArg("\thInfoType=SQL_TXN_ISOLATION_OPTION");
            break;  
        }

        case SQL_UNION: 
        {
            traceArg("\thInfoType=SQL_UNION");
            break;  
        }

        case SQL_USER_NAME: 
        {
            traceArg("\thInfoType=SQL_USER_NAME");
            break;  
        }

        case SQL_XOPEN_CLI_YEAR: 
        {
            traceArg("\thInfoType=SQL_XOPEN_CLI_YEAR");
            break;  
        }

        default:
        {
            traceShortVal("hInfoType",hInfoType );
            break;
        }
    } // Switch
}

/*====================================================================================================================================================*/

void RsTrace::traceGetInfoOutput(SQLUSMALLINT hInfoType, 
                        SQLPOINTER pInfoValue,
                        SQLSMALLINT cbLen,
                        int iUnicode)
{
    switch (hInfoType) 
    {
        case SQL_ACCESSIBLE_PROCEDURES:
        case SQL_ACCESSIBLE_TABLES:
        case SQL_CATALOG_NAME:
        case SQL_DATA_SOURCE_READ_ONLY:
        case SQL_ODBC_SQL_OPT_IEF: /* SQL_INTEGRITY */
        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
        case SQL_ROW_UPDATES:
        case SQL_COLLATION_SEQ:
        case SQL_QUALIFIER_NAME_SEPARATOR: /* SQL_CATALOG_NAME_SEPARATOR */
        case SQL_QUALIFIER_TERM:           /* SQL_CATALOG_TERM */
        case SQL_SPECIAL_CHARACTERS:
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
        case SQL_DATA_SOURCE_NAME:
        case SQL_DATABASE_NAME:
        case SQL_DBMS_NAME:
        case SQL_DBMS_VER:
        case SQL_DM_VER:
        case SQL_DRIVER_NAME:
        case SQL_DRIVER_ODBC_VER:
        case SQL_DRIVER_VER:
        case SQL_IDENTIFIER_QUOTE_CHAR:
        case SQL_OWNER_TERM: /* SQL_SCHEMA_TERM */
        case SQL_PROCEDURE_TERM:
        case SQL_SEARCH_PATTERN_ESCAPE:
        case SQL_SERVER_NAME:
        case SQL_TABLE_TERM:
        case SQL_USER_NAME:
        case SQL_XOPEN_CLI_YEAR:
        {
            if(iUnicode)
                traceWStrValWithSmallLen("*pwInfoValue",(SQLWCHAR *)pInfoValue, (cbLen > 0 ) ? cbLen/sizeof(WCHAR) : cbLen);
            else
                traceStrValWithSmallLen("*pInfoValue",(char *)pInfoValue, cbLen);
            break;
        }

        case SQL_ACTIVE_CONNECTIONS: /* SQL_MAX_DRIVER_CONNECTIONS */
        case SQL_ACTIVE_ENVIRONMENTS:
        case SQL_ACTIVE_STATEMENTS: /* SQL_MAX_CONCURRENT_ACTIVITIES */
        case SQL_MAX_QUALIFIER_NAME_LEN: /* SQL_MAX_CATALOG_NAME_LEN */
        case SQL_MAX_COLUMNS_IN_GROUP_BY:
        case SQL_MAX_COLUMNS_IN_INDEX:
        case SQL_MAX_COLUMNS_IN_ORDER_BY:
        case SQL_MAX_COLUMNS_IN_SELECT:
        case SQL_MAX_COLUMNS_IN_TABLE:
        case SQL_MAX_TABLES_IN_SELECT:
        case SQL_QUALIFIER_LOCATION: /* SQL_CATALOG_LOCATION */
        case SQL_CONCAT_NULL_BEHAVIOR:
        case SQL_CORRELATION_NAME:
        case SQL_CURSOR_COMMIT_BEHAVIOR:
        case SQL_CURSOR_ROLLBACK_BEHAVIOR:
        case SQL_FILE_USAGE:
        case SQL_GROUP_BY:
        case SQL_IDENTIFIER_CASE:
        case SQL_KEYWORDS:
        case SQL_MAX_COLUMN_NAME_LEN:
        case SQL_MAX_CURSOR_NAME_LEN:
        case SQL_MAX_IDENTIFIER_LEN:
        case SQL_MAX_PROCEDURE_NAME_LEN:
        case SQL_MAX_OWNER_NAME_LEN: /* SQL_MAX_SCHEMA_NAME_LEN */
        case SQL_MAX_TABLE_NAME_LEN:
        case SQL_MAX_USER_NAME_LEN:
        case SQL_NON_NULLABLE_COLUMNS:
        case SQL_NULL_COLLATION:
        case SQL_ODBC_API_CONFORMANCE:
        case SQL_ODBC_SAG_CLI_CONFORMANCE:
        case SQL_ODBC_SQL_CONFORMANCE:
        case SQL_QUOTED_IDENTIFIER_CASE:
        case SQL_TXN_CAPABLE:
        {
            traceShortPtrVal("*pInfoValue",(short *)pInfoValue);
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
        case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
        case SQL_MAX_BINARY_LITERAL_LEN:
        case SQL_MAX_CHAR_LITERAL_LEN:
        case SQL_MAX_ROW_SIZE:
        case SQL_MAX_STATEMENT_LEN:
        case SQL_QUALIFIER_USAGE: /* SQL_CATALOG_USAGE */
        case SQL_STATIC_SENSITIVITY:
        case SQL_TIMEDATE_ADD_INTERVALS:
        case SQL_TIMEDATE_DIFF_INTERVALS:
        case SQL_AGGREGATE_FUNCTIONS:
        case SQL_ALTER_DOMAIN:
        case SQL_ALTER_TABLE:
        case SQL_ASYNC_MODE:
        case SQL_BATCH_ROW_COUNT:
        case SQL_BATCH_SUPPORT:
        case SQL_BOOKMARK_PERSISTENCE:
        case SQL_CONVERT_BIGINT:
        case SQL_CONVERT_BIT:
        case SQL_CONVERT_CHAR:
        case SQL_CONVERT_DATE:
        case SQL_CONVERT_DECIMAL:
        case SQL_CONVERT_DOUBLE:
        case SQL_CONVERT_FLOAT:
        case SQL_CONVERT_INTEGER:
        case SQL_CONVERT_LONGVARCHAR:
        case SQL_CONVERT_NUMERIC:
        case SQL_CONVERT_REAL:
        case SQL_CONVERT_SMALLINT:
        case SQL_CONVERT_TIME:
        case SQL_CONVERT_TIMESTAMP:
        case SQL_CONVERT_TINYINT:
        case SQL_CONVERT_VARCHAR:
        case SQL_CONVERT_BINARY:
        case SQL_CONVERT_INTERVAL_YEAR_MONTH:
        case SQL_CONVERT_INTERVAL_DAY_TIME:
        case SQL_CONVERT_LONGVARBINARY:
        case SQL_CONVERT_VARBINARY:
        case SQL_CONVERT_WCHAR:
        case SQL_CONVERT_WLONGVARCHAR:
        case SQL_CONVERT_WVARCHAR:
        case SQL_CONVERT_FUNCTIONS:
        case SQL_CREATE_DOMAIN:
        case SQL_CREATE_SCHEMA:
        case SQL_CREATE_TABLE:
        case SQL_CREATE_VIEW:
        case SQL_CURSOR_SENSITIVITY:
        case SQL_DATETIME_LITERALS:
        case SQL_DDL_INDEX:
        case SQL_DEFAULT_TXN_ISOLATION:
        case SQL_DROP_DOMAIN:
        case SQL_DROP_SCHEMA:
        case SQL_DROP_TABLE:
        case SQL_DROP_VIEW:
        case SQL_FETCH_DIRECTION:
        case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
        case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
        case SQL_GETDATA_EXTENSIONS:
        case SQL_INSERT_STATEMENT:
        case SQL_LOCK_TYPES:
        case SQL_MAX_INDEX_SIZE:
        case SQL_NUMERIC_FUNCTIONS:
        case SQL_ODBC_INTERFACE_CONFORMANCE:
        case SQL_OJ_CAPABILITIES:
        case SQL_OWNER_USAGE: /* SQL_SCHEMA_USAGE */
        case SQL_PARAM_ARRAY_ROW_COUNTS:
        case SQL_PARAM_ARRAY_SELECTS:
        case SQL_POS_OPERATIONS:
        case SQL_POSITIONED_STATEMENTS:
        case SQL_SCROLL_OPTIONS:
        case SQL_SCROLL_CONCURRENCY:
        case SQL_SQL_CONFORMANCE:
        case SQL_SQL92_DATETIME_FUNCTIONS:
        case SQL_SQL92_FOREIGN_KEY_DELETE_RULE:
        case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:
        case SQL_SQL92_GRANT:
        case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
        case SQL_SQL92_PREDICATES:
        case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
        case SQL_SQL92_REVOKE:
        case SQL_SQL92_ROW_VALUE_CONSTRUCTOR:
        case SQL_SQL92_STRING_FUNCTIONS:
        case SQL_SQL92_VALUE_EXPRESSIONS:
        case SQL_STANDARD_CLI_CONFORMANCE:
        case SQL_STATIC_CURSOR_ATTRIBUTES1:
        case SQL_STATIC_CURSOR_ATTRIBUTES2:
        case SQL_STRING_FUNCTIONS:
        case SQL_SUBQUERIES:
        case SQL_SYSTEM_FUNCTIONS:
        case SQL_TIMEDATE_FUNCTIONS:
        case SQL_TXN_ISOLATION_OPTION:
        case SQL_UNION:
        {
            traceIntPtrVal("*pInfoValue",(int *)pInfoValue);
            break;
        }

        default: 
        {
            traceArg("*pInfoValue type is unknown");
            break;
        }
     } // Switch
}

/*====================================================================================================================================================*/

void RsTrace::traceBulkOperationOption(SQLSMALLINT hOperation)
{
    switch(hOperation)
    {
        case SQL_ADD:
        {
            traceArg("\thOperation=SQL_ADD");
            break;
        }

        case SQL_UPDATE_BY_BOOKMARK:
        {
            traceArg("\thOperation=SQL_UPDATE_BY_BOOKMARK");
            break;
        }
        case SQL_DELETE_BY_BOOKMARK:
        {
            traceArg("\thOperation=SQL_DELETE_BY_BOOKMARK");
            break;
        }
        case SQL_FETCH_BY_BOOKMARK:
        {
            traceArg("\thOperation=SQL_FETCH_BY_BOOKMARK");
            break;
        }

        default:
        {
            traceShortVal("hOperation",hOperation);
            break;
        }
    } // Switch
}

/*====================================================================================================================================================*/

void RsTrace::traceCType(const char *pArgName, SQLSMALLINT hCType)
{
    switch(hCType) 
    {
        case SQL_C_CHAR:
            traceArg("\t%s=SQL_C_CHAR",pArgName);
            break;
        case SQL_C_WCHAR:
            traceArg("\t%s=SQL_C_WCHAR",pArgName);
            break;
        case SQL_C_NUMERIC:
            traceArg("\t%s=SQL_C_NUMERIC",pArgName);
            break;
        case SQL_C_SHORT:
            traceArg("\t%s=SQL_C_SHORT",pArgName);
            break;
        case SQL_C_SSHORT:
            traceArg("\t%s=SQL_C_SSHORT",pArgName);
            break;
        case SQL_C_USHORT:
            traceArg("\t%s=SQL_C_USHORT",pArgName);
            break;
        case SQL_C_LONG:
            traceArg("\t%s=SQL_C_LONG",pArgName);
            break;
        case SQL_C_SLONG:
            traceArg("\t%s=SQL_C_SLONG",pArgName);
            break;
        case SQL_C_ULONG :
            traceArg( "\t%s=SQL_C_ULONG",pArgName);
            break;
        case SQL_C_SBIGINT:
            traceArg( "\t%s=SQL_C_SBIGINT",pArgName);
            break;
        case SQL_C_UBIGINT:
            traceArg( "\t%s=SQL_C_UBIGINT",pArgName);
            break;
        case SQL_C_FLOAT:
            traceArg( "\t%s=SQL_C_FLOAT",pArgName);
            break;
        case SQL_C_DOUBLE:
            traceArg( "\t%s=SQL_C_DOUBLE",pArgName);
            break;
        case SQL_C_TYPE_DATE:
            traceArg("\t%s=SQL_C_TYPE_DATE",pArgName);
            break;
        case SQL_C_TYPE_TIMESTAMP:
            traceArg( "\t%s=SQL_C_TYPE_TIMESTAMP",pArgName);
            break;
        case SQL_C_TYPE_TIME:
            traceArg( "\t%s=SQL_C_TYPE_TIME",pArgName);
            break;
        case SQL_C_DATE:
            traceArg("\t%s=SQL_C_DATE",pArgName);
            break;
        case SQL_C_TIMESTAMP:
            traceArg( "\t%s=SQL_C_TIMESTAMP",pArgName);
            break;
        case SQL_C_TIME:
            traceArg( "\t%s=SQL_C_TIME",pArgName);
            break;
        case SQL_C_BIT:
            traceArg("\t%s=SQL_C_BIT",pArgName);
            break;
        case SQL_C_TINYINT:
            traceArg("\t%s=SQL_C_TINYINT",pArgName);
            break;
        case SQL_C_STINYINT:
            traceArg("\t%s=SQL_C_STINYINT",pArgName);
            break;
        case SQL_C_UTINYINT:
            traceArg("\t%s=SQL_C_UTINYINT",pArgName);
            break;
        case SQL_C_DEFAULT:
            traceArg( "\t%s=SQL_C_DEFAULT",pArgName);
            break;
        default:
            traceShortVal(pArgName, hCType);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceSQLType(const char *pArgName,SQLSMALLINT hSQLType)
{
    switch(hSQLType) 
    {
        case SQL_CHAR:
            traceArg("\t%s=SQL_CHAR",pArgName);
            break;
        case SQL_VARCHAR:
            traceArg("\t%s=SQL_VARCHAR",pArgName);
            break;
        case SQL_WCHAR:
            traceArg("\t%s=SQL_WCHAR",pArgName);
            break;
        case SQL_WVARCHAR:
            traceArg("\t%s=SQL_WVARCHAR",pArgName);
            break;
        case SQL_DECIMAL:
            traceArg("\t%s=SQL_DECIMAL",pArgName);
            break;
        case SQL_NUMERIC:
            traceArg("\t%s=SQL_NUMERIC",pArgName);
            break;
        case SQL_SMALLINT :
            traceArg("\t%s=SQL_SMALLINT",pArgName);
            break;
        case SQL_INTEGER:
            traceArg("\t%s=SQL_INTEGER",pArgName);
            break;
        case SQL_FLOAT:
            traceArg("\t%s=SQL_FLOAT",pArgName);
            break;
        case SQL_DOUBLE:
            traceArg("\t%s=SQL_DOUBLE",pArgName);
            break;
        case SQL_REAL:
            traceArg("\t%s=SQL_REAL",pArgName);
            break;
        case SQL_BIGINT:
            traceArg("\t%s=SQL_BIGINT",pArgName);
            break;
        case SQL_TYPE_DATE  :
            traceArg( "\t%s=SQL_TYPE_DATE",pArgName);
            break;
        case SQL_TYPE_TIMESTAMP :
            traceArg( "\t%s=SQL_TYPE_TIMESTAMP",pArgName);
            break;
        case SQL_DATE  :
            traceArg( "\t%s=SQL_DATE",pArgName);
            break;
        case SQL_TIMESTAMP :
            traceArg( "\t%s=SQL_TIMESTAMP",pArgName);
            break;
        case SQL_BIT:
            traceArg("\t%s=SQL_BIT",pArgName);
            break;
        case SQL_TINYINT:
            traceArg("\t%s=SQL_TINYINT",pArgName);
            break;
        case SQL_TYPE_TIME :
            traceArg( "\t%s=SQL_TYPE_TIME",pArgName);
            break;
        case SQL_ALL_TYPES:
            traceArg("\t%s=SQL_ALL_TYPES",pArgName);
            break;
        default:
            traceShortVal(pArgName,hSQLType);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceNullableOutput(const char *pArgName,SQLSMALLINT *pNullable)
{
    if(pNullable) 
    {
        switch (*pNullable) 
        {
            case SQL_NO_NULLS:
                traceArg("\t%s=SQL_NO_NULLS", pArgName);
                break;
            case SQL_NULLABLE:
                traceArg("\t%s=SQL_NULLABLE",pArgName);
                break;
            case SQL_NULLABLE_UNKNOWN:
                traceArg("\t%s=SQL_NULLABLE_UNKNOWN",pArgName);
                break;
            default:
                traceShortVal(pArgName,*pNullable);
                break;
        }
    }
    else
        traceArg("\t%s=NULL",pArgName);
}

/*====================================================================================================================================================*/

void RsTrace::traceLongLongVal(const char *pArgName,long long llVal)
{
    traceArg("\t%s=0x%llx",pArgName,llVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceLongVal(const char *pArgName,long lVal)
{
    traceArg("\t%s=0x%lx",pArgName,lVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceIntVal(const char *pArgName,int iVal)
{
    traceArg("\t%s=0x%x",pArgName,iVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceShortVal(const char *pArgName,short hVal)
{
    traceArg("\t%s=%hd",pArgName,hVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceFloatVal(const char *pArgName,float fVal)
{
    traceArg("\t%s=%f",pArgName,fVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceDoubleVal(const char *pArgName,double dVal)
{
    traceArg("\t%s=%g",pArgName,dVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceBitVal(const char *pArgName,char bVal)
{
    traceArg("\t%s=%d",pArgName,(int)bVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceFieldIdentifier(const char *pArgName, SQLUSMALLINT hFieldIdentifier)
{
    switch(hFieldIdentifier)
    {
        case SQL_DESC_COUNT: traceArg("\t%s=SQL_DESC_COUNT", pArgName); break;
        case SQL_DESC_AUTO_UNIQUE_VALUE: traceArg("\t%s=SQL_DESC_AUTO_UNIQUE_VALUE", pArgName); break;
        case SQL_DESC_BASE_COLUMN_NAME: traceArg("\t%s=SQL_DESC_BASE_COLUMN_NAME", pArgName); break;
        case SQL_DESC_LABEL: traceArg("\t%s=SQL_DESC_LABEL", pArgName); break;
        case SQL_DESC_NAME: traceArg("\t%s=SQL_DESC_NAME", pArgName); break;
        case SQL_DESC_BASE_TABLE_NAME: traceArg("\t%s=SQL_DESC_BASE_TABLE_NAME", pArgName); break;
        case SQL_DESC_TABLE_NAME: traceArg("\t%s=SQL_DESC_TABLE_NAME", pArgName); break;
        case SQL_DESC_CASE_SENSITIVE: traceArg("\t%s=SQL_DESC_CASE_SENSITIVE", pArgName); break;
        case SQL_DESC_CATALOG_NAME: traceArg("\t%s=SQL_DESC_CATALOG_NAME", pArgName); break;
        case SQL_DESC_CONCISE_TYPE: traceArg("\t%s=SQL_DESC_CONCISE_TYPE", pArgName); break;
        case SQL_DESC_TYPE: traceArg("\t%s=SQL_DESC_TYPE", pArgName); break;
        case SQL_DESC_DISPLAY_SIZE: traceArg("\t%s=SQL_DESC_DISPLAY_SIZE", pArgName); break;
        case SQL_DESC_FIXED_PREC_SCALE: traceArg("\t%s=SQL_DESC_FIXED_PREC_SCALE", pArgName); break;
        case SQL_DESC_LENGTH: traceArg("\t%s=SQL_DESC_LENGTH", pArgName); break;
        case SQL_DESC_LITERAL_PREFIX: traceArg("\t%s=SQL_DESC_LITERAL_PREFIX", pArgName); break;
        case SQL_DESC_LITERAL_SUFFIX: traceArg("\t%s=SQL_DESC_LITERAL_SUFFIX", pArgName); break;
        case SQL_DESC_LOCAL_TYPE_NAME: traceArg("\t%s=SQL_DESC_LOCAL_TYPE_NAME", pArgName); break;
        case SQL_DESC_TYPE_NAME: traceArg("\t%s=SQL_DESC_TYPE_NAME", pArgName); break;
        case SQL_DESC_NULLABLE: traceArg("\t%s=SQL_DESC_NULLABLE", pArgName); break;
        case SQL_DESC_NUM_PREC_RADIX: traceArg("\t%s=SQL_DESC_NUM_PREC_RADIX", pArgName); break;
        case SQL_DESC_OCTET_LENGTH: traceArg("\t%s=SQL_DESC_OCTET_LENGTH", pArgName); break;
        case SQL_DESC_PRECISION: traceArg("\t%s=SQL_DESC_PRECISION", pArgName); break;
        case SQL_DESC_SCALE: traceArg("\t%s=SQL_DESC_SCALE", pArgName); break;
        case SQL_DESC_SCHEMA_NAME: traceArg("\t%s=SQL_DESC_SCHEMA_NAME", pArgName); break;
        case SQL_DESC_SEARCHABLE: traceArg("\t%s=SQL_DESC_SEARCHABLE", pArgName); break;
        case SQL_DESC_UNNAMED: traceArg("\t%s=SQL_DESC_UNNAMED", pArgName); break;
        case SQL_DESC_UNSIGNED: traceArg("\t%s=SQL_DESC_UNSIGNED", pArgName); break;
        case SQL_DESC_UPDATABLE: traceArg("\t%s=SQL_DESC_UPDATABLE", pArgName); break;

        case SQL_COLUMN_COUNT: traceArg("\t%s=SQL_COLUMN_COUNT", pArgName); break;
        case SQL_COLUMN_NAME: traceArg("\t%s=SQL_COLUMN_NAME", pArgName); break;
        case SQL_COLUMN_LENGTH: traceArg("\t%s=SQL_COLUMN_LENGTH", pArgName); break;
        case SQL_COLUMN_NULLABLE: traceArg("\t%s=SQL_COLUMN_NULLABLE", pArgName); break;
        case SQL_COLUMN_PRECISION: traceArg("\t%s=SQL_COLUMN_PRECISION", pArgName); break;
        case SQL_COLUMN_SCALE: traceArg("\t%s=SQL_COLUMN_SCALE", pArgName); break;

        case SQL_DESC_ALLOC_TYPE: traceArg("\t%s=SQL_DESC_ALLOC_TYPE", pArgName); break;
        case SQL_DESC_ARRAY_SIZE: traceArg("\t%s=SQL_DESC_ARRAY_SIZE", pArgName); break;
        case SQL_DESC_ARRAY_STATUS_PTR: traceArg("\t%s=SQL_DESC_ARRAY_STATUS_PTR", pArgName); break;
        case SQL_DESC_BIND_OFFSET_PTR: traceArg("\t%s=SQL_DESC_BIND_OFFSET_PTR", pArgName); break;
        case SQL_DESC_BIND_TYPE: traceArg("\t%s=SQL_DESC_BIND_TYPE", pArgName); break;
        case SQL_DESC_ROWS_PROCESSED_PTR: traceArg("\t%s=SQL_DESC_ROWS_PROCESSED_PTR", pArgName); break;
        case SQL_DESC_DATA_PTR: traceArg("\t%s=SQL_DESC_DATA_PTR", pArgName); break;
        case SQL_DESC_DATETIME_INTERVAL_CODE: traceArg("\t%s=SQL_DESC_DATETIME_INTERVAL_CODE", pArgName); break;
        case SQL_DESC_PARAMETER_TYPE: traceArg("\t%s=SQL_DESC_PARAMETER_TYPE", pArgName); break;
        case SQL_DESC_OCTET_LENGTH_PTR: traceArg("\t%s=SQL_DESC_OCTET_LENGTH_PTR", pArgName); break;
        case SQL_DESC_INDICATOR_PTR: traceArg("\t%s=SQL_DESC_INDICATOR_PTR", pArgName); break;

        default: traceShortVal(pArgName, hFieldIdentifier); break;
    } // Switch

}

/*====================================================================================================================================================*/

void RsTrace::traceEnvAttr(const char *pArgName, SQLINTEGER iAttribute)
{
    switch(iAttribute)
    {
        case SQL_ATTR_CONNECTION_POOLING: traceArg("\t%s=SQL_ATTR_CONNECTION_POOLING", pArgName); break;
        case SQL_ATTR_CP_MATCH: traceArg("\t%s=SQL_ATTR_CP_MATCH", pArgName); break;
        case SQL_ATTR_ODBC_VERSION: traceArg("\t%s=SQL_ATTR_ODBC_VERSION", pArgName); break;
        case SQL_ATTR_OUTPUT_NTS: traceArg("\t%s=SQL_ATTR_OUTPUT_NTS", pArgName); break;
        default: traceLongVal(pArgName, iAttribute); break;
    } // Switch
}

/*====================================================================================================================================================*/

void RsTrace::traceEnvAttrVal(const char *pArgName, SQLINTEGER iAttribute,SQLPOINTER pVal, SQLINTEGER    cbLen)
{
    traceLongVal(pArgName, (long)pVal);
}

/*====================================================================================================================================================*/

void RsTrace::traceConnectAttr(const char *pArgName, SQLINTEGER iAttribute)
{
    switch(iAttribute)
    {
        case SQL_ATTR_ACCESS_MODE: traceArg("\t%s=SQL_ATTR_ACCESS_MODE", pArgName); break;
        case SQL_ATTR_ASYNC_ENABLE: traceArg("\t%s=SQL_ATTR_ASYNC_ENABLE", pArgName); break;
        case SQL_ATTR_AUTOCOMMIT: traceArg("\t%s=SQL_ATTR_AUTOCOMMIT", pArgName); break;
        case SQL_ATTR_CONNECTION_DEAD: traceArg("\t%s=SQL_ATTR_CONNECTION_DEAD", pArgName); break;
        case SQL_ATTR_CONNECTION_TIMEOUT: traceArg("\t%s=SQL_ATTR_CONNECTION_TIMEOUT", pArgName); break;
        case SQL_ATTR_CURRENT_CATALOG: traceArg("\t%s=SQL_ATTR_CURRENT_CATALOG", pArgName); break;
        case SQL_ATTR_LOGIN_TIMEOUT: traceArg("\t%s=SQL_ATTR_LOGIN_TIMEOUT", pArgName); break;
        case SQL_ATTR_METADATA_ID: traceArg("\t%s=SQL_ATTR_METADATA_ID", pArgName); break;
        case SQL_ATTR_ODBC_CURSORS: traceArg("\t%s=SQL_ATTR_ODBC_CURSORS", pArgName); break;
        case SQL_ATTR_PACKET_SIZE: traceArg("\t%s=SQL_ATTR_PACKET_SIZE", pArgName); break;
        case SQL_ATTR_QUIET_MODE: traceArg("\t%s=SQL_ATTR_QUIET_MODE", pArgName); break;
        case SQL_ATTR_TRACE: traceArg("\t%s=SQL_ATTR_TRACE", pArgName); break;
        case SQL_ATTR_TRACEFILE: traceArg("\t%s=SQL_ATTR_TRACEFILE", pArgName); break;
        case SQL_ATTR_TRANSLATE_LIB: traceArg("\t%s=SQL_ATTR_TRANSLATE_LIB", pArgName); break;
        case SQL_ATTR_TRANSLATE_OPTION: traceArg("\t%s=SQL_ATTR_TRANSLATE_OPTION", pArgName); break;
        case SQL_ATTR_TXN_ISOLATION: traceArg("\t%s=SQL_ATTR_TXN_ISOLATION", pArgName); break;
        case SQL_ATTR_ANSI_APP: traceArg("\t%s=SQL_ATTR_ANSI_APP", pArgName); break;
        default: traceLongVal(pArgName, iAttribute); break;
    } // Switch
}

/*====================================================================================================================================================*/

void RsTrace::traceConnectAttrVal(const char *pArgName, SQLINTEGER iAttribute,SQLPOINTER pVal, SQLINTEGER    cbLen, int iUnicode, int iSetVal)
{
    if(iAttribute == SQL_ATTR_CURRENT_CATALOG
        || iAttribute == SQL_ATTR_TRACEFILE
        || iAttribute == SQL_ATTR_TRANSLATE_LIB)
    {
        if(iUnicode)
            traceWStrValWithLargeLen(pArgName, (SQLWCHAR *)pVal, (cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
        else
            traceStrValWithLargeLen(pArgName, (char *)pVal, cbLen);
    }
    else
    {
        if(iSetVal)
            traceIntVal(pArgName, (int)(long)pVal);
        else
            traceIntPtrVal(pArgName, (int *)pVal);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceStmtAttr(const char *pArgName, SQLINTEGER iAttribute)
{
    switch(iAttribute)
    {
        // Connection/Stmt
        case SQL_ATTR_ACCESS_MODE: traceArg("\t%s=SQL_ATTR_ACCESS_MODE", pArgName); break;
        case SQL_ATTR_ASYNC_ENABLE: traceArg("\t%s=SQL_ATTR_ASYNC_ENABLE", pArgName); break;
        case SQL_ATTR_AUTOCOMMIT: traceArg("\t%s=SQL_ATTR_AUTOCOMMIT", pArgName); break;
        case SQL_ATTR_CONNECTION_TIMEOUT: traceArg("\t%s=SQL_ATTR_CONNECTION_TIMEOUT", pArgName); break;
        case SQL_ATTR_CURRENT_CATALOG: traceArg("\t%s=SQL_ATTR_CURRENT_CATALOG", pArgName); break;
        case SQL_ATTR_LOGIN_TIMEOUT: traceArg("\t%s=SQL_ATTR_LOGIN_TIMEOUT", pArgName); break;
        case SQL_ATTR_METADATA_ID: traceArg("\t%s=SQL_ATTR_METADATA_ID", pArgName); break;
        case SQL_ATTR_ODBC_CURSORS: traceArg("\t%s=SQL_ATTR_ODBC_CURSORS", pArgName); break;
        case SQL_ATTR_PACKET_SIZE: traceArg("\t%s=SQL_ATTR_PACKET_SIZE", pArgName); break;
        case SQL_ATTR_QUIET_MODE: traceArg("\t%s=SQL_ATTR_QUIET_MODE", pArgName); break;
        case SQL_ATTR_TRACE: traceArg("\t%s=SQL_ATTR_TRACE", pArgName); break;
        case SQL_ATTR_TRACEFILE: traceArg("\t%s=SQL_ATTR_TRACEFILE", pArgName); break;
        case SQL_ATTR_TRANSLATE_LIB: traceArg("\t%s=SQL_ATTR_TRANSLATE_LIB", pArgName); break;
        case SQL_ATTR_TRANSLATE_OPTION: traceArg("\t%s=SQL_ATTR_TRANSLATE_OPTION", pArgName); break;
        case SQL_ATTR_TXN_ISOLATION: traceArg("\t%s=SQL_ATTR_TXN_ISOLATION", pArgName); break;
        case SQL_ATTR_ANSI_APP: traceArg("\t%s=SQL_ATTR_ANSI_APP", pArgName); break;

        // Stmt
        case SQL_ATTR_APP_PARAM_DESC: traceArg("\t%s=SQL_ATTR_APP_PARAM_DESC", pArgName); break;
        case SQL_ATTR_APP_ROW_DESC: traceArg("\t%s=SQL_ATTR_APP_ROW_DESC", pArgName); break;
        case SQL_ATTR_IMP_PARAM_DESC: traceArg("\t%s=SQL_ATTR_IMP_PARAM_DESC", pArgName); break;
        case SQL_ATTR_IMP_ROW_DESC: traceArg("\t%s=SQL_ATTR_IMP_ROW_DESC", pArgName); break;
        case SQL_ATTR_CONCURRENCY: traceArg("\t%s=SQL_ATTR_CONCURRENCY", pArgName); break;
        case SQL_ATTR_CURSOR_SCROLLABLE: traceArg("\t%s=SQL_ATTR_CURSOR_SCROLLABLE", pArgName); break;
        case SQL_ATTR_CURSOR_SENSITIVITY: traceArg("\t%s=SQL_ATTR_CURSOR_SENSITIVITY", pArgName); break;
        case SQL_ATTR_CURSOR_TYPE: traceArg("\t%s=SQL_ATTR_CURSOR_TYPE", pArgName); break;
        case SQL_ATTR_FETCH_BOOKMARK_PTR: traceArg("\t%s=SQL_ATTR_FETCH_BOOKMARK_PTR", pArgName); break;
        case SQL_ATTR_KEYSET_SIZE: traceArg("\t%s=SQL_ATTR_KEYSET_SIZE", pArgName); break;
        case SQL_ATTR_MAX_LENGTH: traceArg("\t%s=SQL_ATTR_MAX_LENGTH", pArgName); break;
        case SQL_ATTR_MAX_ROWS: traceArg("\t%s=SQL_ATTR_MAX_ROWS", pArgName); break;
        case SQL_ATTR_NOSCAN: traceArg("\t%s=SQL_ATTR_NOSCAN", pArgName); break;
        case SQL_ATTR_PARAM_BIND_OFFSET_PTR: traceArg("\t%s=SQL_ATTR_PARAM_BIND_OFFSET_PTR", pArgName); break;
        case SQL_ATTR_PARAM_BIND_TYPE: traceArg("\t%s=SQL_ATTR_PARAM_BIND_TYPE", pArgName); break;
        case SQL_ATTR_PARAM_OPERATION_PTR: traceArg("\t%s=SQL_ATTR_PARAM_OPERATION_PTR", pArgName); break;
        case SQL_ATTR_PARAM_STATUS_PTR: traceArg("\t%s=SQL_ATTR_PARAM_STATUS_PTR", pArgName); break;
        case SQL_ATTR_PARAMS_PROCESSED_PTR: traceArg("\t%s=SQL_ATTR_PARAMS_PROCESSED_PTR", pArgName); break;
        case SQL_ATTR_PARAMSET_SIZE: traceArg("\t%s=SQL_ATTR_PARAMSET_SIZE", pArgName); break;
        case SQL_ATTR_QUERY_TIMEOUT: traceArg("\t%s=SQL_ATTR_QUERY_TIMEOUT", pArgName); break;
        case SQL_ATTR_RETRIEVE_DATA: traceArg("\t%s=SQL_ATTR_RETRIEVE_DATA", pArgName); break;
        case SQL_ATTR_ROW_ARRAY_SIZE: traceArg("\t%s=SQL_ATTR_ROW_ARRAY_SIZE", pArgName); break;
        case SQL_ROWSET_SIZE: traceArg("\t%s=SQL_ROWSET_SIZE", pArgName); break;
        case SQL_ATTR_ROW_BIND_OFFSET_PTR: traceArg("\t%s=SQL_ATTR_ROW_BIND_OFFSET_PTR", pArgName); break;
        case SQL_ATTR_ROW_BIND_TYPE: traceArg("\t%s=SQL_ATTR_ROW_BIND_TYPE", pArgName); break;
        case SQL_ATTR_ROW_OPERATION_PTR: traceArg("\t%s=SQL_ATTR_ROW_OPERATION_PTR", pArgName); break;
        case SQL_ATTR_ROW_STATUS_PTR: traceArg("\t%s=SQL_ATTR_ROW_STATUS_PTR", pArgName); break;
        case SQL_ATTR_ROWS_FETCHED_PTR: traceArg("\t%s=SQL_ATTR_ROWS_FETCHED_PTR", pArgName); break;
        case SQL_ATTR_SIMULATE_CURSOR: traceArg("\t%s=SQL_ATTR_SIMULATE_CURSOR", pArgName); break;
        case SQL_ATTR_USE_BOOKMARKS: traceArg("\t%s=SQL_ATTR_USE_BOOKMARKS", pArgName); break;

        default: traceLongVal(pArgName, iAttribute); break;
    } // Switch
}

/*====================================================================================================================================================*/

void RsTrace::traceStmtAttrVal(const char *pArgName, SQLINTEGER iAttribute,SQLPOINTER pVal, SQLINTEGER    cbLen, int iUnicode, int iSetVal)
{
    if(iAttribute == SQL_ATTR_CURRENT_CATALOG
        || iAttribute == SQL_ATTR_TRACEFILE
        || iAttribute == SQL_ATTR_TRANSLATE_LIB)
    {
        if(iUnicode)
            traceWStrValWithLargeLen(pArgName, (SQLWCHAR *)pVal, (cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
        else
            traceStrValWithLargeLen(pArgName, (char *)pVal, cbLen);
    }
    else
    {
        if(iSetVal)
            traceIntVal(pArgName, (int)(long)pVal);
        else
            traceIntPtrVal(pArgName, (int *)pVal);
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceWStrValWithSmallLen(const char *pArgName, SQLWCHAR *pwVal, SQLSMALLINT cchLen)
{
    if(pwVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        if (cchLen == SQL_NULL_DATA) 
            traceArg("\t%s=NULL_DATA",pArgName);
        else 
#ifdef WIN32
        if (cchLen == SQL_NTS) 
            traceArg("\t%s=%.*S",pArgName,TRACE_MAX_STR_VAL_LEN,pwVal);
        else 
        if(cchLen > 0) 
        {
            traceArg("\t%s=%.*S",pArgName,(cchLen > TRACE_MAX_STR_VAL_LEN) ? TRACE_MAX_STR_VAL_LEN : cchLen,
                                    pwVal);
        }
#endif
#if defined LINUX 
        {
            char *pTemp = (char *)convertWcharToUtf8(pwVal, cchLen);

            if(pTemp)
            {
                if (cchLen == SQL_NTS)
                    traceArg("\t%s=%.*s",pArgName,TRACE_MAX_STR_VAL_LEN,pTemp);
                else
                if(cchLen > 0)
                {
                    traceArg("\t%s=%.*s",pArgName,(cchLen > TRACE_MAX_STR_VAL_LEN) ? TRACE_MAX_STR_VAL_LEN : cchLen,
                                                pTemp);
                }
            }

            pTemp = (char *)rs_free(pTemp);
        }
#endif
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceWStrValWithLargeLen(const char *pArgName, SQLWCHAR *pwVal, SQLINTEGER cchLen)
{
    if(pwVal == NULL) 
        traceArg("\t%s=NULL",pArgName);
    else
    {
        if (cchLen == SQL_NULL_DATA) 
            traceArg("\t%s=NULL_DATA",pArgName);
        else 
#ifdef WIN32
        if (cchLen == SQL_NTS) 
            traceArg("\t%s=%.*S",pArgName,TRACE_MAX_STR_VAL_LEN,pwVal);
        else 
        if(cchLen > 0) 
        {
            traceArg("\t%s=%.*S",pArgName,(cchLen > TRACE_MAX_STR_VAL_LEN) ? TRACE_MAX_STR_VAL_LEN : cchLen,
                                    pwVal);
        }
#endif
#if defined LINUX 
        {
            char *pTemp = (char *)convertWcharToUtf8(pwVal, cchLen);

            if(pTemp)
            {
                if (cchLen == SQL_NTS)
                    traceArg("\t%s=%.*s",pArgName,TRACE_MAX_STR_VAL_LEN,pTemp);
                else
                if(cchLen > 0)
                {
                    traceArg("\t%s=%.*s",pArgName,(cchLen > TRACE_MAX_STR_VAL_LEN) ? TRACE_MAX_STR_VAL_LEN : cchLen,
                                                pTemp);
                }
            }

            pTemp = (char *)rs_free(pTemp);
        }
#endif

    }
}

/*====================================================================================================================================================*/

void RsTrace::traceDriverCompletion(const char *pArgName, SQLUSMALLINT hDriverCompletion)
{
    switch (hDriverCompletion) 
    {
        case SQL_DRIVER_PROMPT:
            traceArg("\t%s=SQL_DRIVER_PROMPT", pArgName);
            break;
        case SQL_DRIVER_COMPLETE:
            traceArg("\t%s=SQL_DRIVER_COMPLETE", pArgName);
            break;
        case SQL_DRIVER_COMPLETE_REQUIRED:
            traceArg("\t%s=SQL_DRIVER_COMPLETE_REQUIRED", pArgName);
            break;
        case SQL_DRIVER_NOPROMPT:
            traceArg("\t%s=SQL_DRIVER_NOPROMPT", pArgName);
            break;
        default:
            traceShortVal(pArgName, hDriverCompletion);
            break;
    }                  
}

/*====================================================================================================================================================*/

void RsTrace::traceIdenTypeSpecialColumns(const char *pArgName, SQLUSMALLINT   hIdenType)
{
    switch (hIdenType)
    {
        case SQL_BEST_ROWID:
            traceArg("\t%s=SQL_BEST_ROWID", pArgName);
            break;
        case SQL_ROWVER:
            traceArg("\t%s=SQL_ROWVER", pArgName);
            break;
        default:
            traceShortVal(pArgName,hIdenType);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceScopeSpecialColumns(const char *pArgName, SQLUSMALLINT   hScope)
{
    switch (hScope)
    {
        case SQL_SCOPE_CURROW:
            traceArg("\t%s=SQL_SCOPE_CURROW", pArgName);
            break;
        case SQL_SCOPE_TRANSACTION:
            traceArg("\t%s=SCOPE_TRANSACTION", pArgName);
            break;
        case SQL_SCOPE_SESSION:
            traceArg("\t%s=SCOPE_SESSION", pArgName);
            break;
        default:
            traceShortVal(pArgName,hScope);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceUniqueStatistics(const char *pArgName, SQLUSMALLINT hUnique)
{
    switch(hUnique)
    {
        case SQL_INDEX_UNIQUE:
            traceArg("\t%s=SQL_INDEX_UNIQUE", pArgName);
            break;
        case SQL_INDEX_ALL:
            traceArg("\t%s=SQL_INDEX_ALL", pArgName);
            break;
        default:
            traceShortVal(pArgName,hUnique);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceReservedStatistics(const char *pArgName, SQLUSMALLINT hReserved)
{
    switch(hReserved)
    {
        case SQL_ENSURE:
            traceArg("\t%s=SQL_ENSURE", pArgName);
            break;
        case SQL_QUICK:
            traceArg("\t%s=SQL_QUICK", pArgName);
            break;
        default:
            traceShortVal(pArgName,hReserved);
            break;
    }
}

/*====================================================================================================================================================*/

void RsTrace::traceData(const char *pArgName, SQLSMALLINT hType, SQLPOINTER pValue, SQLLEN cbLen)
{
    switch(hType)
    {
        case SQL_C_CHAR:
        {
            traceStrValWithLargeLen(pArgName,(char *)pValue,(SQLINTEGER)cbLen);
            break;
        }

        case SQL_C_WCHAR:
        {
            traceWStrValWithLargeLen(pArgName,(SQLWCHAR *)pValue,(SQLINTEGER)cbLen);
            break;
        }

        case SQL_C_SHORT:
        case SQL_C_SSHORT:
        case SQL_C_USHORT:
        {
            traceShortPtrVal(pArgName, (short *)pValue);
            break;
        }

        case SQL_C_LONG:
        case SQL_C_SLONG:
        case SQL_C_ULONG:
        {
            traceIntPtrVal(pArgName, (int *)pValue);
            break;
        }

        case SQL_C_SBIGINT:
        case SQL_C_UBIGINT:
        {
            traceLongLongPtrVal(pArgName, (long long *)pValue);
            break;
        }

        case SQL_C_FLOAT:
        {
            traceFloatPtrVal(pArgName, (float *)pValue);
            break;
        }

        case SQL_C_DOUBLE:
        {
            traceDoublePtrVal(pArgName, (double *)pValue);
            break;
        }

        case SQL_C_BIT:
        case SQL_C_TINYINT:
        case SQL_C_STINYINT:
        case SQL_C_UTINYINT:
        {
            traceBitPtrVal(pArgName, (char *)pValue);
            break;
        }

        case SQL_C_TYPE_DATE:
        case SQL_C_DATE:
        {
            traceDatePtrVal(pArgName, (DATE_STRUCT *)pValue);
            break;
        }

        case SQL_C_TYPE_TIMESTAMP:
        case SQL_C_TIMESTAMP:
        {
            traceTimeStampPtrVal(pArgName, (TIMESTAMP_STRUCT *)pValue);
            break;
        }

        case SQL_C_TYPE_TIME:
        case SQL_C_TIME:
        {
            traceTimePtrVal(pArgName, (TIME_STRUCT *)pValue);
            break;
        }

        case SQL_C_NUMERIC:
        {
            traceNumericPtrVal(pArgName, (SQL_NUMERIC_STRUCT *)pValue);
            break;
        }
        case SQL_C_DEFAULT:
        {
            traceArg("\t%s=SQL_C_DEFAULT without SQL Type",pArgName);
            break;
        }
            
        default:
        {
            traceArg("\t%s=Unknown CType data",pArgName);
            break;
        }

    } // Switch
}


// API calls

/*====================================================================================================================================================*/

void RsTrace::TraceSQLAllocEnv(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV *pphenv)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLAllocEnv(");
                traceHandle("pphenv",pphenv);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLAllocEnv() return %s", getRcString(iRc));
                traceHandle("*pphenv",(pphenv) ? *pphenv : NULL);

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList((pphenv) ? *pphenv : NULL,NULL,NULL,NULL);

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLAllocConnect(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHENV   phenv,
                            SQLHDBC   *pphdbc)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLAllocConnect(");
                traceHandle("phenv",phenv);
                traceHandle("pphdbc",pphdbc);
                traceClosingBracket();

                break;
            }
        
            case FUNC_RETURN:
            {
                traceAPICall("SQLAllocConnect() return %s", getRcString(iRc));
                traceHandle("*pphdbc",(pphdbc) ? *pphdbc : NULL);

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(phenv,NULL,NULL,NULL); 

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLAllocStmt(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC   phdbc,
                        SQLHSTMT *pphstmt)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLAllocStmt(");
                traceHandle("phdbc",phdbc);
                traceHandle("pphstmt",pphstmt);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLAllocStmt() return %s", getRcString(iRc));
                traceHandle("*pphstmt",(pphstmt) ? *pphstmt : NULL);

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLFreeEnv(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV    phenv)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
         switch (logWhat(iCallOrRet)) 
         {
            case FUNC_CALL:
            {
                traceAPICall("SQLFreeEnv(");
                traceHandle("phenv",phenv);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLFreeEnv() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(phenv,NULL,NULL,NULL);

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLFreeConnect(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDBC    phdbc)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
         switch (logWhat(iCallOrRet)) 
         {
            case FUNC_CALL:
            {
                traceAPICall("SQLFreeConnect(");
                traceHandle("phdbc",phdbc);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLFreeConnect() return %s",getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL);   

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLFreeStmt(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLUSMALLINT uhOption)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
         switch (logWhat(iCallOrRet)) 
         {
            case FUNC_CALL:
            {
                traceAPICall("SQLFreeStmt(");
                traceHandle("phstmt",phstmt);

                switch (uhOption) 
                {
                    case SQL_CLOSE:
                        traceArg("\tuhOption=SQL_CLOSE");
                        break;
                    case SQL_DROP :
                        traceArg("\tuhOption=SQL_DROP");
                        break;
                    case SQL_UNBIND:
                        traceArg("\tuhOption=SQL_UNBIND");
                        break;
                    case SQL_RESET_PARAMS:
                        traceArg("\tuhOption=SQL_RESET_PARAMS");
                        break;
                    default:
                        traceShortVal("uhOption",uhOption);
                        break;
                }
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLFreeStmt() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,NULL,phstmt,NULL); 

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLAllocHandle(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLSMALLINT hHandleType,
                            SQLHANDLE pInputHandle, 
                            SQLHANDLE *ppOutputHandle)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLAllocHandle(");
                traceHandleType(hHandleType);
                traceHandle("pInputHandle",pInputHandle);
                traceHandle("ppOutputHandle",ppOutputHandle);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLAllocHandle() return %s", getRcString(iRc));
                traceHandle("*ppOutputHandle",(ppOutputHandle) ? *ppOutputHandle : NULL);

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pInputHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DBC) ? pInputHandle : NULL,
                                    (hHandleType == SQL_HANDLE_STMT) ? pInputHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DESC) ? pInputHandle : NULL);

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLFreeHandle(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE pHandle)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLFreeHandle(");
                traceHandleType(hHandleType);
                traceHandle("pHandle",pHandle);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLFreeHandle() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                        traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL,
                                        (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL,
                                        (hHandleType == SQL_HANDLE_STMT) ? pHandle : NULL,
                                        (hHandleType == SQL_HANDLE_DESC) ? pHandle : NULL);

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLConnect(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC phdbc,
                        SQLCHAR *szDSN, 
                        SQLSMALLINT cchDSN,
                        SQLCHAR *szUID, 
                        SQLSMALLINT cchUID,
                        SQLCHAR *szAuthStr, 
                        SQLSMALLINT cchAuthStr)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLConnect(");
                traceHandle("phdbc",phdbc);
                traceStrValWithSmallLen("szDSN",(char *)szDSN,cchDSN);
                traceStrSmallLen("cchDSN",cchDSN);
                traceStrValWithSmallLen("szUID",(char *)szUID,cchUID);
                traceStrSmallLen("cchUID",cchUID);
                traceStrValWithSmallLen("szAuthStr",(char *)((szAuthStr) ? "****" : NULL),cchAuthStr);
                traceStrSmallLen("cchAuthStr",cchAuthStr);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLConnect() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLDisconnect(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC phdbc)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLDisconnect(");
                traceHandle("phdbc",phdbc);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLDisconnect() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLDriverConnect(int              iCallOrRet,
                            SQLRETURN      iRc,
                            SQLHDBC          phdbc,
                            SQLHWND          hwnd,
                            SQLCHAR       *szConnStrIn,
                            SQLSMALLINT   cbConnStrIn,
                            SQLCHAR       *szConnStrOut,
                            SQLSMALLINT   cbConnStrOut,
                            SQLSMALLINT   *pcbConnStrOut,
                            SQLUSMALLINT   hDriverCompletion)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLDriverConnect(");
                traceHandle("phdbc",phdbc);
                traceHandle("hwnd",hwnd);
                tracePasswordConnectString("szConnStrIn",(char *)szConnStrIn,cbConnStrIn);
                traceStrSmallLen("cbConnStrIn",cbConnStrIn);
                tracePointer("szConnStrOut",szConnStrOut);
                traceStrSmallLen("cbConnStrOut",cbConnStrOut);
                tracePointer("pcbConnStrOut",pcbConnStrOut);
                traceDriverCompletion("hDriverCompletion", hDriverCompletion);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLDriverConnect() return %s",getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 
                else 
                {
                    traceStrOutSmallLen("*pcbConnStrOut",pcbConnStrOut);
                    tracePasswordConnectString("*szConnStrOut",(char *)szConnStrOut,cbConnStrOut);
                }
                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLBrowseConnect(int              iCallOrRet,
                            SQLRETURN      iRc,
                            SQLHDBC          phdbc,
                            SQLCHAR       *szConnStrIn,
                            SQLSMALLINT   cbConnStrIn,
                            SQLCHAR       *szConnStrOut,
                            SQLSMALLINT   cbConnStrOut,
                            SQLSMALLINT   *pcbConnStrOut)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLBrowseConnect(");
                traceHandle("phdbc",phdbc);
                tracePasswordConnectString("szConnStrIn",(char *)szConnStrIn,cbConnStrIn);
                traceStrSmallLen("cbConnStrIn",cbConnStrIn);
                tracePointer("szConnStrOut",szConnStrOut);
                traceStrSmallLen("cbConnStrOut",cbConnStrOut);
                tracePointer("pcbConnStrOut",pcbConnStrOut);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLBrowseConnect() return %s",getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 
                else 
                {
                    tracePasswordConnectString("*szConnStrOut",(char *)szConnStrOut,cbConnStrOut);
                    traceStrOutSmallLen("*pcbConnStrOut",pcbConnStrOut);
                }
                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLError(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHENV phenv,
                    SQLHDBC phdbc, 
                    SQLHSTMT phstmt,
                    SQLCHAR *pSqlState,  
                    SQLINTEGER *piNativeError,
                    SQLCHAR *pMessageText, 
                    SQLSMALLINT cbLen,
                    SQLSMALLINT *pcbLen)
{
    if(true || IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLError(");
                traceHandle("phenv",phenv);
                traceHandle("phdbc",phdbc);
                traceHandle("phstmt",phstmt);
                tracePointer("pSqlState",pSqlState);
                tracePointer("piNativeError",piNativeError);
                tracePointer("pMessageText",pMessageText);
                traceStrSmallLen("cbLen",cbLen);
                tracePointer("pcbLen",pcbLen);

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLError() return %s", getRcString(iRc));

                if(iRc != SQL_SUCCESS && iRc != SQL_SUCCESS_WITH_INFO && iRc != SQL_NO_DATA)
                    traceErrorList(phenv,phdbc,phstmt,NULL); 
                else 
                {
                    if(iRc != SQL_NO_DATA)
                    {
                        traceStrValWithSmallLen("*pSqlState",(char *)pSqlState,SQL_NTS);
                        traceIntPtrVal("*piNativeError",(int *)piNativeError);
                        traceStrValWithSmallLen("*pMessageText",(char *)pMessageText,cbLen);
                        traceStrOutSmallLen("*pcbLen",pcbLen);
                    }
                }

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDiagRec(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE pHandle,
                        SQLSMALLINT hRecNumber, 
                        SQLCHAR *pSqlState,
                        SQLINTEGER *piNativeError, 
                        SQLCHAR *pMessageText,
                        SQLSMALLINT cbLen, 
                        SQLSMALLINT *pcbLen)
{
    if(true || IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLGetDiagRec(");
                traceHandleType(hHandleType);
                traceHandle("pHandle",pHandle);
                traceShortVal("hRecNumber",hRecNumber);
                tracePointer("pSqlState",pSqlState);
                tracePointer("piNativeError",piNativeError);
                tracePointer("pMessageText",pMessageText);
                traceStrSmallLen("cbLen",cbLen);
                tracePointer("pcbLen",pcbLen);

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLGetDiagRec() return %s", getRcString(iRc));

                if(iRc != SQL_SUCCESS && iRc != SQL_SUCCESS_WITH_INFO && iRc != SQL_NO_DATA)
                    traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_STMT) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DESC) ? pHandle : NULL);
                else 
                {
                    if(iRc != SQL_NO_DATA)
                    {
                        traceStrValWithSmallLen("*pSqlState",(char *)pSqlState,SQL_NTS);
                        traceIntPtrVal("*piNativeError",(int *)piNativeError);
                        traceStrValWithSmallLen("*pMessageText",(char *)pMessageText,cbLen);
                        traceStrOutSmallLen("*pcbLen",pcbLen);
                    }
                }

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDiagField(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLSMALLINT hHandleType, 
                            SQLHANDLE pHandle,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hDiagIdentifier,
                            SQLPOINTER  pDiagInfo, 
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen)
{
    if(true || IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLGetDiagField(");
                traceHandleType(hHandleType);
                traceHandle("pHandle",pHandle);
                traceShortVal("hRecNumber",hRecNumber);
                traceDiagIdentifier(hDiagIdentifier);
                tracePointer("pDiagInfo",pDiagInfo);
                traceStrSmallLen("cbLen",cbLen);
                tracePointer("pcbLen",pcbLen);

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLGetDiagField() return %s", getRcString(iRc));

                if(iRc != SQL_SUCCESS && iRc != SQL_SUCCESS_WITH_INFO && iRc != SQL_NO_DATA)
                    traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_STMT) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DESC) ? pHandle : NULL);
                else 
                {
                    if(iRc != SQL_NO_DATA)
                    {
                        traceDiagIdentifierOutput(hDiagIdentifier, pDiagInfo,cbLen, FALSE); 
                        traceStrOutSmallLen("*pcbLen",pcbLen);
                    }
                }

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLTransact(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV phenv,
                        SQLHDBC phdbc, 
                        SQLUSMALLINT hCompletionType)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
         switch (logWhat(iCallOrRet)) 
         {
            case FUNC_CALL:
            {
                traceAPICall("SQLTransact(");
                traceHandle("phenv",phenv);
                traceHandle("phdbc",phdbc);

                switch (hCompletionType) 
                {
                    case SQL_COMMIT:
                        traceArg("\thCompletionType=SQL_COMMIT");
                        break;
                    case SQL_ROLLBACK:
                        traceArg("\thCompletionType=SQL_ROLLBACK");
                        break;
                    default:
                        traceShortVal("hCompletionType",hCompletionType);
                        break;
                }

                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLTransact() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL);

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLEndTran(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE pHandle,
                        SQLSMALLINT hCompletionType)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
         switch (logWhat(iCallOrRet)) 
         {
            case FUNC_CALL:
            {
                traceAPICall("SQLEndTran(");
                traceHandleType(hHandleType);
                traceHandle("pHandle",pHandle);

                switch (hCompletionType) 
                {
                    case SQL_COMMIT:
                        traceArg("\thCompletionType=SQL_COMMIT");
                        break;
                    case SQL_ROLLBACK:
                        traceArg("\thCompletionType=SQL_ROLLBACK");
                        break;
                    default:
                        traceShortVal("hCompletionType",hCompletionType);
                        break;
                }

                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLEndTran() return %s",  getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL,
                                    NULL, NULL);
                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetInfo(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC    phdbc,
                        SQLUSMALLINT hInfoType, 
                        SQLPOINTER pInfoValue,
                        SQLSMALLINT cbLen, 
                        SQLSMALLINT *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetInfo(");
            traceHandle("phdbc",phdbc);
            traceGetInfoType(hInfoType);
            tracePointer("pInfoValue",pInfoValue);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetInfo() return %s", getRcString(iRc));
            traceStrOutSmallLen("*pcbLen",pcbLen);
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL);
            else
                traceGetInfoOutput(hInfoType, pInfoValue, cbLen, FALSE);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetFunctions(int iCallOrRet, 
                           SQLRETURN  iRc,
                           SQLHDBC phdbc,
                           SQLUSMALLINT uhFunctionId, 
                           SQLUSMALLINT *puhSupported)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetFunctions(");
            traceHandle("phdbc",phdbc);
            traceShortVal("uhFunctionId",uhFunctionId);    
            tracePointer("puhSupported",puhSupported);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetFunctions() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL);
            else 
            {
                if(puhSupported)
                    traceShortVal("*puhSupported",*puhSupported);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLExecDirect(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLCHAR* pCmd,
                        SQLINTEGER cbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLExecDirect(");
            traceHandle("phstmt",phstmt);
            traceStrValWithLargeLen("pCmd",(char *)pCmd,cbLen);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLExecDirect() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLExecute(int iCallOrRet,
                     SQLRETURN  iRc,
                     SQLHSTMT   phstmt)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLExecute(");
            traceHandle("phstmt",phstmt);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLExecute() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLPrepare(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLCHAR* pCmd,
                        SQLINTEGER cbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLPrepare(");
            traceHandle("phstmt",phstmt);
            traceStrValWithLargeLen("pCmd",(char *)pCmd,cbLen);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLPrepare() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLCancel(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT   phstmt)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLCancel(");
            traceHandle("phstmt",phstmt);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLCancel() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLParamData(int iCallOrRet,
                       SQLRETURN  iRc,
                       SQLHSTMT   phstmt,
                       SQLPOINTER *ppValue)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLParamData(");
            traceHandle("phstmt",phstmt);
            tracePointer("ppValue",ppValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLParamData() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLPutData(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT   phstmt,
                    SQLPOINTER pData, 
                    SQLLEN cbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLPutData(");
            traceHandle("phstmt",phstmt);
            traceStrValWithLargeLen("pData",(char *)pData,(SQLINTEGER)cbLen);
            traceStrLargeLen("cbLen",(SQLINTEGER)cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLPutData() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLBulkOperations(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLSMALLINT hOperation) 
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLBulkOperations(");
            traceHandle("phstmt",phstmt);
            traceBulkOperationOption(hOperation);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLBulkOperations() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLNativeSql(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC   phdbc,
                        SQLCHAR*    szSqlStrIn,
                        SQLINTEGER    cbSqlStrIn,
                        SQLCHAR*    szSqlStrOut,
                        SQLINTEGER  cbSqlStrOut,
                        SQLINTEGER  *pcbSqlStrOut)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLNativeSql(");
            traceHandle("phdbc",phdbc);
            traceStrValWithLargeLen("szSqlStrIn",(char *)szSqlStrIn,cbSqlStrIn);
            traceStrLargeLen("cbSqlStrIn",cbSqlStrIn);
            tracePointer("szSqlStrOut",szSqlStrOut);
            traceStrLargeLen("cbSqlStrOut",cbSqlStrOut);
            tracePointer("pcbSqlStrOut",pcbSqlStrOut);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLNativeSql() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 
            else 
            {
                traceStrOutLargeLen("*pcbSqlStrOut",pcbSqlStrOut);
                traceStrValWithLargeLen("*szSqlStrOut",(char *)szSqlStrOut,cbSqlStrOut);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetCursorName(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLCHAR* pCursorName,
                            SQLSMALLINT cbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetCursorName(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCursorName",(char *)pCursorName,cbLen);
            traceStrSmallLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetCursorName() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetCursorName(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLCHAR *pCursorName,
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetCursorName(");
            traceHandle("phstmt",phstmt);
            tracePointer("pCursorName",pCursorName);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetCursorName() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
            {
                traceStrOutSmallLen("*pcbLen",pcbLen);
                traceStrValWithSmallLen("*pCursorName",(char *)pCursorName,cbLen);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLCloseCursor(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLCloseCursor(");
            traceHandle("phstmt",phstmt);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLCloseCursor() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLBindParameter(int iCallOrRet, 
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLUSMALLINT   hParam,
                            SQLSMALLINT    hInOutType,
                            SQLSMALLINT    hType,
                            SQLSMALLINT    hSQLType,
                            SQLULEN        iColSize,
                            SQLSMALLINT    hScale,
                            SQLPOINTER     pValue,
                            SQLLEN         cbLen,
                            SQLLEN         *pcbLenInd)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLBindParameter(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hParam",hParam); 
            
            switch(hInOutType)
            {
                case SQL_PARAM_INPUT:
                    traceArg("\thInOutType=SQL_PARAM_INPUT");
                    break;
                case SQL_PARAM_INPUT_OUTPUT:
                    traceArg("\thInOutType=SQL_PARAM_INPUT_OUTPUT");
                    break;
                case SQL_PARAM_OUTPUT:
                    traceArg("\thInOutType=SQL_PARAM_OUTPUT");
                    break;
                default:
                    traceShortVal("hInOutType",hInOutType);
                    break;
            }

            traceCType("hType",hType);
            traceSQLType("hSQLType",hSQLType);
            traceLongVal("iColSize",(long)iColSize);
            traceShortVal("hScale",hScale);
            tracePointer("pValue",pValue);
            traceStrLargeLen("cbLen",(long)cbLen);
            tracePointer("pcbLenInd",pcbLenInd);
            traceStrOutLargeLen("*pcbLenInd",(SQLINTEGER *)pcbLenInd);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLBindParameter() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetParam(int iCallOrRet, 
                        SQLRETURN  iRc,
                        SQLHSTMT    phstmt,
                        SQLUSMALLINT hParam, 
                        SQLSMALLINT hValType,
                        SQLSMALLINT hParamType, 
                        SQLULEN iLengthPrecision,
                        SQLSMALLINT hParamScale, 
                        SQLPOINTER pParamVal,
                        SQLLEN *piStrLen_or_Ind)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetParam(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hParam",hParam); 
            traceCType("hValType",hValType);
            traceSQLType("hParamType",hParamType);
            traceLongVal("iLengthPrecision",(long)iLengthPrecision);
            traceShortVal("hParamScale",hParamScale);
            tracePointer("pParamVal",pParamVal);
            tracePointer("piStrLen_or_Ind",piStrLen_or_Ind);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetParam() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLBindParam(int iCallOrRet, 
                        SQLRETURN  iRc,
                        SQLHSTMT     phstmt,
                        SQLUSMALLINT hParam, 
                        SQLSMALLINT hValType,
                        SQLSMALLINT hParamType, 
                        SQLULEN        iLengthPrecision,
                        SQLSMALLINT hParamScale, 
                        SQLPOINTER  pParamVal,
                        SQLLEN        *piStrLen_or_Ind)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLBindParam(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hParam",hParam); 
            traceCType("hValType",hValType);
            traceSQLType("hParamType",hParamType);
            traceLongVal("iLengthPrecision",(long)iLengthPrecision);
            traceShortVal("hParamScale",hParamScale);
            tracePointer("pParamVal",pParamVal);
            tracePointer("piStrLen_or_Ind",piStrLen_or_Ind);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLBindParam() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLNumParams(int iCallOrRet, 
                       SQLRETURN  iRc,
                       SQLHSTMT      phstmt,
                       SQLSMALLINT   *pParamCount)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLNumParams(");
            traceHandle("phstmt",phstmt);
            tracePointer("pParamCount",pParamCount);
            traceClosingBracket();
            break;
        }
            
        case FUNC_RETURN:
        {
            traceAPICall("SQLNumParams() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
                traceShortPtrVal("*pParamCount",pParamCount);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLParamOptions(int iCallOrRet, 
                           SQLRETURN  iRc,
                           SQLHSTMT   phstmt,
                           SQLULEN  iCrow,
                           SQLULEN  *piRow)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLParamOptions(");
            traceHandle("phstmt",phstmt);
            traceLongVal("iCrow",(long)iCrow);
            tracePointer("piRow",piRow);
            traceClosingBracket();

            break;
        }
            
        case FUNC_RETURN:
        {
            traceAPICall("SQLParamOptions() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLDescribeParam(int iCallOrRet, 
                            SQLRETURN  iRc,
                            SQLHSTMT        phstmt,
                            SQLUSMALLINT    hParam,
                            SQLSMALLINT     *pDataType,
                            SQLULEN         *pParamSize,
                            SQLSMALLINT     *pDecimalDigits,
                            SQLSMALLINT     *pNullable)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLDescribeParam(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hParam",hParam); 
            tracePointer("pDataType",pDataType);
            tracePointer("pParamSize",pParamSize);
            tracePointer("pDecimalDigits",pDecimalDigits);
            tracePointer("pNullable",pNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLDescribeParam() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
            {
                if(pDataType)
                    traceSQLType("*pDataType", *pDataType);
                else
                    traceArg("\tpDataType=NULL");

                traceLongPtrVal("*pParamSize",(long *)pParamSize);
                traceShortPtrVal("*pDecimalDigits",pDecimalDigits);
                traceNullableOutput("*pNullable", pNullable);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLNumResultCols(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLSMALLINT *pColumnCount)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLNumResultCols(");
            traceHandle("phstmt",phstmt);
            tracePointer("pColumnCount",pColumnCount);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLNumResultCols() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
                traceShortPtrVal("*pColumnCount",pColumnCount);

            break;
        }
    }

}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLDescribeCol(int iCallOrRet,
                           SQLRETURN  iRc,
                           SQLHSTMT    phstmt,
                           SQLUSMALLINT hCol, 
                           SQLCHAR *pColName,
                           SQLSMALLINT cbLen, 
                           SQLSMALLINT *pcbLen,
                           SQLSMALLINT *pDataType,  
                           SQLULEN *pColSize,
                           SQLSMALLINT *pDecimalDigits,  
                           SQLSMALLINT *pNullable)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLDescribeCol(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol); 
            tracePointer("pColName",pColName);
            traceShortVal("cbLen",cbLen); 
            tracePointer("pcbLen",pcbLen);
            tracePointer("pDataType",pDataType);
            tracePointer("pColSize",pColSize);
            tracePointer("pDecimalDigits",pDecimalDigits);
            tracePointer("pNullable",pNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLDescribeCol() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
            {
                traceStrValWithSmallLen("pColName",(char *)pColName,cbLen);
                traceStrOutSmallLen("*pcbLen",pcbLen);
                if(pDataType)
                    traceSQLType("*pDataType", *pDataType);
                else
                    traceArg("\tpDataType=NULL");

                traceLongPtrVal("*pColSize",(long *)pColSize);
                traceShortPtrVal("*pDecimalDigits",pDecimalDigits);
                traceNullableOutput("*pNullable", pNullable);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColAttribute(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER    pcValue, 
                            SQLSMALLINT    cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN       *plValue)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLColAttribute(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            tracePointer("pcValue",pcValue);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            tracePointer("plValue",plValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColAttribute() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                 traceErrorList(NULL,NULL,phstmt,NULL); 
            else
            {
                if(isStrFieldIdentifier(hFieldIdentifier))
                {
                    traceStrValWithSmallLen("*pcValue",(char *)pcValue,cbLen);
                    traceStrOutSmallLen("*pcbLen",pcbLen);
                }
                else
                    traceLongPtrVal("*plValue",(long *)plValue);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColAttributes(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER    pcValue, 
                            SQLSMALLINT    cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN       *plValue)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLColAttributes(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            tracePointer("pcValue",pcValue);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            tracePointer("plValue",plValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColAttributes() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                 traceErrorList(NULL,NULL,phstmt,NULL); 
            else
            {
                if(isStrFieldIdentifier(hFieldIdentifier))
                {
                    traceStrValWithSmallLen("*pcValue",(char *)pcValue,cbLen);
                    traceStrOutSmallLen("*pcbLen",pcbLen);
                }
                else
                    traceLongPtrVal("*plValue", (long *)plValue);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLBindCol(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT   phstmt,
                        SQLUSMALLINT hCol, 
                        SQLSMALLINT hType,
                        SQLPOINTER pValue, 
                        SQLLEN cbLen, 
                        SQLLEN *pcbLenInd)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLBindCol(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol);
            traceCType("hType",hType);
            tracePointer("pValue",pValue);
            traceStrLargeLen("cbLen",(long)cbLen);
            tracePointer("pcbLenInd",pcbLenInd);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLBindCol() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            break;
        }
    }
}      

/*====================================================================================================================================================*/

void RsTrace::TraceSQLFetch(int iCallOrRet,
                     SQLRETURN  iRc,
                     SQLHSTMT   phstmt)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLFetch(");
            traceHandle("phstmt",phstmt);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLFetch() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLMoreResults(int iCallOrRet,
                         SQLRETURN  iRc,
                         SQLHSTMT   phstmt)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLMoreResults(");
            traceHandle("phstmt",phstmt);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLMoreResults() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLExtendedFetch(int  iCallOrRet,
                            SQLRETURN    iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT    hFetchOrientation,
                            SQLLEN          iFetchOffset,
                            SQLULEN         *piRowCount,
                            SQLUSMALLINT    *phRowStatus)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLExtendedFetch(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hFetchOrientation",hFetchOrientation);
            traceLongVal("iFetchOffset", (long) iFetchOffset);
            tracePointer("piRowCount",piRowCount);
            tracePointer("phRowStatus",phRowStatus);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLExtendedFetch() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetData(int  iCallOrRet,
                      SQLRETURN    iRc,
                      SQLHSTMT    phstmt,
                      SQLUSMALLINT hCol, 
                      SQLSMALLINT hType,
                      SQLPOINTER pValue, 
                      SQLLEN cbLen,
                      SQLLEN *pcbLenInd)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetData(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol);
            traceCType("hType",hType);
            tracePointer("pValue",pValue);
            traceStrLargeLen("cbLen",(long)cbLen);
            tracePointer("pcbLenInd",pcbLenInd);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetData() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL);
            else 
            {
                traceData("*pValue", hType, pValue, cbLen);
                traceStrOutLargeLen("*pcbLenInd",(SQLINTEGER *)pcbLenInd);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLRowCount(int  iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT  phstmt,
                        SQLLEN* pRowCount)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLRowCount(");
            traceHandle("phstmt",phstmt);
            tracePointer("pRowCount",pRowCount);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLRowCount() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
                traceLongPtrVal("*pRowCount",(long *)pRowCount);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetPos(int  iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT  phstmt,
                    SQLSETPOSIROW  iRow,
                    SQLUSMALLINT   hOperation,
                    SQLUSMALLINT   hLockType)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetPos(");
            traceHandle("phstmt",phstmt);
            traceLongVal("iRow",(long)iRow);
            traceShortVal("hOperation",hOperation);
            traceShortVal("hLockType",hLockType);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetPos() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }

}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLFetchScroll(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLSMALLINT hFetchOrientation,
                            SQLLEN      iFetchOffset)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLFetchScroll(");
            traceHandle("phstmt",phstmt);

            switch (hFetchOrientation) 
            {
                case SQL_FETCH_NEXT:
                    traceArg("\thFetchOrientation=SQL_FETCH_NEXT");
                    break;
                case SQL_FETCH_PRIOR:
                    traceArg("\thFetchOrientation=SQL_FETCH_PRIOR");
                    break;
                case SQL_FETCH_FIRST:
                    traceArg("\thFetchOrientation=SQL_FETCH_FIRST");
                    break;
                case SQL_FETCH_LAST:
                    traceArg("\thFetchOrientation=SQL_FETCH_LAST");
                    break;
                case SQL_FETCH_ABSOLUTE:
                    traceArg("\thFetchOrientation=SQL_FETCH_ABSOLUTE");
                    break;
                case SQL_FETCH_RELATIVE:
                    traceArg("\thFetchOrientation=SQL_FETCH_RELATIVE");
                    break;
                case SQL_FETCH_BOOKMARK:
                    traceArg("\thFetchOrientation=SQL_FETCH_BOOKMARK");
                    break;
                default:
                    traceShortVal("hFetchOrientation",hFetchOrientation);
                    break;
            }

            traceLongVal("iFetchOffset", (long)(iFetchOffset));
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLFetchScroll() return %s",getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLCopyDesc(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDESC phdescSrc,
                        SQLHDESC phdescDest)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLCopyDesc(");
            traceHandle("phdescSrc",phdescSrc);
            traceHandle("phdescDest",phdescDest);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLCopyDesc() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
            {
                traceErrorList(NULL,NULL,NULL,phdescSrc); 
                traceErrorList(NULL,NULL,NULL,phdescDest); 
            }
            
            break;
        }
    }

}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDescField(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pValue, 
                            SQLINTEGER cbLen,
                            SQLINTEGER *pcbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetDescField(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            tracePointer("pValue",pValue);
            traceStrLargeLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetDescField() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            else
            {
                if(isStrFieldIdentifier(hFieldIdentifier))
                {
                    traceStrValWithLargeLen("*pValue",(char *)pValue,cbLen);
                    traceStrOutLargeLen("*pcbLen",pcbLen);
                }
                else
                if(cbLen == sizeof(short))
                    traceShortPtrVal("*pValue",(short *)pValue); 
                else
                if(cbLen == sizeof(long long))
                    traceLongLongPtrVal("*pValue",(long long *)pValue); 
                else
                    traceIntPtrVal("*pValue",(int *)pValue); // Can be a short pointer
            }
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDescRec(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLCHAR *pName,
                            SQLSMALLINT cbName, 
                            SQLSMALLINT *pcbName,
                            SQLSMALLINT *phType, 
                            SQLSMALLINT *phSubType,
                            SQLLEN     *plOctetLength, 
                            SQLSMALLINT *phPrecision,
                            SQLSMALLINT *phScale, 
                            SQLSMALLINT *phNullable)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetDescRec(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            tracePointer("pName",pName);
            traceStrSmallLen("cbName",cbName);
            tracePointer("pcbName",pcbName);
            tracePointer("phType",phType);
            tracePointer("phSubType",phSubType);
            tracePointer("plOctetLength",plOctetLength);
            tracePointer("phPrecision",phPrecision);
            tracePointer("phScale",phScale);
            tracePointer("phNullable",phNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetDescRec() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            else
            {
                traceStrValWithSmallLen("*pName",(char *)pName,cbName);
                traceStrOutSmallLen("*pcbName",pcbName);
                traceShortPtrVal("*phType",phType); 
                traceShortPtrVal("*phSubType",phSubType); 
                traceLongPtrVal("*plOctetLength",(long *)plOctetLength); 
                traceShortPtrVal("*phPrecision",phPrecision); 
                traceShortPtrVal("*phScale",phScale); 
                traceShortPtrVal("*phNullable",phNullable); 
            }
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetDescField(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pValue, 
                            SQLINTEGER cbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetDescField(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            if(isStrFieldIdentifier(hFieldIdentifier))
            {
                tracePointer("pValue",pValue);
                traceStrValWithLargeLen("*pValue",(char *)pValue,cbLen);
            }
            else
            if(cbLen == sizeof(short))
                traceShortVal("pValue",(short)(long)pValue);
            else
            if(cbLen == sizeof(long long))
                traceLongLongVal("pValue",(long long)(long)pValue); 
            else
                traceLongVal("pValue",(long)pValue); // Can be a short 

            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetDescField() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetDescRec(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hType,
                            SQLSMALLINT hSubType, 
                            SQLLEN        iOctetLength,
                            SQLSMALLINT hPrecision, 
                            SQLSMALLINT hScale,
                            SQLPOINTER    pData, 
                            SQLLEN *    plStrLen,
                            SQLLEN *    plIndicator)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetDescRec(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            traceShortVal("hType",hType);
            traceShortVal("hSubType",hSubType);
            traceLongVal("iOctetLength",(long)iOctetLength);
            traceShortVal("hPrecision",hPrecision);
            traceShortVal("hScale",hScale);
            tracePointer("pData",pData);
            tracePointer("plStrLen",plStrLen);
            tracePointer("plIndicator",plIndicator);
            traceLongPtrVal("*plStrLen",(long *)plStrLen);
            traceLongPtrVal("*plIndicator",(long *)plIndicator);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetDescRec() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetConnectOption(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLULEN pValue)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetConnectOption(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("hOption",hOption);
            traceConnectAttrVal("pValue", hOption, (SQLPOINTER)pValue, SQL_NTS, FALSE, TRUE);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetConnectOption() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetConnectOption(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLPOINTER pValue)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetConnectOption(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("hOption",hOption);
            tracePointer("pValue",pValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetConnectOption() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 
            else
                traceConnectAttrVal("*pValue", hOption, pValue, SQL_NTS, FALSE, FALSE);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetStmtOption(int   iCallOrRet,
                            SQLRETURN   iRc,
                            SQLHSTMT phstmt,
                            SQLUSMALLINT hOption, 
                            SQLULEN Value)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetStmtOption(");
            traceHandle("phstmt",phstmt);
            traceStmtAttr("hOption",hOption);
            traceStmtAttrVal("Value", hOption, (SQLPOINTER)Value, SQL_NTS, FALSE, TRUE);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetStmtOption() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetStmtOption(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHSTMT phstmt,
                                SQLUSMALLINT hOption, 
                                SQLPOINTER pValue)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetStmtOption(");
            traceHandle("phstmt",phstmt);
            traceStmtAttr("hOption",hOption);
            tracePointer("pValue",pValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetStmtOption() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else
                traceStmtAttrVal("*pValue", hOption, pValue, SQL_NTS, FALSE, FALSE);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetScrollOptions(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHSTMT phstmt,
                                SQLUSMALLINT  hConcurrency,
                                SQLLEN        iKeysetSize,
                                SQLUSMALLINT  hRowsetSize)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetScrollOptions(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hConcurrency",hConcurrency);
            traceLongVal("iKeysetSize",(long)iKeysetSize);
            traceShortVal("hRowsetSize",hRowsetSize);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetScrollOptions() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetConnectAttr(int   iCallOrRet,
                            SQLRETURN   iRc,
                            SQLHDBC    phdbc,
                            SQLINTEGER    iAttribute, 
                            SQLPOINTER    pValue,
                            SQLINTEGER    cbLen, 
                            SQLINTEGER  *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetConnectAttr(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("iAttribute",iAttribute);
            tracePointer("pValue",pValue);
            traceLongVal("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetConnectAttr() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 
            else
            {
                traceConnectAttrVal("*pValue", iAttribute, pValue, cbLen, FALSE, FALSE);
                traceIntPtrVal("*pcbLen",(int *)pcbLen);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetStmtAttr(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pValue,
                           SQLINTEGER    cbLen, 
                           SQLINTEGER  *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetStmtAttr(");
            traceHandle("phstmt",phstmt);
            traceStmtAttr("iAttribute",iAttribute);
            tracePointer("pValue",pValue);
            traceLongVal("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetStmtAttr() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else
            {
                traceStmtAttrVal("*pValue", iAttribute, pValue, cbLen, FALSE, FALSE);
                traceIntPtrVal("*pcbLen",(int *)pcbLen);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetConnectAttr(int   iCallOrRet,
                               SQLRETURN   iRc,
                               SQLHDBC    phdbc,
                               SQLINTEGER    iAttribute, 
                               SQLPOINTER    pValue,
                               SQLINTEGER    cbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetConnectAttr(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("iAttribute",iAttribute);
            traceConnectAttrVal("pValue", iAttribute, pValue, cbLen, FALSE, TRUE);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetConnectAttr() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetStmtAttr(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pValue,
                           SQLINTEGER    cbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetStmtAttr(");
            traceHandle("phstmt",phstmt);
            traceStmtAttr("iAttribute",iAttribute);
            traceStmtAttrVal("pValue", iAttribute, pValue, cbLen, FALSE, TRUE);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetStmtAttr() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetEnvAttr(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV phenv,
                        SQLINTEGER    iAttribute, 
                        SQLPOINTER    pValue,
                        SQLINTEGER    cbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetEnvAttr(");
            traceHandle("phenv",phenv);
            traceEnvAttr("iAttribute",iAttribute);
            traceEnvAttrVal("pValue", iAttribute, pValue, cbLen);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();
            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetEnvAttr() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(phenv,NULL,NULL,NULL); 
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetEnvAttr(int   iCallOrRet,
                        SQLRETURN   iRc,
                        SQLHENV phenv,
                        SQLINTEGER    iAttribute, 
                        SQLPOINTER    pValue,
                        SQLINTEGER    cbLen, 
                        SQLINTEGER    *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetEnvAttr(");
            traceHandle("phenv",phenv);
            traceStmtAttr("iAttribute",iAttribute);
            tracePointer("pValue",pValue);
            traceLongVal("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetEnvAttr() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(phenv,NULL,NULL,NULL); 
            else
            {
                traceEnvAttrVal("*pValue", iAttribute, pValue, cbLen);
                traceIntPtrVal("*pcbLen",(int *)pcbLen);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLTables(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLCHAR *pCatalogName, 
                    SQLSMALLINT cbCatalogName,
                    SQLCHAR *pSchemaName, 
                    SQLSMALLINT cbSchemaName,
                    SQLCHAR *pTableName, 
                    SQLSMALLINT cbTableName,
                    SQLCHAR *pTableType, 
                    SQLSMALLINT cbTableType)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLTables(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceStrValWithSmallLen("pTableType",(char *)pTableType,cbTableType);
            traceStrSmallLen("cbTableType",cbTableType);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLTables() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColumns(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLCHAR *pCatalogName, 
                    SQLSMALLINT cbCatalogName,
                    SQLCHAR *pSchemaName, 
                    SQLSMALLINT cbSchemaName,
                    SQLCHAR *pTableName, 
                    SQLSMALLINT cbTableName,
                    SQLCHAR *pColumnName, 
                    SQLSMALLINT cbColumnName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLColumns(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceStrValWithSmallLen("pColumnName",(char *)pColumnName,cbColumnName);
            traceStrSmallLen("cbColumnName",cbColumnName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColumns() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLStatistics(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT       phstmt,
                        SQLCHAR *pCatalogName, 
                        SQLSMALLINT cbCatalogName,
                        SQLCHAR *pSchemaName, 
                        SQLSMALLINT cbSchemaName,
                        SQLCHAR *pTableName, 
                        SQLSMALLINT cbTableName,
                        SQLUSMALLINT hUnique, 
                        SQLUSMALLINT hReserved)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLStatistics(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceUniqueStatistics("hUnique",hUnique);
            traceReservedStatistics("hReserved",hReserved);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLStatistics() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSpecialColumns(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                           SQLUSMALLINT hIdenType, 
                           SQLCHAR *pCatalogName, 
                           SQLSMALLINT cbCatalogName,
                           SQLCHAR *pSchemaName, 
                           SQLSMALLINT cbSchemaName, 
                           SQLCHAR *pTableName, 
                           SQLSMALLINT cbTableName, 
                           SQLUSMALLINT hScope, 
                           SQLUSMALLINT hNullable)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSpecialColumns(");
            traceHandle("phstmt",phstmt);
            traceIdenTypeSpecialColumns("hIdenType",hIdenType);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceScopeSpecialColumns("hScope", hScope);
            traceNullableOutput("hNullable",(SQLSMALLINT *)&hNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSpecialColumns() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLProcedureColumns(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLCHAR          *pCatalogName,
                    SQLSMALLINT      cbCatalogName,
                    SQLCHAR          *pSchemaName,
                    SQLSMALLINT      cbSchemaName,
                    SQLCHAR          *pProcName,
                    SQLSMALLINT      cbProcName,
                    SQLCHAR          *pColumnName,
                    SQLSMALLINT      cbColumnName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLProcedureColumns(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pProcName",(char *)pProcName,cbProcName);
            traceStrSmallLen("cbProcName",cbProcName);
            traceStrValWithSmallLen("pColumnName",(char *)pColumnName,cbColumnName);
            traceStrSmallLen("cbColumnName",cbColumnName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLProcedureColumns() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLProcedures(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLCHAR        *pCatalogName,
                    SQLSMALLINT    cbCatalogName,
                    SQLCHAR        *pSchemaName,
                    SQLSMALLINT    cbSchemaName,
                    SQLCHAR        *pProcName,
                    SQLSMALLINT    cbProcName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLProcedures(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pProcName",(char *)pProcName,cbProcName);
            traceStrSmallLen("cbProcName",cbProcName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLProcedures() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLForeignKeys(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLCHAR           *pPkCatalogName,
                            SQLSMALLINT        cbPkCatalogName,
                            SQLCHAR           *pPkSchemaName,
                            SQLSMALLINT        cbPkSchemaName,
                            SQLCHAR           *pPkTableName,
                            SQLSMALLINT        cbPkTableName,
                            SQLCHAR           *pFkCatalogName,
                            SQLSMALLINT        cbFkCatalogName,
                            SQLCHAR           *pFkSchemaName,
                            SQLSMALLINT        cbFkSchemaName,
                            SQLCHAR           *pFkTableName,
                            SQLSMALLINT        cbFkTableName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLForeignKeys(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pPkCatalogName",(char *)pPkCatalogName,cbPkCatalogName);
            traceStrSmallLen("cbPkCatalogName",cbPkCatalogName);
            traceStrValWithSmallLen("pPkSchemaName",(char *)pPkSchemaName,cbPkSchemaName);
            traceStrSmallLen("cbPkSchemaName",cbPkSchemaName);
            traceStrValWithSmallLen("pPkTableName",(char *)pPkTableName,cbPkTableName);
            traceStrSmallLen("cbPkTableName",cbPkTableName);
            traceStrValWithSmallLen("pFkCatalogName",(char *)pFkCatalogName,cbFkCatalogName);
            traceStrSmallLen("cbFkCatalogName",cbFkCatalogName);
            traceStrValWithSmallLen("pFkSchemaName",(char *)pFkSchemaName,cbFkSchemaName);
            traceStrSmallLen("cbFkSchemaName",cbFkSchemaName);
            traceStrValWithSmallLen("pFkTableName",(char *)pFkTableName,cbFkTableName);
            traceStrSmallLen("cbFkTableName",cbFkTableName);

            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLForeignKeys() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLPrimaryKeys(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT   phstmt,
                    SQLCHAR      *pCatalogName,
                    SQLSMALLINT  cbCatalogName,
                    SQLCHAR      *pSchemaName,
                    SQLSMALLINT   cbSchemaName,
                    SQLCHAR      *pTableName,
                    SQLSMALLINT   cbTableName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLPrimaryKeys(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLPrimaryKeys() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetTypeInfo(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLSMALLINT hType)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetTypeInfo(");
            traceHandle("phstmt",phstmt);
            traceSQLType("hType",hType);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetTypeInfo() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }

}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColumnPrivileges(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLCHAR          *pCatalogName,
                                SQLSMALLINT      cbCatalogName,
                                SQLCHAR          *pSchemaName,
                                SQLSMALLINT      cbSchemaName,
                                SQLCHAR          *pTableName,
                                SQLSMALLINT      cbTableName,
                                SQLCHAR          *pColumnName,
                                SQLSMALLINT      cbColumnName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLColumnPrivileges(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceStrValWithSmallLen("pColumnName",(char *)pColumnName,cbColumnName);
            traceStrSmallLen("cbColumnName",cbColumnName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColumnPrivileges() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLTablePrivileges(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLCHAR         *pCatalogName,
                                SQLSMALLINT     cbCatalogName,
                                SQLCHAR         *pSchemaName,
                                SQLSMALLINT     cbSchemaName,
                                SQLCHAR         *pTableName,
                                SQLSMALLINT      cbTableName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLTablePrivileges(");
            traceHandle("phstmt",phstmt);
            traceStrValWithSmallLen("pCatalogName",(char *)pCatalogName,cbCatalogName);
            traceStrSmallLen("cbCatalogName",cbCatalogName);
            traceStrValWithSmallLen("pSchemaName",(char *)pSchemaName,cbSchemaName);
            traceStrSmallLen("cbSchemaName",cbSchemaName);
            traceStrValWithSmallLen("pTableName",(char *)pTableName,cbTableName);
            traceStrSmallLen("cbTableName",cbTableName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLTablePrivileges() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLConnectW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC phdbc,
                        SQLWCHAR*            wszDSN,
                        SQLSMALLINT         cchDSN,
                        SQLWCHAR*            wszUID,
                        SQLSMALLINT         cchUID,
                        SQLWCHAR*            wszAuthStr,
                        SQLSMALLINT         cchAuthStr)
{
    if(IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLConnectW(");
                traceHandle("phdbc",phdbc);
                traceWStrValWithSmallLen("wszDSN",wszDSN,cchDSN);
                traceStrSmallLen("cchDSN",cchDSN);
                traceWStrValWithSmallLen("wszUID",wszUID,cchUID);
                traceStrSmallLen("cchUID",cchUID);
                traceStrValWithSmallLen("wszAuthStr",(char *)((wszAuthStr) ? "****" : NULL),cchAuthStr);
                traceStrSmallLen("cchAuthStr",cchAuthStr);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLConnectW() return %s", getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLDriverConnectW(int              iCallOrRet,
                            SQLRETURN      iRc,
                            SQLHDBC             phdbc,
                            SQLHWND             hwnd,
                            SQLWCHAR*            wszConnStrIn,
                            SQLSMALLINT         cchConnStrIn,
                            SQLWCHAR*            wszConnStrOut,
                            SQLSMALLINT         cchConnStrOut,
                            SQLSMALLINT*        pcchConnStrOut,
                            SQLUSMALLINT        hDriverCompletion)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLDriverConnectW(");
                traceHandle("phdbc",phdbc);
                traceHandle("hwnd",hwnd);
                tracePasswordConnectStringW("wszConnStrIn",wszConnStrIn,cchConnStrIn); // traceWStrValWithSmallLen
                traceStrSmallLen("cchConnStrIn",cchConnStrIn);
                tracePointer("wszConnStrOut",wszConnStrOut);
                traceStrSmallLen("cchConnStrOut",cchConnStrOut);
                tracePointer("pcchConnStrOut",pcchConnStrOut);
                traceDriverCompletion("hDriverCompletion", hDriverCompletion);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLDriverConnectW() return %s",getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 
                else 
                {
                    traceStrOutSmallLen("*pcchConnStrOut",pcchConnStrOut);
                    tracePasswordConnectStringW("*wszConnStrOut",wszConnStrOut,cchConnStrOut); 
                }
                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLBrowseConnectW(int             iCallOrRet,
                            SQLRETURN     iRc,
                            SQLHDBC         phdbc,
                            SQLWCHAR*     wszConnStrIn,
                            SQLSMALLINT  cchConnStrIn,
                            SQLWCHAR*     wszConnStrOut,
                            SQLSMALLINT  cchConnStrOut,
                            SQLSMALLINT* pcchConnStrOut)
{
    if(true || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLBrowseConnectW(");
                traceHandle("phdbc",phdbc);
                tracePasswordConnectStringW("wszConnStrIn",wszConnStrIn,cchConnStrIn);
                traceStrSmallLen("cchConnStrIn",cchConnStrIn);
                tracePointer("wszConnStrOut",wszConnStrOut);
                traceStrSmallLen("cchConnStrOut",cchConnStrOut);
                tracePointer("pcchConnStrOut",pcchConnStrOut);
                traceClosingBracket();

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLBrowseConnectW() return %s",getRcString(iRc));

                if(!SQL_SUCCEEDED(iRc))
                    traceErrorList(NULL,phdbc,NULL,NULL); 
                else 
                {
                    tracePasswordConnectStringW("*wszConnStrOut",wszConnStrOut,cchConnStrOut); 
                    traceStrOutSmallLen("*pcchConnStrOut",pcchConnStrOut);
                }
                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetInfoW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC    phdbc,
                        SQLUSMALLINT    hInfoType,
                        SQLPOINTER        pwInfoValue,
                        SQLSMALLINT     cbLen,
                        SQLSMALLINT*    pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetInfoW(");
            traceHandle("phdbc",phdbc);
            traceGetInfoType(hInfoType);
            tracePointer("pwInfoValue",pwInfoValue);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetInfoW() return %s", getRcString(iRc));
            traceStrOutSmallLen("*pcbLen",pcbLen);
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL);
            else
                traceGetInfoOutput(hInfoType, pwInfoValue, cbLen, TRUE);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLErrorW(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHENV phenv,
                    SQLHDBC phdbc, 
                    SQLHSTMT phstmt,
                    SQLWCHAR*    pwSqlState,
                    SQLINTEGER* piNativeError,
                    SQLWCHAR*    pwMessageText,
                    SQLSMALLINT  cchLen,
                    SQLSMALLINT* pcchLen)
{
    if(true || IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLErrorW(");
                traceHandle("phenv",phenv);
                traceHandle("phdbc",phdbc);
                traceHandle("phstmt",phstmt);
                tracePointer("pwSqlState",pwSqlState);
                tracePointer("piNativeError",piNativeError);
                tracePointer("pwMessageText",pwMessageText);
                traceStrSmallLen("cchLen",cchLen);
                tracePointer("pcchLen",pcchLen);

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLErrorW() return %s", getRcString(iRc));

                if(iRc != SQL_SUCCESS && iRc != SQL_SUCCESS_WITH_INFO && iRc != SQL_NO_DATA)
                    traceErrorList(phenv,phdbc,phstmt,NULL); 
                else 
                {
                    if(iRc != SQL_NO_DATA)
                    {
                        traceWStrValWithSmallLen("*pwSqlState",pwSqlState,SQL_NTS); 
                        traceIntPtrVal("*piNativeError",(int *)piNativeError);
                        traceWStrValWithSmallLen("*pwMessageText",pwMessageText,cchLen); 
                        traceStrOutSmallLen("*pcchLen",pcchLen);
                    }
                }

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDiagRecW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE      pHandle,
                        SQLSMALLINT    hRecNumber,
                        SQLWCHAR       *pwSqlState,
                        SQLINTEGER     *piNativeError,
                        SQLWCHAR       *pwMessageText,
                        SQLSMALLINT    cchLen,
                        SQLSMALLINT    *pcchLen)
{
    if(true || IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLGetDiagRecW(");
                traceHandleType(hHandleType);
                traceHandle("pHandle",pHandle);
                traceShortVal("hRecNumber",hRecNumber);
                tracePointer("pwSqlState",pwSqlState);
                tracePointer("piNativeError",piNativeError);
                tracePointer("pwMessageText",pwMessageText);
                traceStrSmallLen("cchLen",cchLen);
                tracePointer("pcchLen",pcchLen);

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLGetDiagRecW() return %s", getRcString(iRc));

                if(iRc != SQL_SUCCESS && iRc != SQL_SUCCESS_WITH_INFO && iRc != SQL_NO_DATA)
                    traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_STMT) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DESC) ? pHandle : NULL);
                else 
                {
                    if(iRc != SQL_NO_DATA)
                    {
                        traceWStrValWithSmallLen("*pwSqlState",pwSqlState,SQL_NTS); 
                        traceIntPtrVal("*piNativeError",(int *)piNativeError);
                        traceWStrValWithSmallLen("*pwMessageText",pwMessageText,cchLen); 
                        traceStrOutSmallLen("*pcchLen",pcchLen);
                    }
                }

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDiagFieldW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLSMALLINT hHandleType, 
                            SQLHANDLE pHandle,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hDiagIdentifier,
                            SQLPOINTER  pwDiagInfo, 
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen)
{
    if(true || IS_TRACE_LEVEL_ERROR()
        || IS_TRACE_LEVEL_API_CALL())
    {
        switch (logWhat(iCallOrRet)) 
        {
            case FUNC_CALL:
            {
                traceAPICall("SQLGetDiagFieldW(");
                traceHandleType(hHandleType);
                traceHandle("pHandle",pHandle);
                traceShortVal("hRecNumber",hRecNumber);
                traceDiagIdentifier(hDiagIdentifier);
                tracePointer("pwDiagInfo",pwDiagInfo);
                traceStrSmallLen("cbLen",cbLen);
                tracePointer("pcbLen",pcbLen);

                break;
            }

            case FUNC_RETURN:
            {
                traceAPICall("SQLGetDiagFieldW() return %s", getRcString(iRc));

                if(iRc != SQL_SUCCESS && iRc != SQL_SUCCESS_WITH_INFO && iRc != SQL_NO_DATA)
                    traceErrorList((hHandleType == SQL_HANDLE_ENV) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DBC) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_STMT) ? pHandle : NULL,
                                    (hHandleType == SQL_HANDLE_DESC) ? pHandle : NULL);
                else 
                {
                    if(iRc != SQL_NO_DATA)
                    {
                        traceDiagIdentifierOutput(hDiagIdentifier, pwDiagInfo,cbLen,TRUE); 
                        traceStrOutSmallLen("*pcbLen",pcbLen);
                    }
                }

                break;
            }
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLExecDirectW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLWCHAR* pwCmd,
                        SQLINTEGER  cchLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLExecDirectW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithLargeLen("pwCmd",pwCmd,cchLen);
            traceStrLargeLen("cchLen",cchLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLExecDirectW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLNativeSqlW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC   phdbc,
                        SQLWCHAR*    wszSqlStrIn,
                        SQLINTEGER   cchSqlStrIn,
                        SQLWCHAR*    wszSqlStrOut,
                        SQLINTEGER   cchSqlStrOut,
                        SQLINTEGER*  pcchSqlStrOut)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLNativeSqlW(");
            traceHandle("phdbc",phdbc);
            traceWStrValWithLargeLen("wszSqlStrIn",wszSqlStrIn,cchSqlStrIn);
            traceStrLargeLen("cchSqlStrIn",cchSqlStrIn);
            tracePointer("wszSqlStrOut",wszSqlStrOut);
            traceStrLargeLen("cchSqlStrOut",cchSqlStrOut);
            tracePointer("pcchSqlStrOut",pcchSqlStrOut);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLNativeSqlW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 
            else 
            {
                traceStrOutLargeLen("*pcchSqlStrOut",pcchSqlStrOut);
                traceWStrValWithLargeLen("*wszSqlStrOut",wszSqlStrOut,cchSqlStrOut); 
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetConnectAttrW(int   iCallOrRet,
                            SQLRETURN   iRc,
                            SQLHDBC    phdbc,
                            SQLINTEGER    iAttribute, 
                            SQLPOINTER    pwValue,
                            SQLINTEGER    cbLen, 
                            SQLINTEGER  *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetConnectAttrW(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("iAttribute",iAttribute);
            tracePointer("pwValue",pwValue);
            traceLongVal("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetConnectAttrW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 
            else
            {
                traceConnectAttrVal("*pwValue", iAttribute, pwValue, cbLen, TRUE, FALSE);
                traceIntPtrVal("*pcbLen",(int *)pcbLen);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetConnectOptionW(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLPOINTER pwValue)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetConnectOptionW(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("hOption",hOption);
            tracePointer("pwValue",pwValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetConnectOptionW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 
            else
                traceConnectAttrVal("*pwValue", hOption, pwValue, SQL_NTS, TRUE, FALSE);

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetStmtAttrW(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pwValue,
                           SQLINTEGER    cbLen, 
                           SQLINTEGER  *pcbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetStmtAttrW(");
            traceHandle("phstmt",phstmt);
            traceStmtAttr("iAttribute",iAttribute);
            tracePointer("pwValue",pwValue);
            traceLongVal("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetStmtAttrW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else
            {
                traceStmtAttrVal("*pwValue", iAttribute, pwValue, cbLen, TRUE, FALSE);
                traceIntPtrVal("*pcbLen",(int *)pcbLen);
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetConnectAttrW(int   iCallOrRet,
                               SQLRETURN   iRc,
                               SQLHDBC    phdbc,
                               SQLINTEGER    iAttribute, 
                               SQLPOINTER    pwValue,
                               SQLINTEGER    cbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetConnectAttrW(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("iAttribute",iAttribute);
            traceConnectAttrVal("pwValue", iAttribute, pwValue, cbLen, TRUE, TRUE);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetConnectAttrW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetConnectOptionW(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLULEN pwValue)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetConnectOptionW(");
            traceHandle("phdbc",phdbc);
            traceConnectAttr("hOption",hOption);
            traceConnectAttrVal("pwValue", hOption, (SQLPOINTER)pwValue, SQL_NTS, TRUE, TRUE);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetConnectOptionW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,phdbc,NULL,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetStmtAttrW(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pwValue,
                           SQLINTEGER    cbLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetStmtAttrW(");
            traceHandle("phstmt",phstmt);
            traceStmtAttr("iAttribute",iAttribute);
            traceStmtAttrVal("pwValue", iAttribute, pwValue, cbLen, TRUE, TRUE);
            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetStmtAttrW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetCursorNameW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLWCHAR*        pwCursorName,
                            SQLSMALLINT     cchLen,
                            SQLSMALLINT*    pcchLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetCursorNameW(");
            traceHandle("phstmt",phstmt);
            tracePointer("pwCursorName",pwCursorName);
            traceStrSmallLen("cchLen",cchLen);
            tracePointer("pcchLen",pcchLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetCursorNameW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
            {
                traceStrOutSmallLen("*pcchLen",pcchLen);
                traceWStrValWithSmallLen("*pwCursorName",pwCursorName,cchLen); 
            }

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLPrepareW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLWCHAR* pwCmd,
                        SQLINTEGER cchLen)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLPrepareW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithLargeLen("pwCmd",pwCmd,cchLen);
            traceStrLargeLen("cchLen",cchLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLPrepareW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetCursorNameW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLWCHAR*    pwCursorName,
                            SQLSMALLINT cchLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetCursorNameW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCursorName",pwCursorName,cchLen);
            traceStrSmallLen("cchLen",cchLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetCursorNameW() return %s",getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColAttributesW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER   pwValue, 
                            SQLSMALLINT  cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN        *plValue)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLColAttributesW(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            tracePointer("pwValue",pwValue);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            tracePointer("plValue",plValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColAttributesW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                 traceErrorList(NULL,NULL,phstmt,NULL); 
            else
            {
                if(isStrFieldIdentifier(hFieldIdentifier))
                {
                    traceWStrValWithSmallLen("*pwValue",(SQLWCHAR *)pwValue, (cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
                    traceStrOutSmallLen("*pcbLen",pcbLen);
                }
                else
                    traceLongPtrVal("*plValue", (long *)plValue);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColAttributeW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER    pwValue, 
                            SQLSMALLINT    cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN       *plValue)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLColAttributeW(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            tracePointer("pwValue",pwValue);
            traceStrSmallLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            tracePointer("plValue",plValue);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColAttributeW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                 traceErrorList(NULL,NULL,phstmt,NULL); 
            else
            {
                if(isStrFieldIdentifier(hFieldIdentifier))
                {
                    traceWStrValWithSmallLen("*pwValue",(SQLWCHAR *)pwValue,(cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
                    traceStrOutSmallLen("*pcbLen",pcbLen);
                }
                else
                    traceLongPtrVal("*plValue",(long *)plValue);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLDescribeColW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT    hCol,
                            SQLWCHAR*        pwColName,
                            SQLSMALLINT     cchLen,
                            SQLSMALLINT*    pcchLen,
                            SQLSMALLINT*    pDataType,
                            SQLULEN*        pColSize,
                            SQLSMALLINT*    pDecimalDigits,
                            SQLSMALLINT*    pNullable)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLDescribeColW(");
            traceHandle("phstmt",phstmt);
            traceShortVal("hCol",hCol); 
            tracePointer("pwColName",pwColName);
            traceShortVal("cchLen",cchLen); 
            tracePointer("pcchLen",pcchLen);
            tracePointer("pDataType",pDataType);
            tracePointer("pColSize",pColSize);
            tracePointer("pDecimalDigits",pDecimalDigits);
            tracePointer("pNullable",pNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLDescribeColW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 
            else 
            {
                traceWStrValWithSmallLen("pwColName",pwColName,cchLen);
                traceStrOutSmallLen("*pcchLen",pcchLen);
                if(pDataType)
                    traceSQLType("*pDataType", *pDataType);
                else
                    traceArg("\tpDataType=NULL");

                traceLongPtrVal("*pColSize",(long *)pColSize);
                traceShortPtrVal("*pDecimalDigits",pDecimalDigits);
                traceNullableOutput("*pNullable", pNullable);
            }
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDescFieldW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pwValue, 
                            SQLINTEGER cbLen,
                            SQLINTEGER *pcbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetDescFieldW(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            tracePointer("pwValue",pwValue);
            traceStrLargeLen("cbLen",cbLen);
            tracePointer("pcbLen",pcbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetDescFieldW() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            else
            {
                if(isStrFieldIdentifier(hFieldIdentifier))
                {
                    traceWStrValWithLargeLen("*pwValue",(SQLWCHAR *)pwValue,(cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
                    traceStrOutLargeLen("*pcbLen",pcbLen);
                }
                else
                if(cbLen == sizeof(short))
                    traceShortPtrVal("*pwValue",(short *)pwValue); 
                else
                if(cbLen == sizeof(long long))
                    traceLongLongPtrVal("*pwValue",(long long *)pwValue); 
                else
                    traceIntPtrVal("*pwValue",(int *)pwValue); // Can be a short pointer
            }
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetDescRecW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLWCHAR*     pwName,
                            SQLSMALLINT  cchName,
                            SQLSMALLINT  *pcchName,
                            SQLSMALLINT *phType, 
                            SQLSMALLINT *phSubType,
                            SQLLEN     *plOctetLength, 
                            SQLSMALLINT *phPrecision,
                            SQLSMALLINT *phScale, 
                            SQLSMALLINT *phNullable)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetDescRecW(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            tracePointer("pwName",pwName);
            traceStrSmallLen("cchName",cchName);
            tracePointer("pcchName",pcchName);
            tracePointer("phType",phType);
            tracePointer("phSubType",phSubType);
            tracePointer("plOctetLength",plOctetLength);
            tracePointer("phPrecision",phPrecision);
            tracePointer("phScale",phScale);
            tracePointer("phNullable",phNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetDescRecW() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            else
            {
                traceWStrValWithSmallLen("*pwName",pwName,cchName);
                traceStrOutSmallLen("*pcchName",pcchName);
                traceShortPtrVal("*phType",phType); 
                traceShortPtrVal("*phSubType",phSubType); 
                traceLongPtrVal("*plOctetLength",(long *)plOctetLength); 
                traceShortPtrVal("*phPrecision",phPrecision); 
                traceShortPtrVal("*phScale",phScale); 
                traceShortPtrVal("*phNullable",phNullable); 
            }
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSetDescFieldW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pwValue, 
                            SQLINTEGER cbLen)
{
    switch (logWhat(iCallOrRet)) 
    {
        case FUNC_CALL:
        {
            traceAPICall("SQLSetDescFieldW(");
            traceHandle("phdesc",phdesc);
            traceShortVal("hRecNumber",hRecNumber);
            traceFieldIdentifier("hFieldIdentifier",hFieldIdentifier);
            if(isStrFieldIdentifier(hFieldIdentifier))
            {
                tracePointer("pwValue",pwValue);
                traceWStrValWithLargeLen("*pwValue",(SQLWCHAR *)pwValue,(cbLen > 0) ? cbLen/sizeof(WCHAR) : cbLen);
            }
            else
            if(cbLen == sizeof(short))
                traceShortVal("pwValue",(short)(long)pwValue);
            else
            if(cbLen == sizeof(long long))
                traceLongLongVal("pwValue",(long long)(long)pwValue); 
            else
                traceLongVal("pwValue",(long)pwValue); // Can be a short 

            traceStrLargeLen("cbLen",cbLen);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSetDescFieldW() return %s", getRcString(iRc));

            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,NULL,phdesc); 
            
            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColumnPrivilegesW(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLWCHAR*     pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwTableName,
                                SQLSMALLINT  cchTableName,
                                SQLWCHAR*    pwColumnName,
                                SQLSMALLINT  cchColumnName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLColumnPrivilegesW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceWStrValWithSmallLen("pwColumnName",pwColumnName,cchColumnName);
            traceStrSmallLen("cchColumnName",cchColumnName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColumnPrivilegesW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLColumnsW(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLWCHAR*    pwCatalogName,
                    SQLSMALLINT  cchCatalogName,
                    SQLWCHAR*    pwSchemaName,
                    SQLSMALLINT  cchSchemaName,
                    SQLWCHAR*    pwTableName,
                    SQLSMALLINT  cchTableName,
                    SQLWCHAR*    pwColumnName,
                    SQLSMALLINT  cchColumnName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLColumnsW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceWStrValWithSmallLen("pwColumnName",pwColumnName,cchColumnName);
            traceStrSmallLen("cchColumnName",cchColumnName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLColumnsW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLForeignKeysW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLWCHAR*    pwPkCatalogName,
                            SQLSMALLINT  cchPkCatalogName,
                            SQLWCHAR*    pwPkSchemaName,
                            SQLSMALLINT  cchPkSchemaName,
                            SQLWCHAR*    pwPkTableName,
                            SQLSMALLINT  cchPkTableName,
                            SQLWCHAR*    pwFkCatalogName,
                            SQLSMALLINT  cchFkCatalogName,
                            SQLWCHAR*    pwFkSchemaName,
                            SQLSMALLINT  cchFkSchemaName,
                            SQLWCHAR*    pwFkTableName,
                            SQLSMALLINT  cchFkTableName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLForeignKeysW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwPkCatalogName",pwPkCatalogName,cchPkCatalogName);
            traceStrSmallLen("cchPkCatalogName",cchPkCatalogName);
            traceWStrValWithSmallLen("pwPkSchemaName",pwPkSchemaName,cchPkSchemaName);
            traceStrSmallLen("cchPkSchemaName",cchPkSchemaName);
            traceWStrValWithSmallLen("pwPkTableName",pwPkTableName,cchPkTableName);
            traceStrSmallLen("cchPkTableName",cchPkTableName);
            traceWStrValWithSmallLen("pwFkCatalogName",pwFkCatalogName,cchFkCatalogName);
            traceStrSmallLen("cchFkCatalogName",cchFkCatalogName);
            traceWStrValWithSmallLen("pwFkSchemaName",pwFkSchemaName,cchFkSchemaName);
            traceStrSmallLen("cchFkSchemaName",cchFkSchemaName);
            traceWStrValWithSmallLen("pwFkTableName",pwFkTableName,cchFkTableName);
            traceStrSmallLen("cchFkTableName",cchFkTableName);

            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLForeignKeysW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLGetTypeInfoW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLSMALLINT hType)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLGetTypeInfoW(");
            traceHandle("phstmt",phstmt);
            traceSQLType("hType",hType);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLGetTypeInfoW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }

}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLPrimaryKeysW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLWCHAR*    pwCatalogName,
                            SQLSMALLINT  cchCatalogName,
                            SQLWCHAR*    pwSchemaName,
                            SQLSMALLINT  cchSchemaName,
                            SQLWCHAR*    pwTableName,
                            SQLSMALLINT  cchTableName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLPrimaryKeysW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLPrimaryKeysW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLProcedureColumnsW(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLWCHAR*    pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwProcName,
                                SQLSMALLINT  cchProcName,
                                SQLWCHAR*    pwColumnName,
                                SQLSMALLINT  cchColumnName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLProcedureColumnsW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwProcName",pwProcName,cchProcName);
            traceStrSmallLen("cchProcName",cchProcName);
            traceWStrValWithSmallLen("pwColumnName",pwColumnName,cchColumnName);
            traceStrSmallLen("cchColumnName",cchColumnName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLProcedureColumnsW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLProceduresW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLWCHAR*    pwCatalogName,
                            SQLSMALLINT  cchCatalogName,
                            SQLWCHAR*    pwSchemaName,
                            SQLSMALLINT  cchSchemaName,
                            SQLWCHAR*    pwProcName,
                            SQLSMALLINT  cchProcName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLProceduresW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwProcName",pwProcName,cchProcName);
            traceStrSmallLen("cchProcName",cchProcName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLProceduresW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLSpecialColumnsW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLUSMALLINT   hIdenType,
                            SQLWCHAR*       pwCatalogName,
                            SQLSMALLINT    cchCatalogName,
                            SQLWCHAR*      pwSchemaName,
                            SQLSMALLINT    cchSchemaName,
                            SQLWCHAR*      pwTableName,
                            SQLSMALLINT    cchTableName,
                            SQLUSMALLINT   hScope,
                            SQLUSMALLINT   hNullable)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLSpecialColumnsW(");
            traceHandle("phstmt",phstmt);
            traceIdenTypeSpecialColumns("hIdenType",hIdenType);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceScopeSpecialColumns("hScope", hScope);
            traceNullableOutput("hNullable",(SQLSMALLINT *)&hNullable);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLSpecialColumnsW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLStatisticsW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT       phstmt,
                        SQLWCHAR*    pwCatalogName,
                        SQLSMALLINT  cchCatalogName,
                        SQLWCHAR*    pwSchemaName,
                        SQLSMALLINT  cchSchemaName,
                        SQLWCHAR*    pwTableName,
                        SQLSMALLINT  cchTableName,
                        SQLUSMALLINT hUnique,
                        SQLUSMALLINT hReserved)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLStatisticsW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceUniqueStatistics("hUnique",hUnique);
            traceReservedStatistics("hReserved",hReserved);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLStatisticsW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLTablePrivilegesW(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLWCHAR*    pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwTableName,
                                SQLSMALLINT  cchTableName)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLTablePrivilegesW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLTablePrivilegesW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

void RsTrace::TraceSQLTablesW(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLWCHAR*      pwCatalogName,
                    SQLSMALLINT    cchCatalogName,
                    SQLWCHAR*      pwSchemaName,
                    SQLSMALLINT    cchSchemaName,
                    SQLWCHAR*      pwTableName,
                    SQLSMALLINT    cchTableName,
                    SQLWCHAR*      pwTableType,
                    SQLSMALLINT    cchTableType)
{
     switch (logWhat(iCallOrRet)) 
     {
        case FUNC_CALL:
        {
            traceAPICall("SQLTablesW(");
            traceHandle("phstmt",phstmt);
            traceWStrValWithSmallLen("pwCatalogName",pwCatalogName,cchCatalogName);
            traceStrSmallLen("cchCatalogName",cchCatalogName);
            traceWStrValWithSmallLen("pwSchemaName",pwSchemaName,cchSchemaName);
            traceStrSmallLen("cchSchemaName",cchSchemaName);
            traceWStrValWithSmallLen("pwTableName",pwTableName,cchTableName);
            traceStrSmallLen("cchTableName",cchTableName);
            traceWStrValWithSmallLen("pwTableType",pwTableType,cchTableType);
            traceStrSmallLen("cchTableType",cchTableType);
            traceClosingBracket();

            break;
        }

        case FUNC_RETURN:
        {
            traceAPICall("SQLTablesW() return %s", getRcString(iRc));
            if(!SQL_SUCCEEDED(iRc))
                traceErrorList(NULL,NULL,phstmt,NULL); 

            break;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Replace password with * in the trace file.
//
void RsTrace::tracePasswordConnectString(char *var,char *szConnStr, SQLSMALLINT  cbConnStr)
{
    if (!szConnStr || cbConnStr < 0) {
        return;
    }
    std::string pStr;
    std::string szKeywords[] = { "Password", "PWD"};
    for (auto& kv : parseConnectionString(std::string(szConnStr, cbConnStr)))
    {
        const auto key = kv.first;
        auto& value = kv.second;
        for (auto& pwd : szKeywords) {
            if (isStrNoCaseEequal(key, pwd)) {
                value = std::string(value.size(), '*');
            }
        }
        pStr += key + "=" + value + ";";
    }

    if(false == pStr.empty())
    {
        traceStrValWithSmallLen(var,pStr.c_str(),cbConnStr);
    }
 }

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Replace password with * in the trace file.
//
void RsTrace::tracePasswordConnectStringW(char *var,SQLWCHAR *wszConnStr, SQLSMALLINT  cchConnStr)
{
    char *pTemp = (char *)convertWcharToUtf8(wszConnStr, cchConnStr);

    tracePasswordConnectString(var, pTemp, cchConnStr);

    pTemp = (char *)rs_free(pTemp);
}


void TraceSQLAllocEnv(int iCallOrRet, SQLRETURN iRc, SQLHENV *pphenv) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLAllocEnv(iCallOrRet, iRc, pphenv);
        logger.process();
    }
}

void TraceSQLAllocConnect(int iCallOrRet, SQLRETURN iRc, HENV phenv,
                          HDBC *pphdbc) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLAllocConnect(iCallOrRet, iRc, phenv, pphdbc);
        logger.process();
    }
}

void TraceSQLAllocStmt(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                       SQLHSTMT *pphstmt) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLAllocStmt(iCallOrRet, iRc, phdbc, pphstmt);
        logger.process();
    }
}

void TraceSQLFreeEnv(int iCallOrRet, SQLRETURN iRc, SQLHENV phenv) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLFreeEnv(iCallOrRet, iRc, phenv);
        logger.process();
    }
}

void TraceSQLFreeConnect(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLFreeConnect(iCallOrRet, iRc, phdbc);
        logger.process();
    }
}

void TraceSQLFreeStmt(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                      SQLUSMALLINT uhOption) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLFreeStmt(iCallOrRet, iRc, phstmt, uhOption);
        logger.process();
    }
}

void TraceSQLAllocHandle(int iCallOrRet, SQLRETURN iRc, SQLSMALLINT hHandleType,
                         SQLHANDLE pInputHandle, SQLHANDLE *ppOutputHandle) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLAllocHandle(iCallOrRet, iRc, hHandleType, pInputHandle,
                                ppOutputHandle);
        logger.process();
    }
}

void TraceSQLFreeHandle(int iCallOrRet, SQLRETURN iRc, SQLSMALLINT hHandleType,
                        SQLHANDLE pHandle) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLFreeHandle(iCallOrRet, iRc, hHandleType, pHandle);
        logger.process();
    }
}

void TraceSQLConnect(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                     SQLCHAR *szDSN, SQLSMALLINT cchDSN, SQLCHAR *szUID,
                     SQLSMALLINT cchUID, SQLCHAR *szAuthStr,
                     SQLSMALLINT cchAuthStr) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLConnect(iCallOrRet, iRc, phdbc, szDSN, cchDSN, szUID,
                                cchUID, szAuthStr, cchAuthStr);
        logger.process();
    }
}

void TraceSQLDisconnect(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLDisconnect(iCallOrRet, iRc, phdbc);
        logger.process();
    }
}

void TraceSQLDriverConnect(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                           SQLHWND hwnd, SQLCHAR *szConnStrIn,
                           SQLSMALLINT cbConnStrIn, SQLCHAR *szConnStrOut,
                           SQLSMALLINT cbConnStrOut, SQLSMALLINT *pcbConnStrOut,
                           SQLUSMALLINT hDriverCompletion) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLDriverConnect(iCallOrRet, iRc, phdbc, hwnd, szConnStrIn,
                                      cbConnStrIn, szConnStrOut, cbConnStrOut,
                                      pcbConnStrOut, hDriverCompletion);
        logger.process();
    }
}

void TraceSQLBrowseConnect(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                           SQLCHAR *szConnStrIn, SQLSMALLINT cbConnStrIn,
                           SQLCHAR *szConnStrOut, SQLSMALLINT cbConnStrOut,
                           SQLSMALLINT *pcbConnStrOut) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLBrowseConnect(iCallOrRet, iRc, phdbc, szConnStrIn,
                                      cbConnStrIn, szConnStrOut, cbConnStrOut,
                                      pcbConnStrOut);
        logger.process();
    }
}

void TraceSQLError(int iCallOrRet, SQLRETURN iRc, SQLHENV phenv, SQLHDBC phdbc,
                   SQLHSTMT phstmt, SQLCHAR *pSqlstate,
                   SQLINTEGER *pNativeError, SQLCHAR *pMessageText,
                   SQLSMALLINT cbLen, SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLError(iCallOrRet, iRc, phenv, phdbc, phstmt, pSqlstate,
                              pNativeError, pMessageText, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetDiagRec(int iCallOrRet, SQLRETURN iRc, SQLSMALLINT hHandleType,
                        SQLHANDLE pHandle, SQLSMALLINT hRecNumber,
                        SQLCHAR *pSqlstate, SQLINTEGER *piNativeError,
                        SQLCHAR *pMessageText, SQLSMALLINT cbLen,
                        SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDiagRec(iCallOrRet, iRc, hHandleType, pHandle,
                                   hRecNumber, pSqlstate, piNativeError,
                                   pMessageText, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetDiagField(int iCallOrRet, SQLRETURN iRc,
                          SQLSMALLINT hHandleType, SQLHANDLE pHandle,
                          SQLSMALLINT hRecNumber, SQLSMALLINT hDiagIdentifier,
                          SQLPOINTER pDiagInfo, SQLSMALLINT cbLen,
                          SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDiagField(iCallOrRet, iRc, hHandleType, pHandle,
                                     hRecNumber, hDiagIdentifier, pDiagInfo,
                                     cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLTransact(int iCallOrRet, SQLRETURN iRc, SQLHENV phenv,
                      SQLHDBC phdbc, SQLUSMALLINT hCompletionType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLTransact(iCallOrRet, iRc, phenv, phdbc,
                                 hCompletionType);
        logger.process();
    }
}

void TraceSQLEndTran(int iCallOrRet, SQLRETURN iRc, SQLSMALLINT hHandleType,
                     SQLHANDLE pHandle, SQLSMALLINT hCompletionType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLEndTran(iCallOrRet, iRc, hHandleType, pHandle,
                                hCompletionType);
        logger.process();
    }
}

void TraceSQLGetInfo(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                     SQLUSMALLINT hInfoType, SQLPOINTER pInfoValue,
                     SQLSMALLINT cbLen, SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetInfo(iCallOrRet, iRc, phdbc, hInfoType, pInfoValue,
                                cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetFunctions(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                          SQLUSMALLINT uhFunctionId,
                          SQLUSMALLINT *puhSupported) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetFunctions(iCallOrRet, iRc, phdbc, uhFunctionId,
                                     puhSupported);
        logger.process();
    }
}

void TraceSQLExecDirect(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                        SQLCHAR *pCmd, SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLExecDirect(iCallOrRet, iRc, phstmt, pCmd, cbLen);
        logger.process();
    }
}

void TraceSQLExecute(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLExecute(iCallOrRet, iRc, phstmt);
        logger.process();
    }
}

void TraceSQLPrepare(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                     SQLCHAR *pCmd, SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLPrepare(iCallOrRet, iRc, phstmt, pCmd, cbLen);
        logger.process();
    }
}

void TraceSQLCancel(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLCancel(iCallOrRet, iRc, phstmt);
        logger.process();
    }
}

void TraceSQLParamData(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                       SQLPOINTER *ppValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLParamData(iCallOrRet, iRc, phstmt, ppValue);
        logger.process();
    }
}

void TraceSQLPutData(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                     SQLPOINTER pData, SQLLEN cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLPutData(iCallOrRet, iRc, phstmt, pData, cbLen);
        logger.process();
    }
}

void TraceSQLBulkOperations(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                            SQLSMALLINT hOperation) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLBulkOperations(iCallOrRet, iRc, phstmt, hOperation);
        logger.process();
    }
}

void TraceSQLNativeSql(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                       SQLCHAR *szSqlStrIn, SQLINTEGER cbSqlStrIn,
                       SQLCHAR *szSqlStrOut, SQLINTEGER cbSqlStrOut,
                       SQLINTEGER *pcbSqlStrOut) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLNativeSql(iCallOrRet, iRc, phdbc, szSqlStrIn,
                                  cbSqlStrIn, szSqlStrOut, cbSqlStrOut,
                                  pcbSqlStrOut);
        logger.process();
    }
}

void TraceSQLSetCursorName(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLCHAR *pCursorName, SQLSMALLINT cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetCursorName(iCallOrRet, iRc, phstmt, pCursorName,
                                      cbLen);
        logger.process();
    }
}

void TraceSQLGetCursorName(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLCHAR *pCursorName, SQLSMALLINT cbLen,
                           SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetCursorName(iCallOrRet, iRc, phstmt, pCursorName,
                                      cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLCloseCursor(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLCloseCursor(iCallOrRet, iRc, phstmt);
        logger.process();
    }
}

void TraceSQLBindParameter(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hParam, SQLSMALLINT hInOutType,
                           SQLSMALLINT hType, SQLSMALLINT hSQLType,
                           SQLULEN iColSize, SQLSMALLINT hScale,
                           SQLPOINTER pValue, SQLLEN cbLen, SQLLEN *pcbLenInd) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLBindParameter(iCallOrRet, iRc, phstmt, hParam,
                                      hInOutType, hType, hSQLType, iColSize,
                                      hScale, pValue, cbLen, pcbLenInd);
        logger.process();
    }
}

void TraceSQLSetParam(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                      SQLUSMALLINT hParam, SQLSMALLINT hValType,
                      SQLSMALLINT hParamType, SQLULEN iLengthPrecision,
                      SQLSMALLINT hParamScale, SQLPOINTER pParamVal,
                      SQLLEN *piStrLen_or_Ind) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetParam(iCallOrRet, iRc, phstmt, hParam, hValType,
                                 hParamType, iLengthPrecision, hParamScale,
                                 pParamVal, piStrLen_or_Ind);
        logger.process();
    }
}

void TraceSQLBindParam(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                       SQLUSMALLINT hParam, SQLSMALLINT hValType,
                       SQLSMALLINT hParamType, SQLULEN iLengthPrecision,
                       SQLSMALLINT hParamScale, SQLPOINTER pParamVal,
                       SQLLEN *piStrLen_or_Ind) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLBindParam(iCallOrRet, iRc, phstmt, hParam, hValType,
                                  hParamType, iLengthPrecision, hParamScale,
                                  pParamVal, piStrLen_or_Ind);
        logger.process();
    }
}

void TraceSQLNumParams(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                       SQLSMALLINT *pParamCount) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLNumParams(iCallOrRet, iRc, phstmt, pParamCount);
        logger.process();
    }
}

void TraceSQLParamOptions(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLULEN iCrow, SQLULEN *piRow) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLParamOptions(iCallOrRet, iRc, phstmt, iCrow, piRow);
        logger.process();
    }
}

void TraceSQLDescribeParam(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hParam, SQLSMALLINT *pDataType,
                           SQLULEN *pParamSize, SQLSMALLINT *pDecimalDigits,
                           SQLSMALLINT *pNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLDescribeParam(iCallOrRet, iRc, phstmt, hParam,
                                      pDataType, pParamSize, pDecimalDigits,
                                      pNullable);
        logger.process();
    }
}
    

void TraceSQLNumResultCols(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLSMALLINT *pColumnCount) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLNumResultCols(iCallOrRet, iRc, phstmt, pColumnCount);
        logger.process();
    }
}

void TraceSQLDescribeCol(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLUSMALLINT hCol, SQLCHAR *pColName,
                         SQLSMALLINT cbLen, SQLSMALLINT *pcbLen,
                         SQLSMALLINT *pDataType, SQLULEN *pColSize,
                         SQLSMALLINT *pDecimalDigits, SQLSMALLINT *pNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLDescribeCol(iCallOrRet, iRc, phstmt, hCol, pColName,
                                    cbLen, pcbLen, pDataType, pColSize,
                                    pDecimalDigits, pNullable);
        logger.process();
    }
}

void TraceSQLColAttribute(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLUSMALLINT hCol, SQLUSMALLINT hFieldIdentifier,
                          SQLPOINTER pcValue, SQLSMALLINT cbLen,
                          SQLSMALLINT *pcbLen, SQLLEN *plValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColAttribute(iCallOrRet, iRc, phstmt, hCol,
                                     hFieldIdentifier, pcValue, cbLen, pcbLen,
                                     plValue);
        logger.process();
    }
}

void TraceSQLColAttributes(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hCol, SQLUSMALLINT hFieldIdentifier,
                           SQLPOINTER pcValue, SQLSMALLINT cbLen,
                           SQLSMALLINT *pcbLen, SQLLEN *plValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColAttributes(iCallOrRet, iRc, phstmt, hCol,
                                      hFieldIdentifier, pcValue, cbLen, pcbLen,
                                      plValue);
        logger.process();
    }
}

void TraceSQLBindCol(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                     SQLUSMALLINT hCol, SQLSMALLINT hType, SQLPOINTER pValue,
                     SQLLEN cbLen, SQLLEN *pcbLenInd) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLBindCol(iCallOrRet, iRc, phstmt, hCol, hType, pValue,
                                cbLen, pcbLenInd);
        logger.process();
    }
}

void TraceSQLFetch(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLFetch(iCallOrRet, iRc, phstmt);
        logger.process();
    }
}

void TraceSQLMoreResults(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLMoreResults(iCallOrRet, iRc, phstmt);
        logger.process();
    }
}

void TraceSQLExtendedFetch(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hFetchOrientation, SQLLEN iFetchOffset,
                           SQLULEN *piRowCount, SQLUSMALLINT *phRowStatus) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLExtendedFetch(iCallOrRet, iRc, phstmt,
                                      hFetchOrientation, iFetchOffset,
                                      piRowCount, phRowStatus);
        logger.process();
    }
}

void TraceSQLGetData(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                     SQLUSMALLINT hCol, SQLSMALLINT hType, SQLPOINTER pValue,
                     SQLLEN cbLen, SQLLEN *pcbLenInd) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetData(iCallOrRet, iRc, phstmt, hCol, hType, pValue,
                                cbLen, pcbLenInd);
        logger.process();
    }
}

void TraceSQLRowCount(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                      SQLLEN *pRowCount) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLRowCount(iCallOrRet, iRc, phstmt, pRowCount);
        logger.process();
    }
}

void TraceSQLSetPos(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                    SQLSETPOSIROW iRow, SQLUSMALLINT hOperation,
                    SQLUSMALLINT hLockType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetPos(iCallOrRet, iRc, phstmt, iRow, hOperation,
                               hLockType);
        logger.process();
    }
}

void TraceSQLFetchScroll(int iCallOrRet, SQLRETURN iRc, SQLHSTMT hstmt,
                         SQLSMALLINT FetchOrientation, SQLLEN FetchOffset) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLFetchScroll(iCallOrRet, iRc, hstmt, FetchOrientation,
                                    FetchOffset);
        logger.process();
    }
}

void TraceSQLCopyDesc(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdescSrc,
                      SQLHDESC phdescDest) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLCopyDesc(iCallOrRet, iRc, phdescSrc, phdescDest);
        logger.process();
    }
}

void TraceSQLGetDescField(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                          SQLSMALLINT hRecNumber, SQLSMALLINT hFieldIdentifier,
                          SQLPOINTER pValue, SQLINTEGER cbLen,
                          SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDescField(iCallOrRet, iRc, phdesc, hRecNumber,
                                     hFieldIdentifier, pValue, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetDescRec(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                        SQLSMALLINT hRecNumber, SQLCHAR *pName,
                        SQLSMALLINT cbName, SQLSMALLINT *pcbName,
                        SQLSMALLINT *phType, SQLSMALLINT *phSubType,
                        SQLLEN *plOctetLength, SQLSMALLINT *phPrecision,
                        SQLSMALLINT *phScale, SQLSMALLINT *phNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDescRec(
        iCallOrRet, iRc, phdesc, hRecNumber, pName, cbName, pcbName, phType,
        phSubType, plOctetLength, phPrecision, phScale, phNullable);
        logger.process();
    }
}

void TraceSQLSetDescField(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                          SQLSMALLINT hRecNumber, SQLSMALLINT hFieldIdentifier,
                          SQLPOINTER pValue, SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetDescField(iCallOrRet, iRc, phdesc, hRecNumber,
                                     hFieldIdentifier, pValue, cbLen);
        logger.process();
    }
}

void TraceSQLSetDescRec(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                        SQLSMALLINT hRecNumber, SQLSMALLINT hType,
                        SQLSMALLINT hSubType, SQLLEN iOctetLength,
                        SQLSMALLINT hPrecision, SQLSMALLINT hScale,
                        SQLPOINTER pData, SQLLEN *plStrLen,
                        SQLLEN *plIndicator) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetDescRec(iCallOrRet, iRc, phdesc, hRecNumber, hType,
                                   hSubType, iOctetLength, hPrecision, hScale,
                                   pData, plStrLen, plIndicator);
        logger.process();
    }
}

void TraceSQLSetConnectOption(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                              SQLUSMALLINT hOption, SQLULEN pValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetConnectOption(iCallOrRet, iRc, phdbc, hOption,
                                         pValue);
        logger.process();
    }
}

void TraceSQLGetConnectOption(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                              SQLUSMALLINT hOption, SQLPOINTER pValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetConnectOption(iCallOrRet, iRc, phdbc, hOption,
                                         pValue);
        logger.process();
    }
}

void TraceSQLSetStmtOption(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hOption, SQLULEN pValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetStmtOption(iCallOrRet, iRc, phstmt, hOption, pValue);
        logger.process();
    }
}

void TraceSQLGetStmtOption(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hOption, SQLPOINTER pValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetStmtOption(iCallOrRet, iRc, phstmt, hOption, pValue);
        logger.process();
    }
}

void TraceSQLSetScrollOptions(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                              SQLUSMALLINT hConcurrency, SQLLEN iKeysetSize,
                              SQLUSMALLINT hRowsetSize) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetScrollOptions(iCallOrRet, iRc, phstmt, hConcurrency,
                                         iKeysetSize, hRowsetSize);
        logger.process();
    }
}

void TraceSQLGetConnectAttr(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                            SQLINTEGER iAttribute, SQLPOINTER pValue,
                            SQLINTEGER cbLen, SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetConnectAttr(iCallOrRet, iRc, phdbc, iAttribute,
                                       pValue, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetStmtAttr(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLINTEGER iAttribute, SQLPOINTER pValue,
                         SQLINTEGER cbLen, SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetStmtAttr(iCallOrRet, iRc, phstmt, iAttribute, pValue,
                                    cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLSetConnectAttr(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                            SQLINTEGER iAttribute, SQLPOINTER pValue,
                            SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetConnectAttr(iCallOrRet, iRc, phdbc, iAttribute,
                                       pValue, cbLen);
        logger.process();
    }
}

void TraceSQLSetStmtAttr(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLINTEGER iAttribute, SQLPOINTER pValue,
                         SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetStmtAttr(iCallOrRet, iRc, phstmt, iAttribute, pValue,
                                    cbLen);
        logger.process();
    }
}

void TraceSQLSetEnvAttr(int iCallOrRet, SQLRETURN iRc, SQLHENV phenv,
                        SQLINTEGER iAttribute, SQLPOINTER pValue,
                        SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetEnvAttr(iCallOrRet, iRc, phenv, iAttribute, pValue,
                                   cbLen);
        logger.process();
    }
}

void TraceSQLGetEnvAttr(int iCallOrRet, SQLRETURN iRc, SQLHENV phenv,
                        SQLINTEGER iAttribute, SQLPOINTER pValue,
                        SQLINTEGER cbLen, SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetEnvAttr(iCallOrRet, iRc, phenv, iAttribute, pValue,
                                   cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLTables(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                    SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                    SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                    SQLCHAR *pTableName, SQLSMALLINT cbTableName,
                    SQLCHAR *pTableType, SQLSMALLINT cbTableType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLTables(
        iCallOrRet, iRc, phstmt, pCatalogName, cbCatalogName, pSchemaName,
        cbSchemaName, pTableName, cbTableName, pTableType, cbTableType);
        logger.process();
    }
}

void TraceSQLColumns(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                     SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                     SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                     SQLCHAR *pTableName, SQLSMALLINT cbTableName,
                     SQLCHAR *pColumnName, SQLSMALLINT cbColumnName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColumns(
        iCallOrRet, iRc, phstmt, pCatalogName, cbCatalogName, pSchemaName,
        cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);
        logger.process();
    }
}

void TraceSQLStatistics(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                        SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                        SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                        SQLCHAR *pTableName, SQLSMALLINT cbTableName,
                        SQLUSMALLINT hUnique, SQLUSMALLINT hReserved) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLStatistics(iCallOrRet, iRc, phstmt, pCatalogName,
                                   cbCatalogName, pSchemaName, cbSchemaName,
                                   pTableName, cbTableName, hUnique, hReserved);
        logger.process();
    }
}
    

void TraceSQLSpecialColumns(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                            SQLUSMALLINT hIdenType, SQLCHAR *pCatalogName,
                            SQLSMALLINT cbCatalogName, SQLCHAR *pSchemaName,
                            SQLSMALLINT cbSchemaName, SQLCHAR *pTableName,
                            SQLSMALLINT cbTableName, SQLUSMALLINT hScope,
                            SQLUSMALLINT hNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSpecialColumns(
        iCallOrRet, iRc, phstmt, hIdenType, pCatalogName, cbCatalogName,
        pSchemaName, cbSchemaName, pTableName, cbTableName, hScope, hNullable);
        logger.process();
    }
}

void TraceSQLProcedureColumns(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                              SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                              SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                              SQLCHAR *pProcName, SQLSMALLINT cbProcName,
                              SQLCHAR *pColumnName, SQLSMALLINT cbColumnName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLProcedureColumns(
        iCallOrRet, iRc, phstmt, pCatalogName, cbCatalogName, pSchemaName,
        cbSchemaName, pProcName, cbProcName, pColumnName, cbColumnName);
        logger.process();
    }
}

void TraceSQLProcedures(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                        SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                        SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                        SQLCHAR *pProcName, SQLSMALLINT cbProcName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLProcedures(iCallOrRet, iRc, phstmt, pCatalogName,
                                   cbCatalogName, pSchemaName, cbSchemaName,
                                   pProcName, cbProcName);
        logger.process();
    }
}

void TraceSQLForeignKeys(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLCHAR *pPkCatalogName, SQLSMALLINT cbPkCatalogName,
                         SQLCHAR *pPkSchemaName, SQLSMALLINT cbPkSchemaName,
                         SQLCHAR *pPkTableName, SQLSMALLINT cbPkTableName,
                         SQLCHAR *pFkCatalogName, SQLSMALLINT cbFkCatalogName,
                         SQLCHAR *pFkSchemaName, SQLSMALLINT cbFkSchemaName,
                         SQLCHAR *pFkTableName, SQLSMALLINT cbFkTableName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLForeignKeys(
        iCallOrRet, iRc, phstmt, pPkCatalogName, cbPkCatalogName, pPkSchemaName,
        cbPkSchemaName, pPkTableName, cbPkTableName, pFkCatalogName,
        cbFkCatalogName, pFkSchemaName, cbFkSchemaName, pFkTableName,
        cbFkTableName);
        logger.process();
    }
}

void TraceSQLPrimaryKeys(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                         SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                         SQLCHAR *pTableName, SQLSMALLINT cbTableName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLPrimaryKeys(iCallOrRet, iRc, phstmt, pCatalogName,
                                    cbCatalogName, pSchemaName, cbSchemaName,
                                    pTableName, cbTableName);
        logger.process();
    }
}

void TraceSQLGetTypeInfo(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLSMALLINT hType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetTypeInfo(iCallOrRet, iRc, phstmt, hType);
        logger.process();
    }
}

void TraceSQLColumnPrivileges(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                              SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                              SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                              SQLCHAR *pTableName, SQLSMALLINT cbTableName,
                              SQLCHAR *pColumnName, SQLSMALLINT cbColumnName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColumnPrivileges(
        iCallOrRet, iRc, phstmt, pCatalogName, cbCatalogName, pSchemaName,
        cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);
        logger.process();
    }
}

void TraceSQLTablePrivileges(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                             SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName,
                             SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName,
                             SQLCHAR *pTableName, SQLSMALLINT cbTableName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLTablePrivileges(iCallOrRet, iRc, phstmt, pCatalogName,
                                        cbCatalogName, pSchemaName,
                                        cbSchemaName, pTableName, cbTableName);
        logger.process();
    }
}

void TraceSQLConnectW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                      SQLWCHAR *wszDSN, SQLSMALLINT cchDSN, SQLWCHAR *wszUID,
                      SQLSMALLINT cchUID, SQLWCHAR *wszAuthStr,
                      SQLSMALLINT cchAuthStr) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLConnectW(iCallOrRet, iRc, phdbc, wszDSN, cchDSN, wszUID,
                                 cchUID, wszAuthStr, cchAuthStr);
        logger.process();
    }
}

void TraceSQLDriverConnectW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                            SQLHWND hwnd, SQLWCHAR *wszConnStrIn,
                            SQLSMALLINT cchConnStrIn, SQLWCHAR *wszConnStrOut,
                            SQLSMALLINT cchConnStrOut,
                            SQLSMALLINT *pcchConnStrOut,
                            SQLUSMALLINT hDriverCompletion) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLDriverConnectW(
        iCallOrRet, iRc, phdbc, hwnd, wszConnStrIn, cchConnStrIn, wszConnStrOut,
        cchConnStrOut, pcchConnStrOut, hDriverCompletion);
        logger.process();
    }
}

void TraceSQLBrowseConnectW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                            SQLWCHAR *wszConnStrIn, SQLSMALLINT cchConnStrIn,
                            SQLWCHAR *wszConnStrOut, SQLSMALLINT cchConnStrOut,
                            SQLSMALLINT *pcchConnStrOut) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLBrowseConnectW(iCallOrRet, iRc, phdbc, wszConnStrIn,
                                       cchConnStrIn, wszConnStrOut,
                                       cchConnStrOut, pcchConnStrOut);
        logger.process();
    }
}

void TraceSQLGetInfoW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                      SQLUSMALLINT hInfoType, SQLPOINTER pwInfoValue,
                      SQLSMALLINT cbLen, SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetInfoW(iCallOrRet, iRc, phdbc, hInfoType, pwInfoValue,
                                 cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLErrorW(int iCallOrRet, SQLRETURN iRc, SQLHENV phenv, SQLHDBC phdbc,
                    SQLHSTMT phstmt, SQLWCHAR *pwSqlState,
                    SQLINTEGER *piNativeError, SQLWCHAR *pwMessageText,
                    SQLSMALLINT cchLen, SQLSMALLINT *pcchLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLErrorW(iCallOrRet, iRc, phenv, phdbc, phstmt,
                               pwSqlState, piNativeError, pwMessageText, cchLen,
                               pcchLen);
        logger.process();
    }
}

void TraceSQLGetDiagRecW(int iCallOrRet, SQLRETURN iRc, SQLSMALLINT hHandleType,
                         SQLHANDLE pHandle, SQLSMALLINT hRecNumber,
                         SQLWCHAR *pwSqlState, SQLINTEGER *piNativeError,
                         SQLWCHAR *pwMessageText, SQLSMALLINT cchLen,
                         SQLSMALLINT *pcchLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDiagRecW(iCallOrRet, iRc, hHandleType, pHandle,
                                    hRecNumber, pwSqlState, piNativeError,
                                    pwMessageText, cchLen, pcchLen);
        logger.process();
    }
}

void TraceSQLGetDiagFieldW(int iCallOrRet, SQLRETURN iRc,
                           SQLSMALLINT hHandleType, SQLHANDLE pHandle,
                           SQLSMALLINT hRecNumber, SQLSMALLINT hDiagIdentifier,
                           SQLPOINTER pwDiagInfo, SQLSMALLINT cbLen,
                           SQLSMALLINT *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDiagFieldW(iCallOrRet, iRc, hHandleType, pHandle,
                                      hRecNumber, hDiagIdentifier, pwDiagInfo,
                                      cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLExecDirectW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLWCHAR *pwCmd, SQLINTEGER cchLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLExecDirectW(iCallOrRet, iRc, phstmt, pwCmd, cchLen);
        logger.process();
    }
}

void TraceSQLNativeSqlW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                        SQLWCHAR *wszSqlStrIn, SQLINTEGER cchSqlStrIn,
                        SQLWCHAR *wszSqlStrOut, SQLINTEGER cchSqlStrOut,
                        SQLINTEGER *pcchSqlStrOut) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLNativeSqlW(iCallOrRet, iRc, phdbc, wszSqlStrIn,
                                   cchSqlStrIn, wszSqlStrOut, cchSqlStrOut,
                                   pcchSqlStrOut);
        logger.process();
    }
}

void TraceSQLGetConnectAttrW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                             SQLINTEGER iAttribute, SQLPOINTER pwValue,
                             SQLINTEGER cbLen, SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetConnectAttrW(iCallOrRet, iRc, phdbc, iAttribute,
                                        pwValue, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetConnectOptionW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                               SQLUSMALLINT hOption, SQLPOINTER pwValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetConnectOptionW(iCallOrRet, iRc, phdbc, hOption,
                                          pwValue);
        logger.process();
    }
}

void TraceSQLGetStmtAttrW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLINTEGER iAttribute, SQLPOINTER pwValue,
                          SQLINTEGER cbLen, SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetStmtAttrW(iCallOrRet, iRc, phstmt, iAttribute,
                                     pwValue, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLSetConnectAttrW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                             SQLINTEGER iAttribute, SQLPOINTER pwValue,
                             SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetConnectAttrW(iCallOrRet, iRc, phdbc, iAttribute,
                                        pwValue, cbLen);
        logger.process();
    }
}

void TraceSQLSetConnectOptionW(int iCallOrRet, SQLRETURN iRc, SQLHDBC phdbc,
                               SQLUSMALLINT hOption, SQLULEN pwValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetConnectOptionW(iCallOrRet, iRc, phdbc, hOption,
                                          pwValue);
        logger.process();
    }
}

void TraceSQLSetStmtAttrW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLINTEGER iAttribute, SQLPOINTER pwValue,
                          SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetStmtAttrW(iCallOrRet, iRc, phstmt, iAttribute,
                                     pwValue, cbLen);
        logger.process();
    }
}

void TraceSQLGetCursorNameW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                            SQLWCHAR *pwCursorName, SQLSMALLINT cchLen,
                            SQLSMALLINT *pcchLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetCursorNameW(iCallOrRet, iRc, phstmt, pwCursorName,
                                       cchLen, pcchLen);
        logger.process();
    }
}

void TraceSQLPrepareW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                      SQLWCHAR *pwCmd, SQLINTEGER cchLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLPrepareW(iCallOrRet, iRc, phstmt, pwCmd, cchLen);
        logger.process();
    }
}

void TraceSQLSetCursorNameW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                            SQLWCHAR *pwCursorName, SQLSMALLINT cchLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetCursorNameW(iCallOrRet, iRc, phstmt, pwCursorName,
                                       cchLen);
        logger.process();
    }
}

void TraceSQLColAttributesW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                            SQLUSMALLINT hCol, SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER pwValue, SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen, SQLLEN *plValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColAttributesW(iCallOrRet, iRc, phstmt, hCol,
                                       hFieldIdentifier, pwValue, cbLen, pcbLen,
                                       plValue);
        logger.process();
    }
}

void TraceSQLColAttributeW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                           SQLUSMALLINT hCol, SQLUSMALLINT hFieldIdentifier,
                           SQLPOINTER pwValue, SQLSMALLINT cbLen,
                           SQLSMALLINT *pcbLen, SQLLEN *plValue) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColAttributeW(iCallOrRet, iRc, phstmt, hCol,
                                      hFieldIdentifier, pwValue, cbLen, pcbLen,
                                      plValue);
        logger.process();
    }
}

void TraceSQLDescribeColW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLUSMALLINT hCol, SQLWCHAR *pwColName,
                          SQLSMALLINT cchLen, SQLSMALLINT *pcchLen,
                          SQLSMALLINT *pDataType, SQLULEN *pColSize,
                          SQLSMALLINT *pDecimalDigits, SQLSMALLINT *pNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLDescribeColW(iCallOrRet, iRc, phstmt, hCol, pwColName,
                                     cchLen, pcchLen, pDataType, pColSize,
                                     pDecimalDigits, pNullable);
        logger.process();
    }
}

void TraceSQLGetDescFieldW(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                           SQLSMALLINT hRecNumber, SQLSMALLINT hFieldIdentifier,
                           SQLPOINTER pwValue, SQLINTEGER cbLen,
                           SQLINTEGER *pcbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDescFieldW(iCallOrRet, iRc, phdesc, hRecNumber,
                                      hFieldIdentifier, pwValue, cbLen, pcbLen);
        logger.process();
    }
}

void TraceSQLGetDescRecW(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                         SQLSMALLINT hRecNumber, SQLWCHAR *pwName,
                         SQLSMALLINT cchName, SQLSMALLINT *pcchName,
                         SQLSMALLINT *phType, SQLSMALLINT *phSubType,
                         SQLLEN *plOctetLength, SQLSMALLINT *phPrecision,
                         SQLSMALLINT *phScale, SQLSMALLINT *phNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetDescRecW(
        iCallOrRet, iRc, phdesc, hRecNumber, pwName, cchName, pcchName, phType,
        phSubType, plOctetLength, phPrecision, phScale, phNullable);
        logger.process();
    }
}

void TraceSQLSetDescFieldW(int iCallOrRet, SQLRETURN iRc, SQLHDESC phdesc,
                           SQLSMALLINT hRecNumber, SQLSMALLINT hFieldIdentifier,
                           SQLPOINTER pwValue, SQLINTEGER cbLen) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSetDescFieldW(iCallOrRet, iRc, phdesc, hRecNumber,
                                      hFieldIdentifier, pwValue, cbLen);
        logger.process();
    }
}

void TraceSQLColumnPrivilegesW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                               SQLWCHAR *pwCatalogName,
                               SQLSMALLINT cchCatalogName,
                               SQLWCHAR *pwSchemaName,
                               SQLSMALLINT cchSchemaName, SQLWCHAR *pwTableName,
                               SQLSMALLINT cchTableName, SQLWCHAR *pwColumnName,
                               SQLSMALLINT cchColumnName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColumnPrivilegesW(
        iCallOrRet, iRc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName,
        cchSchemaName, pwTableName, cchTableName, pwColumnName, cchColumnName);
        logger.process();
    }
}

void TraceSQLColumnsW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                      SQLWCHAR *pwCatalogName, SQLSMALLINT cchCatalogName,
                      SQLWCHAR *pwSchemaName, SQLSMALLINT cchSchemaName,
                      SQLWCHAR *pwTableName, SQLSMALLINT cchTableName,
                      SQLWCHAR *pwColumnName, SQLSMALLINT cchColumnName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLColumnsW(
        iCallOrRet, iRc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName,
        cchSchemaName, pwTableName, cchTableName, pwColumnName, cchColumnName);
        logger.process();
    }
}

void TraceSQLForeignKeysW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLWCHAR *pwPkCatalogName,
                          SQLSMALLINT cchPkCatalogName,
                          SQLWCHAR *pwPkSchemaName, SQLSMALLINT cchPkSchemaName,
                          SQLWCHAR *pwPkTableName, SQLSMALLINT cchPkTableName,
                          SQLWCHAR *pwFkCatalogName,
                          SQLSMALLINT cchFkCatalogName,
                          SQLWCHAR *pwFkSchemaName, SQLSMALLINT cchFkSchemaName,
                          SQLWCHAR *pwFkTableName, SQLSMALLINT cchFkTableName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLForeignKeysW(
        iCallOrRet, iRc, phstmt, pwPkCatalogName, cchPkCatalogName,
        pwPkSchemaName, cchPkSchemaName, pwPkTableName, cchPkTableName,
        pwFkCatalogName, cchFkCatalogName, pwFkSchemaName, cchFkSchemaName,
        pwFkTableName, cchFkTableName);
        logger.process();
    }
}

void TraceSQLGetTypeInfoW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLSMALLINT hType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLGetTypeInfoW(iCallOrRet, iRc, phstmt, hType);
        logger.process();
    }
}

void TraceSQLPrimaryKeysW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                          SQLWCHAR *pwCatalogName, SQLSMALLINT cchCatalogName,
                          SQLWCHAR *pwSchemaName, SQLSMALLINT cchSchemaName,
                          SQLWCHAR *pwTableName, SQLSMALLINT cchTableName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLPrimaryKeysW(iCallOrRet, iRc, phstmt, pwCatalogName,
                                     cchCatalogName, pwSchemaName,
                                     cchSchemaName, pwTableName, cchTableName);
        logger.process();
    }
}

void TraceSQLProcedureColumnsW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                               SQLWCHAR *pwCatalogName,
                               SQLSMALLINT cchCatalogName,
                               SQLWCHAR *pwSchemaName,
                               SQLSMALLINT cchSchemaName, SQLWCHAR *pwProcName,
                               SQLSMALLINT cchProcName, SQLWCHAR *pwColumnName,
                               SQLSMALLINT cchColumnName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLProcedureColumnsW(
        iCallOrRet, iRc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName,
        cchSchemaName, pwProcName, cchProcName, pwColumnName, cchColumnName);
        logger.process();
    }
}

void TraceSQLProceduresW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLWCHAR *pwCatalogName, SQLSMALLINT cchCatalogName,
                         SQLWCHAR *pwSchemaName, SQLSMALLINT cchSchemaName,
                         SQLWCHAR *pwProcName, SQLSMALLINT cchProcName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLProceduresW(iCallOrRet, iRc, phstmt, pwCatalogName,
                                    cchCatalogName, pwSchemaName, cchSchemaName,
                                    pwProcName, cchProcName);
        logger.process();
    }
}

void TraceSQLSpecialColumnsW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                             SQLUSMALLINT hIdenType, SQLWCHAR *pwCatalogName,
                             SQLSMALLINT cchCatalogName, SQLWCHAR *pwSchemaName,
                             SQLSMALLINT cchSchemaName, SQLWCHAR *pwTableName,
                             SQLSMALLINT cchTableName, SQLUSMALLINT hScope,
                             SQLUSMALLINT hNullable) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLSpecialColumnsW(
        iCallOrRet, iRc, phstmt, hIdenType, pwCatalogName, cchCatalogName,
        pwSchemaName, cchSchemaName, pwTableName, cchTableName, hScope,
        hNullable);
        logger.process();
    }
}

void TraceSQLStatisticsW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                         SQLWCHAR *pwCatalogName, SQLSMALLINT cchCatalogName,
                         SQLWCHAR *pwSchemaName, SQLSMALLINT cchSchemaName,
                         SQLWCHAR *pwTableName, SQLSMALLINT cchTableName,
                         SQLUSMALLINT hUnique, SQLUSMALLINT hReserved) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLStatisticsW(
        iCallOrRet, iRc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName,
        cchSchemaName, pwTableName, cchTableName, hUnique, hReserved);
        logger.process();
    }
}

void TraceSQLTablePrivilegesW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                              SQLWCHAR *pwCatalogName,
                              SQLSMALLINT cchCatalogName,
                              SQLWCHAR *pwSchemaName, SQLSMALLINT cchSchemaName,
                              SQLWCHAR *pwTableName, SQLSMALLINT cchTableName) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLTablePrivilegesW(
        iCallOrRet, iRc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName,
        cchSchemaName, pwTableName, cchTableName);
        logger.process();
    }
}

void TraceSQLTablesW(int iCallOrRet, SQLRETURN iRc, SQLHSTMT phstmt,
                     SQLWCHAR *pwCatalogName, SQLSMALLINT cchCatalogName,
                     SQLWCHAR *pwSchemaName, SQLSMALLINT cchSchemaName,
                     SQLWCHAR *pwTableName, SQLSMALLINT cchTableName,
                     SQLWCHAR *pwTableType, SQLSMALLINT cchTableType) {
    if (getRsLoglevel() >= LOG_LEVEL_DEBUG) {
        RsTrace logger;
        logger.TraceSQLTablesW(
        iCallOrRet, iRc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName,
        cchSchemaName, pwTableName, cchTableName, pwTableType, cchTableType);
        logger.process();
    }
}

