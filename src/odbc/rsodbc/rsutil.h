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
#include <math.h>
#include <cmath>
#include <limits>
#include <regex>

//#include <strsafe.h>

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

// Macro for calling getParamVal in convertCParamDataToSQLData
#define GET_PARAM_VAL_AND_CHECK() \
    do { \
        pcVal = getParamVal(pParamData, iParamDataLen, plParamDataStrLenInd, hType, \
                           pBindParamStrBuf, hSQLType, iColumnSize, &iConversionError, &sqlstate); \
        if(iConversionError) \
            goto error; \
    } while(0)

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_PRECISION 38 // maximum precision of DECIMAL/NUMERIC in PADB
#define MAX_SCALE 37 // maximum scale of DECIMAL/NUMERIC in PADB

#define MIN_DATE_YEAR -9999
#define MAX_DATE_YEAR 9999
#define MIN_DATE_MONTH 1
#define MAX_DATE_MONTH 12
#define MIN_DATE_DAY 1
#define MIN_TIME_HOUR 0
#define MAX_TIME_HOUR 23
#define MIN_TIME_MINUTE 0
#define MAX_TIME_MINUTE 59
#define MIN_TIME_SECOND 0
#define MAX_TIME_SECOND 59
#define MAX_TIME_FRACTION_NANOSECOND 999999999
#define MAX_TIME_FRACTION_PRECISION 9

#define MAX_INTERVAL_YEAR       100000000UL

#define MAX_INTERVAL_MONTH      12

/** Maximum values for interval day-to-second components */
#define MAX_INTERVAL_DAY        100000000UL
#define MAX_INTERVAL_HOUR       24
#define MAX_INTERVAL_MINUTE     60
#define MAX_INTERVAL_SECOND     60
#define MAX_INTERVAL_FRACTION   1000000UL  // Microseconds (6 digits)

// Maximum length of interval/timestamp string inputs to regex validation.
// Generous upper bound: longest valid interval string is ~56 chars
// (postgres verbose: "@ 999999999 days 23 hours 59 mins 59.123456789 secs ago").
#define MAX_INTERVAL_STRING_LENGTH 64

/**
 * @brief Safe remaining buffer size for incremental snprintf.
 *
 * When building a string with multiple snprintf calls, snprintf returns
 * the number of chars that would have been written (not actual).
 * So `written` can exceed `buf_len` after truncation. Passing
 * `buf_len - written` to the next snprintf would underflow to a huge
 * size_t, causing a buffer overflow.
 *
 * This helper clamps to 0 when exhausted, preventing the underflow.
 *
 * @param written  Total chars reported by prior snprintf calls (may exceed buf_len)
 * @param buf_len  Total buffer capacity
 * @return         Remaining writable bytes (0 if buffer is exhausted)
 */
static inline int rs_buf_remaining(int written, int buf_len) {
    return (written < buf_len) ? (buf_len - written) : 0;
}

/**
 * @brief Safe write offset for incremental snprintf.
 *
 * Returns the offset into buf where the next snprintf should write.
 * Clamped to buf_len-1 (the last valid position for a NUL terminator)
 * when written >= buf_len.
 *
 * @param written  Total chars reported by prior snprintf calls
 * @param buf_len  Total buffer capacity (must be > 0)
 * @return         Offset into buffer for next write
 */
static inline int rs_buf_offset(int written, int buf_len) {
    return (written < buf_len) ? written : (buf_len > 0 ? buf_len - 1 : 0);
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
    SQL_INTERVAL_STRUCT intervalVal; // INTERVAL
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

typedef enum {
    PARSE_SUCCESS = 0,
    PARSE_INVALID_FORMAT = 1, // Invalid characters
    PARSE_OVERFLOW = 2,        // Exponent overflow
} ParseReturnCode;

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
#define TIME_STR_LEN 8 // strlen("hh:mm:ss") = 8

#define DATE_STRING_LEN 10 // strlen("YYYY-MM-DD") = 10
#define TS_NO_FRAC_LEN 19 // strlen("yyyy-mm-dd hh:mm:ss") = 19
#define TIME_MAX_HOUR   23
#define TIME_MAX_MINUTE 59
#define TIME_MAX_SECOND 59

// Fractional seconds precision constants
#define TIME_FRAC_PRECISION_MS    3   // Milliseconds (3 digits)
#define TIME_FRAC_PRECISION_US    6   // Microseconds (6 digits) 
#define TIME_FRAC_PRECISION_NS    9   // Nanoseconds (9 digits)

// Helper macro to safely convert magnitude
#define SAFE_CONVERT_MAG(targetType, isNeg, mag, result, errorList) \
    ((isNeg) ? \
        ((mag > (unsigned long long)LLONG_MAX + 1ULL) ? \
            (addError(errorList, "22003", "Numeric value out of range", 0, NULL), SQL_ERROR) : \
            rsGenericConvert<targetType>((mag == (unsigned long long)LLONG_MAX + 1ULL) ? LLONG_MIN : -(long long)mag, result, errorList)) : \
        rsGenericConvert<targetType>(mag, result, errorList))

#ifdef __cplusplus
extern "C" 
{
#endif /* C++ */

char *rs_strdup(const char *src, size_t cbLen);


unsigned char *makeNullTerminatedStr(char *pData, int64_t cbLen, RS_STR_BUF *pPaStrBuf);
bool isLeapYear(int year);
bool validateDate(int year, int month, int day);
bool validateTime(int hour, int minute, int second, int fraction);
bool validateDateTime(int year, int month, int day, int hour, int minute,
                      int second, int fraction);
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

char *getParamVal(char *pParamData, int iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, short hSQLType, int iColumnSize, int *pConversionError, char **pSqlstate);
short getDefaultCTypeFromSQLType(short hSQLType, int *piConversionError);
char *convertCParamDataToSQLData(RS_STMT_INFO *pStmt, char *pParamData, int iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType,
                                  short hSQLType, short hPrepSQLType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, RS_DESC_REC *pDescRec, int *piConversionError);


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
bool trimWhitespace(const char **startPtr, const char **endPtr) ;
ParseReturnCode parseExponent(const char **currentPos, const char *endPtr, int *exponent);

SQLRETURN copyStrDataSmallLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen, RS_ERROR_INFO **ppErrorList);
SQLRETURN copyStrDataLargeLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLINTEGER cbLen, SQLINTEGER *pcbLen);
SQLRETURN copyStrDataBigLen(RS_STMT_INFO *pStmt, const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *cbLenOffset, SQLLEN *pcbLenInd);

