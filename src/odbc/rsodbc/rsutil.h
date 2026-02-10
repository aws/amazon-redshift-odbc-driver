/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
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
#include <cstring>

#include "rsodbc.h"
#include "rsmem.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#define SHORT_STR_DATA    4096
#define SHORT_CMD_LEN    1024

#define MAX_AUDIT_CMDS_LEN 2048

#define TRACE_KEY_NAME            "SOFTWARE\\Amazon\\Amazon Redshift ODBC Driver (x64)\\Driver"
#define DM_TRACE_VAL_NAME        "Trace"

#define RSODBC_INI_FILE          "amazon.redshiftodbc.ini"
#define DRIVER_SECTION_NAME      "DRIVER"
#define MAX_OPTION_VAL_LEN		 512

#define PARAM_MARKER    '?'

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

#define ENABLE_SANITIZER 0

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

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

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

unsigned char *makeNullTerminatedStr(char *pData, int64_t cbLen, RS_STR_BUF *pPaStrBuf);

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

char *getParamVal(char *pParamData, SQLLEN iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, short hSQLType);
short getDefaultCTypeFromSQLType(short hSQLType, int *piConversionError);
char *convertCParamDataToSQLData(RS_STMT_INFO *pStmt, char *pParamData, SQLLEN iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, 
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
#ifdef WIN32
char *strcasestr(const char *str, const char *subStr);
#endif
char* strcasestrwhole(const char* str, const char* substr);
char *stristr(const char *str, const char *subStr);

SQLRETURN copyStrDataSmallLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen);
SQLRETURN copyStrDataLargeLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLINTEGER cbLen, SQLINTEGER *pcbLen);
SQLRETURN copyStrDataBigLen(RS_STMT_INFO *pStmt, const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *cbLenOffset, SQLLEN *pcbLenInd);

SQLRETURN copyWStrDataBigLen(RS_STMT_INFO *pStmt, const char *pSrc, SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen, SQLLEN *cbLenOffset, SQLLEN *pcbLenInd);

SQLRETURN copyBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyWBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyHexToBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen, SQLLEN *cbLenOffset);
SQLRETURN copyBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyWBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);


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

char *parseForMultiInsertCommand(RS_STMT_INFO *pStmt, char *pCmd, SQLINTEGER cbLen, char **ppLastBatchMultiInsertCmd);
char *getNextTokenForInsertCommand(char **ppSrc, size_t cbLen, int *pi, char delimiter);
int getNumberOfParams(RS_STMT_INFO *pStmt);
int getTotalMultiTuples(int numOfParamMarkers, long lArraySize, int *piLastBatchTotalMultiTuples);

char *findSQLClause(char *pTempCmd, char *pClause);
int DoesEmbedInDoubleQuotes(char *pStart,char *pEnd);
SQLRETURN createLastBatchMultiInsertCmd(RS_STMT_INFO *pStmt, char *pszLastBatchMultiInsertCmd);
SQLRETURN rePrepareMultiInsertCommand(RS_STMT_INFO *pStmt, char *pszCmd);



void checkAndSkipAllResultsOfStreamingCursor(RS_STMT_INFO *pStmt);
void skipAllResultsOfStreamingRowsUsingConnection(RS_CONN_INFO *pConn);
int doesAnyOtherStreamingCursorOpen(RS_CONN_INFO *pConn, RS_STMT_INFO *pStmt);

SQLRETURN getOneQueryVal(RS_CONN_INFO *pConn, char * pSqlCmd, char *pVarBuf, int iBufLen);
int updateOutBindParametersValue(RS_STMT_INFO *pStmt);
/**
 * @brief Bounded strlen (C++17, portable).
 *
 * Examines at most maxlen bytes starting at s and returns the count
 * of bytes before the first '\0'. If no terminator is found within the
 * limit, returns maxlen. If s is nullptr or maxlen is 0, returns 0.
 *
 * Never reads past s + maxlen, does not modify memory, and does not throw.
 * Semantics match POSIX `strnlen` except that nullptr is treated as length 0.
 *
 * @param s      pointer to a possibly unterminated character buffer
 * @param maxlen maximum bytes to inspect
 * @return number of bytes before '\0' (or maxlen if none found)
 */
