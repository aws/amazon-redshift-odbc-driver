/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#include "rsodbc.h"

#define TRACE_OFF        0
#define TRACE_ERROR        1
#define TRACE_API_CALL    2
#define TRACE_INFO        3
#define TRACE_MSG_PROTOCOL        4
#define TRACE_DEBUG        5
#define TRACE_DEBUG_APPEND_MODE        6

#define DEFAULT_TRACE_LEVEL TRACE_OFF

#define TRACE_FILE_NAME    "redshift_odbc.log"

#define IS_TRACE_ON() (gRsGlobalVars.iTraceLevel != TRACE_OFF)
#define IS_TRACE_LEVEL_ERROR() (gRsGlobalVars.iTraceLevel >= TRACE_ERROR)
#define IS_TRACE_LEVEL_API_CALL() (gRsGlobalVars.iTraceLevel >= TRACE_API_CALL)
#define IS_TRACE_LEVEL_INFO() (gRsGlobalVars.iTraceLevel >= TRACE_INFO)
#define IS_TRACE_LEVEL_MSG_PROTOCOL() (gRsGlobalVars.iTraceLevel >= TRACE_MSG_PROTOCOL)
#define IS_TRACE_LEVEL_DEBUG() (gRsGlobalVars.iTraceLevel >= TRACE_DEBUG)
#define IS_TRACE_LEVEL_DEBUG_APPEND() (gRsGlobalVars.iTraceLevel >= TRACE_DEBUG_APPEND_MODE)

#define WRITE_INTO_FILE(newLine) \
        if(gRsGlobalVars.fpTrace) \
        {                          \
            va_list args;         \
            va_start(args, fmt);  \
            vfprintf(gRsGlobalVars.fpTrace, fmt, args); \
            if(newLine) \
                fprintf(gRsGlobalVars.fpTrace,"\n"); \
            fflush(gRsGlobalVars.fpTrace); \
            va_end(args); \
        }

#define WRITE_INTO_FILE_WITH_ARG_LIST(newLine, args) \
        if(gRsGlobalVars.fpTrace) \
        {                          \
            vfprintf(gRsGlobalVars.fpTrace, fmt, args); \
            if(newLine) \
                fprintf(gRsGlobalVars.fpTrace,"\n"); \
            fflush(gRsGlobalVars.fpTrace); \
        }

#define    FUNC_CALL    0
#define    FUNC_RETURN 1

#define TRACE_MAX_STR_VAL_LEN 1024

void setTraceLevelAndFile(int iTracelLevel, char *pTraceFile);
void initTrace();
void createTraceFile();
void closeTraceFile();
FILE *getTraceFileHandle();
char *getTraceFileName();

void traceError(const char *fmt,...);
void traceAPICall(const char *fmt,...);
void traceInfo(const char *fmt,...);
void traceDebug(const char *fmt,...);
void traceArg(const char *fmt,...);
void traceArgVal(const char *fmt,...);
void traceDebugWithArgList(const char *fmt, va_list args);

void traceHandle(const char *pArgName, SQLHANDLE handle);
char *getRcString(SQLRETURN iRc);
void traceErrorList(SQLHENV phenv,SQLHDBC phdbc,SQLHSTMT phstmt,SQLHDESC phdesc);
void traceHandleType(SQLSMALLINT hHandleType);
void traceClosingBracket();
void traceStrValWithSmallLen(const char *pArgName, const char *pVal, SQLSMALLINT cbLen);
void traceStrValWithLargeLen(const char *pArgName, const char *pVal, SQLINTEGER cbLen);
void traceWStrValWithSmallLen(const char *pArgName, SQLWCHAR *pwVal, SQLSMALLINT cchLen);
void traceWStrValWithLargeLen(const char *pArgName, SQLWCHAR *pwVal, SQLINTEGER cchLen);
void traceStrSmallLen(const char *pArgName, SQLSMALLINT cbLen);
void traceStrLargeLen(const char *pArgName, SQLINTEGER cbLen);
void tracePointer(const char *var, void *ptr);
void traceStrOutSmallLen(const char *pArgName, SQLSMALLINT *pcbLen);
void traceStrOutLargeLen(const char *pArgName, SQLINTEGER *pcbLen);