SQLRETURN copyWStrDataBigLen(RS_STMT_INFO *pStmt, const char *pSrc, SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen, SQLLEN *cbLenOffset, SQLLEN *pcbLenInd);

SQLRETURN copyBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyWBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyHexToBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen, SQLLEN *cbLenOffset);
SQLRETURN copyBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen);
SQLRETURN copyWBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen, SQLLEN *pcbLen);

/**
 * @brief Copy a fixed-size value to a SQL_C_BINARY output buffer.
 *
 * For fixed-size SQL types (integers, floats, dates, timestamps, intervals,
 * numerics), the buffer must be large enough to hold the entire value.
 * No partial data is written. Returns SQL_ERROR with SQLSTATE 22003 if
 * the buffer is too small, or HY009 if the buffer pointer is NULL.
 * Indicator is only set on success.
 *
 * @param pSrc       Pointer to source data.
 * @param srcSize    Size of source data in bytes.
 * @param pBuf       Application output buffer.
 * @param cbLen      Size of output buffer in bytes.
 * @param pcbLenInd  Pointer to length/indicator output (receives srcSize on success).
 * @param pStmt      Statement handle for error posting.
 * @param typeName   Type name string for error messages.
 * @return SQL_SUCCESS, or SQL_ERROR with SQLSTATE 22003 or HY009.
 */
SQLRETURN copyToCBinary(const void *pSrc, SQLLEN srcSize, void *pBuf, SQLLEN cbLen, SQLLEN *pcbLenInd, RS_STMT_INFO *pStmt, const char *typeName);

/**
 * @brief Copy variable-length data to a SQL_C_BINARY output buffer.
 *
 * For variable-length SQL types (char, varchar, binary, varbinary),
 * if the buffer is too small, data is truncated to fit and SQLSTATE 01004
 * is returned with SQL_SUCCESS_WITH_INFO. No null terminator is appended.
 * Supports chunked retrieval (successive SQLGetData calls on the same column)
 * via cbLenOffset which tracks the read position between calls.
 *
 * @param pSrc         Pointer to source data.
 * @param srcLen       Length of source data in bytes.
 * @param pBuf         Application output buffer (NULL skips copy, indicator still set).
 * @param cbLen        Size of output buffer in bytes.
 * @param cbLenOffset  Pointer to offset tracking chunked retrieval position (may be NULL).
 * @param pcbLenInd    Pointer to length/indicator output (receives remaining data length).
 * @param pStmt        Statement handle for error/warning posting.
 * @return SQL_SUCCESS, or SQL_SUCCESS_WITH_INFO with SQLSTATE 01004 on truncation.
 */
SQLRETURN copyVariableToCBinary(const char *pSrc, SQLLEN srcLen, void *pBuf, SQLLEN cbLen, SQLLEN *cbLenOffset, SQLLEN *pcbLenInd, RS_STMT_INFO *pStmt);

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