inline size_t rs_strnlen(const char *s, size_t maxlen) {
    if (!s) return 0;
    for (size_t i = 0; i < maxlen; ++i) {
        if (s[i] == '\0') return i;
    }
    return maxlen;
}

char *rs_strncpy(char *dest, const char *src, size_t n);
char *rs_strncpy_safe(char *dest, const char *src, size_t n);
char *rs_strncat(char *dest, const char *src, size_t n);

#ifdef WIN32
unsigned char *decode64Password(const char *input, int length);
int date_out_wchar(int date, SQLWCHAR *buf, int buf_len);
int timestamp_out_wchar(long long timestamp, SQLWCHAR *buf, int buf_len, char *session_timezone);
int time_out_wchar(long long time, SQLWCHAR *buf, int buf_len, int *tzp);
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
int intervald2s_out_wchar(INTERVALD2S_STRUCT* d2s, SQLWCHAR *buf, int buf_len);
int intervaly2m_out_wchar(INTERVALY2M_STRUCT* y2m, SQLWCHAR *buf, int buf_len);
int intervaly2m_out_wchar(INTERVALD2S_STRUCT* d2s, SQLWCHAR *buf, int buf_len);
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

bool isEmptyString(SQLCHAR *str);
bool isNullOrEmptyString(SQLCHAR *str);
std::string char2String(const unsigned char* str);
std::string_view char2StringView(const unsigned char* str);
int showDiscoveryVersion(RS_STMT_INFO *pStmt);
bool getCaseSensitive(RS_STMT_INFO *pStmt);
std::string getDatabase(RS_STMT_INFO *pStmt);
int getIndex(RS_STMT_INFO *pStmt, std::string columnName);
bool isSqlAllCatalogs(SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName);
bool isSqlAllSchemas(SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName);
bool isSqlAllTableTypes(SQLCHAR *pTableType, SQLSMALLINT cbTableType);
std::string escapedFilter(const std::string& input);

char* sqlTypeNameMap(short value);
char* cTypeNameMap(short value);

class ExceptionInvalidParameter : public std::invalid_argument {
  public:
    // Constructor that takes a std::string message
    ExceptionInvalidParameter(const std::string &message);
};

/**
 * Dump an SQLWCHAR buffer as hex bytes, safely.
 *
 * Contract:
 *  - sqlwchr points to a buffer of SQLWCHAR code units (2 or 4 bytes each).
 *  - charLen is the number of code units (NOT bytes), or SQL_NTS for NUL-terminated.
 *  - On builds where sizeof(SQLWCHAR)==2, supplementary code points must be
 *    represented as surrogate pairs; thus charLen counts *code units* (pairs count as 2).
 *  - Output is truncated to kMaxDumpBytes for safety.
 */
void printHexSQLWCHR(SQLWCHAR *sqlwchr, int charLen,
                     const std::function<void(const std::string &)> &logFunc,
                     int cuSize = sizeof(SQLWCHAR));
void printHexSQLCHAR(SQLCHAR *sqlchar, int charLen,
                     const std::function<void(const std::string &)> &logFunc);