void traceIntPtrVal(const char *pArgName, int *piVal);
void traceLongPtrVal(const char *pArgName, long *plVal);
void traceShortPtrVal(const char *pArgName, short *phVal);
void traceLongLongPtrVal(const char *pArgName, long long*pllVal);
void traceFloatPtrVal(const char *pArgName, float *pfVal);
void traceDoublePtrVal(const char *pArgName, double *pdVal);
void traceBitPtrVal(const char *pArgName, char *pbVal);

void traceIntVal(const char *pArgName,int iVal);
void traceLongVal(const char *pArgName,long lVal);
void traceShortVal(const char *pArgName,short hVal);
void traceFloatVal(const char *pArgName,float fVal);
void traceDoubleVal(const char *pArgName,double dVal);
void traceLongLongVal(const char *pArgName,long long llVal);
void traceBitVal(const char *pArgName,char bVal);
void traceDatePtrVal(const char *pArgName, DATE_STRUCT *pdtVal);
void traceTimeStampPtrVal(const char *pArgName, TIMESTAMP_STRUCT *ptsVal);
void traceTimePtrVal(const char *pArgName, TIME_STRUCT *ptVal);
void traceNumericPtrVal(const char *pArgName, SQL_NUMERIC_STRUCT *pnVal);

void traceDiagIdentifier(SQLSMALLINT hDiagIdentifier);
void traceDiagIdentifierOutput(SQLSMALLINT hDiagIdentifier,
                                SQLPOINTER  pDiagInfo, 
                                SQLSMALLINT cbLen,
                                int iUnicode);

void traceGetInfoType(SQLUSMALLINT hInfoType);
void traceGetInfoOutput(SQLUSMALLINT hInfoType, 
                        SQLPOINTER pInfoValue,
                        SQLSMALLINT cbLen,
                        int iUnicode);
void traceBulkOperationOption(SQLSMALLINT hOperation);

void traceCType(const char *pArgName, SQLSMALLINT hCType);
void traceSQLType(const char *pArgName,SQLSMALLINT hSQLType);
void traceNullableOutput(const char *pArgName,SQLSMALLINT *pNullable);

void traceFieldIdentifier(const char *pArgName, SQLUSMALLINT hFieldIdentifier);

void traceEnvAttr(const char *pArgName, SQLINTEGER iAttribute);
void traceEnvAttrVal(const char *pArgName, SQLINTEGER iAttribute,SQLPOINTER pVal, SQLINTEGER    cbLen);
void traceConnectAttr(const char *pArgName, SQLINTEGER iAttribute);
void traceConnectAttrVal(const char *pArgName, SQLINTEGER iAttribute,SQLPOINTER pVal, SQLINTEGER    cbLen, int iUnicode, int iSetVal);
void traceStmtAttr(const char *pArgName, SQLINTEGER iAttribute);
void traceStmtAttrVal(const char *pArgName, SQLINTEGER iAttribute,SQLPOINTER pVal, SQLINTEGER    cbLen, int iUnicode, int iSetVal);

void traceDriverCompletion(const char *pArgName, SQLUSMALLINT hDriverCompletion);
void traceIdenTypeSpecialColumns(const char *pArgName, SQLUSMALLINT   hIdenType);
void traceScopeSpecialColumns(const char *pArgName, SQLUSMALLINT   hScope);
void traceUniqueStatistics(const char *pArgName, SQLUSMALLINT hUnique);
void traceReservedStatistics(const char *pArgName, SQLUSMALLINT hReserved);

void traceData(const char *pArgName, SQLSMALLINT hType, SQLPOINTER pValue, SQLLEN cbLen);

void tracePasswordConnectString(char *var,char *szConnStr, SQLSMALLINT  cbConnStr);
void tracePasswordConnectStringW(char *var,SQLWCHAR *wszConnStr, SQLSMALLINT  cchConnStr);


void TraceSQLAllocEnv(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV *pphenv);

void TraceSQLAllocConnect(int iCallOrRet,
                            SQLRETURN  iRc,
                            HENV   phenv,
                            HDBC   *pphdbc);

