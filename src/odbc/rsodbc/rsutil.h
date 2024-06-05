/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <stdlib.h>
#include <map>
#include <functional>
#include <algorithm>
#include <vector>
#include <iostream>
#include <stdexcept>

//#include <strsafe.h>

#include "rsodbc.h"
#include "rsmem.h"

#define SHORT_STR_DATA    4096
#define SHORT_CMD_LEN    1024

#define MAX_ODBC_ESCAPE_CLAUSE_REPLACE_LEN 20
#define MAX_AUDIT_CMDS_LEN 2048

#define TRACE_KEY_NAME            "SOFTWARE\\Amazon\\Amazon Redshift ODBC Driver (x64)\\Driver"
#define DM_TRACE_VAL_NAME        "Trace"

#define RSODBC_INI_FILE          "amazon.redshiftodbc.ini"
#define DRIVER_SECTION_NAME      "DRIVER"
#define MAX_OPTION_VAL_LEN		 512

#define PARAM_MARKER    '?'
#define ODBC_ESCAPE_CLAUSE_START_MARKER    '{'
#define ODBC_ESCAPE_CLAUSE_END_MARKER    '}'

#define    SINGLE_QUOTE    '\''
#define    DOUBLE_QUOTE    '\"' 
#define    DOLLAR_SIGN        '$'
#define SLASH            '/'
#define STAR            '*'
#define DASH            '-'
#define NEW_LINE        '\n'
#define COMMA_SIGN        ','
#define SEMI_COLON      ';'

#define TEXT_FORMAT		0
#define BINARY_FORMAT	1


#define INT_LEN(cbStrLen) ((int)cbStrLen)
#define IS_TEXT_FORMAT(format)  ((format == TEXT_FORMAT))

// Macro for ODBC2 behavior
#define    CONVERT_TO_ODBC2_SQL_DATE_TYPES(pStmt, phSqlType) \
if(pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion == SQL_OV_ODBC2) \
{ \
    if(phSqlType) \
    { \
        if(*phSqlType == SQL_TYPE_DATE) \
            *phSqlType = SQL_DATE; \
        else \
        if(*phSqlType == SQL_TYPE_TIMESTAMP) \
            *phSqlType = SQL_TIMESTAMP; \
        else \
        if(*phSqlType == SQL_TYPE_TIME) \
            *phSqlType = SQL_TIME; \
    } \
} 


// Control allocation of string data by small array, app buffer or new allocation buffer.
typedef struct _RS_STR_BUF
{
    int iAllocDataLen; // Allocated data len. 0 means not allocated and data is in array. > 0 mean allocated. < 0 means data buffer point to application buffer.
    char buf[SHORT_STR_DATA + 1]; // Short data store in array without allocation.
    char *pBuf;           // Allocated buffer of iAllocDataLen + 1 data.
}RS_STR_BUF;

// TIME information. SQL header file don't contain fraction.
typedef struct _RS_TIME_STRUCT 
{
    TIME_STRUCT sqltVal;       // TIME;
    SQLUINTEGER fraction;   // Microseconds precision.
}RS_TIME_STRUCT;

// TIMETZ information.
typedef struct _RS_TIMETZ_STRUCT
{
	long long time;
	int zone;   
}RS_TIMETZ_STRUCT;

typedef struct _INTERVALY2M_STRUCT
{
		SQLUINTEGER		year;
		SQLUINTEGER		month;
}INTERVALY2M_STRUCT;

typedef struct _INTERVALD2S_STRUCT
{
		SQLUINTEGER		day;
		SQLUINTEGER		hour;
		SQLUINTEGER		minute;
		SQLUINTEGER		second;
		SQLUINTEGER		fraction;
}INTERVALD2S_STRUCT;