/**
 * @class scope_exit
 * @brief Minimal RAII guard that runs a callable when the scope exits.
 *
 * ### What it does
 * Runs a user-provided callable exactly once when the `scope_exit` object
 * is destroyed (i.e., when the current scope ends), regardless of whether
 * the scope exits normally (return) or via exception.
 *
 * ### Why use it
 * - Centralize cleanup (logging, unlocking, tracing) in one place.
 * - Eliminate duplicated epilogues and early-return boilerplate.
 * - Make functions read linearly without `goto` or repeated code.
 *
 * ### Semantics
 * - **Move-only.** Copying is disabled to prevent double execution.
 * - **Destruction is `noexcept`.** Your callable must not throw.
 * - **Releaseable.** Call `release()` to cancel execution if needed.
 *
 * ### Thread-safety
 * - Thread-safe as long as the provided callable is thread-safe.
 *
 * ### Performance
 * - Essentially zero-overhead after inlining (stores a small functor).
 *
 * ### C++23 tip
 * When you start using C++23, prefer `std::scope_exit` from `<scope>` and drop
 * this.
 */
template <class F> class scope_exit {
    static_assert(std::is_nothrow_destructible<F>::value ||
                      std::is_trivially_destructible<F>::value,
                  "scope_exit functor should be noexcept-destructible");
    static_assert(std::is_invocable<F>::value,
                  "scope_exit requires a callable type");
    static_assert(std::is_nothrow_invocable<F>::value,
              "scope_exit functor must be noexcept-invocable");

    F f_;
    bool active_ = true;

  public:
    explicit scope_exit(F f) noexcept(
        std::is_nothrow_move_constructible<F>::value)
        : f_(std::move(f)) {}

    scope_exit(const scope_exit &) = delete;
    scope_exit &operator=(const scope_exit &) = delete;

    scope_exit(scope_exit &&other) noexcept(
        std::is_nothrow_move_constructible<F>::value)
        : f_(std::move(other.f_)), active_(other.active_) {
        other.active_ = false;
    }

    scope_exit& operator=(scope_exit&&) = delete;

    ~scope_exit() noexcept {
        if (active_) {
            // The callable must not throw; keep this noexcept.
            f_();
        }
    }

    /// Prevent execution on destruction.
    void release() noexcept { active_ = false; }
};

template <class F>
inline scope_exit<typename std::decay<F>::type> make_scope_exit(F &&f) {
    return scope_exit<typename std::decay<F>::type>(std::forward<F>(f));
}


/**
 * @brief Copy up to (dstCapacityChars - 1) characters from src into dst and
 *        write a full-width U+0000 terminator.
 *
 * @param dst              Destination buffer (SQLWCHAR*).
 * @param dstCapacityChars Capacity in characters (code units).
 * @param src              Source buffer, already in client width.
 * @param srcChars         Number of characters to copy.
 * @param charSize         Size of SQLWCHAR in bytes (2 or 4).
 * @param copiedChars      Out: actual number of characters copied (no NUL).
 */
static inline void copyAndTerminateSqlwchar(void *dst, size_t dstCapacityChars,
                                            const void *src, size_t srcChars,
                                            size_t charSize,
                                            size_t *copiedChars = nullptr) {
    if (!dst || dstCapacityChars == 0) {
        if (copiedChars) {
            *copiedChars = 0;
        }
        return;
    }

    // Leave room for terminator
    const size_t maxPayload =
        (dstCapacityChars > 0) ? (dstCapacityChars - 1) : 0;
    const size_t toCopy =
        src ? (srcChars < maxPayload ? srcChars : maxPayload) : 0;

    // Copy payload
    if (toCopy) {
        std::memcpy(dst, src, toCopy * charSize);
    }

    // Write full-width null terminator
    if (charSize == 2) {
        reinterpret_cast<uint16_t *>(dst)[toCopy] = 0;
    } else if (charSize == 4) {
        reinterpret_cast<uint32_t *>(dst)[toCopy] = 0;
    } else {
        std::memset(static_cast<char *>(dst) + toCopy * charSize, 0, charSize);
    }

    if (copiedChars) {
        *copiedChars = toCopy;
    }
}