void TraceSQLAllocStmt(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC   phdbc,
                        SQLHSTMT *pphstmt);


void TraceSQLFreeEnv(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV    phenv);

void TraceSQLFreeConnect(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDBC    phdbc);

void TraceSQLFreeStmt(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLUSMALLINT uhOption);

void TraceSQLAllocHandle(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLSMALLINT hHandleType,
                            SQLHANDLE pInputHandle, 
                            SQLHANDLE *ppOutputHandle);

void TraceSQLFreeHandle(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE pHandle);

void TraceSQLConnect(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC phdbc,
                        SQLCHAR *szDSN, 
                        SQLSMALLINT cchDSN,
                        SQLCHAR *szUID, 
                        SQLSMALLINT cchUID,
                        SQLCHAR *szAuthStr, 
                        SQLSMALLINT cchAuthStr);

void TraceSQLDisconnect(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC phdbc);


void TraceSQLDriverConnect(int              iCallOrRet,
                            SQLRETURN      iRc,
                            SQLHDBC          phdbc,
                            SQLHWND          hwnd,
                            SQLCHAR       *szConnStrIn,
                            SQLSMALLINT   cbConnStrIn,
                            SQLCHAR       *szConnStrOut,
                            SQLSMALLINT   cbConnStrOut,
                            SQLSMALLINT   *pcbConnStrOut,
                            SQLUSMALLINT   hDriverCompletion);

void TraceSQLBrowseConnect(int              iCallOrRet,
                            SQLRETURN      iRc,
                            SQLHDBC          phdbc,
                            SQLCHAR       *szConnStrIn,
                            SQLSMALLINT   cbConnStrIn,
                            SQLCHAR       *szConnStrOut,
                            SQLSMALLINT   cbConnStrOut,
                            SQLSMALLINT   *pcbConnStrOut);

void TraceSQLError(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHENV phenv,
                    SQLHDBC phdbc, 
                    SQLHSTMT phstmt,
                    SQLCHAR *pSqlstate,  
                    SQLINTEGER *pNativeError,
                    SQLCHAR *pMessageText, 
                    SQLSMALLINT cbLen,
                    SQLSMALLINT *pcbLen);

void TraceSQLGetDiagRec(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE pHandle,
                        SQLSMALLINT hRecNumber, 
                        SQLCHAR *pSqlstate,
                        SQLINTEGER *piNativeError, 
                        SQLCHAR *pMessageText,
                        SQLSMALLINT cbLen, 
                        SQLSMALLINT *pcbLen);

void TraceSQLGetDiagField(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLSMALLINT hHandleType, 
                            SQLHANDLE pHandle,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hDiagIdentifier,
                            SQLPOINTER  pDiagInfo, 
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen);

void TraceSQLTransact(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV phenv,
                        SQLHDBC phdbc, 
                        SQLUSMALLINT hCompletionType);

void TraceSQLEndTran(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE pHandle,
                        SQLSMALLINT hCompletionType);

void TraceSQLGetInfo(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC    phdbc,
                        SQLUSMALLINT hInfoType, 
                        SQLPOINTER pInfoValue,
                        SQLSMALLINT cbLen, 
                        SQLSMALLINT *pcbLen);

void TraceSQLGetFunctions(int iCallOrRet, 
                           SQLRETURN  iRc,
                           SQLHDBC phdbc,
                           SQLUSMALLINT uhFunctionId, 
                           SQLUSMALLINT *puhSupported);

void TraceSQLExecDirect(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLCHAR* pCmd,
                        SQLINTEGER cbLen);

void TraceSQLExecute(int iCallOrRet,
                     SQLRETURN  iRc,
                     SQLHSTMT   phstmt);


void TraceSQLPrepare(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLCHAR* pCmd,
                        SQLINTEGER cbLen);

void TraceSQLCancel(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT   phstmt);

void TraceSQLParamData(int iCallOrRet,
                       SQLRETURN  iRc,
                       SQLHSTMT   phstmt,
                       SQLPOINTER *ppValue);

void TraceSQLPutData(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT   phstmt,
                        SQLPOINTER pData, 
                        SQLLEN cbLen);