// Different types of column values supported by PADB
typedef union _RS_VALUE
{
    char *pcVal; // CHAR/VARCHAR
    short hVal;  // SMALLINT/INT2
    int  iVal;  // INTEGER/INT4/INTERVALY2M (as binary)
    long long llVal; // BIGINT/INT8/INTERVALD2S (as binary)
    float fVal;  // REAL/FLOAT4
    double dVal; // DOUBLE PRECISION/FLOAT8
    char   bVal; // BOOLEAN
    DATE_STRUCT dtVal; // DATE
    TIMESTAMP_STRUCT tsVal; // TIMESTAMP;
    SQL_NUMERIC_STRUCT nVal; // DECIMAL/NUMERIC
    RS_TIME_STRUCT tVal; // TIME;
	RS_TIMETZ_STRUCT tzVal; // TimeTZ val as BINARY
    INTERVALY2M_STRUCT y2mVal; // INTERVALY2M (as text)
    INTERVALD2S_STRUCT d2sVal; // INTERVALD2S (as text)
}RS_VALUE;

// Convert C type to char * when pass it to libpq
typedef struct _RS_BIND_PARAM_STR_BUF
{
    int iAllocDataLen; // Allocated data len. 0 means not allocated and data is in array. > 0 mean allocated. < 0 means data buffer point to application buffer.
    char buf[MAX_NUMBER_BUF_LEN + 1]; // Short numeric, boolean, date, datetime data store in array without allocation.
    char *pBuf;           // User bind buf, buf[] or allocated buffer of iAllocDataLen + 1 data.
}RS_BIND_PARAM_STR_BUF;

// ODBC2 behavior
typedef struct _RS_MAP_SQL_STATE
{
    const std::string pszOdbc3State;
    const std::string pszOdbc2State;
}RS_MAP_SQL_STATE;

// Map ODBC function name to PADB function name
typedef struct _RS_MAP_FUNC_NAME
{
    std::string pszOdbcFuncName;
    std::string pszPadbFuncName;
}RS_MAP_FUNC_NAME;

// Map ODBC interval name to PADB DatePart name
typedef struct _RS_MAP_INTERVAL_NAME
{
    const char *pszOdbcIntervalName;
    const char *pszPadbDatePartName;
}RS_MAP_INTERVAL_NAME;

// Map ODBC SQL Type name to PADB SQL Type name
typedef struct _RS_MAP_SQL_TYPE_NAME
{
    const char *pszOdbcSQLTypeName;
    const char *pszPadbSQLTypeName;
}RS_MAP_SQL_TYPE_NAME;

// Copied from pgtime.h of the server code
struct pg_tm
{
	int			tm_sec;
	int			tm_min;
	int			tm_hour;
	int			tm_mday;
	int			tm_mon;			/* origin 0, not 1 */
	int			tm_year;		/* relative to 1900 */
	int			tm_wday;
	int			tm_yday;
	int			tm_isdst;
	long int	tm_gmtoff;
	const char *tm_zone;
};

/* Julian-date equivalents of Day 0 in Unix and Postgres reckoning */
#define UNIX_EPOCH_JDATE		2440588 /* == date2j(1970, 1, 1) */
#define POSTGRES_EPOCH_JDATE	2451545 /* == date2j(2000, 1, 1) */

#define INT64CONST(x)  ((long long) x##LL)

// DT_NOBEGIN represents timestamp -infinity; DT_NOEND represents +infinity
#define DT_NOBEGIN		(-INT64CONST(0x7fffffffffffffff) - 1)
#define DT_NOEND		(INT64CONST(0x7fffffffffffffff))

#define TIMESTAMP_NOBEGIN(j)	do {j = DT_NOBEGIN;} while (0)
#define TIMESTAMP_IS_NOBEGIN(j) ((j) == DT_NOBEGIN)

#define TIMESTAMP_NOEND(j)		do {j = DT_NOEND;} while (0)
#define TIMESTAMP_IS_NOEND(j)	((j) == DT_NOEND)

#define TIMESTAMP_NOT_FINITE(j) (TIMESTAMP_IS_NOBEGIN(j) || TIMESTAMP_IS_NOEND(j))

#define EARLY			"-infinity"
#define LATE			"infinity"

#define TMODULO(t,q,u) \
do { \
	q = (t / u); \
	if (q != 0) t -= (q * u); \
} while(0)

#define MAX_TIME_VALUE INT64CONST(86400000000)

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

char *rs_strdup(const char *src, size_t cbLen);

unsigned char *makeNullTerminatedStr(char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf);