/**
 * @brief Copy a string into a client buffer with ODBC semantics.
 *
 * Delegates the actual copy/termination to copyAndTerminateSqlwchar().
 *
 * @param dst              Destination buffer (SQLWCHAR*).
 * @param src              Source buffer (already in client width).
 * @param totalCharsNeeded Logical length in characters (no terminator).
 * @param cchLen           Client buffer size in characters (incl. NUL).
 * @param pcbLen           Out: required size in bytes.
 * @param copiedChars      Out: number of characters actually copied (no NUL).
 * @param charSize         Width of SQLWCHAR (2 or 4).
 */
SQLRETURN copySqlwForClient(void *dst, const void *src, size_t totalCharsNeeded,
                            size_t cchLen, SQLLEN *pcbLen, size_t *copiedChars,
                            size_t charSize);

/**
 * @brief Set the Nth SQLWCHAR character in a buffer to null (0).
 *
 * Handles both UTF-16 (2-byte) and UTF-32 (4-byte) SQLWCHAR encodings.
 *
 * @param dst Pointer to the destination buffer (SQLWCHAR* or void*).
 * @param charIndex Zero-based index of the character to set to null.
 */
void setNthSqlwcharNull(void *dst, size_t charIndex);

/**
 * @brief Set the first SQLWCHAR in a buffer to U+0000.
 *
 * This writes a full-width null terminator (0x0000 or 0x00000000)
 * into the first character slot of the destination buffer.
 *
 * @param dst Pointer to the destination buffer (SQLWCHAR* or void*).
 */
void setFirstSqlwcharNull(void *dst);

/**
 * @brief Check if the first SQLWCHAR in a buffer is U+0000.
 *
 * @param src Pointer to the source buffer (SQLWCHAR* or void*).
 * @return true if the first character is null, false otherwise.
 */
bool isFirstSqlwcharNull(const void *src);

// Helper function to set SQLWCHAR to null if no characters were copied
//
static inline void setSqlwcharNullIfEmpty(SQLWCHAR *pwParam,
                                          size_t copiedChars) {
    if (pwParam && copiedChars == 0)
        setFirstSqlwcharNull(pwParam);
}

// Variadic helper to set multiple SQLWCHAR parameters to null if empty
//
static inline void setSqlwcharParamsNullIfEmpty() {}

template <typename... Args>
static inline void setSqlwcharParamsNullIfEmpty(SQLWCHAR *pwParam,
                                                size_t copiedChars,
                                                Args... args) {
    setSqlwcharNullIfEmpty(pwParam, copiedChars);
    setSqlwcharParamsNullIfEmpty(args...);
}

/*====================================================================================================================================================*/

/**
 * @brief Result codes for wide character to UTF-8 conversion operations.
 */
enum ConversionResult {
    CONVERSION_SUCCESS = 0,    /**< Conversion completed successfully */
    CONVERSION_TRUNCATED = 1,  /**< Conversion succeeded but output was truncated */
    CONVERSION_ERROR = 2       /**< Conversion failed due to invalid input or internal error */
};

/*====================================================================================================================================================*/

/**
 * @brief Convert wide character parameter to UTF-8 with error and truncation detection.
 *
 * Converts ODBC wide character input parameters to UTF-8 encoded strings, handling
 * NULL inputs, empty strings, invalid Unicode sequences, and buffer truncation.
 *
 * @param pwParam      Input wide character string (may be NULL)
 * @param cchParam     Length of input string in characters, or SQL_NTS for null-terminated
 * @param szParam      Output buffer for UTF-8 encoded string
 * @param bufLen       Size of output buffer in bytes
 * @param paramName    Name of parameter for error messages (may be NULL)
 * @param logTag       Tag for logging (may be NULL, defaults to "RSUTIL")
 * @param pStmt        Statement handle for error reporting
 * @param copiedChars  Output: number of bytes written to szParam (excluding null terminator)
 *
 * @return CONVERSION_SUCCESS if conversion completed without issues
 * @return CONVERSION_TRUNCATED if output was truncated due to insufficient buffer space
 * @return CONVERSION_ERROR if conversion failed due to invalid Unicode or internal error
 */