void TraceSQLBulkOperations(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLSMALLINT hOperation);

void  TraceSQLNativeSql(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC   phdbc,
                        SQLCHAR*    szSqlStrIn,
                        SQLINTEGER    cbSqlStrIn,
                        SQLCHAR*    szSqlStrOut,
                        SQLINTEGER  cbSqlStrOut,
                        SQLINTEGER  *pcbSqlStrOut);

void TraceSQLSetCursorName(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLCHAR* pCursorName,
                            SQLSMALLINT cbLen);

void TraceSQLGetCursorName(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLCHAR *pCursorName,
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen);

void TraceSQLCloseCursor(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt);

void TraceSQLBindParameter(int iCallOrRet, 
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
                            SQLLEN         *pcbLenInd);


void TraceSQLSetParam(int iCallOrRet, 
                        SQLRETURN  iRc,
                        SQLHSTMT    phstmt,
                        SQLUSMALLINT hParam, 
                        SQLSMALLINT hValType,
                        SQLSMALLINT hParamType, 
                        SQLULEN iLengthPrecision,
                        SQLSMALLINT hParamScale, 
                        SQLPOINTER pParamVal,
                        SQLLEN *piStrLen_or_Ind);

void TraceSQLBindParam(int iCallOrRet, 
                        SQLRETURN  iRc,
                        SQLHSTMT     phstmt,
                        SQLUSMALLINT hParam, 
                        SQLSMALLINT hValType,
                        SQLSMALLINT hParamType, 
                        SQLULEN        iLengthPrecision,
                        SQLSMALLINT hParamScale, 
                        SQLPOINTER  pParamVal,
                        SQLLEN        *piStrLen_or_Ind);

void TraceSQLNumParams(int iCallOrRet, 
                       SQLRETURN  iRc,
                       SQLHSTMT      phstmt,
                       SQLSMALLINT   *pParamCount);

void TraceSQLParamOptions(int iCallOrRet, 
                           SQLRETURN  iRc,
                           SQLHSTMT   phstmt,
                           SQLULEN  iCrow,
                           SQLULEN  *piRow);

void TraceSQLDescribeParam(int iCallOrRet, 
                            SQLRETURN  iRc,
                            SQLHSTMT        phstmt,
                            SQLUSMALLINT    hParam,
                            SQLSMALLINT     *pDataType,
                            SQLULEN         *pParamSize,
                            SQLSMALLINT     *pDecimalDigits,
                            SQLSMALLINT     *pNullable);

void TraceSQLNumResultCols(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLSMALLINT *pColumnCount);

void TraceSQLDescribeCol(int iCallOrRet,
                           SQLRETURN  iRc,
                           SQLHSTMT    phstmt,
                           SQLUSMALLINT hCol, 
                           SQLCHAR *pColName,
                           SQLSMALLINT cbLen, 
                           SQLSMALLINT *pcbLen,
                           SQLSMALLINT *pDataType,  
                           SQLULEN *pColSize,
                           SQLSMALLINT *pDecimalDigits,  
                           SQLSMALLINT *pNullable);

void TraceSQLColAttribute(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER    pcValue, 
                            SQLSMALLINT    cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN       *plValue);

void TraceSQLColAttributes(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER    pcValue, 
                            SQLSMALLINT    cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN       *plValue);

void TraceSQLBindCol(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT   phstmt,
                        SQLUSMALLINT hCol, 
                        SQLSMALLINT hType,
                        SQLPOINTER pValue, 
                        SQLLEN cbLen, 
                        SQLLEN *pcbLenInd);

void TraceSQLFetch(int iCallOrRet,
                     SQLRETURN  iRc,
                     SQLHSTMT   phstmt);

void TraceSQLMoreResults(int iCallOrRet,
                         SQLRETURN  iRc,
                         SQLHSTMT   phstmt);

void TraceSQLExtendedFetch(int  iCallOrRet,
                            SQLRETURN    iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT    hFetchOrientation,
                            SQLLEN          iFetchOffset,
                            SQLULEN         *piRowCount,
                            SQLUSMALLINT    *phRowStatus);