void addConnection(RS_ENV_INFO *pEnv, RS_CONN_INFO *pConn);
void removeConnection(RS_CONN_INFO *pConn);
void addStatement(RS_CONN_INFO *pConn, RS_STMT_INFO *pStmt);
void removeStatement(RS_STMT_INFO *pStmt);
void addResult(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult);
void addDescriptor(RS_CONN_INFO *pConn, RS_DESC_INFO *pDesc);
void removeDescriptor(RS_DESC_INFO *pDesc);
void addPrepare(RS_STMT_INFO *pStmt, RS_PREPARE_INFO *pPrepare);

void clearBindColList(RS_DESC_INFO *pARD);

void clearBindParamList(RS_STMT_INFO *pStmt);
int countBindParams(RS_DESC_REC *pDescRecHead);

char *getParamVal(char *pParamData, int iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, short hSQLType);
short getDefaultCTypeFromSQLType(short hSQLType, int *piConversionError);
char *convertCParamDataToSQLData(RS_STMT_INFO *pStmt, char *pParamData, int iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, 
                                  short hSQLType, short hPrepSQLType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, int *piConversionError);


RS_ERROR_INFO * getNextError(RS_ERROR_INFO **ppErrorList, SQLSMALLINT recNumber, int remove);
RS_ERROR_INFO * clearErrorList(RS_ERROR_INFO *pErrorList);
void addError(RS_ERROR_INFO **ppErrorList, char *pSqlState, char *pMsg, long nativeError, RS_CONN_INFO *pConn);
int getTotalErrors(RS_ERROR_INFO *pErrorList);

void addWarning(RS_ERROR_INFO **ppErrorList, char *pSqlState, char *pMsg, long nativeError, RS_CONN_INFO *pConn);

void initGlobals(HMODULE hModule);
void releaseGlobals();
void initODBC(HMODULE hModule);
void uninitODBC();

// Initialize tracing with option to override previous tracing system
void initTrace(int canOverride);

// Close and Cleanup tracing system.
void uninitTrace();

// Set trace level and trace file info.
void setTraceLevelAndFile(int iTracelLevel, char *pTraceFile);

// Set trace level and trace file info from connection string properties.
// Return 0 if respective properties exist and processed, 1 otherwise.
int readAndSetLogInfoFromConnectionString(RS_CONNECT_PROPS_INFO *pConnectProps);

// Initialize tracing system from connection string properties.
void initTraceFromConnectionString(RS_CONNECT_PROPS_INFO *pConnectProps);

std::string& rtrim(std::string& s);
// trim from beginning of string (left)
std::string& ltrim(std::string& s);
// trim from both ends of string (right then left)
std::string& trim(std::string& s);
char *trim_whitespaces(char *str);

char *appendStr(char *pStrOut, size_t *pcbStrOut,char *szStrIn);
char *stristr(char *str, char *subStr);

SQLRETURN copyStrDataSmallLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen);
SQLRETURN copyStrDataLargeLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLINTEGER cbLen, SQLINTEGER *pcbLen);
SQLRETURN copyStrDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);

SQLRETURN copyWStrDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, WCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);

SQLRETURN copyBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyWBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, WCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyHexToBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen, SQLLEN *cbLenOffset);
SQLRETURN copyBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyWBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, WCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);


void resetPaStrBuf(RS_STR_BUF *pPaStrBuf);
void releasePaStrBuf(RS_STR_BUF *pPaStrBuf);
unsigned char *checkLenAndAllocatePaStrBuf(size_t cbLen, RS_STR_BUF *pPaStrBuf);

void releaseResults(RS_STMT_INFO *pStmt);
void releasePrepares(RS_STMT_INFO *pStmt);
void makeItReadyForNewQueryExecution(RS_STMT_INFO *pStmt, int executePrepared, int iReprepareForMultiInsert,int iResetMultiInsert);

int isAsyncEnable(RS_STMT_INFO *pStmt);
SQLRETURN onConnectExecute(RS_CONN_INFO *pConn, char *pCmd);
SQLRETURN onConnectAuditInfoExecute(RS_CONN_INFO *pConn);