SQLRETURN getTinyIntData(int8_t hVal, void *pBuf, SQLLEN *pcbLenInd);
SQLRETURN getUTinyIntData(uint8_t hVal, void *pBuf, SQLLEN *pcbLenInd);
SQLRETURN getShortData(short hVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getUShortData(unsigned short hVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getIntData(int iVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getUIntData(unsigned int iVal, void *pBuf, SQLLEN *pcbLenInd);
SQLRETURN getBigIntData(long long llVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getUBigIntData(unsigned long long llVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getFloatData(float fVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getDoubleData(double dVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getBooleanData(char bVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getDateData(DATE_STRUCT *pdtVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getTimeStampData(TIMESTAMP_STRUCT *ptsVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getIntervalY2MData(SQL_INTERVAL_STRUCT *pIntervalVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN getIntervalD2SData(SQL_INTERVAL_STRUCT *pIntervalVal, void *pBuf,  SQLLEN *pcbLenInd);
SQLRETURN convertCharToIntervalD2S(char *pColData, int iColDataLen, int format,
                                   SQL_INTERVAL_STRUCT *pIntervalVal, void *pBuf,
                                   SQLLEN *pcbLenInd, RS_ERROR_INFO **ppErrorList);
SQLRETURN convertCharToIntervalY2M(char *pColData, int iColDataLen, int format,
                                   SQL_INTERVAL_STRUCT *pIntervalVal, void *pBuf,
                                   SQLLEN *pcbLenInd, RS_ERROR_INFO **ppErrorList);
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
void getSQLLENVal(long lVal, SQLLEN *plVal, SQLINTEGER *pcbLen);
void getSQLULENVal(long lVal, SQLULEN *pulVal, SQLINTEGER *pcbLen);

short getDateTimeIntervalCode(short hDateTimeIntervalCode, short hType);
short getCTypeFromConciseType(short hConciseType, short hDateTimeIntervalCode, short hType);

/**
 * Maps datetime concise types to their corresponding subtype codes.
 *
 * Handles both ODBC 2.x (SQL_DATE, SQL_TIME, SQL_TIMESTAMP) and
 * ODBC 3.x (SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP) datetime types.
 *
 * @param conciseType The concise datetime type (e.g., SQL_TYPE_DATE, SQL_DATE)
 * @return The corresponding subtype code (e.g., SQL_CODE_DATE) or 0 if not a
 * datetime type
 */
SQLSMALLINT mapDatetimeConciseTypeToCode(SQLSMALLINT conciseType);

/**
 * Maps datetime interval codes to their corresponding concise types.
 *
 * Returns different concise types based on descriptor type and ODBC version:
 * - IPD (Implementation Parameter Descriptor): Returns SQL types (SQL_DATE vs
 * SQL_TYPE_DATE)
 * - APD/ARD (Application descriptors): Returns C types (SQL_C_TYPE_DATE)
 *
 * For IPD, ODBC 2.x uses deprecated types (SQL_DATE) while ODBC 3.x uses
 * SQL_TYPE_DATE for consistency with the specification.
 *
 * @param code The datetime interval code (e.g., SQL_CODE_DATE)
 * @param isIPD True if this is an Implementation Parameter Descriptor
 * @param isODBC2 True if the driver is operating in ODBC 2.x mode
 * @return The corresponding concise type (SQL_TYPE_DATE, SQL_DATE, or
 * SQL_C_TYPE_DATE)
 */
SQLSMALLINT mapDatetimeCodeToConciseType(SQLSMALLINT code, bool isIPD,
                                         bool isODBC2);

/**
 * Maps interval concise types to their corresponding interval codes.
 *
 * Supports all ODBC interval types including single-field intervals
 * (YEAR, MONTH, DAY, HOUR, MINUTE, SECOND) and multi-field intervals
 * (YEAR_TO_MONTH, DAY_TO_HOUR, etc.).
 *
 * @param conciseType The concise interval type (e.g., SQL_INTERVAL_YEAR)
 * @return The corresponding interval code (e.g., SQL_CODE_YEAR) or 0 if not an
 * interval type
 */
SQLSMALLINT mapIntervalCodeToConciseType(SQLSMALLINT code);

/**
 * Maps interval codes to their corresponding concise types.
 *
 * Inverse operation of mapIntervalConciseTypeToCode. Used when
 * SQL_DESC_DATETIME_INTERVAL_CODE is set and SQL_DESC_CONCISE_TYPE
 * needs to be synchronized.
 *
 * @param code The interval code (e.g., SQL_CODE_YEAR)
 * @return The corresponding concise interval type (e.g., SQL_INTERVAL_YEAR) or
 * 0 if invalid
 */
SQLSMALLINT mapIntervalConciseTypeToCode(SQLSMALLINT conciseType);

/**
 * Checks if a given type is a datetime type.
 *
 * Supports both ODBC 2.x deprecated types (SQL_DATE, SQL_TIME, SQL_TIMESTAMP),
 * ODBC 3.x SQL types (SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP),
 * and C types (SQL_C_TYPE_DATE, SQL_C_TYPE_TIME, SQL_C_TYPE_TIMESTAMP).
 *
 * @param type The SQL type to check
 * @return true if the type is a datetime type, false otherwise
 */
bool isDatetimeType(SQLSMALLINT type);

/**
 * Checks if a given type is an interval type.
 *
 * All interval types fall within a contiguous range from SQL_INTERVAL_YEAR
 * to SQL_INTERVAL_MINUTE_TO_SECOND, allowing for efficient range checking.
 *
 * @param type The SQL type to check
 * @return true if the type is an interval type, false otherwise
 */
bool isIntervalType(SQLSMALLINT type);

/**
 * Checks if a given code is a valid interval code.
 *
 * Interval codes range from SQL_CODE_YEAR to SQL_CODE_MINUTE_TO_SECOND.
 * Used for validation when SQL_DESC_DATETIME_INTERVAL_CODE is set.
 *
 * @param code The interval code to check
 * @return true if the code is a valid interval code, false otherwise
 */
bool isIntervalCode(SQLSMALLINT code);

/**
 * Checks if a given code is a valid datetime code.
 *
 * Datetime codes are SQL_CODE_DATE, SQL_CODE_TIME, and SQL_CODE_TIMESTAMP.
 * Used for validation when SQL_DESC_DATETIME_INTERVAL_CODE is set.
 *
 * @param code The datetime code to check
 * @return true if the code is a valid datetime code, false otherwise
 */
bool isDateTimeCode(SQLSMALLINT code);

/**
 * Checks if an interval code represents an interval with a seconds component.
 *
 * Intervals with seconds components require special handling for precision:
 * - SQL_DESC_PRECISION is set to 6 (default interval seconds precision)
 *
 * @param code The interval code to check
 * @return true if the interval has a seconds component, false otherwise
 */
bool isIntervalSecondCode(SQLSMALLINT code);

bool isValidOdbcSQLType(SQLSMALLINT type);

bool isValidOdbcCType(SQLSMALLINT type);

bool isValidDatetimeIntervalCode(SQLSMALLINT code, SQLSMALLINT type);

bool isNumericType(SQLSMALLINT type);

bool validateDescriptorConsistency(RS_DESC_REC *pDescRec, bool isODBC2);

/**
 * Synchronizes descriptor fields when SQL_DESC_CONCISE_TYPE is set.
 *
 * Per ODBC specification, when SQL_DESC_CONCISE_TYPE is set:
 * 1. For datetime types: SQL_DESC_TYPE = SQL_DATETIME,
 *    SQL_DESC_DATETIME_INTERVAL_CODE = appropriate datetime code
 * 2. For interval types: SQL_DESC_TYPE = SQL_INTERVAL,
 *    SQL_DESC_DATETIME_INTERVAL_CODE = appropriate interval code
 * 3. For other types: SQL_DESC_TYPE = same as concise type,
 *    SQL_DESC_DATETIME_INTERVAL_CODE = 0
 *
 * @param pDescRec Pointer to the descriptor record to update
 * @param conciseType The concise type being set
 */
void syncFieldsFromConciseType(RS_DESC_REC *pDescRec, SQLSMALLINT conciseType);

/**
 * Synchronizes descriptor fields when SQL_DESC_TYPE is set.
 *
 * Per ODBC specification, when SQL_DESC_TYPE is set:
 * 1. For verbose datetime type (SQL_DATETIME): SQL_DESC_CONCISE_TYPE is set
 *    based on SQL_DESC_DATETIME_INTERVAL_CODE (must be set separately or
 * already set)
 * 2. For verbose interval type (SQL_INTERVAL): SQL_DESC_CONCISE_TYPE is set
 *    based on SQL_DESC_DATETIME_INTERVAL_CODE
 * 3. For concise types: SQL_DESC_CONCISE_TYPE = same value,
 *    SQL_DESC_DATETIME_INTERVAL_CODE = 0
 *
 * Additionally sets default values for related fields per ODBC specification:
 * - Datetime types: SQL_DESC_PRECISION based on datetime code
 * - Interval types: SQL_DESC_DATETIME_INTERVAL_PRECISION = 2 (default leading
 * precision), SQL_DESC_PRECISION = 6 for intervals with seconds component
 * - Character types: SQL_DESC_LENGTH = 1, SQL_DESC_PRECISION = 0
 * - Numeric types: SQL_DESC_SCALE = 0
 * - Float types: SQL_DESC_PRECISION = 24 (SQL_REAL precision)
 *
 * @param pDescRec Pointer to the descriptor record to update
 * @param type The type being set
 * @param isIPD True if this is an Implementation Parameter Descriptor
 * @param isODBC2 True if operating in ODBC 2.x mode
 */
void syncFieldsFromType(RS_DESC_REC *pDescRec, SQLSMALLINT type, bool isIPD,
                        bool isODBC2);

/**
 * Synchronizes descriptor fields when SQL_DESC_DATETIME_INTERVAL_CODE is set.
 *
 * Per ODBC specification, when SQL_DESC_DATETIME_INTERVAL_CODE is set:
 * - If SQL_DESC_TYPE is SQL_DATETIME: SQL_DESC_CONCISE_TYPE is set to the
 *   corresponding datetime concise type (SQL_TYPE_DATE, SQL_TYPE_TIME, or
 * SQL_TYPE_TIMESTAMP)
 * - If SQL_DESC_TYPE is SQL_INTERVAL: SQL_DESC_CONCISE_TYPE is set to the
 *   corresponding interval concise type
 *
 * This function requires SQL_DESC_TYPE to already be set to SQL_DATETIME or
 * SQL_INTERVAL.
 *
 * @param pDescRec Pointer to the descriptor record to update
 * @param code The datetime/interval code being set
 * @param isIPD True if this is an Implementation Parameter Descriptor
 * @param isODBC2 True if operating in ODBC 2.x mode
 */
void syncFieldsFromIntervalCode(RS_DESC_REC *pDescRec, SQLSMALLINT code,
                                bool isIPD, bool isODBC2);

SQLRETURN checkAndAutoFetchRefCursor(RS_STMT_INFO *pStmt);
void releaseResult(RS_RESULT_INFO *pResult, int iAtHeadResult, RS_STMT_INFO *pStmt);

int isNullOrEmpty(SQLCHAR *pData);
RS_DESC_INFO *releaseExplicitDescs(RS_DESC_INFO *phdescHead);

void beginApiMutex(SQLHENV phenv, SQLHDBC pConn);
void endApiMutex(SQLHENV phenv, SQLHDBC pConn);

SQLRETURN checkHstmtHandleAndAddError(SQLHSTMT phstmt, SQLRETURN rc, char *pSqlState, char *pSqlErrMsg);
SQLRETURN checkHdbcHandleAndAddError(SQLHDBC phdbc, SQLRETURN rc, char *pSqlState, char *pSqlErrMsg);


int isODBC2Behavior(RS_STMT_INFO *pStmt);
int isODBC2BehaviorByDesc(RS_DESC_INFO *pDesc);
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
SQLRETURN prepareStringForNumericConversion(RS_STMT_INFO *pStmt, char *pColData,
                                            int iColDataLen, char *pTempBuf,
                                            int iTempBufSize,
                                            char **ppPreparedStr,
                                            int *pTruncated);
SQLRETURN parseDatePart(const char *dateStr, int *y, int *m, int *d,
                               RS_ERROR_INFO **err);
SQLRETURN parseTimePart(const char *timeStr, int *h, int *m, int *s,
                               const char **fracStart, RS_ERROR_INFO **err);
SQLRETURN parseFraction(const char *fracPtr, unsigned int *fraction,
                               bool *trunc, RS_ERROR_INFO **err);
bool validateDate(int y, int m, int d);
bool isLeapYear(int year);
bool isDigitStr(const char *s, int len);

SQLRETURN parseTimestampString(const char *pStr,
                               TIMESTAMP_STRUCT *pTimestampStruct,
                               RS_ERROR_INFO **ppErrorList);
SQLRETURN parseDateString(const char *pStr, DATE_STRUCT *pDateStruct,
                          RS_ERROR_INFO **ppErrorList);
SQLRETURN parseTimeString(const char *pTimeStr, RS_TIME_STRUCT *pTimeStruct,
                          RS_ERROR_INFO **ppErrorList);

ParseReturnCode parseAndBuildInteger(const char* src, int len, bool& isNeg, unsigned long long& magnitude, bool& droppedFraction);
SQLRETURN convertStringNumericToIntegerCType(RS_STMT_INFO *pStmt, char *pColData, int iColDataLen, void *pBuf, SQLLEN *pcbLenInd, SQLSMALLINT hType);
SQLRETURN convertStringNumericToFloatCType(RS_STMT_INFO *pStmt, char *pColData, int iColDataLen, void *pBuf, SQLLEN *pcbLenInd, SQLSMALLINT hType);
SQLRETURN convertNumericStringToScaledIntegerExtended(RS_STMT_INFO *pStmt, char *pNumData, int iColDataLen, SQL_NUMERIC_STRUCT *pnVal);
SQLRETURN validateNumericBufferSize(RS_STMT_INFO *pStmt, const char *numStr, int numStrLen,
                                   SQLLEN cbLen, short hSQLType, short hType, bool isWideChar);
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
int intervaly2m_out(SQL_INTERVAL_STRUCT* pInterval, char *buf, int buf_len);
int intervald2s_out(SQL_INTERVAL_STRUCT* pInterval, char *buf, int buf_len);
int interval2tm(long long time, int months, struct pg_tm * tm, long long *fsec);
SQL_INTERVAL_STRUCT parse_intervaly2m(const char *buf, int buf_len);
SQL_INTERVAL_STRUCT parse_intervald2s(const char *buf, int buf_len);
SQL_INTERVAL_STRUCT returnInvalidIntervalY2M();
SQL_INTERVAL_STRUCT returnInvalidIntervalD2S();
void dt2time(long long jd, int *hour, int *min, int *sec, long long *fsec);
void TrimTrailingZeros(char *str, int *plen);
int time_out(long long time, char *buf, int buf_len, int *tzp);
int time2tm(long long time, struct pg_tm* tm, long long* fsec);

int getInt32FromBinary(char *pColData, int idx);
long long getInt64FromBinary(char *pColData, int idx);
#ifdef __cplusplus
}

// Static regex patterns
namespace RegexPatterns {
// Pattern for time format: HH:MM:SS with optional fractional seconds
// Hours restricted to 0-23 (excess should be in day component)
// Uses anchors for strict matching when used with regex_match
static const std::regex
    TIME_FORMAT_PATTERN(R"(^(?:[01]?\d|2[0-3]):[0-5][0-9]:[0-5][0-9](?:\.\d+)?$)");

// Pattern for year-month format: Y-M
static const std::regex YEAR_MONTH_FORMAT_PATTERN(R"(^\d+-\d+$)");

// Pattern for day-to-second SQL standard format: [+/-]D HH:MM:SS[.fraction]
// Day allows any number of digits; hours restricted to 0-23
// Matches: "3 04:30:15", "+10 05:00:00", "99999999999 00:00:00"
// Does NOT match: "3 25:30:15" (hours > 23), "3--04:30:15" (double dash)
static const std::regex DAY_TO_SECOND_SQL_PATTERN(
    R"(^[-+]?\d+\s+(?:[01]?\d|2[0-3]):[0-5][0-9]:[0-5][0-9](?:\.\d+)?$)");

// Pattern for timestamp format: YYYY-MM-DD HH:MM:SS[.fraction]
// Hours restricted to 0-23, minutes and seconds to 0-59
static const std::regex TIMESTAMP_PATTERN(
    R"(^\d{4}-\d{2}-\d{2}\s+(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d(?:\.\d+)?$)");
} // namespace RegexPatterns


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

bool isTimePortionZero(const TIMESTAMP_STRUCT *ts);
bool isEmptyString(SQLCHAR *str);
bool isNullOrEmptyString(SQLCHAR *str);
std::string char2String(const unsigned char* str);
std::string_view char2StringView(const unsigned char* str);
int showDiscoveryVersion(RS_STMT_INFO *pStmt);
/**
 * @brief Validates statement handle and cursor state before executing a catalog function.
 *
 * Performs two validations required by ODBC specification:
 * 1. Validates that the statement handle is valid
 * 2. Validates that no cursor is open on the statement handle
 *
 * According to ODBC specification, catalog functions must return SQLSTATE 24000 (Invalid cursor state)
 * when a cursor is open on the StatementHandle (statement is in RS_EXECUTE_STMT state).
 *
 * @param phstmt Statement handle to validate
 * @return SQL_SUCCESS if validations pass
 *         SQL_INVALID_HANDLE if statement handle is invalid
 *         SQL_ERROR if a cursor is open (with SQLSTATE 24000 added to error list)
 */
SQLRETURN validateStatementForCatalogFunction(SQLHSTMT phstmt);
bool getCaseSensitive(RS_STMT_INFO *pStmt);
std::string getDatabase(RS_STMT_INFO *pStmt);
int getIndex(RS_STMT_INFO *pStmt, std::string columnName);
bool isSqlAllCatalogs(SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName);
bool isSqlAllSchemas(SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName);
bool isSqlAllTableTypes(SQLCHAR *pTableType, SQLSMALLINT cbTableType);
std::string escapedFilter(const std::string& input);

char* sqlTypeNameMap(short value);
char* cTypeNameMap(short value);

std::string formatConversionPrefix(short sqlType, short cType, bool sqlToC = 1);

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

// Template function for safe numeric range checking
// Validates if a value can be safely converted from SourceType to TargetType without overflow.
// Handles all combinations: float->int, int->int, int->float, float->float
template<typename TargetType, typename SourceType>
bool isInRange(SourceType value) {
    using Src = SourceType;
    using Dst = TargetType;

    constexpr bool sourceIsFloat = std::is_floating_point<Src>::value;
    constexpr bool targetIsFloat = std::is_floating_point<Dst>::value;
    constexpr bool sourceIsInt   = std::is_integral<Src>::value;
    constexpr bool targetIsInt   = std::is_integral<Dst>::value;
    constexpr bool sameType      = std::is_same<Src, Dst>::value;
    constexpr bool sourceSigned  = std::is_signed<Src>::value;
    constexpr bool targetSigned  = std::is_signed<Dst>::value;

    // 1) Floating point -> Integer
    if (sourceIsFloat && targetIsInt) {

        // 1. Reject NaN and infinities: they cannot be converted to any integer.
        if (std::isnan(value) || std::isinf(value)) {
            return false;
        }

        // 2. Work in long double for the comparison to reduce risk of
        //    intermediate rounding issues. This does NOT assume long double
        //    is wider than Src; the logic below is based on integer bit-width.
        using LD = long double;
        const LD v = static_cast<LD>(value);

        // 3. Use the mathematical range defined by Dst's value bits.
        //
        // For an integer type Dst:
        //   - std::numeric_limits<Dst>::digits is the number of value bits
        //     (excluding the sign bit for signed types).
        //
        //   Signed Dst:
        //     range = [ -2^digits, 2^digits - 1 ]
        //            => safe v satisfies: -2^digits <= v < 2^digits
        //
        //   Unsigned Dst:
        //     range = [ 0, 2^digits - 1 ]
        //            => safe v satisfies: 0 <= v < 2^digits
        //
        // This avoids the bug where casting Dst::max() to Src can round UP
        // (e.g. (double)LLONG_MAX becomes 2^63), which would incorrectly
        // make 2^63 look "in range" if we compare only rounded endpoints.
        constexpr int dstDigits = std::numeric_limits<Dst>::digits;

        if (std::numeric_limits<Dst>::is_signed) {
            // Signed integer: [-2^digits, 2^digits - 1]
            const LD lo = -std::ldexp((LD)1, dstDigits); // -2^digits
            const LD hi =  std::ldexp((LD)1, dstDigits); //  2^digits (EXCLUSIVE upper bound)

            // Safe iff lo <= v < hi.
            // Example for int64_t (digits = 63):
            //   lo = -2^63, hi = 2^63
            //   -2^63        -> true  (LLONG_MIN)
            //   2^63 - 1     -> true  (LLONG_MAX)
            //   2^63         -> false (would overflow)
            return (v >= lo) && (v < hi);
        } else {
            // Unsigned integer: [0, 2^digits - 1]
            const LD lo = 0.0L;
            const LD hi = std::ldexp((LD)1, dstDigits); // 2^digits (EXCLUSIVE)

            // Safe iff 0 <= v < 2^digits.
            // Example for uint32_t (digits = 32):
            //   0            -> true
            //   2^32 - 1     -> true
            //   2^32         -> false
            //   negative     -> false
            return (v >= lo) && (v < hi);
        }
    }


    // 2) Integer -> Integer conversion
    //    Relatively straightforward: check sign compatibility and magnitude bounds
    //    Examples:
    //      int32 -> int16: must check range [-32768, 32767]
    //      uint32 -> int32: must check range [0, 2147483647]
    //      int16 -> int32: always safe (widening conversion)
    if (sourceIsInt && targetIsInt) {
        if (sameType) {
            return true;
        }

        // Both signed or both unsigned
        if (sourceSigned == targetSigned) {
            if (sizeof(Src) >= sizeof(Dst)) {
                // Source can hold wider range than target: check against target bounds
                // Example: int32 -> int16, must verify value is within [-32768, 32767]
                return value >= static_cast<Src>((std::numeric_limits<Dst>::min)()) &&
                       value <= static_cast<Src>((std::numeric_limits<Dst>::max)());
            } else {
                // Target is wider or equal: any value of Source fits
                // Example: int16 -> int32, all values are safe
                return true;
            }
        }

        // Signed -> Unsigned conversion
        //   Must reject negative values and check upper bound
        //   Example: int32 -> uint16, value must be in [0, 65535]
        if (sourceSigned && !targetSigned) {
            if (value < 0) {
                return false;
            }
            using U64 = unsigned long long;
            return static_cast<U64>(value) <=
                   static_cast<U64>((std::numeric_limits<Dst>::max)());
        }

        // Unsigned -> Signed conversion
        //   No negative value check needed, but must verify value doesn't exceed signed max
        //   Example: uint32 -> int32, value must be <= 2147483647
        if (!sourceSigned && targetSigned) {
            using U64 = unsigned long long;
            return static_cast<U64>(value) <=
                   static_cast<U64>((std::numeric_limits<Dst>::max)());
        }
    }

    // 3) Integer -> Floating point conversion
    //    Main concern: overflow/underflow of the floating point range
    //    Precision loss is acceptable per ODBC specification
    //    Example: int64 -> float may lose precision but should not overflow
    if (sourceIsInt && targetIsFloat) {
        using LD = long double;
        const LD v    = static_cast<LD>(value);
        const LD tmax = static_cast<LD>((std::numeric_limits<Dst>::max)());
        const LD tmin = -tmax; // symmetric for IEEE-754 float/double

        // Only check for overflow/underflow. Precision loss is allowed per ODBC spec.
        // Example: Large int64 values may lose precision when converted to float,
        //   but as long as magnitude fits within float's range, it's acceptable.
        return v >= tmin && v <= tmax;
    }

    // 4) Floating point -> Floating point conversion
    //    Must handle special values and check range compatibility
    //    Examples:
    //      double -> float: must check if value exceeds float max
    //      float -> double: always safe (widening conversion)
    if (sourceIsFloat && targetIsFloat) {
        if (std::isnan(value)) {
            return false;
        }

        if (std::isinf(value)) {
            // Infinity is representable in all IEEE-754 float/double types.
            // For double -> float, INF stays INF, so it's still representable.
            return true;
        }

        // Check if value fits within target range
        // Allow subnormals: they are representable in IEEE-754.
        // Subnormal numbers (denormalized numbers) can represent values
        // smaller than the normal range, down to approximately min() / 2^(mantissa_bits).
        const auto tmax = (std::numeric_limits<Dst>::max)();
        return value <= static_cast<Src>(tmax) &&
               value >= static_cast<Src>(-tmax);
    }

    // 5) Anything else (unsupported conversion combinations)
    return false;
}

template<typename TargetType, typename SourceType>
TargetType safeNumericCast(SourceType value, bool* success = nullptr) {
    bool inRange = isInRange<TargetType>(value);
    if (success) {
        *success = inRange;
    }

    if (!inRange) {
        // Return a safe default value when out of range
        if (std::is_integral<TargetType>::value) {
            return 0;
        } else {
            return static_cast<TargetType>(0.0);
        }
    }

    return static_cast<TargetType>(value);
}

// Helper function to check for fractional truncation
template<typename FloatType>
bool hasFractionalPart(FloatType value) {
    return value != std::trunc(value);
}

// Helper function for safe conversion with truncation detection
template<typename TargetType, typename SourceType>
SQLRETURN safeConvertWithTruncation(SourceType sourceValue, TargetType* result, 
                                   RS_ERROR_INFO** errorList, bool* hasTruncation = nullptr) {
    if (hasTruncation) *hasTruncation = false;
    bool success;
    *result = safeNumericCast<TargetType>(sourceValue, &success);

    if (!success) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
            addError(errorList, "22003", "Numeric value out of range", 0, NULL);
        }
        return SQL_ERROR;
    }

    // Check for fractional truncation if converting from floating point to integer
    if (std::is_floating_point<SourceType>::value && std::is_integral<TargetType>::value) {
        bool truncated = hasFractionalPart(sourceValue);
        if (hasTruncation) *hasTruncation = truncated;
        if (truncated) {
            if (errorList) {
                RS_LOG_DEBUG("RSUTIL", "01S07: Fractional truncation");
                addError(errorList, "01S07", "Fractional truncation", 0, NULL);
            }
            return SQL_SUCCESS_WITH_INFO;
        }
    }

    return SQL_SUCCESS;
}

// Template function for float/double conversion with special value handling
template<typename T>
void convertFloatValue(long double doubleVal, T* result, bool* success, 
                      RS_ERROR_INFO** pErrorList, const char* typeName) {
    *success = true;

    if (std::isinf(doubleVal)) {
        // Store infinity with correct sign
        *result = doubleVal > 0 ? INFINITY : -INFINITY;
    } else if (std::isnan(doubleVal)) {
        // Store NaN
        *result = NAN;
    } else {
        *result = safeNumericCast<T>(doubleVal, success);
        if (!*success) {
            char errMsg[256];
            snprintf(errMsg, sizeof(errMsg), 
                    "Numeric value out of range for %s", typeName);
            addError(pErrorList, "22003", errMsg, 0, NULL);
            RS_LOG_ERROR("RSUTIL", "22003: %s", errMsg);
        }
    }
}
// Integer types to C integer conversions
SQLRETURN rsIntToTinyint(long long value, signed char* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToUTinyint(long long value, unsigned char* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToShort(long long value, short* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToUShort(long long value, unsigned short* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToInt(long long value, int* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToUInt(long long value, unsigned int* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToBigInt(long long value, long long* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToUBigInt(long long value, unsigned long long* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToFloat(long long value, float* result, RS_ERROR_INFO** errorList);
SQLRETURN rsIntToDouble(long long value, double* result, RS_ERROR_INFO** errorList);
SQLRETURN rsDoubleToFloat(double value, float* result, RS_ERROR_INFO** errorList);

// Float types to Integer conversion and also handle truncation detection
SQLRETURN rsFloatToShort(double value, short* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToUShort(double value, unsigned short* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToTinyInt(double value, signed char* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToUTinyInt(double value, unsigned char* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToInt(double value, int* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToUInt(double value, unsigned int* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToBigInt(double value, long long* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToUBigInt(double value, unsigned long long* result, RS_ERROR_INFO** errorList);

// Special functions for SQL_C_BIT conversions
SQLRETURN rsIntToBit(long long value, unsigned char* result, RS_ERROR_INFO** errorList);
SQLRETURN rsFloatToBit(double value, unsigned char* result, RS_ERROR_INFO** errorList);

// Template functions for generic type conversions 
template<typename TargetType, typename SourceType>
SQLRETURN rsGenericConvert(SourceType value, TargetType* result, RS_ERROR_INFO** errorList) {
    if (!result) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "HY009: Invalid use of null pointer");
            addError(errorList, "HY009", "Invalid use of null pointer", 0, NULL);
        }
        return SQL_ERROR;
    }

    bool success;
    *result = safeNumericCast<TargetType>(value, &success);
    if (!success) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
            addError(errorList, "22003", "Numeric value out of range", 0, NULL);
        }
        return SQL_ERROR;
    }
    return SQL_SUCCESS;
}

// Template function for floating point conversions that need truncation detection
template<typename TargetType>
SQLRETURN rsFloatConvertWithTruncation(double value, TargetType* result, RS_ERROR_INFO** errorList) {
    if (!result) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "HY009: Invalid use of null pointer");
            addError(errorList, "HY009", "Invalid use of null pointer", 0, NULL);
        }
        return SQL_ERROR;
    }

    // Check for NaN and infinity
    if (!std::isfinite(value)) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
            addError(errorList, "22003", "Numeric value out of range", 0, NULL);
        }
        return SQL_ERROR;
    }

    // Check if the value would be truncated when converted to integer
    bool hasTruncation = hasFractionalPart(value);

    // Use the generic conversion for the actual conversion
    bool success;
    *result = safeNumericCast<TargetType>(value, &success);
    if (!success) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
            addError(errorList, "22003", "Numeric value out of range", 0, NULL);
        }
        return SQL_ERROR;
    }

    // If there was a fractional part, add a truncation warning
    if (hasTruncation) {
        if (errorList) {
            RS_LOG_DEBUG("RSUTIL", "01S07: Fractional truncation");
            addError(errorList, "01S07", "Fractional truncation", 0, NULL);
        }
        return SQL_SUCCESS_WITH_INFO;
    }

    return SQL_SUCCESS;
}

// Helper function for calculating numeric buffer length
int calculateMinNumericBufferLength(const char* numStr, int strLen);
#endif /* C++ */