void TraceSQLGetData(int  iCallOrRet,
                      SQLRETURN    iRc,
                      SQLHSTMT    phstmt,
                      SQLUSMALLINT hCol, 
                      SQLSMALLINT hType,
                      SQLPOINTER pValue, 
                      SQLLEN cbLen,
                      SQLLEN *pcbLenInd);

void TraceSQLRowCount(int  iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT  phstmt,
                        SQLLEN* pRowCount);

void TraceSQLSetPos(int  iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT  phstmt,
                    SQLSETPOSIROW  iRow,
                    SQLUSMALLINT   hOperation,
                    SQLUSMALLINT   hLockType);

void TraceSQLFetchScroll(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT           hstmt,
                            SQLSMALLINT     FetchOrientation,
                            SQLLEN          FetchOffset);

void TraceSQLCopyDesc(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDESC phdescSrc,
                        SQLHDESC phdescDest);

void TraceSQLGetDescField(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pValue, 
                            SQLINTEGER cbLen,
                            SQLINTEGER *pcbLen);

void TraceSQLGetDescRec(int iCallOrRet,
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
                            SQLSMALLINT *phNullable);

void TraceSQLSetDescField(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pValue, 
                            SQLINTEGER cbLen);

void TraceSQLSetDescRec(int iCallOrRet,
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
                            SQLLEN *    plIndicator);

void TraceSQLSetConnectOption(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLULEN pValue);

void TraceSQLGetConnectOption(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLPOINTER pValue);

void TraceSQLSetStmtOption(int   iCallOrRet,
                            SQLRETURN   iRc,
                            SQLHSTMT phstmt,
                            SQLUSMALLINT hOption, 
                            SQLULEN pValue);

void TraceSQLGetStmtOption(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHSTMT phstmt,
                                SQLUSMALLINT hOption, 
                                SQLPOINTER pValue);

void TraceSQLSetScrollOptions(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHSTMT phstmt,
                                SQLUSMALLINT  hConcurrency,
                                SQLLEN        iKeysetSize,
                                SQLUSMALLINT  hRowsetSize);

void TraceSQLGetConnectAttr(int   iCallOrRet,
                            SQLRETURN   iRc,
                            SQLHDBC    phdbc,
                            SQLINTEGER    iAttribute, 
                            SQLPOINTER    pValue,
                            SQLINTEGER    cbLen, 
                            SQLINTEGER  *pcbLen);

void TraceSQLGetStmtAttr(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pValue,
                           SQLINTEGER    cbLen, 
                           SQLINTEGER  *pcbLen);

void TraceSQLSetConnectAttr(int   iCallOrRet,
                               SQLRETURN   iRc,
                               SQLHDBC    phdbc,
                               SQLINTEGER    iAttribute, 
                               SQLPOINTER    pValue,
                               SQLINTEGER    cbLen);

void TraceSQLSetStmtAttr(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pValue,
                           SQLINTEGER    cbLen);

void TraceSQLSetEnvAttr(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV phenv,
                        SQLINTEGER    iAttribute, 
                        SQLPOINTER    pValue,
                        SQLINTEGER    cbLen);

void TraceSQLSetEnvAttr(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHENV phenv,
                        SQLINTEGER    iAttribute, 
                        SQLPOINTER    pValue,
                        SQLINTEGER    cbLen);

void TraceSQLGetEnvAttr(int   iCallOrRet,
                        SQLRETURN   iRc,
                        SQLHENV phenv,
                        SQLINTEGER    iAttribute, 
                        SQLPOINTER    pValue,
                        SQLINTEGER    cbLen, 
                        SQLINTEGER    *pcbLen);

void TraceSQLTables(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLCHAR *pCatalogName, 
                    SQLSMALLINT cbCatalogName,
                    SQLCHAR *pSchemaName, 
                    SQLSMALLINT cbSchemaName,
                    SQLCHAR *pTableName, 
                    SQLSMALLINT cbTableName,
                    SQLCHAR *pTableType, 
                    SQLSMALLINT cbTableType);