SQLRETURN convertSQLDataToCData(RS_STMT_INFO *pStmt, char *pColData,
                                int iColDataLen, short hSQLType, void *pBuf,
                                SQLLEN cbLen, SQLLEN *cbLenOffset,
                                SQLLEN *pcbLenInd, short hCType,
                                short hRsSpecialType, int format,
                                RS_DESC_REC *pDescRec);
int getRsVal(char *pColData, int iColDataLen, short hSQLType, RS_VALUE  *pPaVal, short hCType, int format, RS_DESC_REC *pDescRec, short hRsSpecialType, bool isTextData);
void makeNullTerminateIntVal(char *pColData, int iColDataLen, char *szNumBuf, int iBufLen);

SQLRETURN getShortData(short hVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getIntData(int iVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getLongData(long lVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getBigIntData(long long llVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getFloatData(float fVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getDoubleData(double dVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getBooleanData(char bVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getDateData(DATE_STRUCT *pdtVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getTimeStampData(TIMESTAMP_STRUCT *ptsVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getIntervalY2MData(INTERVALY2M_STRUCT *py2mVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getIntervalD2SData(INTERVALD2S_STRUCT *pd2sVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getNumericData(SQL_NUMERIC_STRUCT *pnVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getTimeData(RS_TIME_STRUCT *ptVal, void *pBuf,  SQLLEN *pcbLenInd);

long getSize(short hType, int iSize);
short getScale(short hType, short hDecimalDigits);
int getCaseSensitive(short hType, short hRsSpecialType, int case_sensitive_bit);
int getDisplaySize(short hType, int iSize, short hRsSpecialType);
void getLiteralPrefix(short hType, char *pBuf, short hRsSpecialType);
void getLiteralSuffix(short hType, char *pBuf, short hRsSpecialType);
void getTypeName(short hType, char *pBuf, int bufLen, short hRsSpecialType);
int getNumPrecRadix(short hType);
int getOctetLen(short hSQLType, int iSize, short hRsSpecialType);
int getOctetLenUsingCType(short hCType, int iSize);
int getPrecision(short hType, int iSize, short hRsSpecialType);
int getSearchable(short hType, short hRsSpecialType);
int getUnNamed(char *pName);
int getUnsigned(short hType);
int getUpdatable();

int isStrFieldIdentifier(SQLUSMALLINT hFieldIdentifier);
SQLUSMALLINT mapColAttributesToColAttributeIdentifier(SQLUSMALLINT hOption);

void readRegistryKey(HKEY hKey, char *pSubKeyName, char *pName, char *pBuf, int iBufLen);
void readAndSetTraceInfo();

int getParamSize(short hType);
short getParamScale(short hType);

void releaseDescriptorRecs(RS_DESC_INFO *pDesc);
void addDescriptorRec(RS_DESC_INFO *pDesc, RS_DESC_REC *pDescRec,  int iAtFront);
void releaseDescriptorRec(RS_DESC_INFO *pDesc, RS_DESC_REC *pDescRec);
void releaseDescriptorRecByNum(RS_DESC_INFO *pDesc, short hRecNumber);
RS_DESC_REC *findDescRec(RS_DESC_INFO *pDesc, short hRecNumber);
RS_DESC_INFO *allocateDesc(RS_CONN_INFO *pConn, int iType, int iImplicit);
RS_DESC_INFO *releaseDescriptor(RS_DESC_INFO *pDesc, int implicit);
RS_DESC_REC *checkAndAddDescRec(RS_DESC_INFO *pDesc, short hRecNumber,  int iAtFront, int *pNewDecRec);
int isHeaderField(SQLSMALLINT hFieldIdentifier);
int isWritableField(RS_DESC_INFO *pDesc, SQLSMALLINT hFieldIdentifier);
int isReadableField(RS_DESC_INFO *pDesc, SQLSMALLINT hFieldIdentifier);

void copyIRDRecsFromResult(RS_RESULT_INFO *pResultHead, RS_DESC_INFO *pIRD);
void copyIPDRecsFromPrepare(RS_PREPARE_INFO *pPrepareHead, RS_DESC_INFO *pIPD);

int needDataAtExec(RS_STMT_INFO *pStmt, RS_DESC_REC *pDescRecHead, long lParamProcessed,int executePrepared);
RS_DATA_AT_EXEC *allocateAndSetDataAtExec(char *pDataPtr, long lStrLenOrInd);
RS_DATA_AT_EXEC *appendDataAtExec(RS_DATA_AT_EXEC *pDataAtExec, char *pDataPtr, long lStrLenOrInd);
RS_DATA_AT_EXEC *freeDataAtExec(RS_DATA_AT_EXEC *pDataAtExec);
void resetAndReleaseDataAtExec(RS_STMT_INFO *pStmt);

int isCharDiagIdentifier(SQLSMALLINT     hDiagIdentifier);

int countParamMarkers(char *pData, size_t cbLen);
int needToScanODBCEscapeClause(RS_STMT_INFO *pStmt);
int countODBCEscapeClauses(RS_STMT_INFO *pStmt,char *pData, size_t cbLen);
unsigned char *checkReplaceParamMarkerAndODBCEscapeClause(RS_STMT_INFO *pStmt,char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf, int iReplaceParamMarker);
unsigned char *replaceParamMarkerAndODBCEscapeClause(RS_STMT_INFO *pStmt, char *pData, size_t cbLen, RS_STR_BUF *pPaStrBuf, int numOfParamMarkers, int numOfODBCEscapeClauses);
int replaceODBCEscapeClause(RS_STMT_INFO *pStmt, char **ppDest, char *pDestStart, int iDestBufLen,char **ppSrc, size_t cbLen, int i, int numOfParamMarkers, int  *piParamNumber);

int isScrollableCursor(RS_STMT_INFO *pStmt);
int isUpdatableCursor(RS_STMT_INFO *pStmt);

void setCatalogQueryBuf(RS_STMT_INFO *pStmt, char *szCatlogQuery);

void setThreadExecutionStatus(RS_EXEC_THREAD_INFO *pExecThread, SQLRETURN rc);
SQLRETURN checkExecutingThread(RS_STMT_INFO *pStmt);
void waitAndFreeExecThread(RS_STMT_INFO *pStmt, int iWaitFlag);

void setParamMarkerCount(RS_STMT_INFO *pStmt, int iNumOfParamMarkers);
int getParamMarkerCount(RS_STMT_INFO *pStmt);

void Alert();

short findHighestRecCount(RS_DESC_INFO *pDesc);
void getShortVal(short hVal, short *phVal, SQLINTEGER *pcbLen);
void getIntVal(int iVal, int *piVal, SQLINTEGER *pcbLen);
void getLongVal(long lVal, long *plVal, SQLINTEGER *pcbLen);
void getPointerVal(void *ptrVal, void **ppVal, SQLINTEGER *pcbLen);
void getSQLINTEGERVal(long lVal, SQLINTEGER *piVal, SQLINTEGER *pcbLen);

short getConciseType(short hConciseType, short hType);
short getDateTimeIntervalCode(short hDateTimeIntervalCode, short hType);
short getCTypeFromConciseType(short hConciseType, short hDateTimeIntervalCode, short hType);

SQLRETURN checkAndAutoFetchRefCursor(RS_STMT_INFO *pStmt);
void releaseResult(RS_RESULT_INFO *pResult, int iAtHeadResult, RS_STMT_INFO *pStmt);

int isNullOrEmpty(SQLCHAR *pData);
RS_DESC_INFO *releaseExplicitDescs(RS_DESC_INFO *phdescHead);

void beginApiMutex(SQLHENV phenv, SQLHDBC pConn);
void endApiMutex(SQLHENV phenv, SQLHDBC pConn);

SQLRETURN checkHstmtHandleAndAddError(SQLHSTMT phstmt, SQLRETURN rc, char *pSqlState, char *pSqlErrMsg);
SQLRETURN checkHdbcHandleAndAddError(SQLHDBC phdbc, SQLRETURN rc, char *pSqlState, char *pSqlErrMsg);


int isODBC2Behavior(RS_STMT_INFO *pStmt);
void mapToODBC2SqlState(RS_ENV_INFO *pEnv,char *pszSqlState);

char *getNextTokenForODBCEscapeClause(char **ppSrc, size_t cbLen, int *pi, char *fnNameDelimiterList);
const std::string *mapODBCFuncNameToPadbFuncName(char *pODBCFuncName, int iTokenLen);
const char *mapODBCIntervalNameToPadbDatePartName(char *pODBCIntervalName, int iTokenLen);
int checkDelimiterForODBCEscapeClauseToken(const char *pSrc, char *fnNameDelimiterList);
int skipFunctionBracketsForODBCEscapeClauseToken(char **ppSrc, size_t cbLen, int i, int iBoth);
int replaceODBCConvertFunc(RS_STMT_INFO *pStmt, char **ppDest, char *pDestStart, int iDestBufLen, char **ppSrc, size_t cbLen, int i, int numOfParamMarkers, int  *piParamNumber);
const char *mapODBCSQLTypeToPadbSQLTypeName(char *pODBCSQLTypeName, int iTokenLen);

void resetCatalogQueryFlag(RS_STMT_INFO *pStmt);

void getApplicationName(SQLHDBC phdbc);
void getOsUserName(SQLHDBC phdbc);
void getClientHostName(SQLHDBC phdbc);
void getClientDomainName(SQLHDBC phdbc);
void getAuditTrailInfo(SQLHDBC phdbc);
char *getDriverPath();

SQLRETURN getGucVariableVal(RS_CONN_INFO *pConn, char *pVarName, char *pVarVal, int iBufLen);
void convertNumericStringToScaledInteger(char *pNumData, SQL_NUMERIC_STRUCT *pnVal);
void convertScaledIntegerToNumericString(SQL_NUMERIC_STRUCT *pnVal,char *pNumData, int num_data_len);

int fileExists(const char * pFileName);
int readTraceOptionsFromIniFile(char  *pszTraceLevel,int iTraceLevelBufLen, char *pszTraceFile, int iTraceFileBufLen);
int readDriverOptionFromIniFile(const char  *pszOptionName,char *pszOptionValBuf, int iOptionValBufLen);
void readCscOptionsForDsnlessConnection(RS_CONNECT_PROPS_INFO *pConnectProps);

#if defined LINUX 
char *strlwr(char *str);
char *_strupr(char *str);
int GetTempPath(int size, char *pBuf);
void sharedObjectAttach();
void sharedObjectDetach();

#endif

void parseForCopyCommand(RS_STMT_INFO *pStmt, char *pCmd, size_t cbLen);
char *getNextTokenForCopyOrUnloadCommand(char **ppSrc, size_t cbLen, int *pi, int tokenInQuote);
SQLRETURN copyFromLocalFile(RS_STMT_INFO *pStmt, FILE *fp, int iLockRequired);
SQLRETURN checkAndHandleCopyStdinOrClient(RS_STMT_INFO *pStmt, SQLRETURN rc, int iLockRequired);

char *parseForMultiInsertCommand(RS_STMT_INFO *pStmt, char *pCmd, size_t cbLen, int iCallFromPrepare, char **ppLastBatchMultiInsertCmd);
char *getNextTokenForInsertCommand(char **ppSrc, size_t cbLen, int *pi, char delimiter);
int getNumberOfParams(RS_STMT_INFO *pStmt);
int getTotalMultiTuples(int numOfParamMarkers, long lArraySize, int *piLastBatchTotalMultiTuples);

char *findSQLClause(char *pTempCmd, char *pClause);
int DoesEmbedInDoubleQuotes(char *pStart,char *pEnd);
SQLRETURN createLastBatchMultiInsertCmd(RS_STMT_INFO *pStmt, char *pszLastBatchMultiInsertCmd);
SQLRETURN rePrepareMultiInsertCommand(RS_STMT_INFO *pStmt, char *pszCmd);

SQLRETURN checkForCopyExecution(RS_STMT_INFO *pStmt);

void parseForUnloadCommand(RS_STMT_INFO *pStmt, char *pCmd, size_t cbLen);
SQLRETURN copyToLocalFile(RS_STMT_INFO *pStmt, FILE *fp);
SQLRETURN checkAndHandleCopyOutClient(RS_STMT_INFO *pStmt, SQLRETURN rc);

void checkAndSkipAllResultsOfStreamingCursor(RS_STMT_INFO *pStmt);
void skipAllResultsOfStreamingRowsUsingConnection(RS_CONN_INFO *pConn);
int doesAnyOtherStreamingCursorOpen(RS_CONN_INFO *pConn, RS_STMT_INFO *pStmt);

SQLRETURN getOneQueryVal(RS_CONN_INFO *pConn, char * pSqlCmd, char *pVarBuf, int iBufLen);
int updateOutBindParametersValue(RS_STMT_INFO *pStmt);

char *rs_strncpy(char *dest, const char *src, size_t n);
char *rs_strncat(char *dest, const char *src, size_t n);

#ifdef WIN32
unsigned char *decode64Password(const char *input, int length);
int date_out_wchar(int date, WCHAR *buf, int buf_len);
int timestamp_out_wchar(long long timestamp, WCHAR *buf, int buf_len, char *session_timezone);
int time_out_wchar(long long time, WCHAR *buf, int buf_len, int *tzp);
#endif

int date_out(int date, char *buf, int buf_len);
void j2date(int jd, int *year, int *month, int *day);
int timestamp_out(long long timestamp, char *buf, int buf_len, char *session_timezone);
int timestamp2tm(long long dt, int* tzp, struct pg_tm* tm, long long* fsec);
int intervaly2m_out(INTERVALY2M_STRUCT* y2m, char *buf, int buf_len);
int intervald2s_out(INTERVALD2S_STRUCT* d2s, char *buf, int buf_len);
int interval2tm(long long time, int months, struct pg_tm * tm, long long *fsec);
INTERVALY2M_STRUCT parse_intervaly2m(const char *buf, int buf_len);
INTERVALD2S_STRUCT parse_intervald2s(const char *buf, int buf_len);
void dt2time(long long jd, int *hour, int *min, int *sec, long long *fsec);
void TrimTrailingZeros(char *str, int *plen);
int time_out(long long time, char *buf, int buf_len, int *tzp);
int time2tm(long long time, struct pg_tm* tm, long long* fsec);

int getInt32FromBinary(char *pColData, int idx);
long long getInt64FromBinary(char *pColData, int idx);
#ifdef __cplusplus
}

#ifdef WIN32
int intervald2s_out_wchar(INTERVALD2S_STRUCT* d2s, WCHAR *buf, int buf_len);
int intervaly2m_out_wchar(INTERVALY2M_STRUCT* y2m, WCHAR *buf, int buf_len);
int intervaly2m_out_wchar(INTERVALD2S_STRUCT* d2s, WCHAR *buf, int buf_len);
#endif

std::vector<Oid> getParamTypes(int iNoOfBindParams, RS_DESC_REC *pDescRecHead, RS_CONNECT_PROPS_INFO *pConnectProps);

typedef std::map<std::string, std::string,
                 std::function<bool(const std::string &, const std::string &)>>
    StringMap;
StringMap createCaseInsensitiveMap();
StringMap parseConnectionString(const std::string &connStr);

/*
Case insensitive string comparison.
Note: Not unicode compatible
*/
inline bool isStrNoCaseEequal(const std::string& a, const std::string& b)
{
    auto fn = [](char a, char b) -> bool {
                return tolower(a) == tolower(b);
            };
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      fn);
}

// Check DatabaseMetadaCurrentOnly option
bool isDatabaseMetadaCurrentOnly(RS_STMT_INFO *pStmt);

/*
    Checks if the server parameters matches and expected value.

   - The function returns true if a valid value for the given param is
   available AND matches the 'trueValue'.
   - The function returns false if a valid value for the given param is
   available AND does not matche the 'trueValue'.
    - A parameter value is considered invalid if it is not null but is not
   included in the list of 'validValues'. In such cases, the function throws
   ExceptionInvalidParameter.
*/
bool getLibpqParameterStatus(
    RS_STMT_INFO *pStmt, const std::string &param,
    const std::string &trueValue = "on",
    const std::vector<std::string> &validValues = {"on", "off"},
    const bool defaultStatus = false);
class ExceptionInvalidParameter : public std::invalid_argument {
  public:
    // Constructor that takes a std::string message
    ExceptionInvalidParameter(const std::string &message);
};
#endif /* C++ */