ConversionResult convertWCharParamWithTruncCheck(SQLWCHAR *pwParam, SQLSMALLINT cchParam,
                                                  char *szParam, size_t bufLen,
                                                  const char *paramName, const char *logTag,
                                                  RS_STMT_INFO *pStmt, size_t *copiedChars);

/**
 * Information about the Driver Manager (DM) that loaded this driver.
 *
 * This structure identifies which ODBC Driver Manager is present in the
 * current process (iODBC, unixODBC, Windows DM, or unknown), together with
 * an optional version string if the DM exposes one.
 *
 * The detection is performed at runtime using weak symbol inspection
 * (dlopen/dlsym on POSIX, GetProcAddress on Windows).  Only symbols that
 * are globally exported by the Driver Manager are used as fingerprints.
 *
 * This information is used by the driver to adjust behavior that depends
 * on DM-specific conventions—for example, iODBC on macOS typically uses a
 * 4-byte SQLWCHAR (UTF-32 or "packed UTF-16 in W=4"), while unixODBC uses a
 * 2-byte SQLWCHAR (UTF-16).
 */
struct DriverManagerInfo {
    /**
     * Enumeration of recognized Driver Manager families.
     *
     * UNKNOWN   – No recognized DM signatures were found.
     * IODBC     – The iODBC Driver Manager is loaded (macOS or POSIX builds).
     * UNIXODBC  – The unixODBC Driver Manager is loaded (Linux, some Unix).
     * WINDOWS   – Microsoft ODBC Driver Manager (Windows only).
     */
    enum Family { UNKNOWN, IODBC, UNIXODBC, WINDOWS } family = UNKNOWN;

    /**
     * Optional, DM-reported version string (e.g., "3.52.12").
     *
     * Not all DMs export a formal version symbol. When not available,
     * this field is left empty.
     */
    std::string version;

    /**
     * @return A human-readable string for the detected DM family.
     */
    const char *GetFamilyName() const {
        switch (family) {
        case IODBC:
            return "iODBC";
        case UNIXODBC:
            return "unixODBC";
        case WINDOWS:
            return "Windows";
        default:
            return "Unknown";
        }
    }
};

/**
 * Detect the active ODBC Driver Manager at runtime.
 *
 * This function inspects globally visible symbols in the current process
 * to determine which ODBC Driver Manager (DM) loaded the driver.
 * Typical detection rules:
 *
 *   - Presence of "iodbc_version"        → iODBC
 *   - Presence of "uodbc_get_stats"      → unixODBC
 *   - Win32 ODBC exports (SQLDriverConnectW, etc.) → Windows DM
 *
 * For POSIX platforms, the function uses dlopen(NULL) + dlsym().
 * For Windows, GetModuleHandle() + GetProcAddress() is used.
 *
 * @return A DriverManagerInfo structure containing the detected DM family
 *         and optional version information.
 *
 * @note Detection is best-effort. If no known DM signatures are found,
 *       the family will be DriverManagerInfo::UNKNOWN.
 */
DriverManagerInfo detectDriverManager();

/**
 * Convenience helper indicating whether the active Driver Manager is iODBC.
 *
 * This function internally calls detectDriverManager() once and caches
 * the result. Subsequent calls are inexpensive.
 *
 * Typical usage:
 *   if (isIODBC()) {
 *       // Enable UTF-32 / W=4 client-side Unicode decoding defaults
 *       // on macOS, because iODBC commonly uses 4-byte SQLWCHAR.
 *   }
 *
 * @return true if the currently loaded ODBC Driver Manager matches the
 *         iODBC family; false otherwise.
 */
bool isIODBC();
// explicit memory clearing using a secure zeroing
// function that won't be optimized away by the compiler:
static inline void rs_secure_zero(void *ptr, size_t len) {
#ifdef _WIN32
    SecureZeroMemory(ptr, len);
#else
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) {
        *p++ = 0;
    }
#endif
}

#endif /* C++ */