void TraceSQLColumns(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLCHAR *pCatalogName, 
                    SQLSMALLINT cbCatalogName,
                    SQLCHAR *pSchemaName, 
                    SQLSMALLINT cbSchemaName,
                    SQLCHAR *pTableName, 
                    SQLSMALLINT cbTableName,
                    SQLCHAR *pColumnName, 
                    SQLSMALLINT cbColumnName);

void TraceSQLStatistics(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT       phstmt,
                        SQLCHAR *pCatalogName, 
                        SQLSMALLINT cbCatalogName,
                        SQLCHAR *pSchemaName, 
                        SQLSMALLINT cbSchemaName,
                        SQLCHAR *pTableName, 
                        SQLSMALLINT cbTableName,
                        SQLUSMALLINT hUnique, 
                        SQLUSMALLINT hReserved);

void TraceSQLSpecialColumns(int iCallOrRet,
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
                               SQLUSMALLINT hNullable);

void TraceSQLProcedureColumns(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLCHAR          *pCatalogName,
                                SQLSMALLINT      cbCatalogName,
                                SQLCHAR          *pSchemaName,
                                SQLSMALLINT      cbSchemaName,
                                SQLCHAR          *pProcName,
                                SQLSMALLINT      cbProcName,
                                SQLCHAR          *pColumnName,
                                SQLSMALLINT      cbColumnName);

void TraceSQLProcedures(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT       phstmt,
                        SQLCHAR        *pCatalogName,
                        SQLSMALLINT    cbCatalogName,
                        SQLCHAR        *pSchemaName,
                        SQLSMALLINT    cbSchemaName,
                        SQLCHAR        *pProcName,
                        SQLSMALLINT    cbProcName);

void TraceSQLForeignKeys(int iCallOrRet,
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
                            SQLSMALLINT        cbFkTableName);

void TraceSQLPrimaryKeys(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLCHAR      *pCatalogName,
                            SQLSMALLINT  cbCatalogName,
                            SQLCHAR      *pSchemaName,
                            SQLSMALLINT   cbSchemaName,
                            SQLCHAR      *pTableName,
                            SQLSMALLINT   cbTableName);

void TraceSQLGetTypeInfo(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLSMALLINT hType);

void TraceSQLColumnPrivileges(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLCHAR          *pCatalogName,
                                SQLSMALLINT      cbCatalogName,
                                SQLCHAR          *pSchemaName,
                                SQLSMALLINT      cbSchemaName,
                                SQLCHAR          *pTableName,
                                SQLSMALLINT      cbTableName,
                                SQLCHAR          *pColumnName,
                                SQLSMALLINT      cbColumnName);

void TraceSQLTablePrivileges(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLCHAR         *pCatalogName,
                                SQLSMALLINT     cbCatalogName,
                                SQLCHAR         *pSchemaName,
                                SQLSMALLINT     cbSchemaName,
                                SQLCHAR         *pTableName,
                                SQLSMALLINT      cbTableName);

void TraceSQLConnectW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC phdbc,
                        SQLWCHAR*            wszDSN,
                        SQLSMALLINT         cchDSN,
                        SQLWCHAR*            wszUID,
                        SQLSMALLINT         cchUID,
                        SQLWCHAR*            wszAuthStr,
                        SQLSMALLINT         cchAuthStr);

void TraceSQLDriverConnectW(int              iCallOrRet,
                            SQLRETURN      iRc,
                            SQLHDBC             phdbc,
                            SQLHWND             hwnd,
                            SQLWCHAR*            wszConnStrIn,
                            SQLSMALLINT         cchConnStrIn,
                            SQLWCHAR*            wszConnStrOut,
                            SQLSMALLINT         cchConnStrOut,
                            SQLSMALLINT*        pcchConnStrOut,
                            SQLUSMALLINT        hDriverCompletion);

void TraceSQLBrowseConnectW(int             iCallOrRet,
                            SQLRETURN     iRc,
                            SQLHDBC         phdbc,
                            SQLWCHAR*     wszConnStrIn,
                            SQLSMALLINT  cchConnStrIn,
                            SQLWCHAR*     wszConnStrOut,
                            SQLSMALLINT  cchConnStrOut,
                            SQLSMALLINT* pcchConnStrOut);

void TraceSQLGetInfoW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC    phdbc,
                        SQLUSMALLINT    hInfoType,
                        SQLPOINTER        pwInfoValue,
                        SQLSMALLINT     cbLen,
                        SQLSMALLINT*    pcbLen);

void TraceSQLErrorW(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHENV phenv,
                    SQLHDBC phdbc, 
                    SQLHSTMT phstmt,
                    SQLWCHAR*    pwSqlState,
                    SQLINTEGER* piNativeError,
                    SQLWCHAR*    pwMessageText,
                    SQLSMALLINT  cchLen,
                    SQLSMALLINT* pcchLen);

void TraceSQLGetDiagRecW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLSMALLINT hHandleType, 
                        SQLHANDLE      pHandle,
                        SQLSMALLINT    hRecNumber,
                        SQLWCHAR       *pwSqlState,
                        SQLINTEGER     *piNativeError,
                        SQLWCHAR       *pwMessageText,
                        SQLSMALLINT    cchLen,
                        SQLSMALLINT    *pcchLen);

void TraceSQLGetDiagFieldW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLSMALLINT hHandleType, 
                            SQLHANDLE pHandle,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hDiagIdentifier,
                            SQLPOINTER  pwDiagInfo, 
                            SQLSMALLINT cbLen,
                            SQLSMALLINT *pcbLen);

void TraceSQLExecDirectW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLWCHAR* pwCmd,
                        SQLINTEGER  cchLen);

void  TraceSQLNativeSqlW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHDBC   phdbc,
                        SQLWCHAR*    wszSqlStrIn,
                        SQLINTEGER   cchSqlStrIn,
                        SQLWCHAR*    wszSqlStrOut,
                        SQLINTEGER   cchSqlStrOut,
                        SQLINTEGER*  pcchSqlStrOut);

void TraceSQLGetConnectAttrW(int   iCallOrRet,
                            SQLRETURN   iRc,
                            SQLHDBC    phdbc,
                            SQLINTEGER    iAttribute, 
                            SQLPOINTER    pwValue,
                            SQLINTEGER    cbLen, 
                            SQLINTEGER  *pcbLen);

void TraceSQLGetConnectOptionW(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLPOINTER pwValue);

void TraceSQLGetStmtAttrW(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pwValue,
                           SQLINTEGER    cbLen, 
                           SQLINTEGER  *pcbLen);

void TraceSQLSetConnectAttrW(int   iCallOrRet,
                               SQLRETURN   iRc,
                               SQLHDBC    phdbc,
                               SQLINTEGER    iAttribute, 
                               SQLPOINTER    pwValue,
                               SQLINTEGER    cbLen);

void TraceSQLSetConnectOptionW(int   iCallOrRet,
                                SQLRETURN   iRc,
                                SQLHDBC    phdbc,
                                SQLUSMALLINT hOption, 
                                SQLULEN pwValue);

void TraceSQLSetStmtAttrW(int   iCallOrRet,
                           SQLRETURN   iRc,
                           SQLHSTMT phstmt,
                           SQLINTEGER    iAttribute, 
                           SQLPOINTER    pwValue,
                           SQLINTEGER    cbLen);

void TraceSQLGetCursorNameW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLWCHAR*        pwCursorName,
                            SQLSMALLINT     cchLen,
                            SQLSMALLINT*    pcchLen);

void TraceSQLPrepareW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT phstmt,
                        SQLWCHAR* pwCmd,
                        SQLINTEGER cchLen);

void TraceSQLSetCursorNameW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLWCHAR*    pwCursorName,
                            SQLSMALLINT cchLen);

void TraceSQLColAttributesW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER   pwValue, 
                            SQLSMALLINT  cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN        *plValue);

void TraceSQLColAttributeW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT hCol, 
                            SQLUSMALLINT hFieldIdentifier,
                            SQLPOINTER    pwValue, 
                            SQLSMALLINT    cbLen,
                            SQLSMALLINT *pcbLen, 
                            SQLLEN       *plValue);

void TraceSQLDescribeColW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT    phstmt,
                            SQLUSMALLINT    hCol,
                            SQLWCHAR*        pwColName,
                            SQLSMALLINT     cchLen,
                            SQLSMALLINT*    pcchLen,
                            SQLSMALLINT*    pDataType,
                            SQLULEN*        pColSize,
                            SQLSMALLINT*    pDecimalDigits,
                            SQLSMALLINT*    pNullable);

void TraceSQLGetDescFieldW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pwValue, 
                            SQLINTEGER cbLen,
                            SQLINTEGER *pcbLen);

void TraceSQLGetDescRecW(int iCallOrRet,
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
                            SQLSMALLINT *phNullable);

void TraceSQLSetDescFieldW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHDESC phdesc,
                            SQLSMALLINT hRecNumber, 
                            SQLSMALLINT hFieldIdentifier,
                            SQLPOINTER pwValue, 
                            SQLINTEGER cbLen);

void TraceSQLColumnPrivilegesW(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLWCHAR*     pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwTableName,
                                SQLSMALLINT  cchTableName,
                                SQLWCHAR*    pwColumnName,
                                SQLSMALLINT  cchColumnName);

void TraceSQLColumnsW(int iCallOrRet,
                        SQLRETURN  iRc,
                        SQLHSTMT       phstmt,
                        SQLWCHAR*    pwCatalogName,
                        SQLSMALLINT  cchCatalogName,
                        SQLWCHAR*    pwSchemaName,
                        SQLSMALLINT  cchSchemaName,
                        SQLWCHAR*    pwTableName,
                        SQLSMALLINT  cchTableName,
                        SQLWCHAR*    pwColumnName,
                        SQLSMALLINT  cchColumnName);

void TraceSQLForeignKeysW(int iCallOrRet,
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
                            SQLSMALLINT  cchFkTableName);


void TraceSQLGetTypeInfoW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLSMALLINT hType);

void TraceSQLPrimaryKeysW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT   phstmt,
                            SQLWCHAR*    pwCatalogName,
                            SQLSMALLINT  cchCatalogName,
                            SQLWCHAR*    pwSchemaName,
                            SQLSMALLINT  cchSchemaName,
                            SQLWCHAR*    pwTableName,
                            SQLSMALLINT  cchTableName);

void TraceSQLProcedureColumnsW(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLWCHAR*    pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwProcName,
                                SQLSMALLINT  cchProcName,
                                SQLWCHAR*    pwColumnName,
                                SQLSMALLINT  cchColumnName);

void TraceSQLProceduresW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLWCHAR*    pwCatalogName,
                            SQLSMALLINT  cchCatalogName,
                            SQLWCHAR*    pwSchemaName,
                            SQLSMALLINT  cchSchemaName,
                            SQLWCHAR*    pwProcName,
                            SQLSMALLINT  cchProcName);

void TraceSQLSpecialColumnsW(int iCallOrRet,
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
                            SQLUSMALLINT   hNullable);

void TraceSQLStatisticsW(int iCallOrRet,
                            SQLRETURN  iRc,
                            SQLHSTMT       phstmt,
                            SQLWCHAR*    pwCatalogName,
                            SQLSMALLINT  cchCatalogName,
                            SQLWCHAR*    pwSchemaName,
                            SQLSMALLINT  cchSchemaName,
                            SQLWCHAR*    pwTableName,
                            SQLSMALLINT  cchTableName,
                            SQLUSMALLINT hUnique,
                            SQLUSMALLINT hReserved);

void TraceSQLTablePrivilegesW(int iCallOrRet,
                                SQLRETURN  iRc,
                                SQLHSTMT       phstmt,
                                SQLWCHAR*    pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwTableName,
                                SQLSMALLINT  cchTableName);

void TraceSQLTablesW(int iCallOrRet,
                    SQLRETURN  iRc,
                    SQLHSTMT       phstmt,
                    SQLWCHAR*      pwCatalogName,
                    SQLSMALLINT    cchCatalogName,
                    SQLWCHAR*      pwSchemaName,
                    SQLSMALLINT    cchSchemaName,
                    SQLWCHAR*      pwTableName,
                    SQLSMALLINT    cchTableName,
                    SQLWCHAR*      pwTableType,
                    SQLSMALLINT    cchTableType);
