/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"
#include "rslock.h"
#include "rsversion.h"
#include "rsini.h"
#include "rsexecute.h"
#include "rsmin.h"
#include "rsescapeclause.h"
#include <rsversion.h>
#include <algorithm>
#include <sqlucode.h>
#include <vector>
#include <cctype>
#include <regex>
#include <string>
#include <iostream>
#include <codecvt>
#include <optional>
#include <mutex>
#include <math.h>
#include <float.h>

#ifdef WIN32
#include <winsock.h>
#include <openssl/evp.h>
#endif

#if defined LINUX
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#endif



// Memory profiler. Uncomment it when we like to profile memory leaks.
//#include <vld.h>

//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

#if defined LINUX 
static int gInitGlobalVars = 0;
#endif

static const RS_MAP_SQL_STATE   gMapToODBC2SqlState[] =
{
    { "01001", "01S03"},
    { "01001", "01S04"},
    { "HY019", "22003"},
    { "22007", "22008"},
    { "22018", "22005"},
    { "07005", "24000"},
    { "42000", "37000"},
    { "HY018", "70100"},
    { "42S01", "S0001"},
    { "42S02", "S0002"},
    { "42S11", "S0011"},
    { "42S12", "S0012"},
    { "42S21", "S0021"},
    { "42S22", "S0022"},
    { "42S23", "S0023"},
    { "HY000", "S1000"},
    { "HY001", "S1001"},
    { "HY003", "S1003"},
    { "HY004", "S1004"},
    { "HY008", "S1008"},
    { "HY009", "S1009"},
    { "HY011", "S1011"},
    { "HY012", "S1012"},
    { "HY090", "S1090"},
    { "HY091", "S1091"},
    { "HY092", "S1092"},
    { "HY093", "S1093"},
    { "HY096", "S1096"},
    { "HY097", "S1097"},
    { "HY098", "S1098"},
    { "HY099", "S1099"},
    { "HY100", "S1100"},
    { "HY101", "S1101"},
    { "HY103", "S1103"},
    { "HY104", "S1104"},
    { "HY105", "S1105"},
    { "HY106", "S1106"},
    { "HY107", "S1107"},
    { "HY108", "S1108"},
    { "HY109", "S1109"},
    { "HY110", "S1110"},
    { "HY111", "S1111"},
    { "HYC00", "S1C00"},
    { "HYT00", "S1T00"},
    {"",""}
};

char* sqlTypeNameMap(short value){
#define NAME(TYPE) case TYPE: return #TYPE;
    switch (value) {
        NAME(SQL_CHAR)
        NAME(SQL_VARCHAR)
        NAME(SQL_LONGVARCHAR)
        NAME(SQL_WCHAR)
        NAME(SQL_WVARCHAR)
        NAME(SQL_WLONGVARCHAR)
        NAME(SQL_DECIMAL)
        NAME(SQL_NUMERIC)
        NAME(SQL_BIT)
        NAME(SQL_TINYINT)
        NAME(SQL_SMALLINT)
        NAME(SQL_INTEGER)
        NAME(SQL_BIGINT)
        NAME(SQL_REAL)
        NAME(SQL_FLOAT)
        NAME(SQL_DOUBLE)
        NAME(SQL_BINARY)
        NAME(SQL_VARBINARY)
        NAME(SQL_LONGVARBINARY)
        NAME(SQL_TYPE_DATE)
        NAME(SQL_TYPE_TIME)
        NAME(SQL_TYPE_TIMESTAMP)
        NAME(SQL_DATE)
        NAME(SQL_TIME)
        NAME(SQL_TIMESTAMP)
        NAME(SQL_INTERVAL_YEAR_TO_MONTH)
        NAME(SQL_INTERVAL_DAY_TO_SECOND)
    }
    return "UNKNOWN_SQL_TYPE";
#undef NAME
}

char* cTypeNameMap(short value){
#define NAME(TYPE) case TYPE: return #TYPE;
    switch (value) {
        NAME(SQL_C_CHAR)
        NAME(SQL_C_WCHAR)
        NAME(SQL_C_SHORT)
        NAME(SQL_C_SSHORT)
        NAME(SQL_C_USHORT)
        NAME(SQL_C_LONG)
        NAME(SQL_C_SLONG)
        NAME(SQL_C_ULONG)
        NAME(SQL_C_SBIGINT)
        NAME(SQL_C_UBIGINT)
        NAME(SQL_C_FLOAT)
        NAME(SQL_C_DOUBLE)
        NAME(SQL_C_BIT)
        NAME(SQL_C_TINYINT)
        NAME(SQL_C_STINYINT)
        NAME(SQL_C_UTINYINT)
        NAME(SQL_C_BINARY)
        NAME(SQL_C_TYPE_DATE)
        NAME(SQL_C_TYPE_TIME)
        NAME(SQL_C_TYPE_TIMESTAMP)
        NAME(SQL_C_DATE)
        NAME(SQL_C_TIME)
        NAME(SQL_C_TIMESTAMP)
        NAME(SQL_C_NUMERIC)
        NAME(SQL_C_INTERVAL_YEAR_TO_MONTH)
        NAME(SQL_C_INTERVAL_DAY_TO_SECOND)
        NAME(SQL_C_DEFAULT)
    }
    return "UNKNOWN_C_TYPE";
#undef NAME
}

std::string formatConversionPrefix(short sqlType, short cType, bool sqlToC) {
    std::string sqlTypeName = sqlTypeNameMap(sqlType);
    std::string cTypeName = cTypeNameMap(cType);

    return sqlToC ? "[" + sqlTypeName + " - " + cTypeName + "]"
                  : "[" + cTypeName + " - " + sqlTypeName + "]";
}

#ifdef __cplusplus
extern "C" {
#endif

void resetCscStatementConext(void *_pCscStatementContext);
int isEndOfStreamingCursor(void *_pCscStatementContext);
int isEndOfStreamingCursorQuery(void *_pCscStatementContext);
int getStreamingCursorBatchNumber(void *_pCscStatementContext);
void resetStreamingCursorBatchNumber(void *_pCscStatementContext);
unsigned char hex_to_binary(char in_hex);
#ifdef __cplusplus
}
#endif


/*====================================================================================================================================================*/

/*void dumpMemLeak()
{
    _CrtDumpMemoryLeaks();
}*/

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Duplicate the string.
//
char *rs_strdup(const char *src, size_t cbLen)
{
    char *dest;

    if(src)
    {
        size_t len = (INT_LEN(cbLen) == SQL_NTS) ? strlen(src) + 1 : ((INT_LEN(cbLen) == SQL_NULL_DATA) ? 1 : cbLen + 1);
        dest = (char *)rs_malloc(sizeof(char) * len);
        if(dest)
        {
          if(INT_LEN(cbLen) == SQL_NTS)
            rs_strncpy(dest,src,len);
          else
          if(INT_LEN(cbLen) == SQL_NULL_DATA)
          {
              dest[0] = '\0';
          }
          else
          {
            memcpy(dest,src,cbLen);
            dest[cbLen] = '\0';
          }
        }
    }
    else
        dest = NULL;

    return dest;
}

/**
 * Creates a null-terminated string from potentially non-null-terminated input
 * 
 * @param pData   Input string data (may not be null-terminated)
 * @param cbLen   Length of data in bytes, or SQL_NTS if already null-terminated, or SQL_NULL_DATA if null.
 *                 If SQL_NTS, pData is assumed to be null-terminated and assigned to RS_STR_BUF as is.
 *                 If SQL_NULL_DATA, pData is ignored and NULL is returned.
 * @param pPaStrBuf Buffer management structure. If provided, will be used to track allocated memory.
 *                  If null, memory will be allocated but caller is responsible for freeing it.
 * 
 * @return Pointer to null-terminated string, or NULL on error/null input.
 *         If pPaStrBuf is NULL, caller must free the returned pointer (if not NULL).
 */
unsigned char *makeNullTerminatedStr(char *pData, int64_t cbLen, RS_STR_BUF *pPaStrBuf)
{
    // Initialize buffer management structure if provided
    resetPaStrBuf(pPaStrBuf);
    
    // Handle NULL data or SQL_NULL_DATA
    if (pData == NULL || cbLen == SQL_NULL_DATA) {
        return NULL;
    }

    // Already null-terminated string case
    if (INT_LEN(cbLen) == SQL_NTS) {
        if (pPaStrBuf) pPaStrBuf->pBuf = pData;
        return (unsigned char *)pData;
    } 
    // Invalid length case
    else if (cbLen < 0) {
        return NULL;
    }
    // Need to create null-terminated copy
    else {
        // Use rs_strnlen to safely find actual string length without reading beyond cbLen
        size_t actualLen = rs_strnlen(pData, cbLen);
        unsigned char *szData = NULL;

        // Check for integer overflow in allocation size
        if (actualLen > SIZE_MAX - 1) {
            return NULL;
        }

        if (pPaStrBuf) {
            if (actualLen > SHORT_STR_DATA) {
                // Need to dynamically allocate memory
                pPaStrBuf->pBuf = (char *)rs_malloc(actualLen + 1);
                if (!pPaStrBuf->pBuf) return NULL; // Check allocation success
                
                szData = (unsigned char *)(pPaStrBuf->pBuf);
                pPaStrBuf->iAllocDataLen = (int)actualLen;
            } else {
                // Use the built-in buffer for small strings
                pPaStrBuf->pBuf = pPaStrBuf->buf;
                szData = (unsigned char *)(pPaStrBuf->pBuf);
                pPaStrBuf->iAllocDataLen = 0;
            }
        } else {
            // No buffer management provided, allocate directly
            szData = (unsigned char *)rs_malloc(actualLen + 1);
            if (!szData) return NULL; // Check allocation success
        }
        
        // Copy the data and null-terminate
        memcpy(szData, pData, actualLen);
        szData[actualLen] = '\0';
        return szData;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add connection in the list.
//
void addConnection(RS_ENV_INFO *pEnv, RS_CONN_INFO *pConn)
{
    // Put HDBC in front in HENV list
    pConn->pNext = pEnv->phdbcHead;
    pEnv->phdbcHead = pConn;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Remove connection from the list.
//
void removeConnection(RS_CONN_INFO *pConn)
{
    RS_ENV_INFO *pEnv;
    RS_CONN_INFO *curr;
    RS_CONN_INFO *prev;

    // Remove from HENV list
    pEnv = pConn->phenv;
    curr = pEnv->phdbcHead;
    prev = NULL;

    while(curr != NULL)
    {
        if(curr == pConn)
        {
            if(prev == NULL)
                pEnv->phdbcHead = pEnv->phdbcHead->pNext;
            else
                prev->pNext = curr->pNext;

            curr->pNext = NULL;

            break;
        }

        prev = curr;
        curr = curr->pNext;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add statement in the list.
//
void addStatement(RS_CONN_INFO *pConn, RS_STMT_INFO *pStmt)
{
    // Put HSTMT in front in HDBC list
    pStmt->pNext = pConn->phstmtHead;
    pConn->phstmtHead = pStmt;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Remove statement from the list.
//
void removeStatement(RS_STMT_INFO *pStmt)
{
    RS_CONN_INFO *pConn;
    RS_STMT_INFO *curr;
    RS_STMT_INFO *prev;

    // Remove from HDBC list
    pConn = pStmt->phdbc;
    curr  = pConn->phstmtHead;
    prev  = NULL;

    while(curr != NULL)
    {
        if(curr == pStmt)
        {
            if(prev == NULL)
                pConn->phstmtHead = pConn->phstmtHead->pNext;
            else
                prev->pNext = curr->pNext;

            curr->pNext = NULL;

            break;
        }

        prev = curr;
        curr = curr->pNext;
    }
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Clear bind column list.
//
void clearBindColList(RS_DESC_INFO *pARD)
{
    // Release ARD recs
    if (pARD) {
        pARD->pDescHeader.hHighestCount = 0;
    }
    releaseDescriptorRecs(pARD);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get specified error record from the error list. If remove=1 then remove it from the list.
//
RS_ERROR_INFO *getNextError(RS_ERROR_INFO **ppErrorList, SQLSMALLINT recNumber, int remove)
{
    RS_ERROR_INFO *pError = NULL;
    RS_ERROR_INFO *pPrevError = NULL;

    if(ppErrorList != NULL && *ppErrorList != NULL)
    {
        while(recNumber--)
        {
            pPrevError = pError;
            pError = (pError) ? pError->pNext : *ppErrorList;

            if(!pError)
                break;
        }

        if(remove && pError)
        {
            if(pPrevError)
            {
                pPrevError->pNext = pError->pNext;
            }
            else
            {
                *ppErrorList = (*ppErrorList)->pNext;
            }
        }
    }
    else
        pError = NULL;

    return pError;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get total error count in the list.
//
int getTotalErrors(RS_ERROR_INFO *pErrorList)
{
    RS_ERROR_INFO *pError = pErrorList;
    int count = 0;

    while(pError)
    {
        pError =  pError->pNext;
        count++;
    }

    return count;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Clear error list.
//
RS_ERROR_INFO * clearErrorList(RS_ERROR_INFO *pErrorList)
{
    RS_ERROR_INFO *pError;

    for(pError = pErrorList; pError != NULL; pError = pErrorList)
    {
        pErrorList = pErrorList->pNext;
        delete pError;
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add warning in the list.
//
void addWarning(RS_ERROR_INFO **ppErrorList, char *pSqlState, char *pMsg, long nativeError, RS_CONN_INFO *pConn)
{
    addError(ppErrorList, pSqlState, pMsg, nativeError, pConn);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add error in the list.
//
void addError(RS_ERROR_INFO **ppErrorList, char *pSqlState, char *pMsg, long nativeError, RS_CONN_INFO *pConn)
{
    RS_ERROR_INFO *pError = (RS_ERROR_INFO *)new RS_ERROR_INFO();
    char *pOdbcErrPrefix = (char *)((pConn) ? PADB_ERROR_PREFIX : ODBC_DRIVER_ERROR_PREFIX);
    char *pNativeSqlState = libpqGetNativeSqlState(pConn);

    if(pNativeSqlState != NULL && *pNativeSqlState != '\0')
        rs_strncpy_safe(pError->szSqlState,pNativeSqlState,MAX_SQL_STATE_LEN);
    else
    if(pSqlState != NULL)
        rs_strncpy_safe(pError->szSqlState,pSqlState,MAX_SQL_STATE_LEN);
    else
        pError->szSqlState[0] = '\0';

    if(pMsg != NULL)
    {
        size_t prefixMsgLen = strlen(pOdbcErrPrefix);
        int maxMsgLen;

        if(pNativeSqlState && *pNativeSqlState != '\0')
        {
            prefixMsgLen += (MAX_SQL_STATE_LEN - 1) + 1; // Last +1 for :
            maxMsgLen = (int)(MAX_ERR_MSG_LEN-1-prefixMsgLen); 
            snprintf(pError->szErrMsg, sizeof(pError->szErrMsg),"%s%s:%.*s",pOdbcErrPrefix,pNativeSqlState, maxMsgLen,pMsg);
        }
        else
        {
            maxMsgLen = (int)(MAX_ERR_MSG_LEN-1-prefixMsgLen); 
            snprintf(pError->szErrMsg, sizeof(pError->szErrMsg), "%s%.*s",pOdbcErrPrefix, maxMsgLen,pMsg);
        }
    }

    pError->lNativeErrCode = nativeError;

    // Put Error in front of a list
    pError->pNext = *ppErrorList;
    *ppErrorList = pError;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize global variables.
//
void initGlobals(HMODULE hModule)
{
    gRsGlobalVars.hModule = hModule;
    getGlobalLogVars()->iTraceLevel = DEFAULT_TRACE_LEVEL;
    gRsGlobalVars.hApiMutex = rsCreateMutex();
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release global variables.
//
void releaseGlobals()
{
    gRsGlobalVars.hModule = NULL;
    getGlobalLogVars()->iTraceLevel = 0;
    rsDestroyMutex(gRsGlobalVars.hApiMutex);
    gRsGlobalVars.hApiMutex = NULL;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Trim leading and trailing white spaces.
//
const char* WHITE_SPACES = " \t\n\r\f\v";

// trim from end of string (right)
std::string& rtrim(std::string& s)
{
    s.erase(s.find_last_not_of(WHITE_SPACES) + 1);
    return s;
}

// trim from beginning of string (left)
std::string& ltrim(std::string& s)
{
    s.erase(0, s.find_first_not_of(WHITE_SPACES));
    return s;
}

// trim from both ends of string (right then left)
std::string& trim(std::string& s)
{
    return ltrim(rtrim(s));
}

char *trim_whitespaces(char *str) 
{ 
  char *end;
  char *temp;
 
  // Trim leading space 
  while(isspace(*str) && *str) 
      str++; 
 
  if(*str == '\0')  // All spaces? 
    return str; 
 
  // Trim trailing space 
  end = str + strlen(str) - 1; 
  temp = end;
  while(temp > str && isspace(*temp)) 
      temp--; 
 
  // Write new null terminator 
  if(temp != end)
    *(temp+1) = '\0'; 
 
  return str; 
} 

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Append the string and if needed allocate the memory.
//
char *appendStr(char *pStrOut, size_t *pcbStrOut,char *szStrIn)
{
    if(szStrIn)
    {
        size_t len = strlen(szStrIn);
        size_t len1 =  (pStrOut) ? strlen(pStrOut) : 0;
        
        if((len + len1) >= *pcbStrOut) 
        {
            char *temp;
            len = (len + len1 + 1); //  '\0'
            temp = (char *)rs_malloc(len);

            if(pStrOut)
            {
                rs_strncpy(temp,pStrOut,len);
                pStrOut = (char *)rs_free(pStrOut);
            }
            else
                temp[0] = '\0';

            pStrOut = temp;
            *pcbStrOut = len;
            temp = NULL;
        }

        strncat(pStrOut, szStrIn,*pcbStrOut);
    }

    return pStrOut;
}

/*====================================================================================================================================================*/
// search for case-insensitive and whole word substring in a string
char* strcasestrwhole(const char* str, const char* substr) {
    if (str == nullptr || substr == nullptr) {
        return nullptr;
    }

    //to comply with strcasestr behavior:
    if (strlen(substr) == 0 && strlen(str) == 0) {
        return (char*)str;
    }
    if (strlen(str) == 0) {
        return nullptr;
    }

    // Convert char* to std::string
    std::string s(str);
    std::string sub(substr);

    // Construct regex pattern for whole-word, case-insensitive match
    std::string pattern = "\\b" + sub + "\\b";
    std::regex wordRegex(pattern, std::regex_constants::icase);

    // Perform regex search
    std::smatch match;
    if (std::regex_search(s, match, wordRegex)) {
        // Find the position of the match within the original char* string
        size_t pos = match.position(0);
        auto t = str + pos;
        return (char*)(str + pos);
    } else {
        return nullptr;
    }
}

#ifdef WIN32
// Custom strcasestr implementation for cross-platform support
char *strcasestr(const char *str, const char *subStr) {
    if (str == nullptr || subStr == nullptr) return nullptr;

    size_t subStrLen = strlen(subStr);
    if (subStrLen == 0) return (char *)str;
    char *ptr = (char *)str;
    while (*ptr) {
        if (_strnicmp (ptr, subStr, subStrLen) == 0) {
            return ptr;
        }
        ptr++;
    }
    return nullptr;
}
#endif

//---------------------------------------------------------------------------------------------------------igarish
// Case insensitive strstr.
//
char *stristr(const char *str, const char *subStr)
{
    if (str == nullptr || subStr == nullptr) {
        return nullptr;
    }
    return strcasestr((char*)str, (char*)subStr);
}

// Helper function to trim whitespace from both ends
// Returns true if string becomes empty after trimming
bool trimWhitespace(const char **startPtr, const char **endPtr) {
    // Trim leading whitespace
    while (*startPtr < *endPtr && isspace((unsigned char)**startPtr)) {
        ++(*startPtr);
    }

    // Trim trailing whitespace
    while (*endPtr > *startPtr && isspace((unsigned char)(*endPtr)[-1])) {
        --(*endPtr);
    }

    return *startPtr >= *endPtr;
}

// Helper function to parse scientific notation exponent (e.g., "e5", "E-3",
// "e+10") Expected format: [eE][+-]?[0-9]+
//
// @param currentPos    [IN/OUT] Pointer to current parsing position, updated
// after parsing
// @param endPtr        [IN] End of string boundary for safe parsing
// @param exponent      [OUT] Parsed exponent value (0 if no exponent found)
// @return true on success, false on invalid format
ParseReturnCode parseExponent(const char **currentPos, const char *endPtr, int *exponent) {
    if (*currentPos >= endPtr || (**currentPos != 'e' && **currentPos != 'E')) {
        *exponent = 0;
        return PARSE_SUCCESS; // No exponent is valid
    }

    ++(*currentPos); // Skip 'e' or 'E'

    // Parse optional sign
    int expSign = 1;
    if (*currentPos < endPtr && (**currentPos == '+' || **currentPos == '-')) {
        expSign = (**currentPos == '-') ? -1 : 1;
        ++(*currentPos);
    }

    // Validate at least one digit follows
    if (*currentPos >= endPtr || **currentPos < '0' || **currentPos > '9') {
        return PARSE_INVALID_FORMAT; // Invalid: 'e' without digits
    }

    // Parse exponent digits with overflow protection
    int exp = 0;
    int maxAllowed = (expSign == -1) ? (unsigned int)(-SHRT_MIN) : SHRT_MAX;

    while (*currentPos < endPtr && **currentPos >= '0' && **currentPos <= '9') {
        int digit = **currentPos - '0';
        if (exp > (maxAllowed - digit) / 10) {
            return PARSE_OVERFLOW;
        }
        exp = exp * 10 + digit;
        ++(*currentPos);
    }

    *exponent = expSign * exp;
    return PARSE_SUCCESS;
}

/**
 * @brief Determines if a year is a leap year.
 */
bool isLeapYear(int year) {
    // Handle BC leap year
    if (year <= 0) {
        ++year;
    }

    if ((year % 4) == 0) {
        if ((year % 100) == 0) {
            return ((year % 400) == 0);
        }
        return true;
    }
    return false;
}

// Get days for given month and year. Assumes that the year and month are valid values (1-12 range)
static int getDaysInMonth(int month, int year) {
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && isLeapYear(year)) {
        return 29;
    }

    return daysInMonth[month - 1];
}

bool validateDate(int year, int month, int day) {
    if (year == 0 || (MIN_DATE_YEAR > year) || (MAX_DATE_YEAR < year) ||
        (MIN_DATE_MONTH > month) || (MAX_DATE_MONTH < month) ||
        (MIN_DATE_DAY > day) ) {
        return false;
    }

    if (getDaysInMonth(month, year) < day) {
        return false;
    }

    return true;
}

bool validateTime(int hour, int minute, int second, int fraction) {
    if ((MIN_TIME_HOUR > hour) || (MAX_TIME_HOUR < hour)) {
        return false;
    }

    if ((MIN_TIME_MINUTE > minute) || (MAX_TIME_MINUTE < minute)) {
        return false;
    }

    if ((MIN_TIME_SECOND > second) || (MAX_TIME_SECOND < second)) {
        return false;
    }

    if (fraction < 0 || fraction > MAX_TIME_FRACTION_NANOSECOND) {
        return false;
    }

    return true;
}

bool validateDateTime(int year, int month, int day, int hour, int minute,
                      int second, int fraction) {
    return validateDate(year, month, day) &&
           validateTime(hour, minute, second, fraction);
}
//---------------------------------------------------------------------------------------------------------igarish
// Copy small string data.
//
SQLRETURN copyStrDataSmallLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen, RS_ERROR_INFO **ppErrorList)
{
    SQLRETURN rc = SQL_SUCCESS;
    int len = (pSrc && (iSrcLen != SQL_NULL_DATA)) 
                ? ((iSrcLen == SQL_NTS) ? (int) strlen(pSrc) : iSrcLen) 
                : 0;

    if(pDest != NULL)
    {
        if(len > 0)
        {
            if(len < cbLen)
            {
                strncpy(pDest, pSrc, len);
                pDest[len] = '\0';
            }
            else
            {
                if(cbLen > 0)
                {
                    strncpy(pDest,pSrc, cbLen-1);
                    pDest[cbLen-1] = '\0';
                    rc = SQL_SUCCESS_WITH_INFO;
                }
                else
                    rc = SQL_SUCCESS_WITH_INFO;

                if(ppErrorList) {
                    addWarning(ppErrorList, "01004", "String data, right truncation", 0, NULL);
                }
            }
        }
        else
        {
            if(cbLen > 0)
                pDest[0] = '\0';
        }
    }
    else
    {
        rc = SQL_SUCCESS_WITH_INFO;

        if(ppErrorList) {
            addWarning(ppErrorList, "01004", "String data, right truncation", 0, NULL);
        }
    }

    if(pcbLen != NULL)
        *pcbLen = len;
    else
        rc = SQL_SUCCESS;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy large string data.
//
SQLRETURN copyStrDataLargeLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest,
                              SQLINTEGER cbLen, SQLINTEGER *pcbLen) {
    SQLRETURN rc = SQL_SUCCESS;
    int len = (pSrc && (iSrcLen != SQL_NULL_DATA))
                  ? ((iSrcLen == SQL_NTS) ? (int)strlen(pSrc) : iSrcLen)
                  : 0;

    if (pDest != NULL) {
        if (len > 0) {
            if (len < cbLen) {
                strncpy(pDest, pSrc, len);
                pDest[len] = '\0';
            } else {
                if (cbLen > 0) {
                    strncpy(pDest, pSrc, cbLen - 1);
                    pDest[cbLen - 1] = '\0';
                }
                rc = SQL_SUCCESS_WITH_INFO;
            }
        } else {
            if (cbLen > 0)
                pDest[0] = '\0';
        }
    }

    if (pcbLen != NULL)
        *pcbLen = len;

    return rc;
}
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy big string data.
//
SQLRETURN copyStrDataBigLen(RS_STMT_INFO *pStmt, const char *pSrc,
                            SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen,
                            SQLLEN *cbLenOffset, SQLLEN *pcbLenInd) {
    // TODO: Move the check for cbLen and pDest to SQLGetData API
    if (cbLen < 0) {
        if (pStmt) {
            addError(&pStmt->pErrorList, "HY090",
                     "Invalid buffer length specified (length must be greater "
                     "than or equal to 0) ",
                     0, NULL);
        }
        RS_LOG_ERROR("RSUTIL", "Invalid buffer length specified (length "
                               "must be greater than or equal to 0) ");
        return SQL_ERROR;
    }

    if (pDest == NULL) {
        if (pStmt) {
            addError(
                &pStmt->pErrorList, "HY009",
                "Invalid buffer pointer: NULL pointer provided for data buffer",
                0, NULL);
        }
        RS_LOG_ERROR("RSUTIL", "Invalid buffer pointer: NULL pointer "
                               "provided for data buffer");
        return SQL_ERROR;
    }

    SQLRETURN rc = SQL_SUCCESS;

    // Calculate the length of source data
    int len = (pSrc && (iSrcLen != SQL_NULL_DATA))
                  ? ((iSrcLen == SQL_NTS) ? (int)strlen(pSrc) : iSrcLen)
                  : 0;
    // Handle case where source is NULL or NULL data is being returned
    if (pSrc == NULL || iSrcLen == SQL_NULL_DATA) {
        if (cbLenOffset) {
            *cbLenOffset = 0; // Reset offset for next column
        }
        if (pcbLenInd) {
            *pcbLenInd = SQL_NULL_DATA;
        }
        return SQL_SUCCESS;
    }

    // Handle the case where source is empty string
    if (len == 0) {
        if (cbLenOffset) {
            *cbLenOffset = 0;
        }
        if (pcbLenInd) {
            *pcbLenInd = 0; // empty data
        }
        // if valid buffer length is provided add a null terminator
        if (cbLen > 0) {
            pDest[0] = '\0';
            return SQL_SUCCESS;
        }
        // not able to add a null terminator for empty string
        if (pStmt) {
            addWarning(&pStmt->pErrorList, "01004",
                       "String data, right truncation occurred: Buffer too "
                       "small to hold the entire data.",
                       0, NULL);
        }
        RS_LOG_DEBUG("RSUTIL", "String data, right truncation occurred: Buffer "
                               "too small to hold the entire data.");
        return SQL_SUCCESS_WITH_INFO;
    }

    // Get current fetch position for sequential fetches
    int currentOffset = (cbLenOffset) ? *cbLenOffset : 0;

    // Check if we've already fetched everything
    if (currentOffset >= len) {
        if (cbLenOffset) {
            *cbLenOffset = 0; // Reset offset for next column
        }
        return SQL_SUCCESS;
    }

    // Calculate remaining data and how much we can copy now
    int remainingLen = len - currentOffset;

    // Set the indicator to length of data available at the start of current
    // call
    if (pcbLenInd) {
        *pcbLenInd = remainingLen;
    }

    // Calculate how many bytes to copy, ensuring:
    // 1. We only copy if we have a valid destination buffer (cbLen > 0)
    // 2. We leave room for the null terminator (cbLen - 1)
    // 3. We don't copy more than the remaining data (MIN with remainingLen)
    // 4. We never try to copy a negative amount (MAX with 0)
    int copyLen = (cbLen > 0) ? MAX(0, MIN(remainingLen, (int)(cbLen - 1))) : 0;

    // Copy data if destination buffer is provided
    if (cbLen > 0) {
        if (copyLen > 0) {
            memcpy(pDest, pSrc + currentOffset, copyLen);
        }
        pDest[MIN(copyLen, (int)(cbLen - 1))] =
            '\0'; // always null-terminate within bounds
    }

    // Handle sequential fetch offset updating
    if (cbLenOffset) {
        if (remainingLen > copyLen) {
            // More data remains, update offset and signal truncation
            *cbLenOffset += copyLen;
            if (pStmt) {
                addWarning(&pStmt->pErrorList, "01004",
                           "String data, right truncation occurred: Buffer too "
                           "small to hold the entire data.",
                           0, NULL);
            }
            RS_LOG_DEBUG("RSUTIL",
                         "String data, right truncation occurred: Buffer "
                         "too small to hold the entire data.");
            return SQL_SUCCESS_WITH_INFO;
        } else {
            // All data fetched, reset offset
            *cbLenOffset = 0;
            return SQL_SUCCESS;
        }
    } else {
        // Not using sequential fetches
        if (remainingLen > copyLen && cbLen > 0) {
            // Truncation occurred
            if (pStmt) {
                addWarning(&pStmt->pErrorList, "01004",
                           "String data, right truncation occurred: Buffer too "
                           "small to hold the entire data.",
                           0, NULL);
            }
            RS_LOG_DEBUG("RSUTIL",
                         "String data, right truncation occurred: Buffer "
                         "too small to hold the entire data.");
            return SQL_SUCCESS_WITH_INFO;
        }
    }

    // If no destination was provided, signal truncation
    if (cbLen == 0) {
        // Truncation occurred
        if (pStmt) {
            addWarning(&pStmt->pErrorList, "01004",
                       "String data, right truncation occurred: Buffer too "
                       "small to hold the entire data.",
                       0, NULL);
        }
        RS_LOG_DEBUG("RSUTIL", "String data, right truncation occurred: Buffer "
                               "too small to hold the entire data.");
        return SQL_SUCCESS_WITH_INFO;
    }

    return SQL_SUCCESS;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy big binary data.
//
SQLRETURN copyBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen)
{
	SQLRETURN rc = SQL_SUCCESS;
	int len = (pSrc && (iSrcLen != SQL_NULL_DATA))
		? iSrcLen
		: 0;

	if (pDest != NULL)
	{
		if (len > 0)
		{
			if (len < cbLen)
			{
				memcpy(pDest, pSrc, len);
				pDest[len] = '\0';
			}
			else
			{
				if (cbLen > 0)
				{
					memcpy(pDest, pSrc, cbLen - 1);
					pDest[cbLen - 1] = '\0';
					rc = SQL_SUCCESS_WITH_INFO;
				}
				else
					rc = SQL_SUCCESS_WITH_INFO;
			}
		}
		else
		{
			if (cbLen > 0)
				pDest[0] = '\0'; // Indicates NULL data.
		}
	}
	else
		rc = SQL_SUCCESS_WITH_INFO;

	if (pcbLen != NULL)
		*pcbLen = len;
	else
		rc = SQL_SUCCESS;

	return rc;
}

/*====================================================================================================================================================*/

unsigned char hex_to_binary(char in_hex)
{
	int temp = toupper(in_hex);

	if ((65 <= temp) && (70 >= temp))
	{
		// Convert hex characters "A" to "F".
		return (unsigned char)(temp - 55);
	}
	else if ((48 <= temp) && (57 >= temp))
	{
		// Convert hex characters "0" to "9".
		return (unsigned char)(temp - 48);
	}

	// Error
	return 0;
}

/*====================================================================================================================================================*/

SQLRETURN copyHexToBinaryDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen, SQLLEN *cbLenOffset)
{
	SQLRETURN rc = SQL_SUCCESS;

    if (*cbLenOffset != 0) {
        int hexBatchPos = *cbLenOffset * 2;
        pSrc += hexBatchPos;
        iSrcLen -= hexBatchPos;
    }

	int len = (pSrc && (iSrcLen != SQL_NULL_DATA))
		? iSrcLen
		: 0;

	if (iSrcLen & 1)
	{
		// Disallow odd numbers of bytes.
		rc = SQL_ERROR;
	}
	else
	{
		if (pDest != NULL)
		{
			if (len > 0)
			{
				int output_len = 0;

				len = iSrcLen / 2;

				if (len <= cbLen)
					output_len = len;
				else
				{
					output_len = cbLen;
					rc = SQL_SUCCESS_WITH_INFO;
				}

				for (int outputIndex = 0; outputIndex < output_len; ++outputIndex)
				{
					// 4 higher-order bits
					unsigned char higherOrder = hex_to_binary(*pSrc++);

					// 4 lower-order bits
					unsigned char lowerOrder = hex_to_binary(*pSrc++);

					pDest[outputIndex] = ((higherOrder << 4) | lowerOrder);
				}
			}
			else
			{
				if (cbLen > 0)
					pDest[0] = '\0'; // Indicates NULL data.
			}
		}
		else
			rc = SQL_SUCCESS_WITH_INFO;
	}

	if (pcbLen != NULL)
		*pcbLen = len;
	else
		rc = SQL_SUCCESS;

    if (rc == SQL_SUCCESS_WITH_INFO) {
        *cbLenOffset += cbLen;
    } else {
        *cbLenOffset = 0;
    }

	return rc;
}

/*====================================================================================================================================================*/

SQLRETURN copyBinaryToHexDataBigLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLLEN cbLen, SQLLEN *pcbLen)
{
	SQLRETURN rc = SQL_SUCCESS;
	int len = (pSrc && (iSrcLen != SQL_NULL_DATA))
		? iSrcLen
		: 0;
	const char * hex = "0123456789ABCDEF";

	if (cbLen & 1)
	{
		// Reduce the size by 1 to make it even
		cbLen--;
	}

	if (pDest != NULL)
	{
		if (len > 0)
		{
			int output_len = 0;

			len = iSrcLen * 2;

			if (len < cbLen)
				output_len = len;
			else
			{
				output_len = cbLen;
				rc = SQL_SUCCESS_WITH_INFO;
			}

			for (int outputIndex = 0; outputIndex < output_len; )
			{
				pDest[outputIndex++] = hex[(*pSrc >> 4) & 0xF];
				pDest[outputIndex++] = hex[*pSrc & 0xF];
				pSrc++;
			}

			pDest[output_len] = '\0'; // Null terminate the data
		}
		else
		{
			if (cbLen > 0)
				pDest[0] = '\0'; // Indicates NULL data.
		}
	}
	else
		rc = SQL_SUCCESS_WITH_INFO;

	if (pcbLen != NULL)
		*pcbLen = len;
	else
		rc = SQL_SUCCESS;

	return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy UTF-8 source string to wide character (SQLWCHAR) destination buffer with support for sequential fetches.
//
// Converts UTF-8 encoded source data to platform-specific wide character format (UTF-16 on Windows, UTF-32 on Linux).
// Supports partial data retrieval through sequential fetches using cbLenOffset.
//
// Parameters:
//   pStmt        - Statement handle for error reporting (can be NULL)
//   pSrc         - Source UTF-8 string buffer
//   iSrcLen      - Source length in bytes, or SQL_NTS if null-terminated, or SQL_NULL_DATA for NULL
//   pDest        - Destination wide character buffer (must not be NULL)
//   cbLen        - Destination buffer size in bytes (must be >= 0)
//   cbLenOffset  - [in/out] Character offset for sequential fetches; reset to 0 when complete
//   pcbLenInd    - [out] Bytes available at start of call, or SQL_NULL_DATA for NULL
//
// Returns:
//   SQL_SUCCESS           - All data copied successfully
//   SQL_SUCCESS_WITH_INFO - Data truncated (01004 warning added)
//   SQL_ERROR             - Invalid parameters (HY090 or HY009 error added)
//
// Notes:
//   - All length parameters (cbLen, pcbLenInd) are in bytes, not characters
//   - Destination buffer is always null-terminated when cchLen > 0
//   - Allocates temporary buffer for conversion; caller need not free
//   - Sequential fetches: cbLenOffset tracks position across multiple calls
SQLRETURN copyWStrDataBigLen(RS_STMT_INFO *pStmt, const char *pSrc,
                             SQLINTEGER iSrcLen, SQLWCHAR *pDest, SQLLEN cbLen,
                             SQLLEN *cbLenOffset, SQLLEN *pcbLenInd) {

// 1024 characters * max 4 bytes per UTF-8 char
#define MAX_LOG_STRING_LENGTH 1024 * 4
    auto contextCreator = [&]() -> std::string {
        // This lambda will only generate information only when needed,
        // minimizing overhead
        std::ostringstream oss;
        auto pSrcLen = rs_strnlen(pSrc, MAX_LOG_STRING_LENGTH);
        oss << "pStmt=" << pStmt << ", iSrcLen=" << iSrcLen
            << ", cbLen=" << cbLen
            << ", cbLenOffset=" << ((cbLenOffset != NULL) ? *cbLenOffset : -999)
            << ", pcbLenInd=" << pcbLenInd << ", strlen(pSrc)=" << pSrcLen;
        return oss.str();
    };
    // Sanity checks
    if (iSrcLen < 0 && iSrcLen != SQL_NTS && iSrcLen != SQL_NULL_DATA) {
        if (pStmt) {
            addError(&pStmt->pErrorList, "HY090", "Invalid string length.", 0,
                     NULL);
        }
        auto context = contextCreator();
        RS_LOG_ERROR("RSUTIL", "Invalid string length. Context: %s",
                     context.c_str());
        return SQL_ERROR;
    }

    if (cbLen < 0) {
        if (pStmt) {
            addError(&pStmt->pErrorList, "HY090",
                     "Invalid buffer length specified (length must be greater "
                     "than or equal to 0) ",
                     0, NULL);
        }
        auto context = contextCreator();
        RS_LOG_ERROR("RSUTIL",
                     "Invalid buffer length specified (length "
                     "must be greater than or equal to 0). Context: %s",
                     context.c_str());
        return SQL_ERROR;
    }
    if (pDest == NULL) {
        if (pStmt) {
            addError(
                &pStmt->pErrorList, "HY009",
                "Invalid buffer pointer: NULL pointer provided for data buffer",
                0, NULL);
        }
        RS_LOG_ERROR("RSUTIL", "Invalid buffer pointer: NULL pointer "
                               "provided for data buffer");
        return SQL_ERROR;
    }

    SQLRETURN rc = SQL_SUCCESS;

    // Calculate the length of source data in bytes
    int bytesLen = (pSrc && (iSrcLen != SQL_NULL_DATA))
                       ? ((iSrcLen == SQL_NTS) ? (int)strlen(pSrc) : iSrcLen)
                       : 0;

    // Maximum number of wide characters the buffer can hold
    int cchLen = (int)(cbLen / sizeofSQLWCHAR());

    // Handle NULL data case
    if (pSrc == NULL || iSrcLen == SQL_NULL_DATA) {
        if (cbLenOffset) {
            *cbLenOffset = 0;
        }

        if (pcbLenInd) {
            *pcbLenInd = SQL_NULL_DATA;
        }
        return SQL_SUCCESS;
    }

    // Handle the case where source is empty string
    if (bytesLen == 0) {
        if (cbLenOffset) {
            *cbLenOffset = 0;
        }

        if (pcbLenInd) {
            *pcbLenInd = 0; // Empty string has length 0
        }
        if (cchLen > 0) {
            setFirstSqlwcharNull(pDest);
            // Return SUCCESS even with zero buffer
            return SQL_SUCCESS;
        }
        // We can't even fit NULL
        if (pStmt) {
            addWarning(&pStmt->pErrorList, "01004",
                       "String data, right truncation occurred: Buffer too "
                       "small to hold the entire data.",
                       0, NULL);
        }
        auto context = contextCreator();
        RS_LOG_DEBUG("RSUTIL",
                     "String data, right truncation occurred: Buffer "
                     "too small to hold the entire data. Context: %s",
                     context.c_str());
        return SQL_SUCCESS_WITH_INFO;
    }

    // Determine character count and length after conversion
    static const size_t SHORT_BUFFER_LENGTH = 256;
    const int ut = get_app_unicode_type();

    union {
        uint16_t u16[SHORT_BUFFER_LENGTH];
        uint32_t u32[SHORT_BUFFER_LENGTH];
    } sbuf; // lives to end of function

    SQLWCHAR *wcharStr = NULL;
    size_t wcharLen = 0;
    bool useHeap = false;

    const bool canUseStack =
        (bytesLen >= 0) && ((size_t)bytesLen + 1 <= SHORT_BUFFER_LENGTH);

    if (canUseStack) {
        if (ut == SQL_DD_CP_UTF32) {
            wcharStr = (SQLWCHAR *)sbuf.u32; // 256 UTF-32 units
        } else {
            wcharStr = (SQLWCHAR *)sbuf.u16; // 256 UTF-16 units
        }
        wcharLen =
            utf8_to_sqlwchar_str(pSrc, bytesLen, wcharStr, SHORT_BUFFER_LENGTH,
                                 /*totalNeeded*/ NULL, ut);
    } else {
        wcharStr = NULL;
        wcharLen = utf8_to_sqlwchar_alloc(pSrc, bytesLen, &wcharStr, ut);
        useHeap = true;
    }

    // Detect conversion failure for both heap and stack paths:
    // - heap path: wcharStr == nullptr on failure
    // - stack path: wcharStr != nullptr but wcharLen == 0 on failure (with
    // bytesLen > 0)
    if ((bytesLen > 0) && (!wcharStr || wcharLen == 0)) {
        // Free only if heap-allocated
        if (useHeap && wcharStr) {
            wcharStr = (SQLWCHAR *)rs_free(wcharStr);
        }
        if (pStmt) {
            addError(&pStmt->pErrorList, "HY000", "Unicode conversion failed.",
                     0, NULL);
        }
        auto context = contextCreator();
        RS_LOG_ERROR("RSUTIL", "Unicode conversion failed. Context: %s",
                     context.c_str());
        return SQL_ERROR;
    }

    // currentChar: Starting position in the converted wide character string for
    // this fetch. Used for sequential fetches where the application retrieves
    // data in multiple calls. Tracks how many characters have already been
    // returned in previous calls.
    int currentChar = (cbLenOffset) ? *cbLenOffset : 0;

    // If offset is at or past the end, all data has been fetched
    if (currentChar >= wcharLen) {
        if (cbLenOffset) {
            *cbLenOffset = 0; // Reset offset
        }
        if (pcbLenInd) {
            *pcbLenInd = 0; // No remaining data
        }
        if (useHeap && wcharStr) {
            wcharStr = (SQLWCHAR *)rs_free(wcharStr);
        }
        return SQL_SUCCESS;
    }

    // Calculate how many characters remain to be fetched from currentChar
    // position
    int remainingChars = wcharLen - currentChar;
    // Set the indicator to length of data available at the start of current
    // call This matches the behavior of copyStrDataBigLen
    if (pcbLenInd) {
        *pcbLenInd = remainingChars * sizeofSQLWCHAR();
    }

    // Calculate how many characters to copy in this call, starting from
    // currentChar. Limited by: buffer capacity (cchLen - 1 for null terminator)
    // and remaining data. ensuring:
    // 1. We have a valid destination buffer (cchLen > 0)
    // 2. We reserve space for null terminator (cchLen - 1)
    // 3. We don't copy more than available remaining characters (MIN with
    // remainingChars)
    // 4. We never attempt to copy a negative number of characters (MAX with 0)
    int copyChars = MAX(0, MIN(remainingChars, cchLen > 0 ? cchLen - 1 : 0));

    // Copy data if destination buffer is provided
    if (cchLen > 0) {
        // Final green light to do the copy and null termination
        if (copyChars > 0 && currentChar >= 0 &&
            (currentChar + copyChars) <= wcharLen && wcharStr) {
            memcpy(pDest, (char *)wcharStr + (currentChar * sizeofSQLWCHAR()),
                   copyChars * sizeofSQLWCHAR());
        }
        // Write null terminator with proper size for current Unicode format
        if ((copyChars * sizeofSQLWCHAR()) < cbLen) {
            memset((char *)pDest + (copyChars * sizeofSQLWCHAR()), 0,
                   sizeofSQLWCHAR());
        }
    } // else check the offset or cchLen == 0 :

    // Free only if heap allocated
    if (useHeap && wcharStr) {
        wcharStr = (SQLWCHAR *)rs_free(wcharStr);
    }

    // Handle sequential fetch offset updating
    if (cbLenOffset) {
        if (remainingChars > copyChars) {
            // More data remains, update offset and signal truncation
            *cbLenOffset += copyChars;
            std::string err =
                "String data, right truncation occurred: Buffer too small to "
                "hold the entire data";
            if (pStmt) {
                addWarning(&pStmt->pErrorList, "01004", err.data(), 0, NULL);
            }
            err += ": remainingChars(" + std::to_string(remainingChars) +
                   ") > copyChars(" + std::to_string(copyChars) +
                   ") Context:" + contextCreator();

            RS_LOG_DEBUG("RSUTIL", "%s", err.data());
            return SQL_SUCCESS_WITH_INFO;
        } else {
            // All data fetched, reset offset
            *cbLenOffset = 0;
            return SQL_SUCCESS;
        }
    } else {
        // Not using sequential fetches
        if (remainingChars > copyChars && cchLen > 0) {
            std::string err =
                "String data, right truncation occurred: Buffer too small to "
                "hold the entire data";
            if (pStmt) {
                addWarning(&pStmt->pErrorList, "01004", err.data(), 0, NULL);
            }
            err += ": remainingChars(" + std::to_string(remainingChars) +
                   ") > copyChars(" + std::to_string(copyChars) + ") cchLen(" +
                   std::to_string(cchLen) + ") Context:" + contextCreator();

            RS_LOG_DEBUG("RSUTIL", "%s", err.data());
            return SQL_SUCCESS_WITH_INFO;
        }
    }

    // If no destination was provided, signal truncation
    if (cchLen == 0) {
        // Truncation occurred
        if (pStmt) {
            addWarning(&pStmt->pErrorList, "01004",
                       "String data, right truncation occurred: Buffer too "
                       "small to hold the entire data.",
                       0, NULL);
        }

        auto context = contextCreator();
        RS_LOG_DEBUG("RSUTIL",
                     "String data, right truncation occurred: Buffer "
                     "too small to hold the entire data. Context: %s",
                     context.c_str());
        return SQL_SUCCESS_WITH_INFO;
    }

    return SQL_SUCCESS;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy big SQLWCHAR string data from LONGVARBINARY in HEX format (VARBYTE, GEOMETRY, GEOGRAPHY  etc.
//
// cbLen and pcbLen are in bytes
SQLRETURN SQL_API copyWBinaryDataBigLen(const char *pSrc,
                                        SQLINTEGER iSrcLen,
                                        SQLWCHAR  *pDest,
                                        SQLLEN     cbLen,
                                        SQLLEN    *pcbLen)
{
    const bool hasData   = (pSrc && iSrcLen != SQL_NULL_DATA && iSrcLen > 0);
    const size_t inBytes = hasData ? static_cast<size_t>(iSrcLen) : 0u;
    const size_t charSz  = static_cast<size_t>(sizeofSQLWCHAR());
    const size_t cchCap  = (cbLen > 0) ? static_cast<size_t>(cbLen) / charSz : 0u;

    if (pcbLen) {
        unsigned long long need =
            static_cast<unsigned long long>(inBytes) * charSz;
        if (need > static_cast<unsigned long long>((std::numeric_limits<SQLLEN>::max)())) {
            *pcbLen = 0;
            return SQL_ERROR;
        }
        *pcbLen = static_cast<SQLLEN>(need); // bytes, excl NUL
    }

    if (!pDest || cchCap == 0) return SQL_SUCCESS; // length inquiry / no capacity

    const size_t toCopy = (cchCap - 1u < inBytes) ? (cchCap - 1u) : inBytes;

    // Bulk zero the region we’ll use: toCopy chars + 1 NUL
    unsigned char* outB = reinterpret_cast<unsigned char*>(pDest);
    std::memset(outB, 0, (toCopy + 1u) * charSz);

    // Write low byte of each wide cell
    const unsigned char* uSrc = reinterpret_cast<const unsigned char*>(pSrc);
    for (size_t i = 0; i < toCopy; ++i) {
        outB[i * charSz + 0] = uSrc[i];
    }

    return (toCopy < inBytes) ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
}

/*====================================================================================================================================================*/

/*
 * Hex-encode binary -> wide string (W=2 or W=4 at runtime).
 * - Each input byte becomes two ASCII hex chars (uppercase).
 * - *pcbLen returns required BYTES (excluding the null terminator) at the client's wchar width.
 * - Returns:
 *     SQL_SUCCESS            when fully written or length-only query
 *     SQL_SUCCESS_WITH_INFO  when truncated (buffer too small)
 *     SQL_ERROR              on length overflow (required bytes > SQLLEN max)
 *
 * Notes:
 * - Does NOT write anything if pDest==NULL or cbLen<=0 (length-only query).
 * - Always null-terminates if there is at least 1 wchar of capacity.
 * - Uses runtime wchar width: size_t charSize = sizeofSQLWCHAR();  // 2 or 4
 * - Assumes native endianness zero-extends ASCII (0x41 -> 0x0041 / 0x00000041).
 *   If you need specific endianness, add byte swaps accordingly.
 */
SQLRETURN SQL_API copyWBinaryToHexDataBigLen(const char *psrc,
                                             SQLINTEGER iSrcLen,
                                             SQLWCHAR *pDest,
                                             SQLLEN cbLen,
                                             SQLLEN *pcbLen)
{
    static const char HEX[] = "0123456789ABCDEF";

    const unsigned char* pSrc = (const unsigned char*)psrc;
    const bool hasData = (pSrc && iSrcLen != SQL_NULL_DATA && iSrcLen > 0);
    const size_t inBytes = hasData ? (size_t)iSrcLen : 0u;
    const size_t requiredChars = inBytes * 2u;

    const size_t w = (size_t)sizeofSQLWCHAR();     // 2 or 4 at runtime
    // sanity
    if (w != 2 && w != 4) return SQL_ERROR;

    // Report required BYTES (no NUL)
    if (pcbLen) {
        unsigned long long need = (unsigned long long)requiredChars * (unsigned long long)w;
        if (need > (unsigned long long)(std::numeric_limits<SQLLEN>::max)()) {
            *pcbLen = 0;
            return SQL_ERROR;
        }
        *pcbLen = (SQLLEN)need;
    }

    // Length-only / no capacity
    if (!pDest || cbLen <= 0) return SQL_SUCCESS;

    // Capacity in characters
    size_t capChars = (size_t)((unsigned long long)cbLen / (unsigned long long)w);
    if (capChars == 0) return SQL_SUCCESS;

    // Leave room for NUL and keep even count (pairs per byte)
    size_t usable = (capChars > 0) ? (capChars - 1u) : 0u;
    if (usable > requiredChars) usable = requiredChars;
    if (usable & 1u) usable -= 1u;

    unsigned char* wp = (unsigned char*)pDest;     // byte writer
    size_t produced = 0;                            // chars (not bytes)

    auto put_wchar = [&](uint32_t ascii) {
        if (w == 2) {
            uint16_t v = (uint16_t)ascii;
            std::memcpy(wp, &v, 2);
            wp += 2;
        } else { // w == 4
            uint32_t v = ascii;
            std::memcpy(wp, &v, 4);
            wp += 4;
        }
        ++produced;
    };

    for (size_t i = 0; i < inBytes && produced + 2u <= usable; ++i) {
        unsigned v = pSrc[i];
        put_wchar((uint32_t)HEX[(v >> 4) & 0xF]);
        put_wchar((uint32_t)HEX[v & 0xF]);
    }

    // NUL terminator (one wchar)
    if (w == 2) {
        uint16_t z = 0;
        std::memcpy(wp, &z, 2);
    } else {
        uint32_t z = 0;
        std::memcpy(wp, &z, 4);
    }

    const bool truncated = (requiredChars > usable);
    return truncated ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset string buffer.
//
void resetPaStrBuf(RS_STR_BUF *pPaStrBuf)
{
    if(pPaStrBuf != NULL)
    {
        pPaStrBuf->iAllocDataLen = -1; // Default to application buffer.
        pPaStrBuf->pBuf = NULL;
        pPaStrBuf->buf[0] = '\0';
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release string buffer.
//
void releasePaStrBuf(RS_STR_BUF *pPaStrBuf)
{
    if(pPaStrBuf != NULL)
    {
        if(pPaStrBuf->iAllocDataLen > 0)
        {
            pPaStrBuf->pBuf = (char *)rs_free(pPaStrBuf->pBuf);
            pPaStrBuf->iAllocDataLen = -1; // Default to application buffer.
        }
        pPaStrBuf->pBuf = NULL;
        pPaStrBuf->buf[0] = '\0';
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add result in the result list.
//
void addResult(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult)
{
    // Put Result at the end in HSTMT list
    if(pStmt->pResultHead == NULL)
    {
        pStmt->pResultHead = pResult;

        // Copy IRD recs.
        copyIRDRecsFromResult(pStmt->pResultHead, pStmt->pIRD);
    }
    else
    {
        RS_RESULT_INFO *prev = NULL;
        RS_RESULT_INFO *cur  = pStmt->pResultHead;

        while(cur != NULL)
        {
            prev = cur;
            cur = cur->pNext;
        }

        prev->pNext = pResult;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add prepare info in the list.
//
void addPrepare(RS_STMT_INFO *pStmt, RS_PREPARE_INFO *pPrepare)
{
    // Put prepare at the end in HSTMT list
    if(pStmt->pPrepareHead == NULL)
    {
        pStmt->pPrepareHead = pPrepare;

        // Copy IPD recs.
        copyIPDRecsFromPrepare(pStmt->pPrepareHead, pStmt->pIPD);
    }
    else
    {
        RS_PREPARE_INFO *prev = NULL;
        RS_PREPARE_INFO *cur  = pStmt->pPrepareHead;

        while(cur != NULL)
        {
            prev = cur;
            cur = cur->pNext;
        }

        prev->pNext = pPrepare;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Allocate string buffer, if needed.
//
unsigned char *checkLenAndAllocatePaStrBuf(size_t cbLen, RS_STR_BUF *pPaStrBuf)
{
    resetPaStrBuf(pPaStrBuf);

    if(cbLen != SQL_NULL_DATA && cbLen > 0)
    {
        unsigned char *szData;

        if(cbLen > SHORT_STR_DATA)
        {
            pPaStrBuf->pBuf = (char *) rs_malloc(cbLen + 1);
            szData = (unsigned char *) (pPaStrBuf->pBuf);
            pPaStrBuf->iAllocDataLen = (int) cbLen;
        }
        else
        {
            pPaStrBuf->pBuf = pPaStrBuf->buf;
            szData = (unsigned char *) (pPaStrBuf->pBuf);
            pPaStrBuf->iAllocDataLen = 0;
        }

        szData[cbLen] = '\0';

        return szData;
    }
    else
        return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release all results associated with a statement.
//
void releaseResults(RS_STMT_INFO *pStmt)
{
    RS_RESULT_INFO *curr;
    int iAtHeadResult = (pStmt->pResultHead != NULL);

    // close/free result
    curr = pStmt->pResultHead;
    while(curr != NULL)
    {
        RS_RESULT_INFO *next = curr->pNext;

        releaseResult(curr,iAtHeadResult,pStmt);
        iAtHeadResult = FALSE;

        curr = next;
    }

    pStmt->pResultHead = NULL;

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// TRUE if async enable otherwise FALSE.
//
int isAsyncEnable(RS_STMT_INFO *pStmt)
{
    int asyncEnable = FALSE;

    if(pStmt)
    {
        if(pStmt->pStmtAttr)
        {
            asyncEnable = pStmt->pStmtAttr->iAsyncEnable;
        }
    }

    return asyncEnable;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add descriptor in the list.
//
void addDescriptor(RS_CONN_INFO *pConn, RS_DESC_INFO *pDesc)
{
    // Put HDESC in front in HDBC list
    pDesc->pNext = pConn->phdescHead;
    pConn->phdescHead = pDesc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Remove descriptor from the list.
//
void removeDescriptor(RS_DESC_INFO *pDesc)
{
    RS_CONN_INFO *pConn;
    RS_DESC_INFO *curr;
    RS_DESC_INFO *prev;

    // Remove from HDBC list
    pConn = pDesc->phdbc;
    curr  = pConn->phdescHead;
    prev  = NULL;

    while(curr != NULL)
    {
        if(curr == pDesc)
        {
            if(prev == NULL)
                pConn->phdescHead = pConn->phdescHead->pNext;
            else
                prev->pNext = curr->pNext;

            curr->pNext = NULL;

            break;
        }

        prev = curr;
        curr = curr->pNext;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release explictily allocated descriptors.
//
RS_DESC_INFO *releaseExplicitDescs(RS_DESC_INFO *phdescHead)
{
    RS_DESC_INFO *pDesc;
    RS_DESC_INFO *pNext;

    for(pDesc = phdescHead; pDesc != NULL; pDesc = pNext)
    {
        pNext = pDesc->pNext;

        // Free descriptor
        pDesc = releaseDescriptor(pDesc, FALSE);
    } // Loop

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Execute command(s) after the connection.
//
SQLRETURN onConnectExecute(RS_CONN_INFO *pConn, char *pCmd)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLHSTMT  phstmt = NULL;

    rc = RS_SQLAllocHandle(SQL_HANDLE_STMT, pConn, &phstmt);
    if(rc == SQL_SUCCESS)
    {
        rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)pCmd, SQL_NTS, FALSE, FALSE, FALSE, FALSE);
        if(rc == SQL_SUCCESS)
        {
            rc = RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);
        }
        else
        {
            // Move stmt error on connection
            RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
            RS_ERROR_INFO *pError = getNextError(&pStmt->pErrorList,1, FALSE);
            if(pError != NULL)
                addError(&pConn->pErrorList, pError->szSqlState, pError->szErrMsg, pError->lNativeErrCode, NULL);

            goto error;
        }
    }
    else
        goto error;

    return rc;

error:

    if(phstmt)
        RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Execute audit info command(s) after the connection.
//
SQLRETURN onConnectAuditInfoExecute(RS_CONN_INFO *pConn)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLHSTMT  phstmt = NULL;
    char szAuditCommands[MAX_AUDIT_CMDS_LEN];
    int len = 0;

    szAuditCommands[0] = '\0';

    if(pConn && pConn->pConnAttr)
    {
        // Application Name
        if(pConn->pConnAttr->szApplicationName[0] != '\0')
        {
            len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET application_name = '%s';", pConn->pConnAttr->szApplicationName);
        }

        // Client Domain Name
        if(pConn->pConnAttr->szClientDomainName[0] != '\0')
        {
            len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_domain_name = '%s';", pConn->pConnAttr->szClientDomainName);
        }

        // Client Host Name
        if(pConn->pConnAttr->szClientHostName[0] != '\0')
        {
            struct hostent *hostentry = gethostbyname(pConn->pConnAttr->szClientHostName);
            char *ipbuf = NULL;

            if(hostentry)
                ipbuf = inet_ntoa(*((struct in_addr *)hostentry->h_addr_list[0]));

            len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_host_name = '%s';", pConn->pConnAttr->szClientHostName);

            if(ipbuf && ipbuf[0])
                len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_ip_address = '%s';", ipbuf);
        }

        // Client User Name
        if(pConn->pConnAttr->szOsUserName[0] != '\0')
        {
            len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_user_name = '%s';", pConn->pConnAttr->szOsUserName);
        }

        // Client Driver Type
        len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_driver_type = '%s';", RS_DRIVER_TYPE);

        // Client Driver Version Number
        len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_driver_version = '%s';", ODBC_DRIVER_VERSION);

        // Client OS Name. Use in UNLOAD
        {
            char *client_os_name = NULL;
#ifdef WIN32
            client_os_name = "windows";
#endif
#if defined LINUX 
            client_os_name = "linux";
#endif
            if(client_os_name != NULL)
                len += snprintf(szAuditCommands + len, MAX_AUDIT_CMDS_LEN - len, "SET client_os_name = '%s';", client_os_name);
        } // client_os_name
    }

    if(szAuditCommands[0] != '\0')
    {
        rc = RS_SQLAllocHandle(SQL_HANDLE_STMT, pConn, &phstmt);
        if(rc == SQL_SUCCESS)
        {
            rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szAuditCommands, SQL_NTS, FALSE, FALSE, FALSE, FALSE);
            if(rc == SQL_SUCCESS)
            {
                rc = RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);
            }
            else
                goto error;
        }
        else
            goto error;
    }

    return rc;

error:

    if(phstmt)
        RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get GUC or session variable value.
//
SQLRETURN getGucVariableVal(RS_CONN_INFO *pConn, char *pVarName, char *pVarVal, int iBufLen)
{
    SQLRETURN rc = SQL_SUCCESS;
    SQLHSTMT  phstmt = NULL;
    char      szCmd[MAX_TEMP_BUF_LEN];

    snprintf(szCmd, sizeof(szCmd), "SHOW %s",pVarName);
    pVarVal[0] = 0;

    rc = RS_SQLAllocHandle(SQL_HANDLE_STMT, pConn, &phstmt);
    if(rc == SQL_SUCCESS)
    {
        rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCmd, SQL_NTS, FALSE, FALSE, FALSE, FALSE);
        if(rc == SQL_SUCCESS)
        {
            rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0);

            if (rc == SQL_SUCCESS) {
                SQLLEN pcbLenIndInternal = (std::numeric_limits<SQLLEN>::min)();
                rc = RS_STMT_INFO::RS_SQLGetData((RS_STMT_INFO *)phstmt, 1,
                                                 SQL_C_CHAR, pVarVal, iBufLen,
                                                 NULL, TRUE, pcbLenIndInternal);
            } else {
                pVarVal[0] = 0;
            }

            rc = RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);
        }
        else
            goto error;
    }
    else
        goto error;

    return rc;

error:

    pVarVal[0] = 0;

    if(phstmt)
        RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get one value of a SELECT statement.
//
SQLRETURN getOneQueryVal(RS_CONN_INFO *pConn, char * pSqlCmd, char *pVarBuf, int iBufLen)
{
	SQLRETURN rc = SQL_SUCCESS;
	SQLHSTMT  phstmt = NULL;

	rc = RS_SQLAllocHandle(SQL_HANDLE_STMT, pConn, &phstmt);
	if (rc == SQL_SUCCESS)
	{
		rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)pSqlCmd, SQL_NTS, FALSE, FALSE, FALSE, FALSE);
		if (rc == SQL_SUCCESS)
		{
			rc = RS_STMT_INFO::RS_SQLFetchScroll(phstmt, SQL_FETCH_NEXT, 0);

            if (rc == SQL_SUCCESS) {
                SQLLEN pcbLenIndInternal =
                    (std::numeric_limits<SQLLEN>::min)();
                rc = RS_STMT_INFO::RS_SQLGetData(
                    (RS_STMT_INFO *)phstmt, 1, SQL_C_CHAR, pVarBuf,
                    iBufLen, NULL, TRUE, pcbLenIndInternal);
            }

            rc = RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);
		}
		else
			goto error;
	}
	else
		goto error;

	return rc;

error:

	pVarBuf[0] = 0;

	if (phstmt)
		RS_SQLFreeHandle(SQL_HANDLE_STMT, phstmt);

	return rc;
}

SQLRETURN copyToCBinary(const void *pSrc, SQLLEN srcSize,
                        void *pBuf, SQLLEN cbLen,
                        SQLLEN *pcbLenInd,
                        RS_STMT_INFO *pStmt,
                        const char *typeName)
{
    // DM rejects NULL TargetValuePtr with HY009 before reaching the driver.
    // This guards the bound-column fetch path where pDescRec->pValue may be NULL.
    if (pBuf == NULL) {
        RS_LOG_ERROR("RSUTIL", "NULL buffer for %s to SQL_C_BINARY conversion", typeName);
        addError(&pStmt->pErrorList, "HY009", "Invalid use of null pointer", 0, NULL);
        return SQL_ERROR;
    }

    // Fixed-size types cannot be partially copied. Buffer must hold the entire struct.
    if (cbLen < srcSize) {
        char errMsg[MAX_ERR_MSG_LEN];
        snprintf(errMsg, sizeof(errMsg),
                 "Buffer too small for %s to SQL_C_BINARY conversion", typeName);
        RS_LOG_ERROR("RSUTIL", "%s (need %lld, have %lld)",
                     errMsg, (long long)srcSize, (long long)cbLen);
        addError(&pStmt->pErrorList, "22003", errMsg, 0, NULL);
        return SQL_ERROR;
    }

    memcpy(pBuf, pSrc, srcSize);
    // Indicator is only set on success (undefined on error per ODBC spec).
    if (pcbLenInd) {
        *pcbLenInd = srcSize;
    }
    return SQL_SUCCESS;
}

SQLRETURN copyVariableToCBinary(const char *pSrc, SQLLEN srcLen,
                                void *pBuf, SQLLEN cbLen,
                                SQLLEN *cbLenOffset,
                                SQLLEN *pcbLenInd,
                                RS_STMT_INFO *pStmt)
{
    if (srcLen <= 0) {
        if (pcbLenInd) {
            *pcbLenInd = 0;
        }
        return SQL_SUCCESS;
    }

    // Get current fetch position for chunked retrieval (successive SQLGetData calls).
    SQLLEN currentOffset = (cbLenOffset) ? *cbLenOffset : 0;

    // All data already fetched in a prior call.
    if (currentOffset >= srcLen) {
        if (cbLenOffset) {
            *cbLenOffset = 0;
        }
        if (pcbLenInd) {
            *pcbLenInd = 0;
        }
        return SQL_SUCCESS;
    }

    SQLLEN remainingLen = srcLen - currentOffset;

    // Indicator reports data available at the start of this call.
    if (pcbLenInd) {
        *pcbLenInd = remainingLen;
    }

    // DM rejects NULL TargetValuePtr before reaching the driver.
    // This guards the bound-column fetch path where pDescRec->pValue may be NULL.
    if (pBuf == NULL) {
        return SQL_SUCCESS;
    }

    if (remainingLen <= cbLen) {
        // All remaining data fits. No null terminator -- SQL_C_BINARY is raw bytes.
        memcpy(pBuf, pSrc + currentOffset, remainingLen);
        if (cbLenOffset) {
            *cbLenOffset = 0;
        }
        return SQL_SUCCESS;
    }

    // Truncation -- copy what fits, advance offset for next call.
    if (cbLen > 0) {
        memcpy(pBuf, pSrc + currentOffset, cbLen);
        if (cbLenOffset) {
            *cbLenOffset += cbLen;
        }
    }
    RS_LOG_DEBUG("RSUTIL",
                 "SQL_C_BINARY truncation: remaining=%lld, bufLen=%lld, offset=%lld",
                 (long long)remainingLen, (long long)cbLen,
                 (long long)(cbLenOffset ? *cbLenOffset : 0));
    if (pStmt) {
        addWarning(&pStmt->pErrorList, "01004",
                   "String data, right truncated", 0, NULL);
    }
    return SQL_SUCCESS_WITH_INFO;
}

//---------------------------------------------------------------------------------------------------------igarish
// Convert SQL data into C data.
//
SQLRETURN convertSQLDataToCData(RS_STMT_INFO *pStmt, char *pColData,
                                int iColDataLen, short hSQLType, void *pBuf,
                                SQLLEN cbLen, SQLLEN *cbLenOffset,
                                SQLLEN *pcbLenInd, short hCType,
                                short hRsSpecialType, int format,
                                RS_DESC_REC *pDescRec)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_VALUE  rsVal;
    int iConversionError = FALSE;
	int len;
    short hType;

    if(hCType == SQL_C_DEFAULT)
        hType = getDefaultCTypeFromSQLType(hSQLType, &iConversionError);
    else
        hType = hCType;

    // BoolsAsChar: Normalize BOOLOID raw data to "1"/"0" so all C-type conversions
    // work uniformly. hRsSpecialType==BOOLOID is only set when BoolsAsChar is enabled.
    // Must happen before getRsVal so all paths see normalized data.
    if (hRsSpecialType == BOOLOID && pColData && iColDataLen > 0) {
        int boolVal = IS_TEXT_FORMAT(format)
            ? (pColData[0] == 't' || pColData[0] == 'T' || pColData[0] == '1')
            : (pColData[0] == 1);
        pColData = (char *)(boolVal ? "1" : "0");
        iColDataLen = 1;
    }

    int iConversion =
        getRsVal(pColData, iColDataLen, hSQLType, &rsVal, hType, format,
                    pDescRec, hRsSpecialType, FALSE);

    RS_LOG_TRACE("RSUTIL",
                 "convertSQLDataToCData C-Type=%s SQL-Type=%s "
                 "format=%d iColDataLen=%d iConversion=%d",
                 cTypeNameMap(hType), sqlTypeNameMap(hSQLType), format,
                 iColDataLen, iConversion);

    /**
     * Handles data type conversion from SQL types to C data types.
     * Uses nested switch statements to process:
     * Target C data type (SQL_C_CHAR, SQL_C_WCHAR, etc.)
     *  |-> Source SQL data type (SQL_CHAR, SQL_INTEGER, etc.)
     *
     * Possible return values:
     * - SQL_SUCCESS: Conversion completed successfully
     * - SQL_SUCCESS_WITH_INFO: Successful conversion with truncation/warnings
     * - SQL_ERROR: Conversion failed due to invalid data or unsupported type
     */
    switch(hType)
    {
        case SQL_C_CHAR:
        {
            switch(hSQLType)
            {
                case SQL_CHAR:
                case SQL_WCHAR:
                {
                    // SQL_CHAR to SQL_C_CHAR conversion
                    rc = copyStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    break;
                }

                case SQL_VARCHAR:
				case SQL_LONGVARCHAR:
				case SQL_WVARCHAR:
				case SQL_WLONGVARCHAR:
                {
                    // SQL_VARCHAR to SQL_C_CHAR conversion
                    if ((IS_TEXT_FORMAT(format)) || (hRsSpecialType != TIMETZOID
                                                        && hRsSpecialType != TIMESTAMPTZOID))
                    {
                        // As per getRsVal(), when getRsVal() returns false,
                        // we should process the value as string directly.
                        rc = copyStrDataBigLen(pStmt, iConversion ? rsVal.pcVal : pColData,
                                               iColDataLen,
                                               (char *)pBuf,
                                               cbLen,
                                               cbLenOffset,
                                                pcbLenInd);
                    }
					else {
						if (iColDataLen > 0)
						{
							if(hRsSpecialType == TIMETZOID)
								len = time_out(rsVal.tzVal.time, (char *)pBuf, cbLen, &(rsVal.tzVal.zone));
							else
							if (hRsSpecialType == TIMESTAMPTZOID)
							{
								char *pTimeZone = libpqParameterStatus(pStmt->phdbc, "TimeZone");
								len = timestamp_out(rsVal.llVal, (char *)pBuf, cbLen, pTimeZone);
							}
						}
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

				case SQL_BINARY:
				case SQL_LONGVARBINARY:
				{
					if (hRsSpecialType == GEOMETRY
						|| ((!IS_TEXT_FORMAT(format))
							 && (hRsSpecialType == VARBYTE
								 || hRsSpecialType == GEOGRAPHY
								 || hRsSpecialType == GEOMETRYHEX)))
					{
						// Convert Binary to Hex
						rc = copyBinaryToHexDataBigLen(rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, pcbLenInd);
					}
					else
					{
						// Already in HEX format
						rc = copyBinaryDataBigLen(rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, pcbLenInd);
					}
					break;
				}


                case SQL_BIT:
                case SQL_TINYINT:
                {
                    // Now put bit into app buf
                    if(cbLen > 0)
                    {
                        if(cbLen > 1)
                            memset(pBuf, '\0', cbLen);

                        rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);

                        if(rc == SQL_SUCCESS && pBuf)
                            *(char *)pBuf = (rsVal.bVal == 1) ? '1' : '0';
                    }
                    else 
                        rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);

                    break;
                }

                case SQL_SMALLINT:
				{
					if(IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
						if (iColDataLen > 0)
							len = snprintf((char *)pBuf, cbLen, "%hd", rsVal.hVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

				case SQL_INTEGER:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
						if (iColDataLen > 0)
							len = snprintf((char *)pBuf, cbLen, "%d", rsVal.iVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

                case SQL_BIGINT:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
						if (iColDataLen > 0)
							len = snprintf((char *)pBuf, cbLen, "%lld", rsVal.llVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

                case SQL_REAL:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
						if (iColDataLen > 0)
							len = snprintf((char *)pBuf, cbLen, "%g", rsVal.fVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;

				}

                case SQL_FLOAT:
                case SQL_DOUBLE:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
						if (iColDataLen > 0)
							len = snprintf((char *)pBuf, cbLen, "%g", rsVal.dVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;

				}

                case SQL_TYPE_DATE:
				case SQL_DATE:
				{
                    // Check buffer size
                    if (cbLen < (DATE_STRING_LEN + 1)) { // Not enough space for yyyy-mm-dd + null terminator
                        RS_LOG_ERROR("RSUTIL", "Buffer too small for SQL_DATE to SQL_C_CHAR conversion");
                        addError(&pStmt->pErrorList, "22003", "Buffer too small for SQL_DATE to SQL_C_CHAR conversion", 0, NULL);
                        rc = SQL_ERROR;
                        goto error;
                    }
                    if (IS_TEXT_FORMAT(format)) {
                        rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    }
					else
					{
						if (iColDataLen > 0)
							len = date_out(rsVal.iVal,(char *)pBuf, cbLen);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

				case SQL_TYPE_TIMESTAMP:
				case SQL_TIMESTAMP:
				{
                    // the minimum character required for storing timestamp without fractional seconds is yyyy-mm-dd hh:mm:ss (4+1+2+1+2+1+2+1+2+1+2 = 19 + one null character)
                    if (cbLen > 0 && cbLen < (TS_NO_FRAC_LEN + 1))
                    {
                        rc = SQL_ERROR;
                        RS_LOG_ERROR("RSUTIL", "Buffer too small for SQL_TIMESTAMP to SQL_C_CHAR conversion");
                        addError(&pStmt->pErrorList, "22003", "Buffer too small for SQL_TIMESTAMP to SQL_C_CHAR conversion", 0, NULL);
                        goto error;
                    }
                    if (IS_TEXT_FORMAT(format)) {
                        rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    } else {
                        if (iColDataLen > 0) {
                            len = timestamp_out(rsVal.llVal, (char *)pBuf, cbLen, NULL);
                            // Check for truncation
                            if (len >= cbLen && cbLen > 0) {
                                addWarning(&pStmt->pErrorList, "01004", "String data, right truncated", 0, NULL);
                                rc = SQL_SUCCESS_WITH_INFO;
                            }
                        }
                        else {
                            len = 0;
                        }
                        if (pcbLenInd) {
                            *pcbLenInd = len;
                        }
                    }

                    break;
                }

				case SQL_INTERVAL_YEAR_TO_MONTH:
				case SQL_INTERVAL_DAY_TO_SECOND:
				{
					// For both text and binary format, parse into struct then
					// format using output functions.
					// In text mode, getRsVal() returns FALSE (iConversion==0) and
					// pColData contains raw Redshift text (any intervalstyle).
					// We parse it to struct, then format to ODBC standard.
					if (iColDataLen > 0) {
						SQL_INTERVAL_STRUCT intervalVal;
						if (IS_TEXT_FORMAT(format)) {
							// Parse the raw text (any intervalstyle) into struct
                            char szNumBuf[MAX_NUMBER_BUF_LEN + 1];
                            makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);
							if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH) {
								intervalVal = parse_intervaly2m(szNumBuf, strlen(szNumBuf));
							} else {
								intervalVal = parse_intervald2s(szNumBuf, strlen(szNumBuf));
							}
						} else {
							// Binary format: rsVal already populated by getRsVal
							intervalVal = rsVal.intervalVal;
						}
						// Format to string
						char tempBuf[MAX_TEMP_BUF_LEN];
						if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH)
							len = intervaly2m_out(&intervalVal, tempBuf, sizeof(tempBuf));
						else
							len = intervald2s_out(&intervalVal, tempBuf, sizeof(tempBuf));
						rc = copyStrDataBigLen(pStmt, tempBuf, len, (char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					} else {
						len = 0;
						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

				case SQL_NUMERIC:
				case SQL_DECIMAL:
				{
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_CHAR conversion
                    if (IS_TEXT_FORMAT(format)) {
                        // Store the NUMERIC/DECIMAL value to character buffer
                        // Ensure the application-provided buffer has sufficient length to store it
                        // According to the ODBC spec, we must store at least the whole digits or throw an error
                        if (iColDataLen > 0) {
                            rc = validateNumericBufferSize(
                                pStmt, pColData, iColDataLen, cbLen, hSQLType,
                                hType, false);
                            if (rc == SQL_ERROR) {
                                break;
                            }
                            // copyStrDataBigLen will handle cases where the fractional part
                            // may be truncated due to insufficient buffer size
                            rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,
                                                   (char *)pBuf, cbLen,
                                                   cbLenOffset, pcbLenInd);
                        } else {
                            // No data
                            rc = copyStrDataBigLen(
                                pStmt, pColData, SQL_NULL_DATA, (char *)pBuf,
                                cbLen, cbLenOffset, pcbLenInd);
                        }
                    }
					else
					{
						if (iColDataLen > 0)
						{
							SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
							char tempBuf[MAX_TEMP_BUF_LEN];
							char *pNumData = ((pnVal->precision + 2) < cbLen) ? (char *) pBuf : tempBuf;
							int num_data_len = ((pnVal->precision + 2) < cbLen) ? cbLen : sizeof(tempBuf);

							convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

							if ((pnVal->precision + 2) < cbLen)
							{
								len = strlen((char *)pBuf);
							}
							else
							{
								len = snprintf((char *)pBuf, cbLen, "%s", tempBuf);
							}
						}
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;

					}

					break;
				}

				case SQL_TYPE_TIME:
				case SQL_TIME:
                {
					// Check buffer size 
                    if (cbLen > 0 && cbLen < (TIME_STR_LEN + 1)) // Minimum buffer length for TIME string is 8 bytes + null terminator = 9
                    {
                        addError(&pStmt->pErrorList, "22003", "Buffer too small for SQL_TIME to SQL_C_CHAR conversion", 0, NULL);
                        RS_LOG_ERROR("RSUTIL", "Buffer too small for SQL_TIME to SQL_C_CHAR conversion");
                        rc = SQL_ERROR;
                        goto error;
                    }
                    if (IS_TEXT_FORMAT(format)) {
                        rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    }
                    else {
                        if (iColDataLen > 0) {
                            len = time_out(rsVal.llVal, (char *)pBuf, cbLen, NULL);
                        }
                        else {
                            len = 0;
                        }
                        if (pcbLenInd) {
                            *pcbLenInd = len;
                        }
                    }

                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_CHAR

        case SQL_C_WCHAR:
        {
            switch(hSQLType)
            {
                case SQL_CHAR:
                case SQL_WCHAR:
				{
                    // SQL_CHAR to SQL_C_WCHAR conversion
                    rc = copyWStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen,(SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    break;
                }

                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_VARCHAR to SQL_C_WCHAR conversion
                    if ((IS_TEXT_FORMAT(format)) || (hRsSpecialType != TIMETZOID &&
                                                    hRsSpecialType != TIMESTAMPTZOID)) {
                        rc = copyWStrDataBigLen(pStmt,
                            iConversion ? rsVal.pcVal : pColData,
                            iColDataLen,
                            (SQLWCHAR *)pBuf,
                            cbLen, 
                            cbLenOffset,
                            pcbLenInd);
                    } else {
                        // Binary TIMETZOID or TIMESTAMPTZOID
#ifdef WIN32
						if (iColDataLen > 0)
						{
							if (hRsSpecialType == TIMETZOID)
								len = time_out_wchar(rsVal.tzVal.time, (SQLWCHAR *)pBuf, cbLen, &(rsVal.tzVal.zone));
							else
							if (hRsSpecialType == TIMESTAMPTZOID)
							{
								char *pTimeZone = libpqParameterStatus(pStmt->phdbc, "TimeZone");

								len = timestamp_out_wchar(rsVal.llVal, (SQLWCHAR *)pBuf, cbLen, pTimeZone);
							}
						}
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif // WIn32
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];
							if (hRsSpecialType == TIMETZOID)
								len = time_out(rsVal.tzVal.time, (char *)tempBuf, sizeof(tempBuf), &(rsVal.tzVal.zone));
							else
							if (hRsSpecialType == TIMESTAMPTZOID)
							{
								char *pTimeZone = libpqParameterStatus(pStmt->phdbc, "TimeZone");

								len = timestamp_out(rsVal.llVal, (char *)tempBuf, sizeof(tempBuf), pTimeZone);
							}
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;
							if (pcbLenInd)
								*pcbLenInd = len;
						}

#endif
                    }
                    break;

				}


				case SQL_BINARY:
				case SQL_LONGVARBINARY:
				{
					if (hRsSpecialType == GEOMETRY
						|| ((!IS_TEXT_FORMAT(format))
								&& (hRsSpecialType == VARBYTE
									|| hRsSpecialType == GEOGRAPHY
									|| hRsSpecialType == GEOMETRYHEX))
						)
					{
						// Convert Binary to Hex
						rc = copyWBinaryToHexDataBigLen(rsVal.pcVal, iColDataLen, (SQLWCHAR *)pBuf, cbLen, pcbLenInd);
					}
					else
					{
						// Already in HEX format
						rc = copyWBinaryDataBigLen(rsVal.pcVal, iColDataLen, (SQLWCHAR *)pBuf, cbLen, pcbLenInd);
					}

					break;
				}


                case SQL_BIT:
                case SQL_TINYINT:
                {
                    // Now put bit into app buf
                    if(cbLen > 0)
                    {
                        rc = copyWStrDataBigLen(pStmt, (char *)((rsVal.bVal == 1) ? "1" : "0"), 1, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    }
                    else 
                        rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd); 

                    break;
                }

                case SQL_SMALLINT:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = swprintf((SQLWCHAR *)pBuf, cbLen, L"%hd", rsVal.hVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = snprintf((char *)tempBuf, sizeof(tempBuf), "%hd", rsVal.hVal);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;
							if (pcbLenInd)
								*pcbLenInd = len;
						}

#endif

					}

					break;

				}

                case SQL_INTEGER:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = swprintf((SQLWCHAR *)pBuf, cbLen, L"%d", rsVal.iVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = snprintf((char *)tempBuf, sizeof(tempBuf), "%d", rsVal.iVal);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;

							if (pcbLenInd)
								*pcbLenInd = len;
						}

#endif
					}

					break;
				}

                case SQL_BIGINT:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = swprintf((SQLWCHAR *)pBuf, cbLen, L"%lld", rsVal.llVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = snprintf((char *)tempBuf, sizeof(tempBuf), "%lld", rsVal.llVal);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;

							if (pcbLenInd)
								*pcbLenInd = len;
						}

#endif
					}

					break;
				}

                case SQL_REAL:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = swprintf((SQLWCHAR *)pBuf, cbLen, L"%g", rsVal.fVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = snprintf((char *)tempBuf, sizeof(tempBuf), "%g", rsVal.fVal);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;
							if (pcbLenInd)
								*pcbLenInd = len;
						}

#endif
					}

					break;
				}

                case SQL_FLOAT:
                case SQL_DOUBLE:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = swprintf((SQLWCHAR *)pBuf, cbLen, L"%g", rsVal.dVal);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = snprintf((char *)tempBuf, sizeof(tempBuf), "%g", rsVal.dVal);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;
							if (pcbLenInd)
								*pcbLenInd = len;
						}
#endif
					}

					break;
				}

				case SQL_DATE:
				case SQL_TYPE_DATE:
				{
                    // Size in WCHAR units needed for yyyy-mm-dd (10 chars + null terminator)
                    int minRequiredWchars = DATE_STRING_LEN + 1;
                    int cchLen = (cbLen > 0) ? (int)(cbLen/sizeof(SQLWCHAR)) : 0;

                    // Check buffer size according to ODBC spec
                    if (cchLen < minRequiredWchars) { // Not enough space for wide chars
                        RS_LOG_ERROR("RSUTIL", "Buffer too small for SQL_DATE to SQL_C_WCHAR conversion");
                        addError(&pStmt->pErrorList, "22003", "Buffer too small for SQL_DATE to SQL_C_WCHAR conversion", 0, NULL);
                        rc = SQL_ERROR;
                        goto error;
                    }
                    // Format the date string as wide chars
                    if (IS_TEXT_FORMAT(format)) {
                        rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    }
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = date_out_wchar(rsVal.iVal, (SQLWCHAR *)pBuf, cbLen);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = date_out(rsVal.iVal, (char *)tempBuf, sizeof(tempBuf));
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;
							if (pcbLenInd)
								*pcbLenInd = len;
						}
#endif

					}

					break;
				}

				case SQL_TIMESTAMP:
				case SQL_TYPE_TIMESTAMP:
				{
                    // the minimum character required for storing timestamp without fractional seconds is yyyy-mm-dd hh:mm:ss (4+1+2+1+2+1+2+1+2+1+2 = 19 + one null character)
                    int cchLen = (cbLen > 0) ? (int)(cbLen/sizeof(SQLWCHAR)) : 0;

                    // Check for minimum buffer size (20 wide characters)
                    if (cbLen > 0 && cchLen < (TS_NO_FRAC_LEN + 1)) {
                        rc = SQL_ERROR;
                        RS_LOG_ERROR("RSUTIL", "Buffer too small for SQL_TIMESTAMP to SQL_C_WCHAR conversion");
                        addError(&pStmt->pErrorList, "22003", "Buffer too small for SQL_TIMESTAMP to SQL_C_WCHAR conversion", 0, NULL);
                        goto error;
                    }
                    if (IS_TEXT_FORMAT(format)) {
                        rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    }
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = timestamp_out_wchar(rsVal.llVal, (SQLWCHAR *)pBuf, cbLen, NULL);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = timestamp_out(rsVal.llVal, (char *)tempBuf, sizeof(tempBuf), NULL);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;

							if (pcbLenInd)
								*pcbLenInd = len;
						}
#endif
					}

					break;

				}

				case SQL_INTERVAL_YEAR_TO_MONTH:
				case SQL_INTERVAL_DAY_TO_SECOND:
				{
					// For both text and binary format, parse into struct then
					// format using output functions.
					// In text mode, getRsVal() returns FALSE (iConversion==0) and
					// pColData contains raw Redshift text (any intervalstyle).
					// We parse it to struct, then format to ODBC standard.
					if (iColDataLen > 0) {
						SQL_INTERVAL_STRUCT intervalVal;
						if (IS_TEXT_FORMAT(format)) {
                            // Parse the raw text (any intervalstyle) into struct
							char szNumBuf[MAX_NUMBER_BUF_LEN + 1];
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);
							if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH)
								intervalVal = parse_intervaly2m(szNumBuf, strlen(szNumBuf));
							else
								intervalVal = parse_intervald2s(szNumBuf, strlen(szNumBuf));
						} else {
                            // Binary format: rsVal already populated by getRsVal
							intervalVal = rsVal.intervalVal;
						}
						// Format to string
                        char tempBuf[MAX_TEMP_BUF_LEN];
						if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH)
							len = intervaly2m_out(&intervalVal, tempBuf, sizeof(tempBuf));
						else
							len = intervald2s_out(&intervalVal, tempBuf, sizeof(tempBuf));
						rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					} else {
						len = 0;
						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

				case SQL_NUMERIC:
				case SQL_DECIMAL:
				{
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_WCHAR conversion
                    if (IS_TEXT_FORMAT(format)) {
                        if (iColDataLen > 0) {
                            // Store the NUMERIC/DECIMAL value to character buffer
                            // Ensure the application-provided buffer has sufficient length to store it
                            // According to the ODBC spec, we must store at least the whole digits or throw an error
                            rc = validateNumericBufferSize(
                                pStmt, pColData, iColDataLen, cbLen, hSQLType,
                                hType, true);
                            if (rc == SQL_ERROR) {
                                break;
                            }
                            // copyWStrDataBigLen will handle cases where the fractional part
                            // may be truncated due to insufficient buffer size
                            rc = copyWStrDataBigLen(
                                pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf,
                                cbLen, cbLenOffset, pcbLenInd);
                        } else {
                            // No data
                            rc = copyWStrDataBigLen(pStmt, pColData,
                                                    SQL_NULL_DATA,
                                                    (SQLWCHAR *)pBuf, cbLen,
                                                    cbLenOffset, pcbLenInd);
                        }
                    }
					else
					{
						if (iColDataLen > 0)
						{
							SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
							char tempBuf[MAX_TEMP_BUF_LEN];
							char *pNumData = tempBuf;
							int num_data_len = sizeof(tempBuf);

							convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

#ifdef WIN32
							len = swprintf((SQLWCHAR *)pBuf, INT_LEN(cbLen), L"%s", tempBuf);
#endif
#if defined LINUX 
							rc = copyWStrDataBigLen(pStmt, tempBuf, strlen(tempBuf), (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
							if (pcbLenInd)
								len = *pcbLenInd;
#endif
						}
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;


					}

					break;
				}

				case SQL_TIME:
				case SQL_TYPE_TIME:
                {
                    int cchLen = (cbLen > 0) ? (int)(cbLen / sizeof(SQLWCHAR)) : 0;
                    // Minimum buffer length is 8 WCHAR units + null terminator = 9
                    if (cbLen > 0 && cchLen < (TIME_STR_LEN + 1)) {
                        addError(&pStmt->pErrorList, "22003", "Buffer too small for SQL_TIME to SQL_C_WCHAR conversion", 0, NULL);
                        RS_LOG_ERROR("RSUTIL", "Buffer too small for SQL_TIME to SQL_C_WCHAR conversion");
                        rc = SQL_ERROR;
                        goto error;
                    }
                    if (IS_TEXT_FORMAT(format)) {
                       rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    }
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							len = time_out_wchar(rsVal.llVal, (SQLWCHAR *)pBuf, cbLen, NULL);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0)
						{
							char tempBuf[MAX_TEMP_BUF_LEN];

							len = time_out(rsVal.llVal, (char *)tempBuf, sizeof(tempBuf), NULL);
							rc = copyWStrDataBigLen(pStmt, tempBuf, len, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
						}
						else
						{
							len = 0;
							if (pcbLenInd)
								*pcbLenInd = len;
						}

#endif
					}

                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_WCHAR

        case SQL_C_SHORT:
        case SQL_C_SSHORT:
        case SQL_C_USHORT:
        {
            switch(hSQLType)
            {
                case SQL_SMALLINT:
                {
                    // Treating SQL_C_SHORT same as SQL_C_SSHORT (signed 16-bit)
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT){
                        short shortVal;
                        rc = rsIntToShort(rsVal.hVal, &shortVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getShortData(shortVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_USHORT
                    unsigned short ushortVal;
                    rc = rsIntToUShort(rsVal.hVal, &ushortVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc; 
                    }
                    SQLRETURN dataRc = getUShortData(ushortVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_SHORT/SQL_C_SSHORT/SQL_C_USHORT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;

                        // Prepare and validate the string
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf), &numStr, &truncated);

                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }
                        rc = convertStringNumericToIntegerCType(pStmt, numStr, strlen(numStr), pBuf, pcbLenInd, hType);
                    } else {
                        // Convert char to short
                        getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);
                    }
                    break;
                }

                case SQL_BIT:
                {
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT) {
                        // No range check needed - wide conversion
                        rc = getShortData(rsVal.bVal, pBuf, pcbLenInd);
                        break;
                    }
                    // SQL_C_USHORT
                    // No range check needed - wide conversion
                    rc = getUShortData(rsVal.bVal, pBuf, pcbLenInd);
                    break;
                }
                case SQL_TINYINT:
				{
                    getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));

                    // Treating SQL_C_SHORT same as SQL_C_SSHORT
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT) {
                        short shortVal;
                        rc = rsIntToShort(rsVal.hVal, &shortVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getShortData(shortVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_USHORT 
                    unsigned short ushortVal;
                    rc = rsIntToUShort(rsVal.hVal, &ushortVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUShortData(ushortVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_SHORT/SQL_C_USHORT/SQL_C_SSHORT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        rc = convertStringNumericToIntegerCType(pStmt, pColData, iColDataLen, pBuf, pcbLenInd, hType);
                    }
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);
						rsVal.hVal = (short)atoi(pNumData);

                        rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);
                    }
                    break;
                }

                case SQL_INTEGER:
                {
                    // Treating SQL_C_SHORT same as SQL_C_SSHORT (signed 16-bit)
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT) {
                        short shortVal;
                        rc = rsIntToShort(rsVal.iVal, &shortVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getShortData(shortVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_USHORT
                    unsigned short ushortVal;
                    rc = rsIntToUShort(rsVal.iVal, &ushortVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUShortData(ushortVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_BIGINT:
                {
                    // Treating SQL_C_SHORT same as SQL_C_SSHORT (signed 16-bit)
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT) {
                        short shortVal;
                        rc = rsIntToShort(rsVal.llVal, &shortVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getShortData(shortVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_USHORT
                    unsigned short ushortVal;
                    rc = rsIntToUShort(rsVal.llVal, &ushortVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUShortData(ushortVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_REAL:
                {
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT) {
                        short shortVal;
                        rc = rsFloatToShort(rsVal.fVal, &shortVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getShortData(shortVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // case for SQL_C_USHORT
                    unsigned short ushortVal;
                    rc = rsFloatToUShort(rsVal.fVal, &ushortVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUShortData(ushortVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    } 
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    if (hType == SQL_C_SHORT || hType == SQL_C_SSHORT) {
                        short shortVal;
                        rc = rsFloatToShort(rsVal.dVal, &shortVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getShortData(shortVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // case for SQL_C_USHORT
                    unsigned short ushortVal;
                    rc = rsFloatToUShort(rsVal.dVal, &ushortVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUShortData(ushortVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }
            } // SQL Type

            break;
        } // SQL_C_SHORT

        case SQL_C_LONG:
        case SQL_C_SLONG:
        case SQL_C_ULONG:
        {
            switch(hSQLType)
            {
                case SQL_INTEGER:
                {
                    if (hType == SQL_C_LONG || hType == SQL_C_SLONG) {
                        // SQL_C_SLONG or SQL_C_LONG
                        int intVal;
                        rc = rsIntToInt(rsVal.iVal, &intVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getIntData(intVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_ULONG
                    unsigned int uintVal;
                    rc = rsIntToUInt(rsVal.iVal, &uintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUIntData(uintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_LONG/SQL_C_SLONG/SQL_C_ULONG conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;

                        // Prepare and validate the string
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf), &numStr, &truncated);

                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }
                        rc = convertStringNumericToIntegerCType(pStmt, numStr, strlen(numStr), pBuf, pcbLenInd, hType);
                    } else {
                        // Convert char to integer
                        getRsVal(pColData, iColDataLen, SQL_INTEGER, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);
                    }
                    break;
                }

				case SQL_BIT:
				{
					int iVal;

					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to boolean
						getRsVal(pColData, iColDataLen, SQL_BIT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}

					iVal = (rsVal.bVal == 1) ? 1 : 0;

					// Now put int into app buf
					rc = getIntData(iVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_TINYINT:
				{
                    getRsVal(pColData, iColDataLen, SQL_INTEGER, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));

                    if (hType == SQL_C_LONG || hType == SQL_C_SLONG) {
                        // // SQL_C_SLONG or SQL_C_LONG
                        int intVal;
                        rc = rsIntToInt(rsVal.iVal, &intVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getIntData(intVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_ULONG
                    unsigned int uintVal;
                    rc = rsIntToUInt(rsVal.iVal, &uintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUIntData(uintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
				}

                case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_LONG/SQL_C_ULONG/SQL_C_SLONG conversion
                    if (IS_TEXT_FORMAT(format)) {
                        rc = convertStringNumericToIntegerCType(pStmt, pColData, iColDataLen, pBuf, pcbLenInd, hType);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);
						rsVal.iVal = atoi(pNumData);
                        rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);
					}
                    break;
                }

                case SQL_SMALLINT:
                {
                    if (hType == SQL_C_LONG || hType == SQL_C_SLONG) {
                        // SQL_C_SLONG or SQL_C_LONG
                        int intVal;
                        rc = rsIntToInt(rsVal.hVal, &intVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getIntData(intVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_ULONG
                    unsigned int uintVal;
                    rc = rsIntToUInt(rsVal.hVal, &uintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUIntData(uintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_BIGINT:
                {
                    if (hType == SQL_C_LONG || hType == SQL_C_SLONG) {
                        // SQL_C_SLONG or SQL_C_LONG
                        int intVal;
                        rc = rsIntToInt(rsVal.llVal, &intVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getIntData(intVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_ULONG
                    unsigned int uintVal;
                    rc = rsIntToUInt(rsVal.llVal, &uintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUIntData(uintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_REAL:
                {
                    if (hType == SQL_C_LONG || hType == SQL_C_SLONG) {
                        // SQL_C_SLONG or SQL_C_LONG
                        int intVal;
                        rc = rsFloatToInt(rsVal.fVal, &intVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getIntData(intVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_ULONG
                    unsigned int uintVal;
                    rc = rsFloatToUInt(rsVal.fVal, &uintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUIntData(uintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    if (hType == SQL_C_LONG || hType == SQL_C_SLONG) {
                        // SQL_C_SLONG or SQL_C_LONG
                        int intVal;
                        rc = rsFloatToInt(rsVal.dVal, &intVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getIntData(intVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_ULONG
                    unsigned int uintVal;
                    rc = rsFloatToUInt(rsVal.dVal, &uintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUIntData(uintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_LONG

        case SQL_C_SBIGINT:
        case SQL_C_UBIGINT:
        {
            switch(hSQLType)
            {
                case SQL_BIGINT:
                {
                    if (hType == SQL_C_SBIGINT) {
                        // No range check needed - wide conversion
                        rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);
                        break;
                    }
                    // SQL_C_UBIGINT
                    unsigned long long ubigintVal;
                    rc = rsIntToUBigInt(rsVal.llVal, &ubigintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUBigIntData(ubigintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_SBIGINT/SQL_C_UBIGINT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;

                        // Prepare and validate the string
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf), &numStr, &truncated);

                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }
                        rc = convertStringNumericToIntegerCType(pStmt, numStr, strlen(numStr), pBuf, pcbLenInd, hType);
                    } else {
                        // Convert char to long long
                        getRsVal(pColData, iColDataLen, SQL_BIGINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);
                    }
                    break;
                }

                case SQL_BIT:
                {
                    if (hType == SQL_C_SBIGINT) {
                        // No range check needed - wide conversion
                        rc = getBigIntData(rsVal.bVal, pBuf, pcbLenInd);
                        break;
                    }
                    // SQL_C_UBIGINT
                    rc = getUBigIntData(rsVal.bVal, pBuf, pcbLenInd);
                    break;
                }
                case SQL_TINYINT:
				{
                    getRsVal(pColData, iColDataLen, SQL_BIGINT, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));

                    // Treating SQL_C_SBIGINT same as SQL_C_BIGINT
                    if (hType == SQL_C_SBIGINT) {
                        // SQL_C_SBIGINT - No range check needed for TINYINT to BIGINT
                        SQLRETURN dataRc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_UBIGINT
                    unsigned long long ubigintVal;
                    rc = rsIntToUBigInt(rsVal.llVal, &ubigintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUBigIntData(ubigintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_SBIGINT/SQL_C_UBIGINT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        rc = convertStringNumericToIntegerCType(pStmt, pColData, iColDataLen, pBuf, pcbLenInd, hType);
                    }
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);
						sscanf(pNumData, "%lld", &(rsVal.llVal));
                        rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);
                    }
                    break;
                }

                case SQL_SMALLINT:
                {
                    if (hType == SQL_C_SBIGINT) {
                        // No range check needed - wide conversion
                        rc = getBigIntData(rsVal.hVal, pBuf, pcbLenInd);
                        break;
                    }
                    // SQL_C_UBIGINT
                    unsigned long long ubigintVal;
                    rc = rsIntToUBigInt(rsVal.hVal, &ubigintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUBigIntData(ubigintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_INTEGER:
                {
                    if (hType == SQL_C_SBIGINT) {
                        // No range check needed - wide conversion
                        rc = getBigIntData(rsVal.iVal, pBuf, pcbLenInd);
                        break;
                    }
                    // SQL_C_UBIGINT
                    unsigned long long ubigintVal;
                    rc = rsIntToUBigInt(rsVal.iVal, &ubigintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUBigIntData(ubigintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_REAL:
                {
                    if (hType == SQL_C_SBIGINT) {
                        long long bigintVal;
                        rc = rsFloatToBigInt(rsVal.fVal, &bigintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBigIntData(bigintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // case for SQL_C_UBIGINT
                    unsigned long long ubigintVal;
                    rc = rsFloatToUBigInt(rsVal.fVal, &ubigintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUBigIntData(ubigintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    if (hType == SQL_C_SBIGINT) {
                        long long bigintVal;
                        rc = rsFloatToBigInt(rsVal.dVal, &bigintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBigIntData(bigintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_UBIGINT
                    unsigned long long ubigintVal;
                    rc = rsFloatToUBigInt(rsVal.dVal, &ubigintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUBigIntData(ubigintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                case SQL_TIME:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_SBIGINT

        case SQL_C_FLOAT:
        {
            switch(hSQLType)
            {
                case SQL_REAL:
                {
                    // No range check needed - wide conversion
                    rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_FLOAT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;

                        // Prepare and validate the string
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf), &numStr, &truncated);

                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }
                        rc = convertStringNumericToFloatCType(pStmt, numStr, strlen(numStr), pBuf, pcbLenInd, hType);
                    } else {
                        // Convert char to float
                        getRsVal(pColData, iColDataLen, SQL_REAL, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);
                    }
                    break;
                }

				case SQL_BIT:
                {
                    // No range check needed - wide conversion
                    rc = getFloatData(rsVal.bVal, pBuf, pcbLenInd);
                    break;
                }
                case SQL_TINYINT:
				{
                    getRsVal(pColData, iColDataLen, SQL_REAL, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));

                    // No range check needed - wide conversion
					// Now put float into app buf
					rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_FLOAT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        // Convert char to float
                        getRsVal(pColData, iColDataLen, SQL_REAL, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                    }
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

						rsVal.fVal = (float)atof(pNumData);
					}

                    // Now put float into app buf
                    rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_SMALLINT:
                {
                    // No range check needed - wide conversion
                    rc = getFloatData((float)rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    float floatVal;
                    rc = rsIntToFloat(rsVal.iVal, &floatVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getFloatData(floatVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_BIGINT:
                {
                    float floatVal;
                    rc = rsIntToFloat(rsVal.llVal, &floatVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getFloatData(floatVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    float floatVal;
                    rc = rsDoubleToFloat(rsVal.dVal, &floatVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getFloatData(floatVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_FLOAT

        case SQL_C_DOUBLE:
        {
            switch(hSQLType)
            {
                case SQL_DOUBLE:
                {
                    // No range check needed - wide conversion
                    rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_DOUBLE conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;
                        // Prepare and validate the string
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf), &numStr, &truncated);

                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }
                        rc = convertStringNumericToFloatCType(pStmt, numStr, strlen(numStr), pBuf, pcbLenInd, hType);
                    } else {
                        // Convert char to double
                        getRsVal(pColData, iColDataLen, SQL_DOUBLE, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);
                    }
                    break;
                }

				case SQL_BIT:
                {
                    // No range check needed - wide conversion
                    rc = getDoubleData(rsVal.bVal, pBuf, pcbLenInd);
                    break;
                }
                case SQL_TINYINT:
				{
                    getRsVal(pColData, iColDataLen, SQL_DOUBLE, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));

                    // No range check needed - wide conversion
					// Now put double into app buf
					rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_DOUBLE conversion
                    if (IS_TEXT_FORMAT(format)) {
                        // Convert char to double
                        getRsVal(pColData, iColDataLen, SQL_DOUBLE, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                    }
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

						rsVal.dVal = (float)atof(pNumData);
					}

                    // Now put double into app buf
                    rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_SMALLINT:
                {
                    // No range check needed - wide conversion
                    rc = getDoubleData((double)rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    // No range check needed - wide conversion
                    rc = getDoubleData((double)rsVal.iVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_BIGINT:
                {
                    double doubleVal;
                    rc = rsIntToDouble(rsVal.llVal, &doubleVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getDoubleData(doubleVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_REAL:
                {
                    // No range check needed - wide conversion
                    rc = getDoubleData((double)rsVal.fVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_DOUBLE

        case SQL_C_BIT:
        case SQL_C_TINYINT:
        case SQL_C_STINYINT:
        case SQL_C_UTINYINT:
        {
            switch(hSQLType)
            {
                case SQL_BIT:
                {
                    if (hType == SQL_C_BIT) {
                        // Direct bit to bit conversion - no range check needed
                        rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);
                    } else if (hType == SQL_C_UTINYINT) {
                        // No range check needed - wide conversion
                        rc = getUTinyIntData(rsVal.bVal, pBuf, pcbLenInd);
                    } else {
                        // SQL_C_TINYINT or SQL_C_STINYINT
                        rc = getTinyIntData(rsVal.bVal, pBuf, pcbLenInd);
                    }
                    break;
                }
                case SQL_TINYINT:
                {
                    if (hType == SQL_C_BIT) {
                        // Convert TINYINT to BIT - need range validation
                        unsigned char bitVal;
                        rc = rsIntToBit(rsVal.bVal, &bitVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBooleanData(bitVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                    } else if (hType == SQL_C_UTINYINT) {
                        // No range check needed - wide conversion
                        getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));
                        rc = getUTinyIntData(rsVal.hVal, pBuf, pcbLenInd);
                    } else {
                        // SQL_C_TINYINT or SQL_C_STINYINT
                        getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, IS_TEXT_FORMAT(format));
                        rc = getTinyIntData(rsVal.hVal, pBuf, pcbLenInd);
                    }
                    break;
                }

                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
                case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_BIT/SQL_C_TINYINT/SQL_C_STINYINT/SQL_C_UTINYINT conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;

                        // Prepare and validate the string
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf), &numStr, &truncated);

                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }

                        rc = convertStringNumericToIntegerCType(pStmt, numStr, strlen(numStr), pBuf, pcbLenInd, hType);
                    } else {
                        // Convert char to bit
                        getRsVal(pColData, iColDataLen, SQL_BIT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);
                    }
                    break;
                }

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    // SQL_NUMERIC/SQL_DECIMAL to SQL_C_BIT/SQL_C_TINYINT/SQL_C_STINYINT/SQL_C_UTINYINT conversion
                    if (!IS_TEXT_FORMAT(format)) {
                        SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
                        char tempBuf[MAX_TEMP_BUF_LEN];
                        char *pNumData = tempBuf;
                        int num_data_len = sizeof(tempBuf);

                        convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

                        if (pNumData[0] == 't'
                            || pNumData[0] == 'T'
                            || pNumData[0] == '1')
                        {
                            rsVal.bVal = 1;
                        }
                        else
                            rsVal.bVal = 0;
                        rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);
                    }
                    else {
                        rc = convertStringNumericToIntegerCType(pStmt, pColData, iColDataLen, pBuf, pcbLenInd, hType);
                    }
                    break;
                }

                case SQL_SMALLINT:
                {
                    if (hType == SQL_C_BIT)
                    {
                        unsigned char bitVal;
                        rc = rsIntToBit(rsVal.hVal, &bitVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBooleanData(bitVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // Treating SQL_C_TINYINT same as SQL_C_STINYINT (signed 8-bit)
                    else if (hType == SQL_C_STINYINT || hType == SQL_C_TINYINT) {
                        signed char tinyintVal;
                        rc = rsIntToTinyint(rsVal.hVal, &tinyintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getTinyIntData(tinyintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_UTINYINT
                    unsigned char utinyintVal;
                    rc = rsIntToUTinyint(rsVal.hVal, &utinyintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUTinyIntData(utinyintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_INTEGER:
                {
                    if (hType == SQL_C_BIT) {
                        unsigned char bitVal;
                        rc = rsIntToBit(rsVal.iVal, &bitVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBooleanData(bitVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // Treating SQL_C_TINYINT same as SQL_C_STINYINT (signed 8-bit)
                    else if (hType == SQL_C_STINYINT || hType == SQL_C_TINYINT) {
                        signed char tinyintVal;
                        rc = rsIntToTinyint(rsVal.iVal, &tinyintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getTinyIntData(tinyintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_UTINYINT
                    unsigned char utinyintVal;
                    rc = rsIntToUTinyint(rsVal.iVal, &utinyintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUTinyIntData(utinyintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_BIGINT:
                {
                    if (hType == SQL_C_BIT) {
                        unsigned char bitVal;
                        rc = rsIntToBit(rsVal.llVal, &bitVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBooleanData(bitVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // Treating SQL_C_TINYINT same as SQL_C_STINYINT (signed 8-bit)
                    else if (hType == SQL_C_STINYINT || hType == SQL_C_TINYINT) {
                        signed char tinyintVal;
                        rc = rsIntToTinyint(rsVal.llVal, &tinyintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getTinyIntData(tinyintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // SQL_C_UTINYINT
                    unsigned char utinyintVal;
                    rc = rsIntToUTinyint(rsVal.llVal, &utinyintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUTinyIntData(utinyintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_REAL:
                {
                    if (!isfinite(rsVal.fVal)) {
                        addError(&pStmt->pErrorList, "22003", "Value out of range: cannot convert NaN or infinity to integer", 0, NULL);
                        return SQL_ERROR;
                    }
                    // Per ODBC spec for SQL_C_BIT:
                    // - If data is 0 or 1: Return data
                    // - If data is > 0, < 1 or > 1, < 2: Truncate fractional part, SQLSTATE 01S07
                    // - If data is < 0 or >= 2: Undefined, SQLSTATE 22003
                    if (hType == SQL_C_BIT) {
                        unsigned char bitVal;
                        rc = rsFloatToBit(rsVal.fVal, &bitVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBooleanData(bitVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc; // Override rc only if it's not success
                        }
                        break;
                    }
                    else if (hType == SQL_C_TINYINT || hType == SQL_C_STINYINT) {
                        signed char tinyintVal;
                        rc = rsFloatToTinyInt(rsVal.fVal, &tinyintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getTinyIntData(tinyintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // case for SQL_C_UTINYINT
                    unsigned char utinyintVal;
                    rc = rsFloatToUTinyInt(rsVal.fVal, &utinyintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUTinyIntData(utinyintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    if (!isfinite(rsVal.dVal)) {
                        addError(&pStmt->pErrorList, "22003", "Value out of range: cannot convert NaN or infinity to integer", 0, NULL);
                        return SQL_ERROR;
                    }
                    if (hType == SQL_C_BIT) {
                        unsigned char bitVal;
                        rc = rsFloatToBit(rsVal.dVal, &bitVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getBooleanData(bitVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc; // Override rc only if it's not success
                        }
                        break;
                    }
                    else if (hType == SQL_C_TINYINT || hType == SQL_C_STINYINT) {
                        signed char tinyintVal;
                        rc = rsFloatToTinyInt(rsVal.dVal, &tinyintVal, &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            return rc;
                        }
                        SQLRETURN dataRc = getTinyIntData(tinyintVal, pBuf, pcbLenInd);
                        if (dataRc != SQL_SUCCESS) {
                            rc = dataRc;
                        }
                        break;
                    }
                    // case for SQL_C_UTINYINT
                    unsigned char utinyintVal;
                    rc = rsFloatToUTinyInt(rsVal.dVal, &utinyintVal, &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    SQLRETURN dataRc = getUTinyIntData(utinyintVal, pBuf, pcbLenInd);
                    if (dataRc != SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }
            } // SQL Type

            break;
        } // SQL_C_BIT OR SQL_C_TINYINT

        case SQL_C_TYPE_DATE:
        case SQL_C_DATE:
        {
            switch(hSQLType)
            {
                case SQL_TYPE_DATE:
                case SQL_DATE:
                {
                    rc = getDateData(&(rsVal.dtVal), pBuf, pcbLenInd);
                    break;
                }

                case SQL_TYPE_TIMESTAMP:
                case SQL_TIMESTAMP:
                {
                    DATE_STRUCT dtVal;

                    dtVal.year  = rsVal.tsVal.year;
                    dtVal.month = rsVal.tsVal.month;
                    dtVal.day = rsVal.tsVal.day;

                    rc = getDateData(&dtVal, pBuf, pcbLenInd);

                    // Check if time portion is non-zero (requires truncation warning)
                    if (!isTimePortionZero(&rsVal.tsVal)) {
                        addWarning(&pStmt->pErrorList, "01S07", "Time portion truncated", 0, NULL);
                        rc = SQL_SUCCESS_WITH_INFO;
                    }

                    break;
                }
                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WVARCHAR: 
                case SQL_WLONGVARCHAR:{
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_TYPE_DATE/SQL_C_DATE conversion
                    if (iColDataLen <= 0) {
                        // Empty string - initialize date to zeros
                        memset(&(rsVal.dtVal), 0, sizeof(DATE_STRUCT));
                        rc = getDateData(&(rsVal.dtVal), pBuf, pcbLenInd);
                        break;
                    }
                    if (IS_TEXT_FORMAT(format)) {
                        // Ensure null-terminated string
                        char tempBuf[MAX_TEMP_BUF_LEN];
                        makeNullTerminateIntVal(pColData, iColDataLen, tempBuf, MAX_TEMP_BUF_LEN);
                        char *pDateStr = tempBuf;

                        // Parse the date string
                        rc = parseDateString(pDateStr, &(rsVal.dtVal),
                                            &pStmt->pErrorList);
                        if (rc == SQL_ERROR) {
                            break;
                        }
                        // Copy data to output buffer if parsing was successful
                        SQLRETURN dataRc =
                            getDateData(&(rsVal.dtVal), pBuf, pcbLenInd);
                        // We only override the earlier return code when it's SQL_SUCCESS
                        // and getDateData returns a different code
                        if (rc == SQL_SUCCESS) {
                            rc = dataRc;
                        }
                    }
                    // TODO: Add binary conversion for binary format
                    else {
                        iConversionError = TRUE;
                    }
                    break;
                }
                case SQL_BIT:
                case SQL_NUMERIC:
                case SQL_DECIMAL:
                case SQL_SMALLINT:
                case SQL_INTEGER:
                case SQL_BIGINT:
                case SQL_REAL:
                case SQL_FLOAT:
                case SQL_DOUBLE:
                case SQL_TYPE_TIME:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_TYPE_DATE

        case SQL_C_TYPE_TIMESTAMP:
        case SQL_C_TIMESTAMP:
        {
            switch(hSQLType)
            {
                case SQL_TYPE_TIMESTAMP:
                case SQL_TIMESTAMP:
                {
                    rc = getTimeStampData(&(rsVal.tsVal), pBuf, pcbLenInd);
                    break;
                }
                case SQL_TYPE_TIME:
                case SQL_TIME:
                {
                    TIMESTAMP_STRUCT tsVal;

                    // Get current date using thread-safe functions
                    time_t now = time(NULL);
                    struct tm timeinfo_buf;
                    struct tm *timeinfo = NULL;

#ifdef WIN32
                    if (localtime_s(&timeinfo_buf, &now) == 0) {
                        timeinfo = &timeinfo_buf;
                    }
#else
                    timeinfo = localtime_r(&now, &timeinfo_buf);
#endif
                    if (!timeinfo) {
                        // If we can't get current date, return error
                        rc = SQL_ERROR;
                        if (pStmt) {
                            addError(&pStmt->pErrorList, "HY000", "Unable to get current date for TIME to TIMESTAMP conversion", 0, NULL);
                        }
                        break;
                    }

                    // Set date fields to current date
                    tsVal.year = timeinfo->tm_year + 1900; // tm_year is years since 1900
                    tsVal.month = timeinfo->tm_mon + 1; // tm_mon is 0-11
                    tsVal.day = timeinfo->tm_mday; // tm_mday is 1-31

                    tsVal.hour = rsVal.tVal.sqltVal.hour;
                    tsVal.minute = rsVal.tVal.sqltVal.minute;
                    tsVal.second = rsVal.tVal.sqltVal.second;
                    tsVal.fraction = rsVal.tVal.fraction;

                    rc = getTimeStampData(&tsVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_DATE:
                {
                    TIMESTAMP_STRUCT tsVal;

                    tsVal.year  = rsVal.dtVal.year;
                    tsVal.month = rsVal.dtVal.month;
                    tsVal.day = rsVal.dtVal.day;
                    tsVal.hour = 0;
                    tsVal.minute = 0;
                    tsVal.second = 0;
                    tsVal.fraction = 0;

                    rc = getTimeStampData(&tsVal, pBuf, pcbLenInd);
                    break;
                }
                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
				case SQL_LONGVARCHAR:
                case SQL_WVARCHAR:
				case SQL_WLONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_TYPE_TIMESTAMP/SQL_C_TIMESTAMP conversion
                    if (iColDataLen <= 0) {
                        // Empty string - initialize timestamp to zeros
                        memset(&(rsVal.tsVal), 0, sizeof(TIMESTAMP_STRUCT));
                        rc = getTimeStampData(&(rsVal.tsVal), pBuf, pcbLenInd);
                        break;
                    }

                    char *pTimestampStr;
                    if (IS_TEXT_FORMAT(format)) {
                        // Ensure null-terminated string
                        char tempBuf[MAX_TEMP_BUF_LEN];
                        makeNullTerminateIntVal(pColData, iColDataLen, tempBuf, MAX_TEMP_BUF_LEN);
                        pTimestampStr = tempBuf;
                    } else {
                        // Binary format - convert using provided function
                        char timestampBuf[MAX_TEMP_BUF_LEN];
                        char *pTimeZone =
                            libpqParameterStatus(pStmt->phdbc, "TimeZone");
                        len = timestamp_out(rsVal.llVal, timestampBuf,
                                            MAX_TEMP_BUF_LEN, pTimeZone);
                        if (len > 0) {
                            pTimestampStr = timestampBuf;
                        }
                    }

                    // Parse the timestamp string
                    rc = parseTimestampString(pTimestampStr, &(rsVal.tsVal),
                                              &pStmt->pErrorList);
                    if (rc == SQL_ERROR) {
                        break;
                    }
                    // Copy data to output buffer if parsing was successful
                    SQLRETURN dataRc =
                        getTimeStampData(&(rsVal.tsVal), pBuf, pcbLenInd);
                    // We only override the earlier return code when it's SQL_SUCCESS
                    // and getTimeStampData returns a different code
                    if (rc == SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_BIT:
                case SQL_TINYINT:
                case SQL_NUMERIC:
                case SQL_DECIMAL:
                case SQL_SMALLINT:
                case SQL_INTEGER:
                case SQL_BIGINT:
                case SQL_REAL:
                case SQL_FLOAT:
                case SQL_DOUBLE:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_TYPE_TIMESTAMP

        case SQL_C_INTERVAL_YEAR_TO_MONTH:
        {
            switch(hSQLType)
            {
                case SQL_INTERVAL_YEAR_TO_MONTH:
                {
                    rc = getIntervalY2MData(&(rsVal.intervalVal), pBuf, pcbLenInd);
                    break;
                }
                case SQL_CHAR:
                case SQL_VARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_INTERVAL_YEAR_TO_MONTH conversion
                    rc = convertCharToIntervalY2M(pColData, iColDataLen, format,
                                                  &(rsVal.intervalVal), pBuf, pcbLenInd,
                                                  &pStmt->pErrorList);
                    break;
                }
                default:
                {
                    iConversionError = TRUE;
                    break;
                }
            }

            break;
        } // SQL_C_INTERVAL_YEAR_TO_MONTH

        case SQL_C_INTERVAL_DAY_TO_SECOND:
        {
            switch(hSQLType)
            {
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    rc = getIntervalD2SData(&(rsVal.intervalVal), pBuf, pcbLenInd);
                    break;
                }
                case SQL_CHAR:
                case SQL_VARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_INTERVAL_DAY_TO_SECOND conversion
                    rc = convertCharToIntervalD2S(pColData, iColDataLen, format,
                                                  &(rsVal.intervalVal), pBuf, pcbLenInd,
                                                  &pStmt->pErrorList);
                    break;
                }
                default:
                {
                    iConversionError = TRUE;
                    break;
                }
            }

            break;
        } // SQL_C_INTERVAL_DAY_TO_SECOND

        case SQL_C_TYPE_TIME:
        case SQL_C_TIME:
        {
            switch(hSQLType)
            {
                case SQL_TYPE_TIME:
                case SQL_TIME:
                {
                    rc = getTimeData(&(rsVal.tVal), pBuf, pcbLenInd);
                    break;
                }
                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
				case SQL_LONGVARCHAR:
				case SQL_WVARCHAR:
				case SQL_WLONGVARCHAR: {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_TYPE_TIME/SQL_C_TIME conversion
                    if (iColDataLen <= 0) {
                        // Empty string - initialize time to zeros
                        memset(&(rsVal.tVal), 0, sizeof(RS_TIME_STRUCT));
                        rc = getTimeData(&(rsVal.tVal), pBuf, pcbLenInd);
                        break;
                    }
                    char *pTimeStr;
                    if (IS_TEXT_FORMAT(format)) {
                        // Ensure null-terminated string
                        char tempBuf[MAX_TEMP_BUF_LEN];
                        makeNullTerminateIntVal(pColData, iColDataLen, tempBuf, MAX_TEMP_BUF_LEN);
                        pTimeStr = tempBuf;
                    } else {
                        // Binary format - convert using provided function
                        char timeBuf[MAX_TEMP_BUF_LEN];
                        len = time_out(rsVal.tzVal.time, timeBuf,
                                       MAX_TEMP_BUF_LEN, &(rsVal.tzVal.zone));
                        if (len > 0)
                            pTimeStr = timeBuf;
                    }

                    // Parse the time string
                    rc = parseTimeString(pTimeStr, &(rsVal.tVal),
                                         &pStmt->pErrorList);

                    if (rc == SQL_ERROR) {
                        return rc;
                    }
                    // ODBC TIME_STRUCT doesn't have a fraction part, so if there is non-zero fraction we flag it as truncation
                    if (rsVal.tVal.fraction) {
                        addWarning(&pStmt->pErrorList, "01S07", "Fractional truncation", 0, NULL);
                        rc = SQL_SUCCESS_WITH_INFO;
                    }
                    // Copy data to output buffer if parsing was successful
                    SQLRETURN dataRc =
                        getTimeData(&(rsVal.tVal), pBuf, pcbLenInd);
                    // We only override the earlier return code when it's SQL_SUCCESS
                    // and getTimeData returns a different code
                    if (rc == SQL_SUCCESS) {
                        rc = dataRc;
                    }
                    break;
                }

                case SQL_TYPE_TIMESTAMP:
				case SQL_TIMESTAMP:
				{
					char *pTemp;
					char tempBuf[MAX_TEMP_BUF_LEN];
					char szFraction[32]; // Billionth of a second

					rsVal.tsVal.fraction = 0;
					szFraction[0] = '\0';

					if (IS_TEXT_FORMAT(format))
					{
						pTemp = pColData;
					}
					else
					{
						len = timestamp_out(rsVal.llVal, (char *)tempBuf, MAX_TEMP_BUF_LEN, NULL);
						pTemp = tempBuf;
					}

					// Not using  timezone value.
					sscanf(pColData, "%4hd-%2hd-%2hd %2hd:%2hd:%2hd.%s", &(rsVal.tsVal.year),
						&(rsVal.tsVal.month), &(rsVal.tsVal.day),
						&(rsVal.tsVal.hour), &(rsVal.tsVal.minute), &(rsVal.tsVal.second),
						szFraction);

					if (szFraction[0] != '\0')
						rsVal.tsVal.fraction = atoi(szFraction);

                    // Copy only TIME part
                    rsVal.tVal.sqltVal.hour = rsVal.tsVal.hour;
                    rsVal.tVal.sqltVal.minute = rsVal.tsVal.minute;
                    rsVal.tVal.sqltVal.second = rsVal.tsVal.second;
                    rsVal.tVal.fraction = rsVal.tsVal.fraction;

                    rc = getTimeData(&(rsVal.tVal), pBuf, pcbLenInd);

                     // Check if fractional seconds are present
                    if (rsVal.tsVal.fraction) {
                        addWarning(&pStmt->pErrorList, "01S07", "Fractional truncation", 0, NULL);
                        rc = SQL_SUCCESS_WITH_INFO;
                    }


                    break;
                }

                case SQL_BIT:
                case SQL_TINYINT:
                case SQL_NUMERIC:
                case SQL_SMALLINT:
                case SQL_INTEGER:
                case SQL_BIGINT:
                case SQL_REAL:
                case SQL_FLOAT:
                case SQL_DOUBLE:
                case SQL_TYPE_DATE:
                case SQL_DATE:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_TYPE_TIME

        case SQL_C_NUMERIC:
        {
            switch(hSQLType)
            {
                case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
                    SQLRETURN convertRc =
                        convertNumericStringToScaledIntegerExtended(
                            pStmt, pColData, iColDataLen, pnVal);
                    if (convertRc == SQL_ERROR) {
                        return SQL_ERROR;
                    }
                    // Now put numeric into app buf
                    rc = getNumericData(pnVal, pBuf, pcbLenInd);

                    if (rc == SQL_SUCCESS &&
                        convertRc == SQL_SUCCESS_WITH_INFO) {
                        rc = SQL_SUCCESS_WITH_INFO;
                    }
                    break;
                }

                case SQL_CHAR:
				case SQL_WCHAR:
				case SQL_VARCHAR:
				case SQL_LONGVARCHAR:
				case SQL_WVARCHAR:
				case SQL_WLONGVARCHAR: {
                    // SQL_CHAR/SQL_VARCHAR to SQL_C_NUMERIC conversion
                    if (IS_TEXT_FORMAT(format)) {
                        char tempBuf[MAX_NUMBER_BUF_LEN + 1];
                        char *numStr;
                        int truncated = 0;
                        // Convert char to Numeric
                        SQLRETURN preprc = prepareStringForNumericConversion(
                            pStmt, pColData, iColDataLen, tempBuf, sizeof(tempBuf),
                            &numStr, &truncated);
                        if (preprc != SQL_SUCCESS) {
                            rc = preprc;
                            goto error;
                        }

                        SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);

                        // Convert numeric string buffer to scaled integer (in
                        // little endian mode)
                        SQLRETURN convertRc =
                            convertNumericStringToScaledIntegerExtended(
                                pStmt, numStr, strlen(numStr), pnVal);
                        if (convertRc == SQL_ERROR) {
                            rc = convertRc;
                            goto error;
                        }
                        // Now put numeric into app buf
                        rc = getNumericData(pnVal, pBuf, pcbLenInd);
                        // Preserve conversion warnings when data retrieval is successful
                        if (convertRc == SQL_SUCCESS_WITH_INFO) {
                            rc = SQL_SUCCESS_WITH_INFO;
                        }
                    } else {
                        // Convert char to Numeric
                        getRsVal(pColData, iColDataLen, SQL_NUMERIC, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
                        rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);
                    }
                    break;
                }

                case SQL_BIT:
                {
                    // Create a SQL_NUMERIC_STRUCT with the value
                    SQL_NUMERIC_STRUCT numVal;
                    memset(&numVal, 0, sizeof(SQL_NUMERIC_STRUCT));
                    numVal.precision = 1;
                    numVal.scale = 0;
                    numVal.sign = 1;  // Always positive for BIT
                    numVal.val[0] = rsVal.bVal;
                    rc = getNumericData(&numVal, pBuf, pcbLenInd);
                    break;
                }
                case SQL_TINYINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to Numeric
						getRsVal(pColData, iColDataLen, SQL_NUMERIC, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;

						*pNumData = (rsVal.bVal == 1) ? '1' : '0';
						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

                case SQL_SMALLINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to Numeric
						getRsVal(pColData, iColDataLen, SQL_NUMERIC, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;

						snprintf(pNumData, sizeof(tempBuf), "%hd", rsVal.hVal);

						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

                case SQL_INTEGER:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to Numeric
						getRsVal(pColData, iColDataLen, SQL_NUMERIC, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;

						snprintf(pNumData, sizeof(tempBuf), "%d", rsVal.iVal);

						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

                case SQL_BIGINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to Numeric
						getRsVal(pColData, iColDataLen, SQL_NUMERIC, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;

						snprintf(pNumData, sizeof(tempBuf), "%lld", rsVal.llVal);

						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

                case SQL_REAL:
				{
                    if (!isfinite(rsVal.fVal)) {
                        addError(&pStmt->pErrorList, "22003", "Value out of range: cannot convert NaN or infinity to integer", 0, NULL);
                        return SQL_ERROR;
                    }
					if (IS_TEXT_FORMAT(format))
					{
                        SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
                        SQLRETURN convertRc =
                            convertNumericStringToScaledIntegerExtended(
                                pStmt, pColData, iColDataLen, pnVal);
                        if (convertRc == SQL_ERROR) {
                            return SQL_ERROR;
                        }
                        // Now put numeric into app buf
                        rc = getNumericData(pnVal, pBuf, pcbLenInd);

                        if (rc == SQL_SUCCESS &&
                            convertRc == SQL_SUCCESS_WITH_INFO) {
                            rc = SQL_SUCCESS_WITH_INFO;
                        }
                        break;
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;

						snprintf(pNumData, sizeof(tempBuf), "%f", rsVal.fVal);

						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

				case SQL_FLOAT:
				case SQL_DOUBLE:
				{
                    if (!isfinite(rsVal.dVal)) {
                        addError(&pStmt->pErrorList, "22003", "Value out of range: cannot convert NaN or infinity to integer", 0, NULL);
                        return SQL_ERROR;
                    }
					if (IS_TEXT_FORMAT(format))
					{
                        SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
                        SQLRETURN convertRc =
                            convertNumericStringToScaledIntegerExtended(
                                pStmt, pColData, iColDataLen, pnVal);
                        if (convertRc == SQL_ERROR) {
                            return SQL_ERROR;
                        }
                        // Now put numeric into app buf
                        rc = getNumericData(pnVal, pBuf, pcbLenInd);

                        if (rc == SQL_SUCCESS &&
                            convertRc == SQL_SUCCESS_WITH_INFO) {
                            rc = SQL_SUCCESS_WITH_INFO;
                        }
                        break;
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = &(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;

						snprintf(pNumData, sizeof(tempBuf), "%g", rsVal.dVal);

						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

                case SQL_TYPE_DATE:
                case SQL_TYPE_TIMESTAMP:
                case SQL_TYPE_TIME:
                case SQL_DATE:
                case SQL_TIMESTAMP:
                case SQL_TIME:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL Type

            break;
        } // SQL_C_NUMERIC

		case SQL_C_BINARY:
		{
			switch (hSQLType)
			{
				case SQL_BINARY:
				case SQL_VARBINARY:
				case SQL_LONGVARBINARY:
				{
					if (hRsSpecialType == GEOMETRY
						|| ((!IS_TEXT_FORMAT(format))
							&& (hRsSpecialType == VARBYTE
								|| hRsSpecialType == GEOGRAPHY
								|| hRsSpecialType == GEOMETRYHEX))
						)
					{
						// Already in Binary format
						rc = copyVariableToCBinary(rsVal.pcVal, iColDataLen,
												   pBuf, cbLen, cbLenOffset, pcbLenInd, pStmt);
					}
					else
					{
						// Convert HEX format to Binary
						rc = copyHexToBinaryDataBigLen(rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, pcbLenInd, cbLenOffset);
					}
					break;
				}

				case SQL_DATE:
				case SQL_TYPE_DATE:
				{
					rc = copyToCBinary(&rsVal.dtVal, sizeof(DATE_STRUCT),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_DATE");
					break;
				}

				case SQL_INTERVAL_YEAR_TO_MONTH:
				case SQL_INTERVAL_DAY_TO_SECOND:
				{
					rc = copyToCBinary(&rsVal.intervalVal, sizeof(SQL_INTERVAL_STRUCT),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_INTERVAL");
					break;
				}

				case SQL_TYPE_TIME:
				case SQL_TIME:
				{
					rc = copyToCBinary(&rsVal.tVal.sqltVal, sizeof(TIME_STRUCT),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_TIME");
					break;
				}

				case SQL_TYPE_TIMESTAMP:
				case SQL_TIMESTAMP:
				{
					rc = copyToCBinary(&rsVal.tsVal, sizeof(TIMESTAMP_STRUCT),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_TIMESTAMP");
					break;
				}

				case SQL_BIT:
				case SQL_TINYINT:
				{
					rc = copyToCBinary(&rsVal.bVal, sizeof(char),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_BIT");
					break;
				}

				case SQL_SMALLINT:
				{
					rc = copyToCBinary(&rsVal.hVal, sizeof(short),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_SMALLINT");
					break;
				}

				case SQL_INTEGER:
				{
					rc = copyToCBinary(&rsVal.iVal, sizeof(int),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_INTEGER");
					break;
				}

				case SQL_BIGINT:
				{
					rc = copyToCBinary(&rsVal.llVal, sizeof(long long),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_BIGINT");
					break;
				}

				case SQL_REAL:
				{
					rc = copyToCBinary(&rsVal.fVal, sizeof(float),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_REAL");
					break;
				}

				case SQL_FLOAT:
				case SQL_DOUBLE:
				{
					rc = copyToCBinary(&rsVal.dVal, sizeof(double),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_FLOAT");
					break;
				}

				case SQL_NUMERIC:
				case SQL_DECIMAL:
				{
					rc = copyToCBinary(&rsVal.nVal, sizeof(SQL_NUMERIC_STRUCT),
									   pBuf, cbLen, pcbLenInd, pStmt, "SQL_NUMERIC");
					break;
				}

				case SQL_CHAR:
				case SQL_VARCHAR:
				case SQL_LONGVARCHAR:
				case SQL_WCHAR:
				case SQL_WVARCHAR:
				case SQL_WLONGVARCHAR:
				{
					rc = copyVariableToCBinary(rsVal.pcVal, iColDataLen,
											   pBuf, cbLen, cbLenOffset, pcbLenInd, pStmt);
					break;
				}

				default:
				{
					iConversionError = TRUE;
					break;
				}
			}

		break;
		} // SQL_C_BINARY

        case SQL_C_DEFAULT:
        {
            switch(hSQLType)
            {
                case SQL_CHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
                {
                    // SQL_CHAR/SQL_VARCHAR/SQL_LONGVARCHAR to SQL_C_DEFAULT conversion
                    rc = copyStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    break;
                }
                // TODO: Modify this case to separate out SQL_BIGINT from
                // SQL_NUMERIC/SQL_DECIMAL SQL_BIGINT should appropriately call getBigIntData
                case SQL_BIGINT:
                case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
                    rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    break;
                }

                case SQL_SMALLINT:
                {
                    rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_INTEGER:
                {
                    rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_REAL:
                {
                    rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_BIT:
                case SQL_TINYINT:
                {
                    rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_TYPE_DATE:
                case SQL_DATE:
                {
                    rc = getDateData(&(rsVal.dtVal), pBuf, pcbLenInd);
                    break;
                }

                case SQL_TYPE_TIMESTAMP:
                case SQL_TIMESTAMP:
                {
                    rc = getTimeStampData(&(rsVal.tsVal), pBuf, pcbLenInd);
                    break;
                }

                case SQL_INTERVAL_YEAR_TO_MONTH:
                {
                    rc = getIntervalY2MData(&(rsVal.intervalVal), pBuf, pcbLenInd);
                    break;
                }
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    rc = getIntervalD2SData(&(rsVal.intervalVal), pBuf, pcbLenInd);
                    break;
                }

                case SQL_TYPE_TIME:
                case SQL_TIME:
                {
                    rc = getTimeData(&(rsVal.tVal), pBuf, pcbLenInd);
                    break;
                }

				case SQL_BINARY:
				case SQL_LONGVARBINARY:
				{
					rc = copyBinaryDataBigLen(rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, pcbLenInd);
					break;
				}

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // SQL type

            break;
        } // SQL_C_DEFAULT

        default:
        {
            iConversionError = TRUE;
            break;
        }
    } // C Type 


    if(iConversionError) {
        RS_LOG_ERROR("RSUTIL",
                     "Fetch data type conversion is not supported from "
                     "hCType=%d hSQLType=%d "
                     "format=%d iColDataLen%d iConversion=%d",
                     hType, hSQLType, format, iColDataLen, iConversion);
        char szErrMsg[MAX_ERR_MSG_LEN];

        snprintf(szErrMsg, sizeof(szErrMsg), "Fetch data type conversion is not supported from %hd SQL type to %hd C type.", hSQLType,hType);

        rc = SQL_ERROR;

        if(pStmt) {
            addError(&pStmt->pErrorList,"07006", szErrMsg, 0, NULL);
        }
    }

error:

    return rc;
}
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get server data into a union depending upon column type.
// Libpq always return data as string.
// Return TRUE if RS_VALUE contain values, otherwise FALSE
int getRsVal(char *pColData, int iColDataLen, short hSQLType, RS_VALUE  *pRsVal, short hCType, int format, RS_DESC_REC *pDescRec, 
			short hRsSpecialType, bool isTextData)
{
    char szNumBuf[MAX_NUMBER_BUF_LEN + 1];

	if((hCType == SQL_C_CHAR
		|| hCType == SQL_C_WCHAR)
		&&
		(hSQLType == SQL_SMALLINT
			|| hSQLType == SQL_INTEGER
			|| hSQLType == SQL_BIGINT
			|| hSQLType == SQL_REAL
			|| hSQLType == SQL_FLOAT
			|| hSQLType == SQL_DOUBLE
			|| hSQLType == SQL_TYPE_DATE
			|| hSQLType == SQL_DATE
			|| hSQLType == SQL_TYPE_TIMESTAMP
			|| hSQLType == SQL_TYPE_TIME
			|| hSQLType == SQL_TIMESTAMP
			|| hSQLType == SQL_TIME
			|| hSQLType == SQL_NUMERIC
			|| hSQLType == SQL_DECIMAL
			|| hSQLType == SQL_INTERVAL_YEAR_TO_MONTH
			|| hSQLType == SQL_INTERVAL_DAY_TO_SECOND
			|| hRsSpecialType == TIMETZOID
			|| hRsSpecialType == TIMESTAMPTZOID
			)
	)
	{
		if (IS_TEXT_FORMAT(format))
		{
			// No conversion happens. We directly write as string from source to destination in the caller function.
			return FALSE;
		}
		else
		{
			switch (hSQLType)
			{
				case SQL_SMALLINT:
				case SQL_INTEGER:
				case SQL_BIGINT:
				case SQL_REAL:
				case SQL_FLOAT:
				case SQL_DOUBLE:
				case SQL_TYPE_DATE:
				case SQL_DATE:
				case SQL_TYPE_TIMESTAMP:
				case SQL_TIMESTAMP:
				case SQL_TYPE_TIME:
				case SQL_TIME:
				case SQL_NUMERIC:
				case SQL_DECIMAL:
				case SQL_INTERVAL_YEAR_TO_MONTH:
				case SQL_INTERVAL_DAY_TO_SECOND:
				{
					break; // There is a switch below, it will take care of it.
				}
				
				default:
				{
					if (hRsSpecialType != TIMETZOID && hRsSpecialType != TIMESTAMPTZOID)
						return FALSE;
					else
						break;
				}
			}
		}
	}
	else
	if((hCType == SQL_C_DEFAULT)
		&& 
		( hSQLType == SQL_BIGINT
			|| hSQLType == SQL_NUMERIC
			|| hSQLType == SQL_DECIMAL
		)
	)
	{
		if (IS_TEXT_FORMAT(format))
		{
			// No conversion happens. We directly write as string from source to destination in the caller function.
			return FALSE;
		}
		else
		{
			switch (hSQLType)
			{
				case SQL_BIGINT:
				case SQL_NUMERIC:
				case SQL_DECIMAL:
				{
					break; // There is a switch below, it will take care of it.
				}

				default:
				{
					return FALSE;
				}
			}
		}
	}

    if(pColData)
    {
        switch(hSQLType)
        {
            case SQL_CHAR:
            case SQL_WCHAR:
			{
				pRsVal->pcVal = pColData;
				break;
			}

            case SQL_VARCHAR:
            case SQL_LONGVARCHAR:
            case SQL_WVARCHAR:
            case SQL_WLONGVARCHAR:
            {
				if ((hRsSpecialType == TIMETZOID
						|| hRsSpecialType == TIMESTAMPTZOID)
					 &&  !(IS_TEXT_FORMAT(format))
					)
				{
					if (hRsSpecialType == TIMETZOID)
					{
						if (iColDataLen > 0)
						{
							pRsVal->tzVal.time = getInt64FromBinary(pColData, 0);
							pRsVal->tzVal.zone = getInt32FromBinary(pColData, 8);
						}
						else
						{
							memset(&(pRsVal->tzVal), '\0', sizeof(RS_TIMETZ_STRUCT));
						}
					}
					else
					if (hRsSpecialType == TIMESTAMPTZOID)
					{
						if (iColDataLen > 0)
							pRsVal->llVal = getInt64FromBinary(pColData, 0);
						else
							pRsVal->llVal = 0;
					}

				}
				else
					pRsVal->pcVal = pColData;

                break;
            }

			case SQL_LONGVARBINARY:
			{
				pRsVal->pcVal = pColData;
				break;
			}

            case SQL_SMALLINT:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[iColDataLen - 1] == '\0')
							pRsVal->hVal = (short)atoi(pColData);
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							pRsVal->hVal = (short)atoi(szNumBuf);
						}
					}
					else
					{
						pRsVal->hVal = (((pColData[0] & 255) << 8) + ((pColData[1] & 255)));
					}
                }
                else
                    pRsVal->hVal = 0;

                break;
            }

            case SQL_INTEGER:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[iColDataLen - 1] == '\0')
							pRsVal->iVal = atoi(pColData);
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							pRsVal->iVal = atoi(szNumBuf);
						}
					}
					else
					{
						pRsVal->iVal = getInt32FromBinary(pColData, 0);
					}
                }
                else
                    pRsVal->iVal = 0;

                break;
            }

            case SQL_BIGINT:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[iColDataLen - 1] == '\0')
						{
							sscanf(pColData, "%lld", &(pRsVal->llVal));
						}
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							sscanf(szNumBuf, "%lld", &(pRsVal->llVal));
						}
					}
					else
					{
						pRsVal->llVal = getInt64FromBinary(pColData, 0);
					}
                }
                else
                    pRsVal->llVal = 0;

                break;
            }

            case SQL_REAL:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[iColDataLen - 1] == '\0')
							pRsVal->fVal = (float)atof(pColData);
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							pRsVal->fVal = (float)atof(szNumBuf);
						}
					}
					else
					{
						int iVal = getInt32FromBinary(pColData, 0);

						pRsVal->fVal = *(float *)(&iVal);
					}
                }
                else
                    pRsVal->fVal = 0.0;

                break;
            }

            case SQL_FLOAT:
            case SQL_DOUBLE:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[iColDataLen - 1] == '\0')
							pRsVal->dVal = atof(pColData);
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							pRsVal->dVal = atof(szNumBuf);
						}
					}
					else
					{
						long long llVal = getInt64FromBinary(pColData, 0);

						pRsVal->dVal = *(double *)(&llVal);
					}
                }
                else
                    pRsVal->dVal = 0.0;

                break;
            }

            case SQL_BIT:
            case SQL_TINYINT:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[0] == 't'
							|| pColData[0] == 'T'
							|| pColData[0] == '1')
						{
							pRsVal->bVal = 1;
						}
						else
							pRsVal->bVal = 0;
					}
					else
					{
						pRsVal->bVal = (pColData[0] == 1) ? 1 : 0;
					}
                }
                else
                    pRsVal->bVal = 0;

                break;
            }

            case SQL_TYPE_DATE:
            case SQL_DATE:
            {
                if(iColDataLen > 0)
                {
                    if (IS_TEXT_FORMAT(format) || isTextData) {
                        char *pDateStr;

                        if (pColData[iColDataLen - 1] == '\0') {
                            pDateStr = pColData;
                        } else {
                            makeNullTerminateIntVal(pColData, iColDataLen,
                                                    szNumBuf,
                                                    MAX_NUMBER_BUF_LEN + 1);
                            pDateStr = szNumBuf;
                        }
                        // Parse date with sign-aware logic to handle negative
                        // years (BC dates)
                        if (pDateStr[0] == '-') {
                            sscanf(pDateStr + 1, "%4hd-%2hd-%2hd",
                                   &(pRsVal->dtVal.year),
                                   &(pRsVal->dtVal.month),
                                   &(pRsVal->dtVal.day));
                            pRsVal->dtVal.year = -pRsVal->dtVal.year;
                        } else {
                            // Parse without sign for positive years
                            sscanf(pDateStr, "%4hd-%2hd-%2hd",
                                   &pRsVal->dtVal.year, &(pRsVal->dtVal.month),
                                   &(pRsVal->dtVal.day));
                        }
                    } else {
                        pRsVal->iVal = getInt32FromBinary(pColData, 0);
                    }
                }
                else
                {
                    memset(&(pRsVal->dtVal), '\0', sizeof(DATE_STRUCT));
                }

                break;
            }

            case SQL_TYPE_TIMESTAMP:
            case SQL_TIMESTAMP:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						char szFraction[32]; // Billionth of a second
						int fractionLen;
						int i;

						pRsVal->tsVal.fraction = 0;
						szFraction[0] = '\0';

						if (pColData[iColDataLen - 1] == '\0')
						{
							sscanf(pColData, "%4hd-%2hd-%2hd %2hd:%2hd:%2hd.%s", &(pRsVal->tsVal.year), &(pRsVal->tsVal.month), &(pRsVal->tsVal.day),
								&(pRsVal->tsVal.hour), &(pRsVal->tsVal.minute), &(pRsVal->tsVal.second),
								szFraction);
						}
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							sscanf(szNumBuf, "%4hd-%2hd-%2hd %2hd:%2hd:%2hd.%s", &(pRsVal->tsVal.year), &(pRsVal->tsVal.month), &(pRsVal->tsVal.day),
								&(pRsVal->tsVal.hour), &(pRsVal->tsVal.minute), &(pRsVal->tsVal.second),
								szFraction);
						}

						// Pad zeros at the right
						fractionLen = (int)strlen(szFraction);
						if (hRsSpecialType == TIMESTAMPTZOID)
						{
							fractionLen -= 3;
						}
						if (fractionLen > 0)
						{
							for (i = fractionLen + 1; i < 10; i++)
								szFraction[i - 1] = '0';

							szFraction[9] = '\0';
						}

						sscanf(szFraction, "%9d", (int *)(&(pRsVal->tsVal.fraction)));
					}
					else
					{
						pRsVal->llVal = getInt64FromBinary(pColData, 0);
					}
                }
                else
                {
                    memset(&(pRsVal->tsVal), '\0', sizeof(TIMESTAMP_STRUCT));
                }

                break;
            }

            case SQL_INTERVAL_YEAR_TO_MONTH:
            {
                if(iColDataLen > 0)
                {
                    if (IS_TEXT_FORMAT(format) || isTextData) {
                        if (pColData[iColDataLen - 1] == '\0') {
                            pRsVal->intervalVal =
                                parse_intervaly2m(pColData, iColDataLen);
                        } else {
                            makeNullTerminateIntVal(pColData, iColDataLen,
                                                    szNumBuf,
                                                    MAX_NUMBER_BUF_LEN + 1);
                            pRsVal->intervalVal =
                                parse_intervaly2m(szNumBuf, strlen(szNumBuf));
                        }
                    } else {
                        // Binary format: convert from months to year/month
                        int month = getInt32FromBinary(pColData, 0);
                        struct pg_tm tm = {0};
                        long long fsec = 0;
                        interval2tm(0, month, &tm, &fsec);

                        // Determine if negative based on original month value
                        bool is_negative = (month < 0);

                        memset(&(pRsVal->intervalVal), 0, sizeof(SQL_INTERVAL_STRUCT));
                        pRsVal->intervalVal.interval_type = SQL_IS_YEAR_TO_MONTH;
                        pRsVal->intervalVal.interval_sign = is_negative ? SQL_TRUE : SQL_FALSE;
                        pRsVal->intervalVal.intval.year_month.year = (SQLUINTEGER)(is_negative ? -tm.tm_year : tm.tm_year);
                        pRsVal->intervalVal.intval.year_month.month = (SQLUINTEGER)(is_negative ? -tm.tm_mon : tm.tm_mon);
                    }
                }
                else
                {
                    memset(&(pRsVal->intervalVal), '\0', sizeof(SQL_INTERVAL_STRUCT));
                }

                break;
            }
            case SQL_INTERVAL_DAY_TO_SECOND:
            {
                if(iColDataLen > 0)
                {
                    if (IS_TEXT_FORMAT(format) || isTextData) {
                        if (pColData[iColDataLen - 1] == '\0') {
                            pRsVal->intervalVal =
                                parse_intervald2s(pColData, iColDataLen);
                        } else {
                            makeNullTerminateIntVal(pColData, iColDataLen,
                                                    szNumBuf,
                                                    MAX_NUMBER_BUF_LEN + 1);
                            pRsVal->intervalVal =
                                parse_intervald2s(szNumBuf, strlen(szNumBuf));
                        }
                    } else {
                        // Binary format: convert from microseconds to day/hour/min/sec/fraction
                        long long time = getInt64FromBinary(pColData, 0);
                        struct pg_tm tm = {0};
                        long long fsec = 0;
                        interval2tm(time, 0, &tm, &fsec);

                        // Determine if negative based on original time value
                        bool is_negative = (time < 0);

                        memset(&(pRsVal->intervalVal), 0, sizeof(SQL_INTERVAL_STRUCT));
                        pRsVal->intervalVal.interval_type = SQL_IS_DAY_TO_SECOND;
                        pRsVal->intervalVal.interval_sign = is_negative ? SQL_TRUE : SQL_FALSE;
                        pRsVal->intervalVal.intval.day_second.day = (SQLUINTEGER)(is_negative ? -tm.tm_mday : tm.tm_mday);
                        pRsVal->intervalVal.intval.day_second.hour = (SQLUINTEGER)(is_negative ? -tm.tm_hour : tm.tm_hour);
                        pRsVal->intervalVal.intval.day_second.minute = (SQLUINTEGER)(is_negative ? -tm.tm_min : tm.tm_min);
                        pRsVal->intervalVal.intval.day_second.second = (SQLUINTEGER)(is_negative ? -tm.tm_sec : tm.tm_sec);
                        pRsVal->intervalVal.intval.day_second.fraction = (SQLUINTEGER)(is_negative ? -fsec : fsec);
                    }
                }
                else
                {
                    memset(&(pRsVal->intervalVal), '\0', sizeof(SQL_INTERVAL_STRUCT));
                }

                break;
            }

            case SQL_TYPE_TIME:
            case SQL_TIME:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						char szFraction[32]; // Microsecond
						int fractionLen;
						int i;

						pRsVal->tVal.fraction = 0;
						szFraction[0] = '\0';

						if (pColData[iColDataLen - 1] == '\0')
						{
							sscanf(pColData, "%2hd:%2hd:%2hd.%s", &(pRsVal->tVal.sqltVal.hour), &(pRsVal->tVal.sqltVal.minute), &(pRsVal->tVal.sqltVal.second),
								szFraction);
						}
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							sscanf(szNumBuf, "%2hd:%2hd:%2hd.%s", &(pRsVal->tVal.sqltVal.hour), &(pRsVal->tVal.sqltVal.minute), &(pRsVal->tVal.sqltVal.second),
								szFraction);
						}

						// Pad zeros at the right
						fractionLen = (int)strlen(szFraction);
						if (fractionLen > 0)
						{
							for (i = fractionLen + 1; i < 7; i++)
								szFraction[i - 1] = '0';

							szFraction[6] = '\0';
						}

						sscanf(szFraction, "%6d", (int *)(&(pRsVal->tVal.fraction)));
					}
					else
					{
						pRsVal->llVal = getInt64FromBinary(pColData, 0);
					}
                }
                else
                {
                    memset(&(pRsVal->tVal), '\0', sizeof(RS_TIME_STRUCT));
                }

                break;
            }

            case SQL_NUMERIC:
            case SQL_DECIMAL:
            {
                if(iColDataLen > 0)
                {
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						char *pNumData;
						SQL_NUMERIC_STRUCT *pnVal = &(pRsVal->nVal);

						if (pColData[iColDataLen - 1] == '\0')
						{
							pNumData = pColData;
						}
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);
							pNumData = szNumBuf;
						}

						// Convert numeric string buffer to scaled integer (in little endian mode)
						convertNumericStringToScaledInteger(pNumData, pnVal);
					}
					else
					{
						long long llMsbVal = getInt64FromBinary(pColData, 0);
						long long llLsbVal;
						bool is128 = (iColDataLen > 8);
						SQL_NUMERIC_STRUCT *pnVal = &(pRsVal->nVal);
						char *pTemp;
						int len;
						int i;

						if (is128)
						{
							llLsbVal = getInt64FromBinary(pColData, 8);

							pnVal->sign = (llMsbVal > 0) ? 1 : 0;
						}
						else
						{
							llLsbVal = llMsbVal;
							llMsbVal = 0L;

							pnVal->sign = (llLsbVal > 0) ? 1 : 0;
						}

						pnVal->precision = pDescRec->iSize;
						pnVal->scale = (int)pDescRec->hScale;

						pTemp = (char *)&llLsbVal;
						len = sizeof(llLsbVal);

						for (i = 0; i < len; i++)
							pnVal->val[i] = *pTemp++;

						if (is128)
						{
							pTemp = (char *)&llMsbVal;
							len = sizeof(llMsbVal);

							for (i = 0; i < len; i++)
								pnVal->val[i + 8] = *pTemp++;
						}
					}
                }
                else
                    memset(&(pRsVal->nVal), '\0', sizeof(SQL_NUMERIC_STRUCT));

                break;
            }

            default:
            {
				pRsVal->pcVal = NULL;
                break;
            }

        } // SQL Type
    }
    else
        pRsVal->pcVal = NULL;

	return TRUE;
}

/**
 * @brief Checks if a string contains only digits for the given length.
 */
bool isDigitStr(const char *s, int len) {
    for (int i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Parses a date string in the format YYYY-MM-DD into year, month, and
 * day. This function is used by parseDatePart() and parseTimestampString() and is not intended to be used
 * directly.
 *
 * @param dateStr       The date string to parse (must be 10 characters:
 * "YYYY-MM-DD").
 * @param y             Output year.
 * @param m             Output month.
 * @param d             Output day.
 * @param err           Error list pointer for diagnostics.
 * @return              SQL_SUCCESS or SQL_ERROR.
 */
SQLRETURN parseDatePart(const char *dateStr, int *y, int *m, int *d,
                               RS_ERROR_INFO **err) {

    // Validate minimum length for YYYY-MM-DD format (10 characters) and hyphen positions
    // Position 4: first hyphen after 4-digit year (YYYY-)
    // Position 7: second hyphen after 2-digit month (YYYY-MM-)
    if (!dateStr || strlen(dateStr) < DATE_STRING_LEN || dateStr[4] != '-' ||
        dateStr[7] != '-') {
        goto fmt_err;
    }
    // Validate digit sequences: 4 digits for year, 2 for month, 2 for day
    // dateStr+5: month position (skip YYYY-), dateStr+8: day position (skip YYYY-MM-)
    if (!isDigitStr(dateStr, 4) || !isDigitStr(dateStr + 5, 2) ||
        !isDigitStr(dateStr + 8, 2)) {
        goto fmt_err;
    }
    // Parse YYYY-MM-DD format, expecting exactly 3 values (year, month, day)
    if (sscanf(dateStr, "%4d-%2d-%2d", y, m, d) != 3) {
        goto fmt_err;
    }
    if (!validateDate(*y, *m, *d)) {
        goto fmt_err;
    }
    return SQL_SUCCESS;
fmt_err:
    if (err) {
        addError(err, "22018", "Invalid character value for cast to DATE", 0,
                 NULL);
    }
    return SQL_ERROR;
}

/**
 * @brief Parses a time string in the format HH:MM:SS[.fraction] into hour,
 * minute, second. This function is used by parseTimePart(), parseDateString() and
 * parseTimestampString() and is not intended to be used directly.
 *
 * @param timeStr       The time string to parse.
 * @param h             Output hour.
 * @param m             Output minute.
 * @param s             Output second.
 * @param fracStart     Output pointer to fractional part (or NULL).
 * @param err           Error list pointer for diagnostics.
 * @return              SQL_SUCCESS or SQL_ERROR.
 */
SQLRETURN parseTimePart(const char *timeStr, int *h, int *m, int *s,
                               const char **fracStart, RS_ERROR_INFO **err) {
    // Validate minimum length for HH:MM:SS format (8 characters) and colon positions
    // Position 2: first colon after 2-digit hour (HH:)
    // Position 5: second colon after 2-digit minute (HH:MM:)
    if (!timeStr || strlen(timeStr) < TIME_STR_LEN || timeStr[2] != ':' ||
        timeStr[5] != ':') {
        goto fmt_err;
    }
    // Validate digit sequences: 2 digits for hour, 2 for minute, 2 for second
    // timeStr+3: minute position (skip HH:), timeStr+6: second position (skip HH:MM:)
    if (!isDigitStr(timeStr, 2) || !isDigitStr(timeStr + 3, 2) ||
        !isDigitStr(timeStr + 6, 2)) {
        goto fmt_err;
    }
    // Parse HH:MM:SS format, expecting exactly 3 values (hour, minute, second)
    if (sscanf(timeStr, "%2d:%2d:%2d", h, m, s) != 3) {
        goto fmt_err;
    }
    // Validate time component ranges: hour (0-23), minute (0-59), second (0-59)
    if (*h > TIME_MAX_HOUR || *m > TIME_MAX_MINUTE || *s > TIME_MAX_SECOND) {
        goto fmt_err;
    }
    // Fractional seconds start right after seconds at position 8 (HH:MM:SS.)
    *fracStart = strchr(timeStr, '.');
    if (*fracStart && *fracStart != timeStr + 8) {
        goto fmt_err; // misplaced '.'
    }
    return SQL_SUCCESS;
fmt_err:
    if (err) {
        addError(err, "22018", "Invalid character value for cast to TIME", 0,
                 NULL);
    }
    return SQL_ERROR;
}

/**
 * @brief Parses and normalizes the fractional seconds part to nanoseconds (9
 * digits).
 *
 * @param fracPtr       Pointer to the '.' character in the time string (or
 * NULL).
 * @param fraction      Output nanoseconds fraction (0-999999999).
 * @param trunc         Output flag indicating if truncation occurred.
 * @param err           Error list pointer for diagnostics.
 * @return              SQL_SUCCESS or SQL_ERROR.
 */
SQLRETURN parseFraction(const char *fracPtr, unsigned int *fraction,
                        bool *trunc, RS_ERROR_INFO **err) {
    if (!fracPtr) {
        *fraction = 0;
        *trunc = false;
        return SQL_SUCCESS;
    }
    ++fracPtr; // skip '.'
    int digits = 0;
    *fraction = 0;

    // Parse up to 9 digits (nanosecond precision)
    while (isdigit((unsigned char)fracPtr[digits]) && digits < TIME_FRAC_PRECISION_NS) {
        *fraction = (*fraction) * 10 + (fracPtr[digits] - '0');
        ++digits;
    }

    if (digits == 0) {
        goto fmt_err; // no digits after '.'
    }

    // Check if there are more digits beyond nanosecond precision
    if (isdigit((unsigned char)fracPtr[digits])) {
        if (err) {
            addError(err, "22003", "Fractional seconds precision exceeds nanosecond limit", 0, NULL);
        }
        return SQL_ERROR;
    }

    while (digits++ < TIME_FRAC_PRECISION_NS) {
        *fraction *= 10; // pad with zeros
    }
    return SQL_SUCCESS;
fmt_err:
    if (err) {
        addError(err, "22018", "Invalid character value for cast to TIME", 0,
                 NULL);
    }
    return SQL_ERROR;
}


/**
 * @brief Parses a time string (HH:MM:SS[.fraction] or full timestamp) into
 * RS_TIME_STRUCT.
 *
 * @param pTimeStr      The input string containing the time or timestamp.
 * @param pTimeStruct   Output RS_TIME_STRUCT to populate.
 * @param ppErrorList   Error list pointer for diagnostics.
 * @return              SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, or SQL_ERROR.
 */
SQLRETURN parseTimeString(const char *pTimeStr, RS_TIME_STRUCT *pTimeStruct,
                          RS_ERROR_INFO **ppErrorList) {
    if (!pTimeStr || !pTimeStruct) {
        if (ppErrorList) {
            addError(ppErrorList, "HY009", "Invalid parameter - NULL pointer",
                     0, NULL);
        }
        return SQL_ERROR;
    }

    memset(pTimeStruct, 0, sizeof(RS_TIME_STRUCT));

    bool hasDateTruncation = false;
    // Trim leading and trailing whitespace
    const char *startPtr = pTimeStr;
    const char *endPtr = pTimeStr + strlen(pTimeStr);
    if (trimWhitespace(&startPtr, &endPtr)) {
        if (ppErrorList) {
            addError(ppErrorList, "22018", "Invalid character value for cast to TIME", 0, NULL);
        }
        return SQL_ERROR;
    }

    const char *timePtr = startPtr;
    const char *spacePtr = strchr(startPtr, ' ');
    if (spacePtr && spacePtr < endPtr) {
        timePtr = spacePtr + 1;
        hasDateTruncation = true;
    }

    int h = 0, m = 0, s = 0;
    const char *frac = NULL;
    SQLRETURN rc = parseTimePart(timePtr, &h, &m, &s, &frac, ppErrorList);
    if (rc != SQL_SUCCESS) {
        return rc;
    }
    bool trunc = false;
    unsigned int fracVal = 0;

    rc = parseFraction(frac, &fracVal, &trunc, ppErrorList);

    if (rc != SQL_SUCCESS) {
        return rc;
    }

    pTimeStruct->sqltVal.hour = (SQLUSMALLINT)h;
    pTimeStruct->sqltVal.minute = (SQLUSMALLINT)m;
    pTimeStruct->sqltVal.second = (SQLUSMALLINT)s;
    pTimeStruct->fraction = fracVal;

    return SQL_SUCCESS;
}

/**
 * @brief Parses timestamp strings in multiple ODBC-compliant formats into TIMESTAMP_STRUCT.
 *
 * Supports three timestamp formats per ODBC specification:
 * - Full timestamp: "YYYY-MM-DD HH:MM:SS[.fraction]"
 * - Date-only: "YYYY-MM-DD" (time defaults to 00:00:00)
 * - Time-only: "HH:MM:SS[.fraction]" (date defaults to current system date)
 *
 * Fractional seconds are normalized to microsecond precision (6 digits).
 * Nanosecond input (7-9 digits) is truncated with SQL_SUCCESS_WITH_INFO.
 *
 * @param pStr          Input timestamp string in one of the supported formats
 * @param pTs           Output TIMESTAMP_STRUCT to populate with parsed values
 * @param ppErrorList   Error list pointer for diagnostics (can be NULL)
 * @return              SQL_SUCCESS on successful parsing,
 *                      SQL_SUCCESS_WITH_INFO if fractional truncation occurred,
 *                      SQL_ERROR on invalid format or system errors
 */
SQLRETURN parseTimestampString(const char *pStr, TIMESTAMP_STRUCT *pTs,
                               RS_ERROR_INFO **ppErrorList) {
    if (!pStr || !pTs) {
        if (ppErrorList) {
            addError(ppErrorList, "HY009", "Invalid parameter - NULL pointer",
                     0, NULL);
        }
        return SQL_ERROR;
    }
    // Trim leading and trailing whitespace
    const char *startPtr = pStr;
    const char *endPtr = pStr + strlen(pStr);
    if (trimWhitespace(&startPtr, &endPtr)) {
        if (ppErrorList) {
            addError(ppErrorList, "22018", "Invalid character value for cast to TIMESTAMP", 0, NULL);
        }
        return SQL_ERROR;
    }

    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    const char *frac = NULL;
    SQLRETURN rc;

    const char *timePart = strchr(startPtr, ' ');

    if (timePart && timePart < endPtr) {
        // Full timestamp case

        // Extract Date part
        // Buffer for date portion: 10 chars for YYYY-MM-DD + 1 for null terminator
        char dateOnly[DATE_STRING_LEN + 1];
        // Copy first 10 characters (YYYY-MM-DD) from input string
        memcpy(dateOnly, startPtr, DATE_STRING_LEN);
        dateOnly[10] = '\0';

        ++timePart; // skip space
        if ((rc = parseDatePart(dateOnly, &y, &mo, &d, ppErrorList)) != SQL_SUCCESS ||
            (rc = parseTimePart(timePart, &h, &mi, &s, &frac, ppErrorList)) != SQL_SUCCESS) {
            return rc;
        }

    } else if (strchr(startPtr, ':')) {
        // Time‑only string -> use current date
        timePart = startPtr;
        if ((rc = parseTimePart(timePart, &h, &mi, &s, &frac, ppErrorList)) != SQL_SUCCESS) {
            return rc;
        }
        time_t now = time(NULL);
        struct tm tmNow;
        bool success = false;

#ifdef WIN32
        success = (localtime_s(&tmNow, &now) == 0);
#else
        success = (localtime_r(&now, &tmNow) != NULL);
#endif
        if (!success) {
            if (ppErrorList) {
                addError(ppErrorList, "HY000", "Unable to get current date", 0, NULL);
            }
            return SQL_ERROR;
        }

        // tm_year is years since 1900, add 1900 to get actual year
        y = tmNow.tm_year + 1900;
        // tm_mon is 0-11 (January = 0), add 1 to get 1-12 range
        mo = tmNow.tm_mon + 1;
        // tm_mday is already in 1-31 range, no adjustment needed
        d = tmNow.tm_mday;
    } else {
        // Date‑only string -> default 00:00:00
        if ((rc = parseDatePart(startPtr, &y, &mo, &d, ppErrorList)) != SQL_SUCCESS) {
            return rc;
        }
    }

    bool trunc = false;
    unsigned int fracVal = 0;
    if ((rc = parseFraction(frac, &fracVal, &trunc, ppErrorList)) != SQL_SUCCESS) {
        return rc;
    }


    memset(pTs, 0, sizeof(TIMESTAMP_STRUCT));

    pTs->year = y;
    pTs->month = mo;
    pTs->day = d;
    pTs->hour = h;
    pTs->minute = mi;
    pTs->second = s;
    pTs->fraction = fracVal;

    if (trunc) {
        if (ppErrorList) {
            addWarning(ppErrorList, "01S07", "Fractional truncation", 0, NULL);
        }
        return SQL_SUCCESS_WITH_INFO;
    }
    return SQL_SUCCESS;
}

/**
 * @brief Parses a date string (or timestamp) into DATE_STRUCT, truncating time
 * if present.
 *
 * @param pStr              Input string containing the date or timestamp.
 * @param pDateStruct       Output DATE_STRUCT to populate.
 * @param ppErrorList       Error list pointer for diagnostics.
 * @return                  SQL_SUCCESS, SQL_SUCCESS_WITH_INFO, or SQL_ERROR.
 */
SQLRETURN parseDateString(const char *pStr, DATE_STRUCT *pDateStruct,
                          RS_ERROR_INFO **ppErrorList) {
    if (!pStr || !pDateStruct) {
        if (ppErrorList) {
            addError(ppErrorList, "HY009", "Invalid parameter - NULL pointer",
                     0, NULL);
        }
        return SQL_ERROR;
    }
    memset(pDateStruct, 0, sizeof(DATE_STRUCT));

    const char *startPtr = pStr;
    const char *endPtr = pStr + strlen(pStr);
    if (trimWhitespace(&startPtr, &endPtr)) {
        if (ppErrorList) {
            addError(ppErrorList, "22018",
                    "Invalid character value for cast to DATE", 0, NULL);
        }
        return SQL_ERROR;
    }

    const char *timePart = strchr(startPtr, ' ');
    bool nonZeroTime = false;

    int y = 0, mo = 0, d = 0;
    SQLRETURN rc = parseDatePart(startPtr, &y, &mo, &d, ppErrorList);
    if (rc != SQL_SUCCESS) {
        return rc;
    }
    // If we are parsing a timestamp we want to ensure if time part is valid or not. If time part is valid,
    // we need to truncate it, if invalid, we error out
    if (timePart && timePart < endPtr) {
        int h = 0, mi = 0, s = 0;
        const char *frac = NULL;
        rc = parseTimePart(timePart + 1, &h, &mi, &s, &frac, ppErrorList);
        if (rc == SQL_SUCCESS) {
            nonZeroTime = (h || mi || s || frac != NULL);
        } else {
            return rc;
        }
    }

    pDateStruct->year = y;
    pDateStruct->month = mo;
    pDateStruct->day = d;

    if (nonZeroTime) {
        if (ppErrorList) {
            addWarning(ppErrorList, "01S07", "Time component truncated", 0,
                       NULL);
        }
        return SQL_SUCCESS_WITH_INFO;
    }
    return SQL_SUCCESS;
}

/**
 * @brief Prepares and validates a string for numeric conversion
 *
 * This function performs comprehensive validation of numeric string input
 * before it's passed to conversion functions (integer, float, or double).
 *
 * It handles:
 * - Null termination
 * - Whitespace trimming
 * - Special IEEE 754 values (infinity, NaN) - passed through for float/double
 * - Hexadecimal format rejection
 * - Comprehensive format validation including:
 *   * Multiple decimal points
 *   * Decimal after exponent
 *   * Invalid exponent format
 *   * Invalid characters
 *   * Missing digits
 *
 * @param pStmt           Statement handle for error reporting (can be NULL)
 * @param pColData        Input string data to validate
 * @param iColDataLen     Length of input string
 * @param pTempBuf        Temporary buffer for null termination
 * @param iTempBufSize    Size of temporary buffer
 * @param ppPreparedStr   [OUT] Pointer to prepared string (may point to
 * pColData or pTempBuf)
 * @param pTruncated      [OUT] Truncation flag (reserved for future use)
 *
 * @return SQL_SUCCESS if validation succeeds and string is ready for conversion
 *         SQL_ERROR if validation fails (with error details in pStmt if
 * provided)
 *
 * @note This function prepares strings for ANY numeric conversion
 * (integer/float/double)
 * @note Special IEEE 754 values (infinity, NaN) are allowed and should be
 * handled by downstream conversion logic based on target type
 * @note The prepared string returned in ppPreparedStr is trimmed and validated
 */
SQLRETURN prepareStringForNumericConversion(RS_STMT_INFO *pStmt, char *pColData,
                                            int iColDataLen, char *pTempBuf,
                                            int iTempBufSize,
                                            char **ppPreparedStr,
                                            int *pTruncated) {
    char *numStr;

    // Step 1: Initialize output parameters

    // Initialize truncation flag (reserved for future use)
    *pTruncated = 0;

    // Step 2: Handle null termination
    //   Ensure the string is null-terminated for safe processing
    if (iColDataLen > 0 && pColData[iColDataLen - 1] == '\0') {
        // Already null-terminated, use original buffer
        numStr = pColData;
    } else {
        // Not null-terminated, copy to temporary buffer with null terminator
        makeNullTerminateIntVal(pColData, iColDataLen, pTempBuf, iTempBufSize);
        numStr = pTempBuf;
    }

    // Step 3: Trim leading and trailing whitespace
    //   Remove all whitespace characters from both ends
    const char *start = numStr;
    const char *end = numStr + strlen(numStr);

    if (trimWhitespace(&start, &end)) {
        // Step 4: String became empty after trimming - invalid input
        if (pStmt) {
            addError(&pStmt->pErrorList, "22018",
                     "Invalid character value for casting", 0, NULL);
        }
        return SQL_ERROR;
    }

    if (start != numStr || *end != '\0') {
        // Trimming needed - copy to temp buffer
        size_t len = end - start;
        if (len >= iTempBufSize) {
            return SQL_ERROR;
        }
        memcpy(pTempBuf, start, len);
        pTempBuf[len] = '\0';
        numStr = pTempBuf;
        end = pTempBuf + len; // Update end to point to new string's end
    } else {
        numStr = (char *)start;
    }

    // Step 5: Check for special IEEE 754 values
    //   infinity, -infinity, +infinity, inf, -inf, +inf, nan
    //
    //   These are VALID for float/double conversions but INVALID for integers.
    //   We allow them to pass through here, and let the downstream conversion
    //   logic (e.g., parseAndBuildInteger for integers, atof for floats)
    //   handle them appropriately based on the target type.
    if (_stricmp(numStr, "infinity") == 0 || _stricmp(numStr, "inf") == 0 ||
        _stricmp(numStr, "+infinity") == 0 || _stricmp(numStr, "+inf") == 0 ||
        _stricmp(numStr, "-infinity") == 0 || _stricmp(numStr, "-inf") == 0 ||
        _stricmp(numStr, "nan") == 0) {
        // Valid for float/double, will be rejected by integer conversion if
        // needed
        *ppPreparedStr = numStr;
        return SQL_SUCCESS;
    }

    // Step 6: Check for hexadecimal format (0x or 0X prefix)
    //   Hexadecimal numbers are not valid decimal numeric literals
    if (strncmp(numStr, "0x", 2) == 0 || strncmp(numStr, "0X", 2) == 0) {
        if (pStmt) {
            addError(&pStmt->pErrorList, "22018",
                     "Invalid character value - not a decimal numeric literal",
                     0, NULL);
        }
        return SQL_ERROR;
    }

    try {
        // Pattern explanation:
        // ^[+-]?              - Optional leading sign
        // ([0-9]+\.?[0-9]*    - Digits with optional decimal point and trailing
        // digits
        // |\.[0-9]+)          - OR leading decimal point with digits
        // ([eE][+-]?[0-9]+)?  - Optional exponent part
        // $                   - End of string
        std::regex pattern(
            R"(^[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?$)");

        if (!std::regex_match(numStr, pattern)) {
            if (pStmt) {
                addError(&pStmt->pErrorList, "22018",
                         "Invalid numeric format", 0, NULL);
            }
            return SQL_ERROR;
        }

        *ppPreparedStr = numStr;
        return SQL_SUCCESS;

    } catch (const std::regex_error &e) {
        // Handle regex compilation error (shouldn't happen with valid pattern)
        if (pStmt) {
            addError(&pStmt->pErrorList, "22018",
                     "Regex compilation error", 0, NULL);
        }
        return SQL_ERROR;
    }
}

/**
 * @brief Returns an invalid day-to-second interval structure.
 * @return Zero-initialized SQL_INTERVAL_STRUCT with interval_type set to SQL_IS_DAY_TO_SECOND.
 */
SQL_INTERVAL_STRUCT returnInvalidIntervalD2S() {
    SQL_INTERVAL_STRUCT result;
    memset(&result, 0, sizeof(SQL_INTERVAL_STRUCT));
    result.interval_type = SQL_IS_DAY_TO_SECOND;
    return result;
}

/**
 * @brief Returns an invalid year-to-month interval structure.
 * @return Zero-initialized SQL_INTERVAL_STRUCT with interval_type set to SQL_IS_YEAR_TO_MONTH.
 */
SQL_INTERVAL_STRUCT returnInvalidIntervalY2M() {
    SQL_INTERVAL_STRUCT result;
    memset(&result, 0, sizeof(SQL_INTERVAL_STRUCT));
    result.interval_type = SQL_IS_YEAR_TO_MONTH;
    return result;
}

/**
 * @brief Validates if a string matches time format HH:MM:SS[.fraction].
 * @param str Null-terminated string to validate.
 * @return true if string matches HH:MM:SS[.fraction] format, false otherwise.
 */
static inline bool isValidTimeFormat(const char *str) {
    if (!str || strlen(str) > MAX_INTERVAL_STRING_LENGTH) {
        return false;
    }
    return std::regex_match(str, RegexPatterns::TIME_FORMAT_PATTERN);
}

/**
 * @brief Validates if a string matches year-month interval format Y-M.
 * @param str Null-terminated string to validate.
 * @return true if string matches Y-M format, false otherwise.
 */
static inline bool isValidYearMonthFormat(const char *str) {
    if (!str || strlen(str) > MAX_INTERVAL_STRING_LENGTH) {
        return false;
    }
    return std::regex_match(str, RegexPatterns::YEAR_MONTH_FORMAT_PATTERN);
}

/**
 * @brief Validates if a string matches day-to-second SQL standard format.
 * @param str Null-terminated string to validate.
 * @return true if string matches pattern D HH:MM:SS[.fraction], false otherwise.
 */
static inline bool isValidDayToSecondFormat(const char *str) {
    if (!str || strlen(str) > MAX_INTERVAL_STRING_LENGTH) {
        return false;
    }
    return std::regex_match(str, RegexPatterns::DAY_TO_SECOND_SQL_PATTERN);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Null terminate the integer value.
//
void makeNullTerminateIntVal(char *pColData, int iColDataLen, char *szNumBuf, int iBufLen)
{
    int len = redshift_min(iColDataLen, iBufLen-1);

    memcpy(szNumBuf, pColData, len);
    szNumBuf[len] = '\0';
}

/*====================================================================================================================================================*/

 // get signed tiny int data
 SQLRETURN getTinyIntData(int8_t hVal, void *pBuf, SQLLEN *pcbLenInd)
 {
     SQLRETURN rc;
     if (pBuf) {
         *(int8_t *)pBuf = hVal;
         rc = SQL_SUCCESS;
     }
     else
     {
         rc = SQL_SUCCESS_WITH_INFO;
     }

     if (pcbLenInd)
         *pcbLenInd = sizeof(int8_t);

     return rc;
 }

 /*====================================================================================================================================================*/

 // get unsigned tiny int data
 SQLRETURN getUTinyIntData(uint8_t hVal, void *pBuf, SQLLEN *pcbLenInd) {
     SQLRETURN rc;

    if (pBuf) {
        *(uint8_t *)pBuf = hVal;
        rc = SQL_SUCCESS;
    } else {
        rc = SQL_SUCCESS_WITH_INFO;
    }

    if (pcbLenInd) {
        *pcbLenInd = sizeof(uint8_t);
    }

     return rc;
 }

 /*====================================================================================================================================================*/
 
//---------------------------------------------------------------------------------------------------------igarish
// Get short data.
//
SQLRETURN getShortData(short hVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(short *)pBuf = hVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(short);

    return rc;
}

/*====================================================================================================================================================*/

// Get unsigned short data
SQLRETURN getUShortData(unsigned short hVal, void *pBuf, SQLLEN *pcbLenInd) {
    SQLRETURN rc;

    if (pBuf) {
        *(unsigned short *)pBuf = hVal;
        rc = SQL_SUCCESS;
    } else {
        rc = SQL_SUCCESS_WITH_INFO;
    }
    if (pcbLenInd) {
        *pcbLenInd = sizeof(unsigned short);
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get int data.
//
SQLRETURN getIntData(int iVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(int *)pBuf = iVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(int);

    return rc;
}


/*====================================================================================================================================================*/
// Get unsigned int data
SQLRETURN getUIntData(unsigned int iVal, void *pBuf, SQLLEN *pcbLenInd) {
    SQLRETURN rc;

    if (pBuf) {
        *(unsigned int *)pBuf = iVal;
        rc = SQL_SUCCESS;
    } else {
        rc = SQL_SUCCESS_WITH_INFO;
    }

    if (pcbLenInd) {
        *pcbLenInd = sizeof(unsigned int);
    }

    return rc;
}

/*====================================================================================================================================================*/
 // Get unsigned big integer data
SQLRETURN getUBigIntData(unsigned long long llVal, void *pBuf,
                         SQLLEN *pcbLenInd) {
    SQLRETURN rc;

    if (pBuf) {
        *(unsigned long long *)pBuf = llVal;
        rc = SQL_SUCCESS;
    } else {
        rc = SQL_SUCCESS_WITH_INFO;
    }

    if (pcbLenInd) {
        *pcbLenInd = sizeof(unsigned long long);
    }

    return rc;
}
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get big integer data.
//
SQLRETURN getBigIntData(long long llVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(long long *)pBuf = llVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(long long);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get float data.
//
SQLRETURN getFloatData(float fVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(float *)pBuf = fVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(float);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get double data.
//
SQLRETURN getDoubleData(double dVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(double *)pBuf = dVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(double);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get boolean data.
//
SQLRETURN getBooleanData(char bVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(char *)pBuf = (bVal == 1) ? 1 : 0;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(char);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get date data.
//
SQLRETURN getDateData(DATE_STRUCT *pdtVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(DATE_STRUCT *)pBuf = *pdtVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(DATE_STRUCT);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get timestamp data.
//
SQLRETURN getTimeStampData(TIMESTAMP_STRUCT *ptsVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(TIMESTAMP_STRUCT *)pBuf = *ptsVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(TIMESTAMP_STRUCT);

    return rc;
}

/*====================================================================================================================================================*/

/**
 * @brief Copy an INTERVAL YEAR TO MONTH value into the caller's buffer.
 *
 * @param[in]  pIntervalVal  Source interval structure (must not be NULL).
 * @param[out] pBuf          Destination for SQL_INTERVAL_STRUCT, or NULL.
 * @param[out] pcbLenInd     Receives sizeof(SQL_INTERVAL_STRUCT) if non-NULL.
 *
 * @return SQL_SUCCESS if copied; SQL_SUCCESS_WITH_INFO if pBuf is NULL.
 */
SQLRETURN getIntervalY2MData(SQL_INTERVAL_STRUCT *pIntervalVal, void *pBuf,
                             SQLLEN *pcbLenInd) {
    SQLRETURN rc;

    if (pBuf) {
        *(SQL_INTERVAL_STRUCT *)pBuf = *pIntervalVal;
        rc = SQL_SUCCESS;
    } else {
        rc = SQL_SUCCESS_WITH_INFO;
    }

    if (pcbLenInd) {
        *pcbLenInd = sizeof(SQL_INTERVAL_STRUCT);
    }

    return rc;
}

/**
 * @brief Copy an INTERVAL DAY TO SECOND value into the caller's buffer.
 *
 * @param[in]  pIntervalVal  Source interval structure (must not be NULL).
 * @param[out] pBuf          Destination for SQL_INTERVAL_STRUCT, or NULL.
 * @param[out] pcbLenInd     Receives sizeof(SQL_INTERVAL_STRUCT) if non-NULL.
 *
 * @return SQL_SUCCESS if copied; SQL_SUCCESS_WITH_INFO if pBuf is NULL.
 */
SQLRETURN getIntervalD2SData(SQL_INTERVAL_STRUCT *pIntervalVal, void *pBuf,
                             SQLLEN *pcbLenInd) {
    SQLRETURN rc;

    if (pBuf) {
        *(SQL_INTERVAL_STRUCT *)pBuf = *pIntervalVal;
        rc = SQL_SUCCESS;
    } else {
        rc = SQL_SUCCESS_WITH_INFO;
    }

    if (pcbLenInd) {
        *pcbLenInd = sizeof(SQL_INTERVAL_STRUCT);
    }

    return rc;
}

/**
 * @brief Structure to hold analysis results of an interval string.
 *
 * Used by analyzeIntervalString() to return information about the
 * characteristics of a string being parsed for interval conversion.
 */
typedef struct {
    int isNumeric; /**< Non-zero if string is purely numeric (with optional sign and decimal) */
    int hasDecimal; /**< Non-zero if string contains a decimal point */
    int digitCount; /**< Number of digit characters in the string */
    int hasColon;   /**< Non-zero if string contains ':' */
    int hasSpace;   /**< Non-zero if string contains space */
    int hasAlpha;   /**< Non-zero if string contains alphabetic characters */
    int dashCount;  /**< Number of '-' characters in the string */
} IntervalStringInfo;

/**
 * @brief Checks if a keyword at the given position is valid (preceded by space).
 *
 * For interval parsing, keywords like "year", "mon", "day", etc. must be preceded
 * by whitespace to be valid. Direct attachment to digits (e.g., "5years") is invalid.
 * Valid formats: "5 years", "5 year", "@ 1 year"
 * Invalid formats: "5years", "5year"
 *
 * @param[in] str      The full string being searched
 * @param[in] keywordPos  Pointer to where the keyword was found in str
 *
 * @return 1 if keyword position is valid, 0 otherwise
 */
static int isValidKeywordPosition(const char *str, const char *keywordPos) {
    if (keywordPos == NULL) {
        return 0;
    }
    /* Keyword at start of string is valid*/
    if (keywordPos == str) {
        return 1;
    }
    /* Keyword must be preceded by whitespace */
    char prevChar = *(keywordPos - 1);
    return isspace((unsigned char)prevChar);
}

/**
 * @brief Prepares a character string for interval parsing.
 *
 * Validates the input, ensures null-termination, and trims whitespace.
 * Uses existing helper functions for null-termination and whitespace trimming.
 *
 * @param[in]  pColData      Input character data
 * @param[in]  iColDataLen   Length of input data
 * @param[in]  format        Data format (TEXT_FORMAT or BINARY_FORMAT)
 * @param[out] tempBuf       Buffer for null-terminated string
 * @param[in]  tempBufLen    Size of tempBuf
 * @param[out] outLen        Length of the resulting trimmed string
 * @param[out] ppErrorList   Error list for reporting errors
 *
 * @return Pointer to null-terminated string on success, NULL on error
 */
static char *prepareIntervalString(char *pColData, int iColDataLen, int format,
                                   char *tempBuf, int tempBufLen, int *outLen,
                                   RS_ERROR_INFO **ppErrorList) {
    /* Handle empty string */
    if (iColDataLen <= 0 || (iColDataLen == 1 && pColData[0] == '\0')) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return NULL;
    }

    /* Ensure null-terminated string*/
    char *pStr;
    if (IS_TEXT_FORMAT(format)) {
        makeNullTerminateIntVal(pColData, iColDataLen, tempBuf, tempBufLen);
        pStr = tempBuf;
    } else {
        /* Binary format not supported for character to interval conversion */
        addError(ppErrorList, "07006",
                 "Binary format not supported for character to interval conversion", 0, NULL);
        return NULL;
    }

    /* Trim leading and trailing whitespace*/
    pStr = trim_whitespaces(pStr);

    /* Check for empty string after trimming */
    if (*pStr == '\0') {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return NULL;
    }

    *outLen = strlen(pStr);
    return pStr;
}

/**
 * @brief Analyzes string characteristics for interval parsing.
 *
 * Performs a single pass through the string to determine its format
 * characteristics, which helps decide the appropriate parsing strategy.
 *
 * @param[in]  pStr   Null-terminated string to analyze
 * @param[in]  len    Length of the string
 * @param[out] info   Structure to receive analysis results
 */
static void analyzeIntervalString(const char *pStr, int len,
                                  IntervalStringInfo *info) {
    memset(info, 0, sizeof(IntervalStringInfo));
    info->isNumeric = 1;

    for (int i = 0; i < len; i++) {
        char c = pStr[i];

        if (c == ':') {
            info->hasColon = 1;
        } else if (c == ' ') {
            info->hasSpace = 1;
        } else if (c == '-') {
            info->dashCount++;
        } else if (isalpha((unsigned char)c)) {
            info->hasAlpha = 1;
        }

        /* Check numeric format */
        if (info->isNumeric) {
            if (i == 0 && (c == '-' || c == '+')) {
                continue;
            }
            if (c == '.' && !info->hasDecimal) {
                info->hasDecimal = 1;
                continue;
            }
            if (isdigit((unsigned char)c)) {
                info->digitCount++;
                continue;
            }
            info->isNumeric = 0;
        }
    }
}

/**
 * @brief Detects if an interval string has fractional seconds exceeding
 *        microsecond precision (more than 6 digits after the decimal point).
 *
 * @param[in] pStr  Null-terminated interval string to check
 *
 * @return true if fractional digits > 6 (truncation will occur), false otherwise
 */
static bool hasFractionTruncationInInterval(const char *pStr) {
    /* Find the last decimal point - this is the fractional seconds separator */
    const char *dot = strrchr(pStr, '.');
    if (!dot) return false;

    int fracDigits = 0;
    const char *p = dot + 1;
    while (isdigit((unsigned char)*p)) {
        fracDigits++;
        p++;
    }
    return fracDigits > 6; /* microsecond precision = 6 digits */
}

/**
 * @brief Parses a time format string to interval day-to-second.
 *
 * Handles formats like HH:MM:SS or HH:MM:SS.fraction.
 * No normalization is performed
 *
 * @param[in]  pStr          Time format string to parse
 * @param[out] pIntervalVal  Interval structure to populate
 *
 * @return SQL_SUCCESS on success, SQL_ERROR on parse failure
 */
static SQLRETURN
parseTimeFormatToIntervalD2S(const char *pStr,
                             SQL_INTERVAL_STRUCT *pIntervalVal) {
    int hour = 0, min = 0, sec = 0;
    int isNegative = 0;

    const char *parsePtr = pStr;
    if (*parsePtr == '-') {
        isNegative = 1;
        parsePtr++;
    } else if (*parsePtr == '+') {
        isNegative = 0; // Explicitly positive
        parsePtr++;
    }

    // Use parseTimePart to parse time components
    const char *frac = NULL;
    if (parseTimePart(parsePtr, &hour, &min, &sec, &frac, NULL) != SQL_SUCCESS) {
        return SQL_ERROR;
    }

    // Parse the fractional part using parseFraction
    bool trunc = false;
    unsigned int fracNanoseconds = 0;
    if (parseFraction(frac, &fracNanoseconds, &trunc, NULL) != SQL_SUCCESS) {
        return SQL_ERROR;
    }

    // Convert nanoseconds (9 digits) to microseconds (6 digits)
    // parseFraction returns nanoseconds, but intervals use microsecond precision
    // Redshift uses microsecond precision (6 digits), convert from nanoseconds
    unsigned int fracMicroseconds = fracNanoseconds / 1000;

    memset(pIntervalVal, 0, sizeof(SQL_INTERVAL_STRUCT));
    pIntervalVal->interval_type = SQL_IS_DAY_TO_SECOND;
    pIntervalVal->interval_sign = isNegative ? SQL_TRUE : SQL_FALSE;
    pIntervalVal->intval.day_second.day = 0; // Time format has no day component
    pIntervalVal->intval.day_second.hour = (SQLUINTEGER)hour; // Must be 0-23
    pIntervalVal->intval.day_second.minute = (SQLUINTEGER)min; // Must be 0-59
    pIntervalVal->intval.day_second.second = (SQLUINTEGER)sec; // Must be 0-59
    pIntervalVal->intval.day_second.fraction = (SQLUINTEGER)fracMicroseconds;

    return SQL_SUCCESS;
}

/**
 * @brief Checks if string represents a year-month interval format.
 *
 * Year-month intervals are incompatible with day-to-second conversion.
 *
 * @param[in] pStr  String to check
 *
 * @return Non-zero if string appears to be year-month format, 0 otherwise
 */
static int isYearMonthIntervalFormat(const char *pStr) {
    return (strstr(pStr, "year") != NULL && strstr(pStr, "mon") != NULL &&
            strstr(pStr, "day") == NULL && strstr(pStr, "hour") == NULL);
}

/**
 * @brief Checks if a string matches timestamp format
 *
 * @param pStr Null-terminated string to check
 *
 * @return 1 if string matches YYYY-MM-DD HH:MM:SS[.fraction] format, 0
 * otherwise
 *
 * @note Uses regex pattern matching to identify timestamp format
 * @note Returns 0 if regex error occurs during matching
 */
static int isTimestampFormat(const char *pStr) {
    if (!pStr || strlen(pStr) > MAX_INTERVAL_STRING_LENGTH) {
        return 0;
    }
    return std::regex_match(pStr, RegexPatterns::TIMESTAMP_PATTERN) ? 1 : 0;
}

/**
 * @brief Converts a character string to SQL_C_INTERVAL_DAY_TO_SECOND.
 *
 * Handles various input formats:
 * - Time format strings (HH:MM:SS[.fraction])
 * - Interval literal strings (e.g., "1 day 2 hours")
 *
 * @param[in]  pColData      Input character data
 * @param[in]  iColDataLen   Length of input data
 * @param[in]  format        Data format (TEXT_FORMAT or BINARY_FORMAT)
 * @param[out] pIntervalVal  Interval structure to populate
 * @param[out] pBuf          Output buffer for the interval
 * @param[out] pcbLenInd     Length indicator
 * @param[out] ppErrorList   Error list for reporting errors
 *
 * @return SQL_SUCCESS on success, SQL_ERROR on failure
 */
SQLRETURN convertCharToIntervalD2S(char *pColData, int iColDataLen, int format,
                                   SQL_INTERVAL_STRUCT *pIntervalVal,
                                   void *pBuf, SQLLEN *pcbLenInd,
                                   RS_ERROR_INFO **ppErrorList) {
    char tempBuf[MAX_TEMP_BUF_LEN];
    int len = 0;

    /* Step 1: Prepare and validate the input string */
    char *pStr = prepareIntervalString(pColData, iColDataLen, format, tempBuf,
                                       MAX_TEMP_BUF_LEN, &len, ppErrorList);
    if (pStr == NULL) {
        return SQL_ERROR;
    }

    /* Step 2: Analyze string characteristics */
    IntervalStringInfo info;
    analyzeIntervalString(pStr, len, &info);

    /* Step 3: Try parsing based on detected format */

    /* Detect if input has fractional seconds precision > 6 digits
     * (microseconds). Per ODBC spec, the driver should signal this with
     * SQL_SUCCESS_WITH_INFO / SQLSTATE 01S07 (Fractional truncation). */
    bool fractionTruncated = hasFractionTruncationInInterval(pStr);

    /* Case 1: Time format without day component (HH:MM:SS) */
    if (info.hasColon && !info.hasSpace && !info.hasAlpha) {
        SQLRETURN rc = parseTimeFormatToIntervalD2S(pStr, pIntervalVal);
        if (rc == SQL_SUCCESS) {
            SQLRETURN dataRc = getIntervalD2SData(pIntervalVal, pBuf, pcbLenInd);
            if (dataRc == SQL_SUCCESS && fractionTruncated) {
                addWarning(ppErrorList, "01S07", "Fractional truncation", 0, NULL);
                return SQL_SUCCESS_WITH_INFO;
            }
            return dataRc;
        }
        /* Fall through to try other formats */
    }

    /* Case 2: Check for incompatible formats */
    if (isTimestampFormat(pStr)) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    if (isYearMonthIntervalFormat(pStr)) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    /* Case 4: Validate keyword format for PostgreSQL verbose format */
    /* Keywords must be preceded by a space (not directly attached to digits) */
    int hasValidKeyword = 
        isValidKeywordPosition(pStr, strstr(pStr, "day")) ||
        isValidKeywordPosition(pStr, strstr(pStr, "hour")) ||
        isValidKeywordPosition(pStr, strstr(pStr, "min")) ||
        isValidKeywordPosition(pStr, strstr(pStr, "sec"));

    /* If input has alphabetic characters, they must be valid lowercase keywords.
     * This catches uppercase/mixed case like "DAYS", "Days" which are invalid. */
    if (info.hasAlpha && !hasValidKeyword) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    /* Case 5: Check for day value overflow in SQL standard format (D HH:MM:SS).
     * sscanf with %d on very large numbers is undefined behavior, so we use
     * strtoll to safely detect overflow before passing to parse_intervald2s.
     * Hour/minute/second validation is handled by the regex and parse_intervald2s. */
    if (!info.hasAlpha && info.hasSpace && info.hasColon) {
        const char *numStart = pStr;
        if (*numStart == '-' || *numStart == '+') {
            numStart++;
        }
        if (isdigit((unsigned char)*numStart)) {
            char *endPtr;
            errno = 0;
            long long dayVal = strtoll(numStart, &endPtr, 10);
            if (errno == ERANGE || dayVal > INT_MAX || dayVal < 0) {
                addError(ppErrorList, "22003",
                         "Numeric value out of range", 0, NULL);
                return SQL_ERROR;
            }
        }
    }

    /* Case 6: Parse the interval - validation happens inside parse_intervald2s */
    *pIntervalVal = parse_intervald2s(pStr, len);

    /* If result is zeroed but input was non-empty, check if it was a valid zero
     * representation */
    if (len > 0 && pIntervalVal->intval.day_second.day == 0 &&
        pIntervalVal->intval.day_second.hour == 0 &&
        pIntervalVal->intval.day_second.minute == 0 &&
        pIntervalVal->intval.day_second.second == 0 &&
        pIntervalVal->intval.day_second.fraction == 0) {
        /* Check if input was actually "0", "0 00:00:00", or similar valid zero
         * representation */
        int hasDigit = 0;
        int hasNonZeroDigit = 0;

        for (int i = 0; i < len; i++) {
            if (isdigit((unsigned char)pStr[i])) {
                hasDigit = 1;
                if (pStr[i] != '0') {
                    hasNonZeroDigit = 1;
                    break;
                }
            }
        }

        /* Invalid if:
         * - Input has non-zero digits but result is zero (parsing failed)
         * - Input has no digits and no valid interval keywords (e.g., "abc")
         */
        if (hasNonZeroDigit || (!hasDigit && !hasValidKeyword)) {
            addError(ppErrorList, "22018",
                     "Invalid character value for cast to interval", 0, NULL);
            return SQL_ERROR;
        }
    }

    SQLRETURN dataRc = getIntervalD2SData(pIntervalVal, pBuf, pcbLenInd);
    if (dataRc == SQL_SUCCESS && fractionTruncated) {
        addWarning(ppErrorList, "01S07", "Fractional truncation", 0, NULL);
        return SQL_SUCCESS_WITH_INFO;
    }
    return dataRc;
}

/**
 * @brief Checks if string represents a day-to-second interval format.
 *
 * Day-to-second intervals are incompatible with year-to-month conversion.
 *
 * @param[in] pStr  String to check
 *
 * @return Non-zero if string appears to be day-to-second format, 0 otherwise
 */
static int isDayToSecondIntervalFormat(const char *pStr) {
    return (strstr(pStr, "day") != NULL || strstr(pStr, "hour") != NULL ||
            strstr(pStr, "min") != NULL || strstr(pStr, "sec") != NULL ||
            strchr(pStr, ':') != NULL);
}

/**
 * @brief Parses a year-month format string to interval year-to-month.
 *
 * Handles formats like Y-M (e.g., "5-3" for 5 years 3 months).
 * Months greater than or equal to MONTHS_PER_YEAR are not normalized to years.
 *
 * @param[in]  pStr          Year-month format string to parse
 * @param[out] pIntervalVal  Interval structure to populate
 *
 * @return SQL_SUCCESS on success, SQL_ERROR on parse failure
 */
static SQLRETURN
parseYearMonthFormatToIntervalY2M(const char *pStr,
                                  SQL_INTERVAL_STRUCT *pIntervalVal) {
    long long year = 0, month = 0;
    int isNegative = 0;

    const char *parsePtr = pStr;
    if (*parsePtr == '-') {
        isNegative = 1;
        parsePtr++;
    } else if (*parsePtr == '+') {
        isNegative = 0; // Explicitly positive
        parsePtr++;
    }

    // Validate format: digits-digits, no decimal points
    if (!isValidYearMonthFormat(parsePtr)) {
        return SQL_ERROR;
    }

    /* Parse year and month using sscanf. The regex already validated the format,
     * so sscanf will succeed. Use %lld for overflow-safe parsing. */
    if (sscanf(parsePtr, "%lld-%lld", &year, &month) != 2) {
        return SQL_ERROR;
    }

    // Range check: year and month must fit in SQLUINTEGER and month must be 0-11
    if (year < 0 || year > UINT_MAX || month < 0 || month > MAX_DATE_MONTH - 1) {
        return SQL_ERROR;
    }

    memset(pIntervalVal, 0, sizeof(SQL_INTERVAL_STRUCT));
    pIntervalVal->interval_type = SQL_IS_YEAR_TO_MONTH;
    pIntervalVal->interval_sign = isNegative ? SQL_TRUE : SQL_FALSE;
    pIntervalVal->intval.year_month.year = (SQLUINTEGER)year;
    pIntervalVal->intval.year_month.month = (SQLUINTEGER)month;

    return SQL_SUCCESS;
}

/**
 * @brief Converts a character string to SQL_C_INTERVAL_YEAR_TO_MONTH.
 *
 * Handles various input formats:
 * - Year-month format strings (Y-M)
 * - Interval literal strings (e.g., "1 year 2 months")
 *
 * @param[in]  pColData      Input character data
 * @param[in]  iColDataLen   Length of input data
 * @param[in]  format        Data format (TEXT_FORMAT or BINARY_FORMAT)
 * @param[out] pIntervalVal  Interval structure to populate
 * @param[out] pBuf          Output buffer for the interval
 * @param[out] pcbLenInd     Length indicator
 * @param[out] ppErrorList   Error list for reporting errors
 *
 * @return SQL_SUCCESS on success, SQL_ERROR on failure
 */
SQLRETURN convertCharToIntervalY2M(char *pColData, int iColDataLen, int format,
                                   SQL_INTERVAL_STRUCT *pIntervalVal,
                                   void *pBuf, SQLLEN *pcbLenInd,
                                   RS_ERROR_INFO **ppErrorList) {
    char tempBuf[MAX_TEMP_BUF_LEN];
    int len = 0;

    /* Step 1: Prepare and validate the input string */
    char *pStr = prepareIntervalString(pColData, iColDataLen, format, tempBuf,
                                       MAX_TEMP_BUF_LEN, &len, ppErrorList);
    if (pStr == NULL) {
        return SQL_ERROR;
    }

    /* Step 2: Analyze string characteristics */
    IntervalStringInfo info;
    analyzeIntervalString(pStr, len, &info);

    /* Step 3: Try parsing based on detected format */

    /* Case 1: Check for incompatible formats (day-to-second) */
    if (isDayToSecondIntervalFormat(pStr)) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    /* Case 2: Check for timestamp format (incompatible) */
    if (isTimestampFormat(pStr)) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    /* Case 3: Try Y-M format (e.g., "5-3" for 5 years 3 months) */
    if (info.dashCount == 1 && !info.hasAlpha && !info.hasColon) {
        SQLRETURN rc = parseYearMonthFormatToIntervalY2M(pStr, pIntervalVal);
        if (rc == SQL_SUCCESS) {
            return getIntervalY2MData(pIntervalVal, pBuf, pcbLenInd);
        }

        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    /* Case 4: Try parsing as interval string using existing parser */
    *pIntervalVal = parse_intervaly2m(pStr, len);

    /* Validate parsing result - check if the input format was actually valid */
    /* Keywords must be preceded by a space or digit (not directly attached to letters) */
    int hasValidKeyword = 
        isValidKeywordPosition(pStr, strstr(pStr, "year")) ||
        isValidKeywordPosition(pStr, strstr(pStr, "mon"));

    /* If input has alphabetic characters but no valid interval keywords,
     * the parsing result is unreliable */
    if (info.hasAlpha && !hasValidKeyword) {
        addError(ppErrorList, "22018",
                 "Invalid character value for cast to interval", 0, NULL);
        return SQL_ERROR;
    }

    /* If result is zeroed but input was non-empty, check if it was a valid zero
     * representation */
    if (len > 0 && pIntervalVal->intval.year_month.year == 0 &&
        pIntervalVal->intval.year_month.month == 0) {
        /* Check if input was actually "0", "0-0", or similar valid zero
         * representation */
        int hasDigit = 0;
        int hasNonZeroDigit = 0;

        for (int i = 0; i < len; i++) {
            if (isdigit((unsigned char)pStr[i])) {
                hasDigit = 1;
                if (pStr[i] != '0') {
                    hasNonZeroDigit = 1;
                    break;
                }
            }
        }

        /* Invalid if:
         * - Input has non-zero digits but result is zero (parsing failed)
         * - Input has no digits and no valid interval keywords (e.g., "abc")
         */
        if (hasNonZeroDigit || (!hasDigit && !hasValidKeyword)) {
            addError(ppErrorList, "22018",
                     "Invalid character value for cast to interval", 0, NULL);
            return SQL_ERROR;
        }
    }

    return getIntervalY2MData(pIntervalVal, pBuf, pcbLenInd);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get time data.
//
SQLRETURN getTimeData(RS_TIME_STRUCT *ptVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(TIME_STRUCT *)pBuf = ptVal->sqltVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(TIME_STRUCT);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get numeric data.
//
SQLRETURN getNumericData(SQL_NUMERIC_STRUCT *pnVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(SQL_NUMERIC_STRUCT *)pBuf = *pnVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(SQL_NUMERIC_STRUCT);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get size of the given SQL data type.
//
long getSize(short hType, int iSize)
{
    long lSize;

    switch(hType)
    {
        case SQL_CHAR:
        case SQL_VARCHAR:
		case SQL_NUMERIC:
        case SQL_DECIMAL:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		case SQL_LONGVARBINARY:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
		{
            lSize = iSize;
            break;
        }

        case SQL_SMALLINT:
        {
            lSize = 5;
            break;
        }

        case SQL_INTEGER:
        {
            lSize = 10;
            break;
        }

        case SQL_BIGINT:
        {
            lSize = 19; // 20 for unsigned 
            break;
        }

        case SQL_REAL:
        {
            lSize = 7;
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            lSize = 15;
            break;
        }

        case SQL_BIT:
        case SQL_TINYINT:
        {
            lSize = 1;
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            lSize = 10;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            lSize = 26; // 19 + '.' + millionth of seconds (6)
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            lSize = 13;
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            lSize = 26;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            lSize = 8; // hh:mm:ss
            break;
        }

        default:
        {
            lSize = 0;
            break;
        }

    } // SQL Type

    return lSize;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get scale of the given SQL data type.
//
short getScale(short hType, short hDecimalDigits)
{
    short hScale;

    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            hScale = hDecimalDigits;
            break;
        }

        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_DATE:
        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            hScale = 0;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            hScale = 6; // millionth of seconds (6)
            break;
        }

        default:
        {
            hScale = 0;
            break;
        }

    } // SQL Type

    return hScale;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get case sensitivity of the given SQL data type.
//
int getCaseSensitive(short hType, short hRsSpecialType, int case_sensitive_bit)
{
    int iCaseSensitive;

    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
		case SQL_LONGVARBINARY:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_SECOND:
		{
            iCaseSensitive = FALSE;
            break;
        }

        case SQL_VARCHAR:
        case SQL_WVARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		{
			if (hRsSpecialType == TIMETZOID)
				iCaseSensitive = FALSE;
			else
			if (case_sensitive_bit != -1)
			{
				iCaseSensitive = (case_sensitive_bit == 1) ? TRUE : FALSE;
			}
			else
				iCaseSensitive = TRUE;

            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        {
            iCaseSensitive = TRUE;
            break;
        }

        default:
        {
            iCaseSensitive = FALSE;
            break;
        }

    } // SQL Type

    return iCaseSensitive;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get display size of the given SQL data type.
//
int getDisplaySize(short hType, int iSize, short hRsSpecialType)
{
    int iDisplaySize;

    switch(hType)
    {
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
        {
            iDisplaySize = (hRsSpecialType == TIMETZOID)
                ? MAX_TIMETZOID_SIZE : iSize;
            break;
        }

        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        {
            iDisplaySize = iSize * 2; // Each byte displays as 2 hex characters
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        {
            iDisplaySize = iSize;
            break;
        }

        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            iDisplaySize = (iSize > 0) ? iSize + 2 : 0;
            break;
        }

        case SQL_BIT:
        case SQL_TINYINT:
        {
            iDisplaySize = 1;
            break;
        }

        case SQL_SMALLINT:
        {
            iDisplaySize = 6;
            break;
        }

        case SQL_INTEGER:
        {
            iDisplaySize = 11;
            break;
        }

        case SQL_BIGINT:
        {
            iDisplaySize = 20;
            break;
        }

        case SQL_REAL:
        {
            iDisplaySize = 14; // a sign, 7 digits, a decimal point, the letter E, a sign, and 2 digits
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iDisplaySize = 24; // a sign, 15 digits, a decimal point, the letter E, a sign, and 3 digits
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            iDisplaySize = 10;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            iDisplaySize = 26;
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            iDisplaySize = 13;
            break;
        }

        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iDisplaySize = 26;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            iDisplaySize = 8; // the number of characters in the hh-mm-ss format
            break;
        }

        default:
        {
            iDisplaySize = 0;
            break;
        }

    } // SQL Type

    return iDisplaySize;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get literal prefix of the given SQL data type.
//
void getLiteralPrefix(short hType, char *pBuf, short hRsSpecialType)
{
    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            pBuf[0] = '\0';
            break;
        }

        case SQL_VARCHAR:
        case SQL_WVARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		{
            if(hRsSpecialType == TIMETZOID)
                pBuf[0] = '\0';
            else
                rs_strncpy(pBuf,"'",2);

            break;
        }

        case SQL_BINARY:
        case SQL_VARBINARY:
        {
            rs_strncpy(pBuf, "0x", 3);
            break;
        }

		case SQL_LONGVARBINARY:
		{
			if (hRsSpecialType == VARBYTE)
				rs_strncpy(pBuf, "'",2);
			else
                rs_strncpy(pBuf, "0x", 3);
			break;
		}

        case SQL_CHAR:
        case SQL_WCHAR:
        {
            rs_strncpy(pBuf,"'",2);
            break;
        }

        default:
        {
            pBuf[0] = '\0';
            break;
        }

    } // SQL Type

    return;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get literal suffix of the given SQL data type.
//
void getLiteralSuffix(short hType, char *pBuf, short hRsSpecialType)
{
    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            pBuf[0] = '\0';
            break;
        }

        case SQL_VARCHAR:
        case SQL_WVARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		{
            if(hRsSpecialType == TIMETZOID)
                pBuf[0] = '\0';
            else
                rs_strncpy(pBuf,"'",2);

            break;
        }

        case SQL_BINARY:
        case SQL_VARBINARY:
        {
            pBuf[0] = '\0';
            break;
        }

		case SQL_LONGVARBINARY:
		{
			if (hRsSpecialType == VARBYTE)
				rs_strncpy(pBuf, "'",2);
			else
				pBuf[0] = '\0';

			break;
		}

        case SQL_CHAR:
        case SQL_WCHAR:
        {
            rs_strncpy(pBuf,"'",2);
            break;
        }

        default:
        {
            pBuf[0] = '\0';
            break;
        }

    } // SQL Type

    return;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get type name of the given SQL data type.
//
void getTypeName(short hType, char *pBuf, int bufLen, short hRsSpecialType)
{
    switch(hType)
    {
        case SQL_NUMERIC:
        {
            rs_strncpy(pBuf,"NUMERIC", bufLen);
            break;
        }

        case SQL_DECIMAL:
        {
            rs_strncpy(pBuf,"DECIMAL", bufLen);
            break;
        }

        case SQL_SMALLINT:
        {
			rs_strncpy(pBuf,"SMALLINT", bufLen);
            break;
        }

        case SQL_INTEGER:
        {
			rs_strncpy(pBuf,"INTEGER", bufLen);
            break;
        }

        case SQL_BIGINT:
        {
			rs_strncpy(pBuf,"BIGINT", bufLen);
            break;
        }

        case SQL_REAL:
        {
			rs_strncpy(pBuf,"REAL", bufLen);
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
			rs_strncpy(pBuf,"DOUBLE PRECISION", bufLen);
            break;
        }

        case SQL_BIT:
        case SQL_TINYINT:
        {
			rs_strncpy(pBuf,"BOOL", bufLen);
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
			rs_strncpy(pBuf,"DATE", bufLen);
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            if(hRsSpecialType == TIMESTAMPTZOID)
            	rs_strncpy(pBuf,"TIMESTAMPTZ", bufLen);
            else
                rs_strncpy(pBuf,"TIMESTAMP", bufLen);
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            rs_strncpy(pBuf,"INTERVAL YEAR TO MONTH", bufLen);
            break;    
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            rs_strncpy(pBuf,"INTERVAL DAY TO SECOND", bufLen);
            break;   
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            if(hRsSpecialType == TIMETZOID)
            	rs_strncpy(pBuf,"TIMETZ", bufLen);
            else
                rs_strncpy(pBuf,"TIME", bufLen);
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        {
			rs_strncpy(pBuf,"CHARACTER", bufLen);
            break;
        }

        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
        {
            if (hRsSpecialType == SUPER)
                rs_strncpy(pBuf, "SUPER", bufLen);
            else if (hRsSpecialType == BOOLOID)
                rs_strncpy(pBuf, "bool", bufLen);
            else
                rs_strncpy(pBuf, "CHARACTER VARYING",
                    bufLen);
            break;
        }

		case SQL_LONGVARBINARY:
		{
			if (hRsSpecialType == VARBYTE)
				rs_strncpy(pBuf, "VARBYTE", bufLen);
			else
			if (hRsSpecialType == GEOGRAPHY)
				rs_strncpy(pBuf, "GEOGRAPHY", bufLen);
			else
			if (hRsSpecialType == GEOMETRY
				|| hRsSpecialType == GEOMETRYHEX)
				rs_strncpy(pBuf, "GEOMETRY", bufLen);
			else
			if (hRsSpecialType == HLLSKETCH)
				rs_strncpy(pBuf, "HLLSKETCH", bufLen);
			else
				rs_strncpy(pBuf, "BINARY", bufLen);
			break;
		}

        default:
        {
            pBuf[0] = '\0';
            break;
        }

    } // SQL Type

    return;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the numeric precision radix of the given SQL data type.
//
int getNumPrecRadix(short hType)
{
    int iNumPrexRadix;

    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iNumPrexRadix = 10;
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        case SQL_VARCHAR:
        case SQL_WVARCHAR:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        {
            iNumPrexRadix = 0;
            break;
        }

        default:
        {
            iNumPrexRadix = 0;
            break;
        }
    } // SQL Type

    return iNumPrexRadix;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the octet length of the given SQL data type.
//
int getOctetLen(short hSQLType, int iSize, short hRsSpecialType)
{
    int iOctetSize;

    switch(hSQLType)
    {
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        {
            if(hRsSpecialType == TIMETZOID && iSize == 0)
                iOctetSize = MAX_TIMETZOID_SIZE; // 8 + . + 6 (microsecs) + 5 (+/- hh:mm)
            else
                iOctetSize = iSize;
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR: /* bytes */
        case SQL_WVARCHAR: /* bytes */
		case SQL_WLONGVARCHAR:
		{
            iOctetSize = iSize;
            break;
        }

		case SQL_LONGVARBINARY:
		{
			iOctetSize = iSize;
			break;
		}

        case SQL_NUMERIC:
        {
            iOctetSize = (iSize > 0) ? iSize + 2 : sizeof(SQL_NUMERIC_STRUCT);
            break;
        }

        case SQL_BIT:
        case SQL_TINYINT:
        {
            iOctetSize = 1;
            break;
        }

        case SQL_SMALLINT:
        {
            iOctetSize = 2;
            break;
        }

        case SQL_INTEGER:
        {
            iOctetSize = 4;
            break;
        }

        case SQL_BIGINT:
        {
            iOctetSize = 20;
            break;
        }

        case SQL_REAL:
        {
            iOctetSize = 4;
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iOctetSize = 8;
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            iOctetSize = 6; // size of SQL_DATE_STRUCT
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            iOctetSize = 16; // size of SQL_TIMESTAMP_STRUCT
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            iOctetSize = sizeof(SQL_INTERVAL_STRUCT); // size of interval struct
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iOctetSize = sizeof(SQL_INTERVAL_STRUCT); // size of interval struct
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            iOctetSize = 6; // size of SQL_TIME_STRUCT
            break;
        }

        default:
        {
            iOctetSize = 0;
            break;
        }

    } // SQL Type

    return iOctetSize;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the octet length of the given C data type.
//
int getOctetLenUsingCType(short hCType, int iSize)
{
    int iOctetSize;

    switch(hCType)
    {
        case SQL_C_CHAR:
        {
            iOctetSize = iSize;
            break;
        }

        case SQL_C_WCHAR:
        {
            iOctetSize = iSize;
            break;
        }

        case SQL_C_NUMERIC:
        {
            iOctetSize = sizeof(SQL_NUMERIC_STRUCT);
            break;
        }

        case SQL_C_BIT:
        case SQL_C_TINYINT:
        case SQL_C_STINYINT:
        case SQL_C_UTINYINT:
        {
            iOctetSize = 1;
            break;
        }

        case SQL_C_SHORT:
        case SQL_C_SSHORT:
        case SQL_C_USHORT:
        {
            iOctetSize = 2;
            break;
        }

        case SQL_C_LONG:
        case SQL_C_ULONG:
        case SQL_C_SLONG:
        {
            iOctetSize = 4;
            break;
        }

        case SQL_C_SBIGINT:
        case SQL_C_UBIGINT:
        {
            iOctetSize = 8; // size of int64
            break;
        }

        case SQL_C_FLOAT:
        {
            iOctetSize = 4;
            break;
        }

        case SQL_C_DOUBLE:
        {
            iOctetSize = 8;
            break;
        }

        case SQL_C_TYPE_DATE:
        case SQL_C_DATE:
        {
            iOctetSize = 6; // size of SQL_DATE_STRUCT
            break;
        }

        case SQL_C_TYPE_TIMESTAMP:
        case SQL_C_TIMESTAMP:
        {
            iOctetSize = 16; // size of SQL_TIMESTAMP_STRUCT
            break;
        }

        case SQL_C_INTERVAL_YEAR_TO_MONTH:
        {
            iOctetSize = sizeof(SQL_INTERVAL_STRUCT); // size of interval struct
            break;
        }
        case SQL_C_INTERVAL_DAY_TO_SECOND:
        {
            iOctetSize = sizeof(SQL_INTERVAL_STRUCT); // size of interval struct
            break;
        }

        case SQL_C_TYPE_TIME:
        case SQL_C_TIME:
        {
            iOctetSize = 6; // size of SQL_TIME_STRUCT
            break;
        }

        default:
        {
            iOctetSize = 0;
            break;
        }

    } // C Type

    return iOctetSize;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the precision of the given SQL type.
//
int getPrecision(short hType, int iSize, short hRsSpecialType)
{
    int iPrec;

    switch(hType)
    {
        case SQL_VARCHAR:
        case SQL_WVARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		{
            if(hRsSpecialType == TIMETZOID)
                iPrec = MAX_TIMETZOID_SIZE; // 8 + . + 6 (microsecs) + 5 (+/- hh:mm)
            else
                iPrec = 0;
            break;
        }

		case SQL_LONGVARBINARY:
		{
			iPrec = 0;
			break;
		}

        case SQL_CHAR:
        case SQL_WCHAR:
        case SQL_BIT:
        case SQL_TINYINT:
        {
            iPrec = 0;
            break;
        }

        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            iPrec = iSize;
            break;
        }

        case SQL_SMALLINT:
        {
            iPrec = 5;
            break;
        }

        case SQL_INTEGER:
        {
            iPrec = 10;
            break;
        }

        case SQL_BIGINT:
        {
            iPrec = 19; // 20 for unsigned 
            break;
        }

        case SQL_REAL:
        {
            iPrec = 7;
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iPrec = 15;
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            iPrec = 0; // Date doesn't have any second fraction
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            iPrec = 6; // microsecond precision
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            iPrec = 0;
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iPrec = 6;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            iPrec = 6; // time has up to 6 digits of second fraction
            break;
        }

        default:
        {
            iPrec = 0;
            break;
        }
    } // SQL Type

    return iPrec;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get searchable value of the given SQL data type.
//
int getSearchable(short hType, short hRsSpecialType)
{
    int iSearchable;

    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iSearchable = SQL_PRED_BASIC;
            break;
        }

        case SQL_VARCHAR:
        case SQL_WVARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WLONGVARCHAR:
		{
            iSearchable = (hRsSpecialType == TIMETZOID) ? SQL_PRED_BASIC : SQL_PRED_SEARCHABLE;
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        {
            iSearchable = SQL_PRED_SEARCHABLE;
            break;
        }

		case SQL_LONGVARBINARY:
		{
			iSearchable = SQL_PRED_SEARCHABLE;
			break;
		}

        default:
        {
            iSearchable = SQL_PRED_NONE;
            break;
        }

    } // SQL Type

    return iSearchable;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the unnamed value of the given column name.
//
int getUnNamed(char *pName)
{
    return (pName && pName[0] != '\0') ? SQL_NAMED : SQL_UNNAMED;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get whether given SQL data type is unsigned or not.
//
int getUnsigned(short hType)
{
    int iUnSigned;

    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iUnSigned = SQL_FALSE;
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        case SQL_VARCHAR:
        case SQL_WVARCHAR:
        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
        case SQL_LONGVARCHAR:
        case SQL_WLONGVARCHAR:
        {
            iUnSigned = SQL_TRUE;
            break;
        }

        default:
        {
            iUnSigned = SQL_FALSE;
            break;
        }
    } // SQL Type

    return iUnSigned;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get updatable attribute value of a column.
//
int getUpdatable()
{
    return SQL_ATTR_READWRITE_UNKNOWN;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether given field identifier has string value or not.
//
int isStrFieldIdentifier(SQLUSMALLINT hFieldIdentifier)
{
    int iIsStrFieldIdentifier;

    if(hFieldIdentifier == SQL_DESC_BASE_COLUMN_NAME
        || hFieldIdentifier == SQL_DESC_BASE_TABLE_NAME
        || hFieldIdentifier == SQL_DESC_CATALOG_NAME
        || hFieldIdentifier == SQL_DESC_LABEL
        || hFieldIdentifier == SQL_DESC_LITERAL_PREFIX
        || hFieldIdentifier == SQL_DESC_LITERAL_SUFFIX
        || hFieldIdentifier == SQL_DESC_LOCAL_TYPE_NAME
        || hFieldIdentifier == SQL_DESC_NAME
        || hFieldIdentifier == SQL_DESC_SCHEMA_NAME
        || hFieldIdentifier == SQL_DESC_TABLE_NAME
        || hFieldIdentifier == SQL_DESC_TYPE_NAME)
    {
        iIsStrFieldIdentifier = TRUE;
    }
    else
    {
        iIsStrFieldIdentifier = FALSE;
    }

    return iIsStrFieldIdentifier;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Map the column otpion to column attribute constant.
//
SQLUSMALLINT mapColAttributesToColAttributeIdentifier(SQLUSMALLINT hOption)
{
    SQLUSMALLINT hFieldIdentifier;

    switch(hOption)
    {
        case SQL_COLUMN_AUTO_INCREMENT: hFieldIdentifier = SQL_DESC_AUTO_UNIQUE_VALUE; break;
        case SQL_COLUMN_CASE_SENSITIVE: hFieldIdentifier = SQL_DESC_CASE_SENSITIVE; break;
        case SQL_COLUMN_QUALIFIER_NAME: hFieldIdentifier = SQL_DESC_CATALOG_NAME; break;
        case SQL_COLUMN_TYPE: hFieldIdentifier = SQL_DESC_CONCISE_TYPE; break;
        case SQL_COLUMN_COUNT: hFieldIdentifier = SQL_DESC_COUNT; break;
        case SQL_COLUMN_DISPLAY_SIZE: hFieldIdentifier = SQL_DESC_DISPLAY_SIZE; break;
        case SQL_COLUMN_MONEY: hFieldIdentifier = SQL_DESC_FIXED_PREC_SCALE; break;
        case SQL_COLUMN_LABEL: hFieldIdentifier = SQL_DESC_LABEL; break;
        case SQL_COLUMN_LENGTH: hFieldIdentifier = SQL_DESC_LENGTH; break;
        case SQL_COLUMN_NAME: hFieldIdentifier = SQL_DESC_NAME; break;
        case SQL_COLUMN_NULLABLE: hFieldIdentifier = SQL_DESC_NULLABLE; break;
        case SQL_COLUMN_PRECISION: hFieldIdentifier = SQL_DESC_PRECISION; break;
        case SQL_COLUMN_SCALE: hFieldIdentifier = SQL_DESC_SCALE; break;
        case SQL_COLUMN_OWNER_NAME: hFieldIdentifier = SQL_DESC_SCHEMA_NAME; break;
        case SQL_COLUMN_SEARCHABLE: hFieldIdentifier = SQL_DESC_SEARCHABLE; break;
        case SQL_COLUMN_TABLE_NAME: hFieldIdentifier = SQL_DESC_TABLE_NAME; break;
        case SQL_COLUMN_TYPE_NAME: hFieldIdentifier = SQL_DESC_TYPE_NAME; break;
        case SQL_COLUMN_UNSIGNED: hFieldIdentifier = SQL_DESC_UNSIGNED; break;
        case SQL_COLUMN_UPDATABLE: hFieldIdentifier = SQL_DESC_UPDATABLE; break;
        default: hFieldIdentifier = hOption; break;
    }

    return hFieldIdentifier;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release prepare SQL statement(s) associated with ODBC statement.
//
void releasePrepares(RS_STMT_INFO *pStmt)
{
    RS_PREPARE_INFO *curr;

    // free prepare
    curr = pStmt->pPrepareHead;
    while(curr != NULL)
    {
        RS_PREPARE_INFO *next = curr->pNext;

        // Free DescribeParam IPD recs, if any. Normally it move to pStmt->pIPD.
        curr->pIPDRecs = (RS_DESC_REC *)rs_free(curr->pIPDRecs);

        // Free prepare
        libpqReleasePrepare(curr);
        delete curr;

        curr = next;
    }

    if(pStmt->pPrepareHead)
    {
        // Release DescribeParam IPD recs
        pStmt->pIPD->pDescHeader.hHighestCount = 0;
        releaseDescriptorRecs(pStmt->pIPD);
        pStmt->pIPD->iRecListType = RS_DESC_RECS_LINKED_LIST;
    }

    pStmt->pPrepareHead = NULL;
}

/*====================================================================================================================================================*/

// Set trace level and trace file info.
//
void setTraceLevelAndFile(int iTracelLevel, char *pTraceFile) {
    getGlobalLogVars()->iTraceLevel = iTracelLevel;

    if (pTraceFile && *pTraceFile != '\0')
        rs_strncpy(getGlobalLogVars()->szTraceFile, pTraceFile,
                   sizeof(getGlobalLogVars()->szTraceFile));
    else {
        DWORD dwRetVal = 0;
        char szTempPath[MAX_PATH + 1];

        dwRetVal = GetTempPath(MAX_PATH, szTempPath);
        if (dwRetVal > MAX_PATH || (dwRetVal == 0)) {
            szTempPath[0] = '\0';
        }

        snprintf(getGlobalLogVars()->szTraceFile,
                 sizeof(getGlobalLogVars()->szTraceFile), "%s%s%s", szTempPath,
                 (szTempPath[0] != '\0') ? PATH_SEPARATOR : "",
                 TRACE_FILE_NAME);
    }
}

// Set trace level and trace file info from connection string properties
int readAndSetLogInfoFromConnectionString(
    RS_CONNECT_PROPS_INFO *pConnectProps) {
    int rc = 1;
    if (pConnectProps->iLogLevel > -1 &&
        getGlobalLogVars()->iTraceLevel != pConnectProps->iLogLevel) {
        getGlobalLogVars()->iTraceLevel = pConnectProps->iLogLevel;
        rc = 0;
    }

    if (pConnectProps->szLogPath[0] != '\0' &&
        (strcmp(getGlobalLogVars()->szTraceFile, pConnectProps->szLogPath) !=
         0)) {
        snprintf(getGlobalLogVars()->szTraceFile,
                 sizeof(getGlobalLogVars()->szTraceFile), "%s%s%s",
                 pConnectProps->szLogPath, PATH_SEPARATOR, TRACE_FILE_NAME);
        rc = 0;
    }
    return rc;
}

void initTraceFromConnectionString(RS_CONNECT_PROPS_INFO *pConnectProps) {
    if (0 == readAndSetLogInfoFromConnectionString(pConnectProps)) {
        // Anything useful? Then override
        initTrace(true);
    }
}

void initTrace(int canOverride) {
//---------------------------------------------------------------------------------------------------------igarish
    if (false == canOverride) {
        if (getGlobalLogVars()->isInitialized) {
            return;
        }
    }
    // By this time, we assume respective settings are initialized
    getGlobalLogVars()->isInitialized = 0;
    initializeLogging();
    getGlobalLogVars()->isInitialized = 1;
}

void uninitTrace() {
    if (!getGlobalLogVars()->isInitialized) {
        return;
    }
    shutdownLogging();
    getGlobalLogVars()->isInitialized = 0;
}
// Read resgitry or odbc.ini for trace options and set trace variables.
//
void readAndSetTraceInfo()
{
    char  szTraceLevel[MAX_NUMBER_BUF_LEN + 1];
    char  szTraceFile[MAX_PATH + 1];
    int   iTraceLevel;

    // Read the LogLevel from TRACE_KEY_NAME in HKEY_LOCAL_MACHINE
    szTraceLevel[0] = '\0';

#ifdef WIN32
    readRegistryKey(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME, RS_LOG_LEVEL_OPTION_NAME, szTraceLevel, MAX_NUMBER_BUF_LEN);
#endif
#if defined LINUX 
    RsIni::getPrivateProfileString(ODBC_SECTION_NAME, RS_LOG_LEVEL_OPTION_NAME, "", szTraceLevel, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif

	iTraceLevel = atoi(szTraceLevel);
	szTraceFile[0] = '\0';

	if (iTraceLevel != LOG_LEVEL_OFF)
	{
		// Read the LogPath from TRACE_KEY_NAME in HKEY_LOCAL_MACHINE

#ifdef WIN32
		readRegistryKey(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME, RS_LOG_PATH_OPTION_NAME, szTraceFile, MAX_PATH);
#endif
#if defined LINUX 
		RsIni::getPrivateProfileString(ODBC_SECTION_NAME, RS_LOG_PATH_OPTION_NAME, "", szTraceFile, MAX_PATH, ODBC_INI);
#endif

		if (szTraceFile[0] != '\0')
		{
			snprintf(szTraceFile + strlen(szTraceFile), sizeof(szTraceFile) - strlen(szTraceFile),  "%s%s", PATH_SEPARATOR, TRACE_FILE_NAME);
		}
	}


    if(iTraceLevel == LOG_LEVEL_OFF)
    {
        szTraceLevel[0] = '\0';
		szTraceFile[0] = '\0';


        if(iTraceLevel == LOG_LEVEL_OFF)
        {
			// Read LogLevel from INI file
            int readOptions = readTraceOptionsFromIniFile(szTraceLevel, MAX_NUMBER_BUF_LEN, NULL, 0);

            if(readOptions && (szTraceLevel[0] != '\0'))
                iTraceLevel = atoi(szTraceLevel);
            else
                iTraceLevel = DEFAULT_TRACE_LEVEL;

			readTraceOptionsFromIniFile(NULL, 0, szTraceFile, MAX_PATH);
        }

		if (iTraceLevel == LOG_LEVEL_OFF)
		{
			// Read the Trace from TRACE_KEY_NAME in HKEY_LOCAL_MACHINE
			szTraceLevel[0] = '\0';
			szTraceFile[0] = '\0';

#ifdef WIN32
			readRegistryKey(HKEY_LOCAL_MACHINE, TRACE_KEY_NAME, DM_TRACE_VAL_NAME, szTraceLevel, MAX_NUMBER_BUF_LEN);
#endif
#if defined LINUX 
			RsIni::getPrivateProfileString(ODBC_SECTION_NAME, DM_TRACE_VAL_NAME, "", szTraceLevel, MAX_NUMBER_BUF_LEN, ODBC_INI);
#endif

			iTraceLevel = atoi(szTraceLevel);
		}
    }



    // Set the trace level and file
    setTraceLevelAndFile(iTraceLevel, szTraceFile);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get parameter size of the given SQL data type.
//
int getParamSize(short hType)
{
    int iSize;

    switch(hType)
    {
        case SQL_CHAR:
        case SQL_WCHAR:
        case SQL_VARCHAR:
        case SQL_WVARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WLONGVARCHAR:
        {
            iSize = 65535;
            break;
        }

        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            iSize = 1000;
            break;
        }

        case SQL_BIT:
        case SQL_TINYINT:
        {
            iSize = 1;
            break;
        }

        case SQL_SMALLINT:
        {
            iSize = 5;
            break;
        }

        case SQL_INTEGER:
        {
            iSize = 10;
            break;
        }

        case SQL_BIGINT:
        {
            iSize = 19;
            break;
        }

        case SQL_REAL:
        {
            iSize = 15;
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iSize = 53;
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            iSize = 10;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            iSize = 26;
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            iSize = 32;
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iSize = 64;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            iSize = 15;
            break;
        }

        default:
        {
            iSize = 0;
            break;
        }

    } // SQL Type

    return iSize;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the parameter scale of the given SQL data type.
//
short getParamScale(short hType)
{
    short hScale;

    switch(hType)
    {
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            hScale = 5000;
            break;
        }

        case SQL_CHAR:
        case SQL_WCHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            hScale = 0;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            hScale = 6; // millionth of seconds (6)
            break;
        }

        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            hScale = 6;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            hScale = 6; // millionth of seconds (6)
            break;
        }

        default:
        {
            hScale = 0;
            break;
        }

    } // SQL Type

    return hScale;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of bind parameters.
//
void clearBindParamList(RS_STMT_INFO *pStmt)
{
    RS_DESC_INFO *pAPD = pStmt->pStmtAttr->pAPD;

    // Reset previous values of DataAtExec
    pStmt->pszCmdDataAtExec = NULL;
    pStmt->iExecutePreparedDataAtExec = 0;
    pStmt->lParamProcessedDataAtExec = 0;
    pStmt->pAPDRecDataAtExec = NULL;

    // Release APD recs

	if(pAPD) {
		pAPD->pDescHeader.hHighestCount = 0;
    }
    releaseDescriptorRecs(pAPD);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
/**
 * Convert C data into SQL data.
 * 
 * @param pStmt              Pointer to statement info
 * @param pParamData         Pointer to parameter data
 * @param iParamDataLen      Length of parameter data
 * @param plParamDataStrLenInd Pointer to parameter data length/indicator
 * @param hCType             C data type
 * @param hSQLType           SQL data type
 * @param hPrepSQLType       Prepared SQL data type
 * @param pBindParamStrBuf   Pointer to bind parameter string buffer
 * @param piConversionError  Pointer to conversion error flag
 * 
 * @return Pointer to converted parameter value or NULL on error
 */
char *convertCParamDataToSQLData(RS_STMT_INFO *pStmt, char *pParamData, int iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, 
                                  short hSQLType, short hPrepSQLType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, RS_DESC_REC *pDescRec, int *piConversionError)
{
    int iConversionError = FALSE;
    char *pcVal;
    short hType;
    int iColumnSize = 0;
    // Store the specific SQLSTATE code for the error
    char *sqlstate = "HY000"; // Default general error

    if(hCType == SQL_C_DEFAULT)
    {
        hType = getDefaultCTypeFromSQLType(hSQLType, &iConversionError);
    }
    else
        hType = hCType;

    if(iConversionError) {
        sqlstate = "07006"; 
        pcVal = NULL;
        goto error;
    }

    RS_LOG_TRACE("convertCParamDataToSQLData",
                 "Convert C Param Data to SQL Data: hSQLType = %d, hType = %d, "
                 "hPrepSQLType = %d",
                 hSQLType, hType, hPrepSQLType);

    // This could happen when Bind parameter occurs using descriptor
    if(hSQLType == 0)
        hSQLType = hPrepSQLType;

    if (pDescRec) {
        iColumnSize = pDescRec->iSize;
    }

    switch(hSQLType)
    {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_LONGVARCHAR: // SUPER
        case SQL_WLONGVARCHAR:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                case SQL_C_NUMERIC:
                case SQL_C_INTERVAL_YEAR_TO_MONTH:
                case SQL_C_INTERVAL_DAY_TO_SECOND:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_CHAR
		case SQL_LONGVARBINARY: // GEOMETRY, VARBYTE, GEOGRAPHY
		{
			switch (hType)
			{
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_BINARY:
			{
				GET_PARAM_VAL_AND_CHECK();
                break;
            }

			default:
			{
                iConversionError = TRUE;
                sqlstate = "07006";
                break;
			}

			} // C Type

			break;
		} // SQL_LONGVARCHAR (SUPER)

        case SQL_SMALLINT:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }
            } // C Type

            break;
        } // SQL_SMALLINT

        case SQL_INTEGER:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                case SQL_C_BINARY:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_INTEGER

        case SQL_BIGINT:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                case SQL_C_BINARY:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_BIGINT

        case SQL_REAL:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    // If app bind param sql type and describe param sql type is not matching.
                    if(hSQLType != hPrepSQLType && pcVal && *pcVal != '\0')
                    {
                        switch(hPrepSQLType)
                        {
                            case SQL_INTEGER:
                            {
                                // Convert float to integer
                                int iData  = (int) atof(pcVal);

                                if(pBindParamStrBuf->iAllocDataLen > 0)
                                {
                                    pBindParamStrBuf->pBuf = (char *)rs_free(pBindParamStrBuf->pBuf);
                                    pBindParamStrBuf->iAllocDataLen = 0;
                                }

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),"%d", iData);
                                pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;

                                break;
                            }

                            case SQL_SMALLINT:
                            {
                                // Convert float to short
                                short hData  = (short) atof(pcVal);

                                if(pBindParamStrBuf->iAllocDataLen > 0)
                                {
                                    pBindParamStrBuf->pBuf = (char *)rs_free(pBindParamStrBuf->pBuf);
                                    pBindParamStrBuf->iAllocDataLen = 0;
                                }

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", hData);
                                pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;

                                break;
                            }

                            case SQL_BIGINT:
                            {
                                // Convert float to short
                                long long llData  = (long long) atof(pcVal);

                                if(pBindParamStrBuf->iAllocDataLen > 0)
                                {
                                    pBindParamStrBuf->pBuf = (char *)rs_free(pBindParamStrBuf->pBuf);
                                    pBindParamStrBuf->iAllocDataLen = 0;
                                }

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", llData);
                                pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;

                                break;
                            }

                            default:
                            {
                                // Do nothing
                                break;
                            }
                        } // Switch
                    }

                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_REAL

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                case SQL_C_BINARY:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    // If app bind param sql type and describe param sql type is not matching.
                    if(hSQLType != hPrepSQLType && pcVal && *pcVal != '\0')
                    {
                        switch(hPrepSQLType)
                        {
                            case SQL_INTEGER:
                            {
                                // Convert float to integer
                                int iData  = (int) atof(pcVal);

                                if(pBindParamStrBuf->iAllocDataLen > 0)
                                {
                                    pBindParamStrBuf->pBuf = (char *)rs_free(pBindParamStrBuf->pBuf);
                                    pBindParamStrBuf->iAllocDataLen = 0;
                                }

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", iData);
                                pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;

                                break;
                            }

                            case SQL_SMALLINT:
                            {
                                // Convert float to short
                                short hData  = (short) atof(pcVal);

                                if(pBindParamStrBuf->iAllocDataLen > 0)
                                {
                                    pBindParamStrBuf->pBuf = (char *)rs_free(pBindParamStrBuf->pBuf);
                                    pBindParamStrBuf->iAllocDataLen = 0;
                                }

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", hData);
                                pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;

                                break;
                            }

                            case SQL_BIGINT:
                            {
                                // Convert float to short
                                long long llData  = (long long) atof(pcVal);

                                if(pBindParamStrBuf->iAllocDataLen > 0)
                                {
                                    pBindParamStrBuf->pBuf = (char *)rs_free(pBindParamStrBuf->pBuf);
                                    pBindParamStrBuf->iAllocDataLen = 0;
                                }

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", llData);
                                pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;

                                break;
                            }

                            default:
                            {
                                // Do nothing
                                break;
                            }
                        }
                    }

                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_DOUBLE

        case SQL_BIT:
        case SQL_TINYINT:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }
            } // C Type

            break;
        } // SQL_BIT or SQL_TINYINT

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            switch(hType)
            {
                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    if(pcVal)
                    {
                        pcVal = trim_whitespaces(pcVal);
                        size_t len = strlen(pcVal);
                        // Date case
                        // DATE_STRING_LEN + 6 ({d ' '})
                        if (len >= (DATE_STRING_LEN + 6) &&
                            (strncmp(pcVal, "{d '", 4) == 0) &&
                            (strncmp(&pcVal[len - 2], "'}", 2) == 0)) {
                            // Strip the {d ...} wrapper
                            pcVal += 4; // Skip "{d '"
                            len -= 6; // Remove "{d '" (4) and "'}" (2) from length
                            pcVal[len] = '\0';  // Terminate the string after removing wrapper
                        }
                        // Timestamp case
                        else if (len >= (TS_NO_FRAC_LEN + 7) &&
                                (strncmp(pcVal, "{ts '", 5) == 0) &&
                                (strncmp(&pcVal[len - 2], "'}", 2)) == 0) {
                            // Strip the {ts ...} wrapper
                            pcVal += 5; // Skip "{ts '"
                            len -= 7; // Remove "{ts '" (5) and "'}" (2) from length
                            pcVal[len] = '\0';  // Terminate the string after removing wrapper
                        }
                    }

                    break;
                }

                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                case SQL_C_TYPE_TIME:
                case SQL_C_TIME:

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_TYPE_DATE

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            switch(hType)
            {
                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_TIME:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    if(pcVal)
                    {
                        pcVal = trim_whitespaces(pcVal);
                        size_t len = strlen(pcVal);

                        if (len >= (TS_NO_FRAC_LEN + 7) &&
                            (strncmp(pcVal, "{ts '", 5) == 0) &&
                            (strncmp(&pcVal[len - 2], "'}", 2) == 0)) {
                            // Strip the {ts ...} wrapper
                            pcVal += 5; // Skip "{ts '"
                            len -= 7;   // Remove "{ts '" (5) and "'}" (2) from length
                            pcVal[len] = '\0';  // Terminate the string after removing wrapper
                        }
                    }
                    break;
                }

                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_TYPE_TIMESTAMP

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            switch(hType)
            {
                case SQL_C_INTERVAL_YEAR_TO_MONTH:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    if(pcVal)
                    {
                        if(*pcVal == '{' && *(pcVal + 1) == 'i' && *(pcVal + 2) == 'v' && *(pcVal + 3) == 'l')
                        {
                            char *pTemp = strchr(pcVal, '}');

                            if(pTemp)
                            {
                                pcVal += 4;
                                *pTemp = ' ';
                            }
                        }
                    }
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_TYPE_INTERVAL_YEAR_TO_MONTH

        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            switch(hType)
            {
                case SQL_C_INTERVAL_DAY_TO_SECOND:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    if(pcVal)
                    {
                        if(*pcVal == '{' && *(pcVal + 1) == 'i' && *(pcVal + 2) == 'v' && *(pcVal + 3) == 'l')
                        {
                            char *pTemp = strchr(pcVal, '}');

                            if(pTemp)
                            {
                                pcVal += 4;
                                *pTemp = ' ';
                            }
                        }
                    }
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_TYPE_INTERVAL_DAY_TO_SECOND

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            switch(hType)
            {
                case SQL_C_TYPE_TIME:
                case SQL_C_TIME:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TIMESTAMP:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    if(pcVal)
                    {
                        TIMESTAMP_STRUCT *ptsVal = (TIMESTAMP_STRUCT *)pParamData;

                        // Validate time values
                        if (!validateTime(ptsVal->hour, ptsVal->minute, ptsVal->second, ptsVal->fraction))
                        {
                            iConversionError = TRUE;
                            sqlstate = "22007";
                            break;
                        }

                        // Format the time portion only
                        if (ptsVal->fraction == 0) {
                            // No fractional seconds
                            snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), 
                                    "%02hd:%02hd:%02hd", 
                                    ptsVal->hour, ptsVal->minute, ptsVal->second);
                        } else {
                            iConversionError = TRUE;
                            sqlstate = "22008";
                            break;
                        }

                        pcVal = pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                    }
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    if(pcVal)
                    {
                        pcVal = trim_whitespaces(pcVal);
                        if(*pcVal == '{' && *(pcVal + 1) == 't' && ((*(pcVal + 2) == ' ') || *(pcVal + 2) == '\'')
                          )
                        {
                            char *pTemp = strchr(pcVal, '}');

                            if(pTemp)
                            {
                                pcVal += 2;
                                *pTemp = ' ';
                            }
                        }
                    }

                    break;
                }


                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                case SQL_C_TYPE_DATE:
                case SQL_C_DATE:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_TYPE_TIME

        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            switch(hType)
            {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_SHORT:
                case SQL_C_SSHORT:
                case SQL_C_USHORT:
                case SQL_C_LONG:
                case SQL_C_SLONG:
                case SQL_C_ULONG:
                case SQL_C_SBIGINT:
                case SQL_C_UBIGINT:
                case SQL_C_FLOAT:
                case SQL_C_DOUBLE:
                case SQL_C_BIT:
                case SQL_C_TINYINT:
                case SQL_C_STINYINT:
                case SQL_C_UTINYINT:
                case SQL_C_NUMERIC:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_TYPE_TIME:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                case SQL_C_TIME:
                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C Type

            break;
        } // SQL_NUMERIC SQL_DECIMAL

        default:
        {
            switch(hCType)
            {
                case SQL_C_DEFAULT:
                {
                    GET_PARAM_VAL_AND_CHECK();
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    sqlstate = "07006";
                    break;
                }

            } // C type

            break;
        } // Default

    } // SQL Type

    if(iConversionError)
        goto error;

    if(piConversionError)
        *piConversionError = iConversionError;

    if (!iConversionError && sqlstate && strcmp(sqlstate, "HY000") != 0)
    {
        if (pStmt){
            if (strcmp(sqlstate, "22001") == 0)
                addWarning(&pStmt->pErrorList, sqlstate, "Possible truncation", 0, NULL);
            else
                addWarning(&pStmt->pErrorList, sqlstate, "", 0, NULL);
        }
    }

    return pcVal;

error:

    if(iConversionError)
    {
        char szErrMsg[MAX_ERR_MSG_LEN];
        if(hType == SQL_C_DEFAULT){
            snprintf(szErrMsg, sizeof(szErrMsg), "SQL_C_DEFAULT type conversion is not supported for %hd SQL type", hSQLType);
        }
        else if (strcmp(sqlstate, "07006") == 0){
            snprintf(szErrMsg, sizeof(szErrMsg), "Parameter type conversion is not supported from %hd SQL type to %hd C type.", hSQLType,hCType);
        }
        else if (strcmp(sqlstate, "22008") == 0) {
            snprintf(szErrMsg, sizeof(szErrMsg), "Datetime field overflow");
        }
        else if (strcmp(sqlstate, "22015") == 0) {
            snprintf(szErrMsg, sizeof(szErrMsg), "Interval field overflow");
        }
        else if (strcmp(sqlstate, "22007") == 0) {
            snprintf(szErrMsg, sizeof(szErrMsg), "Invalid datetime format");
        }
        else if (strcmp(sqlstate, "22001") == 0) {
            snprintf(szErrMsg, sizeof(szErrMsg), "String data, right truncation");
        }
        else {
            snprintf(szErrMsg, sizeof(szErrMsg), "Conversion error for SQL type %hd to C type %hd", hSQLType, hCType);
        }

        if(pStmt)
            addError(&pStmt->pErrorList, sqlstate, szErrMsg, 0, NULL);
    }

    if(piConversionError)
        *piConversionError = iConversionError;

    return NULL;
}

/**
 * @brief Helper function to format year value with BC suffix handling
 *
 * Determines how to format a year value based on the target SQL type.
 * For string types (CHAR, VARCHAR, WLONGVARCHAR), negative years are kept as-is.
 * For other types, negative years are converted to positive with BC suffix appended.
 *
 * @param year             Input year value (negative for BC dates)
 * @param targetSQLType    Target SQL type (SQL_CHAR, SQL_VARCHAR, SQL_TIMESTAMP, etc.)
 * @param outYear          [OUT] Formatted year value
 * @param outAppendBC      [OUT] Flag indicating if " BC" suffix should be appended
 */
static void formatYearWithBC(short year, short targetSQLType, short *outYear, bool *outAppendBC) {
    *outAppendBC = false;

    if (year <= 0) {
        if (targetSQLType == SQL_CHAR || targetSQLType == SQL_VARCHAR || targetSQLType == SQL_WLONGVARCHAR) {
            *outYear = year;  // Keep negative sign for string types
        } else {
            *outYear = (short)(-year);  // Use absolute value and set BC flag
            *outAppendBC = true;
        }
    } else {
        *outYear = year;  // Positive years handled the same for all types
    }
}

/*====================================================================================================================================================*/
void printHexSQLCHAR(SQLCHAR *sqlchar, int len,
                     const std::function<void(const std::string &)> &logFunc) {
    // Number of bytes to read
    const int numBytes = 1024;
    std::vector<unsigned char> buffer(numBytes, 0);

    bool safeToRead = false;
    try {
        // Attempt to copy the bytes from the SQLCHAR* variable
        std::memcpy(buffer.data(), sqlchar, len);
        safeToRead = true;
    } catch (const std::exception &e) {
        logFunc("Exception occurred while reading memory: " +
                std::string(e.what()) + "\n");
        // Copy as many bytes as safely accessible
        int i;
        for (i = 0; i < len && i < numBytes; ++i) {
            try {
                buffer[i] = reinterpret_cast<unsigned char *>(sqlchar)[i];
            } catch (const std::exception &e) {
                break; // Stop copying if an exception occurs
            }
        }
        // Null-terminate the accessible portion of buffer if partial read
        for (; i < numBytes; ++i) {
            buffer[i] = 0;
        }
    }

    if (safeToRead) {
        // If memory was safely read, proceed to print it
        // Each byte needs 3 chars ("XX ") and one for null-terminator
        std::vector<char> hexString(numBytes * 4, 0);
        int pos = 0;
        logFunc("Hex bytes:\n");
        for (int i = 0; i < len && i < numBytes; ++i) {
            pos += std::sprintf(&hexString[pos], "%02X ", buffer[i]);
            if ((i + 1) % 16 == 0) {
                // Add a newline every 16 bytes for readability
                hexString[pos++] = '\n';
            }
        }
        hexString[pos] = '\0'; // Null-terminate the string
        logFunc(hexString.data());
    }
}

void printHexSQLWCHR(SQLWCHAR *sqlwchr,
                     int charLen, // code units or SQL_NTS
                     const std::function<void(const std::string &)> &logFunc,
                     int cuSize_) {
    constexpr int kMaxDumpBytes = 1024; // hard cap for logging
    if (!sqlwchr) {
        logFunc("Printing SQLWCHAR* as hex bytes:");
        logFunc("Error: sqlwchr pointer is null");
        return;
    }

    const int cuSize = cuSize_ ;

    // Determine how many code units to copy
    int codeUnits = 0;
    if (charLen == SQL_NTS) {
        // Scan for NUL terminator in *code units*, capped by our byte limit
        const int kMaxCU = kMaxDumpBytes / cuSize;
        const SQLWCHAR *p = sqlwchr;
        while (codeUnits < kMaxCU && p[codeUnits] != 0) {
            ++codeUnits;
        }
        // Include the terminator if it fits
        if (codeUnits < kMaxCU)
            ++codeUnits;
    } else if (charLen >= 0) {
        codeUnits = charLen;
    } else {
        // Unknown length: show up to the cap (best-effort debug)
        codeUnits = kMaxDumpBytes / cuSize;
    }

    // Compute bytes, cap to our buffer
    const int totalBytesToCopy = codeUnits * cuSize;
    const int bytesToCopy = (std::min<int>)(totalBytesToCopy, kMaxDumpBytes);

    // Copy to a local buffer (bounded)
    std::vector<unsigned char> buffer(bytesToCopy);
    std::memcpy(buffer.data(), sqlwchr, static_cast<size_t>(bytesToCopy));

    // Build hex dump (with newlines every 16 bytes)
    std::string out;
    out.reserve(bytesToCopy * 3 + (bytesToCopy ? (bytesToCopy - 1) / 16 : 0) +
                8);

    logFunc("Printing SQLWCHAR* as hex bytes:");
    logFunc("Hex bytes:");
    for (int i = 0; i < bytesToCopy; ++i) {
        char tmp[4];
        std::snprintf(tmp, sizeof(tmp), "%02X", buffer[i]);
        out.append(tmp);
        out.push_back(' ');
        if (((i + 1) % 16) == 0)
            out.push_back('\n');
    }
    if (!out.empty() && out.back() == ' ')
        out.pop_back();
    logFunc(out);

    if (bytesToCopy < totalBytesToCopy) {
        logFunc("[TRUNCATED: showing first " + std::to_string(bytesToCopy) +
                " of " + std::to_string(totalBytesToCopy) + " bytes]");
    }
}

//---------------------------------------------------------------------------------------------------------igarish

/**
 * Get parameter value as string from C buffer using given C data type.
 * 
 * @param pParamData           Pointer to parameter data
 * @param iParamDataLen        Length of parameter data
 * @param plParamDataStrLenInd Pointer to parameter data length/indicator
 * @param hCType               C data type
 * @param pBindParamStrBuf     Pointer to bind parameter string buffer
 * @param hSQLType             SQL data type
 * @param iColumnSize          Size of the column (for checking truncation)
 * @param pConversionError     Pointer to conversion error flag
 * @param pSqlstate            Pointer to SQLSTATE code
 * 
 * @return Pointer to converted parameter value or NULL on error
 */
char *getParamVal(char *pParamData, int iParamDataLen, SQLLEN *plParamDataStrLenInd, 
                 short hCType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, short hSQLType,
                 int iColumnSize, int *pConversionError, char **pSqlstate) {

    int iIndicator = (plParamDataStrLenInd) ? (int) *plParamDataStrLenInd : 0;
    pBindParamStrBuf->iAllocDataLen = 0;

    if (pConversionError) *pConversionError = FALSE;

    if (pSqlstate) *pSqlstate = "HY000"; // Default general error

    if(pParamData)
    {
        switch(hCType)
        {
            case SQL_C_CHAR:
            {
                if(iIndicator == SQL_NTS)
                {
                    pBindParamStrBuf->pBuf = pParamData;
                }
                else
                if(iIndicator == SQL_NULL_DATA)
                {
                    pBindParamStrBuf->pBuf = NULL;
                }
                else
                {
                    if(iParamDataLen >= 0 && plParamDataStrLenInd != NULL)
                    {
                        if(iParamDataLen == 0 && iIndicator > 0)
                            iParamDataLen = iIndicator;
                        else
                        if(iParamDataLen > 0 && iIndicator >= 0)
                        {
                            iParamDataLen = redshift_min(iParamDataLen, iIndicator);
                        }
                    }

                    if(iParamDataLen == 0)
                    {
                        pBindParamStrBuf->buf[0] = '\0';
                        pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                    }
                    else
                    if(iParamDataLen > 0)
                    {
                        pBindParamStrBuf->pBuf = rs_strdup(pParamData, iParamDataLen);
                        pBindParamStrBuf->iAllocDataLen = iParamDataLen;
                    }
                    else
                        pBindParamStrBuf->pBuf = NULL;
                }

                break;
            }

            case SQL_C_WCHAR:
            {
                size_t cchLen;
                if(iParamDataLen >= 0 && plParamDataStrLenInd != NULL)
                {
                    if(iParamDataLen == 0 && iIndicator > 0)
                        iParamDataLen = iIndicator;
                    else
                    if(iParamDataLen > 0 && iIndicator >= 0)
                    {
                        iParamDataLen = redshift_min(iParamDataLen, iIndicator);
                    }
                }

                auto convertAndAllocate = [&](size_t cchLen) -> bool {
                    std::string utf8Str;
                    static const int charsize = sizeof(SQLCHAR);
                    size_t len = sqlwchar_to_utf8_str((SQLWCHAR *)pParamData,
                                                      cchLen, utf8Str);

                    size_t bufSize = len;
                    pBindParamStrBuf->pBuf =
                        (char *)rs_malloc(bufSize + charsize);
                    if (!pBindParamStrBuf->pBuf) {
                        RS_LOG_ERROR("RSUTIL",
                                     "Memory allocation for %zu bytes failed "
                                     "in convertAndAllocate",
                                     (bufSize + charsize));
                        return false;
                    }
                    pBindParamStrBuf->iAllocDataLen = (int)(bufSize + charsize);
                    memcpy(pBindParamStrBuf->pBuf, (char *)utf8Str.c_str(),
                           bufSize);
                    memset((char *)pBindParamStrBuf->pBuf + bufSize, '\0',
                           charsize);
                    return true;
                };

                if (iIndicator == SQL_NULL_DATA) {
                    pBindParamStrBuf->pBuf = NULL;
                } else if (iIndicator == SQL_NTS) {
                    // SQL_NTS: Find the actual length of wide string
                    // Respect buffer size limit even for null-terminated strings
                    size_t maxChars = (iParamDataLen > 0)
                                          ? iParamDataLen / sizeofSQLWCHAR()
                                          : kSQLWCHAR_SCAN_CAP;
                    cchLen = sqlwcsnlen_cap((SQLWCHAR *)pParamData, maxChars);
                    if (!convertAndAllocate(cchLen)) {
                        break;
                    }
                } else if (iParamDataLen > 0) {
                    // Use the provided length in bytes, convert to character
                    // count
                    if (plParamDataStrLenInd != NULL && iIndicator >= 0) {
                        iParamDataLen = redshift_min(iParamDataLen, iIndicator);
                    }

                    cchLen = iParamDataLen / sizeofSQLWCHAR();
                    if (!convertAndAllocate(cchLen)) {
                        break;
                    }
                } else if (iParamDataLen == 0 && iIndicator > 0 &&
                           plParamDataStrLenInd != NULL) {
                    // Use indicator as the data length
                    iParamDataLen = iIndicator;

                    cchLen = iParamDataLen / sizeofSQLWCHAR();
                    if (!convertAndAllocate(cchLen)) {
                        break;
                    }
                } else if (iParamDataLen == 0) {
                    pBindParamStrBuf->buf[0] = '\0';
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                } else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_SHORT:
            case SQL_C_SSHORT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    short value = *(short *)pParamData;

                    // Check for range when converting to smaller types
                    if (hSQLType == SQL_TINYINT && (value > CHAR_MAX || value < CHAR_MIN)) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", value);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }
            case SQL_C_USHORT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    unsigned short value = *(unsigned short *)pParamData;

                    // Check for range issues when converting to smaller types
                    if (hSQLType == SQL_TINYINT && value > UCHAR_MAX) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hu", value);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_LONG:
            case SQL_C_SLONG:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    int iVal = *(int *)pParamData;

                    // Check for range issues when converting to smaller types
                    if (hSQLType == SQL_SMALLINT && (iVal > SHRT_MAX || iVal < SHRT_MIN)) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    } 
                    else if (hSQLType == SQL_TINYINT && (iVal > CHAR_MAX || iVal < CHAR_MIN)) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    if(hSQLType == SQL_SMALLINT)
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", (short)iVal);
                    else
                    if(hSQLType == SQL_INTEGER)
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)iVal);
                    else
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", iVal);

                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }
            case SQL_C_ULONG:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    unsigned int uiVal = *(unsigned int *)pParamData;

                    // Check for range issues when converting to smaller types
                    if (hSQLType == SQL_SMALLINT && uiVal > USHRT_MAX) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    } 
                    else if (hSQLType == SQL_TINYINT && uiVal > UCHAR_MAX) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    if(hSQLType == SQL_SMALLINT)
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hu", (unsigned short)uiVal);
                    else
                    if(hSQLType == SQL_INTEGER)
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%u", (unsigned int)uiVal);
                    else
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%u", uiVal);

                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_SBIGINT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    long long llVal = *(long long *)pParamData;

                    // Check for range issues when converting to smaller types
                    if (hSQLType == SQL_INTEGER && (llVal > INT_MAX || llVal < INT_MIN)) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    } 
                    else if (hSQLType == SQL_SMALLINT && (llVal > SHRT_MAX || llVal < SHRT_MIN)) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    } 
                    else if (hSQLType == SQL_TINYINT && (llVal > CHAR_MAX || llVal < CHAR_MIN)) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", llVal);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }
            case SQL_C_UBIGINT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    unsigned long long ullVal = *(unsigned long long *)pParamData;

                    // Check for range issues when converting to smaller types
                    if (hSQLType == SQL_INTEGER && ullVal > UINT_MAX) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    } 
                    else if (hSQLType == SQL_SMALLINT && ullVal > USHRT_MAX) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    } 
                    else if (hSQLType == SQL_TINYINT && ullVal > UCHAR_MAX) 
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;

                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%llu", ullVal);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_FLOAT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    float fVal = *(float *)pParamData;

                    // Check for range issues when converting to integer types
                    if (hSQLType == SQL_BIGINT && (fVal >= (double)LLONG_MAX || fVal < (double)LLONG_MIN
                        || isnan(fVal) || isinf(fVal)))
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;
                        if (pSqlstate) 
                            *pSqlstate = "22003";
                        return NULL;
                    }
                    else if (hSQLType == SQL_INTEGER && (fVal >= (double)INT_MAX || fVal < (double)INT_MIN
                            || isnan(fVal) || isinf(fVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;

                        if (pSqlstate)
                            *pSqlstate = "22003";

                        return NULL;
                    }
                    else if (hSQLType == SQL_SMALLINT && (fVal >= (double)SHRT_MAX || fVal < (double)SHRT_MIN
                            || isnan(fVal) || isinf(fVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22003";
                        return NULL;
                    }
                    else if (hSQLType == SQL_TINYINT && (fVal >= (double)CHAR_MAX || fVal < (double)CHAR_MIN
                            || isnan(fVal) || isinf(fVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    // For integer types, format as integer without decimal places
                    if (hSQLType == SQL_BIGINT) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", (long long)fVal);
                    } else if (hSQLType == SQL_INTEGER) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)fVal);
                    } else if (hSQLType == SQL_SMALLINT) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", (short)fVal);
                    } else if (hSQLType == SQL_TINYINT) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)fVal);
                    } else {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%.9g", fVal);
                    }
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_DOUBLE:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    double dVal = *(double *)pParamData;

                    // Check for range issues when converting to integer types
                    if (hSQLType == SQL_BIGINT && (dVal >= (double)LLONG_MAX || dVal < (double)LLONG_MIN
                        || isnan(dVal) || isinf(dVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22003";
                        return NULL;
                    }
                    else if (hSQLType == SQL_INTEGER && (dVal >= (double)INT_MAX || dVal < (double)INT_MIN
                            || isnan(dVal) || isinf(dVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;

                        if (pSqlstate)
                            *pSqlstate = "22003";

                        return NULL;
                    }
                    else if (hSQLType == SQL_SMALLINT && (dVal >= (double)SHRT_MAX || dVal < (double)SHRT_MIN
                            || isnan(dVal) || isinf(dVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22003";
                        return NULL;
                    }
                    else if (hSQLType == SQL_TINYINT && (dVal >= (double)CHAR_MAX || dVal < (double)CHAR_MIN
                            || isnan(dVal) || isinf(dVal)))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22003";
                        return NULL;
                    }

                    // For integer types, format as integer without decimal places
                    if (hSQLType == SQL_BIGINT) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", (long long)dVal);
                    } else if (hSQLType == SQL_INTEGER) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)dVal);
                    } else if (hSQLType == SQL_SMALLINT) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", (short)dVal);
                    } else if (hSQLType == SQL_TINYINT) {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)dVal);
                    } else {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%.38g", dVal);
                    }
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_BIT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    char val = *(char *)pParamData;

                    if(val == '1' || val == 1 || val == 't' || val == 'T')
                        val = '1';
                    else
                        val = '0';

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%c", val);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }
            case SQL_C_TINYINT:
            case SQL_C_STINYINT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    // Handle as signed char
                    signed char val = *(signed char *)pParamData;

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", val);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }
            case SQL_C_UTINYINT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    // Handle as unsigned char
                    unsigned char val = *(unsigned char *)pParamData;

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%u", val);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_TYPE_DATE:
            case SQL_C_DATE:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    DATE_STRUCT *pdtVal = (DATE_STRUCT *)pParamData;

                    // Validate date values
                    if (!validateDate(pdtVal->year, pdtVal->month, pdtVal->day))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22008";
                        return NULL;
                    }

                    // Format year with BC handling
                    short yearValue;
                    bool appendBC = false;
                    formatYearWithBC(pdtVal->year, hSQLType, &yearValue, &appendBC);

                    // Format the date as YYYY-MM-DD
                    if (hSQLType == SQL_TIMESTAMP || hSQLType == SQL_TYPE_TIMESTAMP) {
                        // Converting DATE to TIMESTAMP - add time portion
                        if (appendBC) {
                            snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                    "%04hd-%02hd-%02hd 00:00:00 BC",
                                    yearValue, pdtVal->month, pdtVal->day);
                        } else {
                            snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                    "%04hd-%02hd-%02hd 00:00:00",
                                    yearValue, pdtVal->month, pdtVal->day);
                        }
                    } else {
                        // Normal DATE case
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                "%04hd-%02hd-%02hd",
                                yearValue, pdtVal->month, pdtVal->day);

                        // Append BC suffix if needed
                        if (appendBC) {
                            size_t len = strlen(pBindParamStrBuf->buf);
                            if (len + 4 <= sizeof(pBindParamStrBuf->buf)) {
                                strcat(pBindParamStrBuf->buf, " BC");
                            }
                        }
                    }

                    // Check for SQL_CHAR column size constraints
                    if (hSQLType == SQL_CHAR && iColumnSize > 0) 
                    {
                        int dateStrLen = strlen(pBindParamStrBuf->buf);
                        if (dateStrLen > iColumnSize) 
                        {
                            // Not enough space in the column for the date string
                            if (pConversionError) 
                                *pConversionError = TRUE;
                            if (pSqlstate) 
                                *pSqlstate = "22001";
                            return NULL;
                        }
                    }

                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_TYPE_TIMESTAMP:
            case SQL_C_TIMESTAMP:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    TIMESTAMP_STRUCT *ptsVal = (TIMESTAMP_STRUCT *)pParamData;

                    // Validate timestamp values
                    if (!validateDateTime(ptsVal->year, ptsVal->month, ptsVal->day,
                        ptsVal->hour, ptsVal->minute, ptsVal->second, ptsVal->fraction))
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate) 
                            *pSqlstate = "22008";
                        return NULL;
                    }

                    // Format year with BC handling
                    short yearValue;
                    bool appendBC = false;
                    formatYearWithBC(ptsVal->year, hSQLType, &yearValue, &appendBC);

                    if (hSQLType == SQL_DATE || hSQLType == SQL_TYPE_DATE) {
                        // Check if time fields are all zero
                        if (!isTimePortionZero(ptsVal)) {
                            if (pConversionError)
                                *pConversionError = TRUE;
                            if (pSqlstate)
                                *pSqlstate = "22008";
                            return NULL;
                        }
                        // Format only the date portion
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                "%04hd-%02hd-%02hd",
                                yearValue, ptsVal->month, ptsVal->day);
                    }
                    else {
                        // Handle the fraction part
                        if (ptsVal->fraction == 0) {
                            // No fraction part
                            snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                    "%04hd-%02hd-%02hd %02hd:%02hd:%02hd",
                                    yearValue, ptsVal->month, ptsVal->day,
                                    ptsVal->hour, ptsVal->minute, ptsVal->second);
                        } else {
                            // With fraction part
                            snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                    "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%06d",
                                    yearValue, ptsVal->month, ptsVal->day,
                                    ptsVal->hour, ptsVal->minute, ptsVal->second,
                                    (int)(ptsVal->fraction / 1000));
                        }
                    }
                    // Append BC suffix if needed with buffer overflow check
                    if (appendBC) {
                        size_t len = strlen(pBindParamStrBuf->buf);
                        if (len + 4 <= sizeof(pBindParamStrBuf->buf)) {
                            strcat(pBindParamStrBuf->buf, " BC");
                        }
                    }
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_INTERVAL_YEAR_TO_MONTH:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    SQL_INTERVAL_STRUCT *pivlVal = (SQL_INTERVAL_STRUCT *)pParamData;
                    // Validate interval year-to-month values
                    // Note: Negative intervals use interval_sign field,
                    // year/month fields store absolute values
                    if (pivlVal->intval.year_month.year >= MAX_INTERVAL_YEAR ||
                        pivlVal->intval.year_month.month >= MAX_INTERVAL_MONTH)
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22015"; // Interval field overflow
                        return NULL;
                    }

                    intervaly2m_out(pivlVal, pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf));
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }
            case SQL_C_INTERVAL_DAY_TO_SECOND:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    SQL_INTERVAL_STRUCT *pivlVal = (SQL_INTERVAL_STRUCT *)pParamData;

                    // Validate interval day-to-second values
                    // Note: Negative intervals use interval_sign field,
                    // day/hour/minute/second/fraction fields store absolute values
                    if (pivlVal->intval.day_second.day >= MAX_INTERVAL_DAY ||
                        pivlVal->intval.day_second.hour >= MAX_INTERVAL_HOUR ||
                        pivlVal->intval.day_second.minute >= MAX_INTERVAL_MINUTE ||
                        pivlVal->intval.day_second.second >= MAX_INTERVAL_SECOND ||
                        pivlVal->intval.day_second.fraction >= MAX_INTERVAL_FRACTION)
                    {
                        if (pConversionError)
                            *pConversionError = TRUE;
                        if (pSqlstate)
                            *pSqlstate = "22015"; // Interval field overflow
                        return NULL;
                    }

                    intervald2s_out(pivlVal, pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf));
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_TYPE_TIME:
            case SQL_C_TIME:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    TIME_STRUCT *ptVal = (TIME_STRUCT *)pParamData;

                    // Validate time values
                    if (!validateTime(ptVal->hour, ptVal->minute, ptVal->second, 0))
                    {
                        if (pConversionError) 
                            *pConversionError = TRUE;
                        if (pSqlstate) 
                            *pSqlstate = "22008";
                        return NULL;
                    }

                    if (hSQLType == SQL_TIMESTAMP || hSQLType == SQL_TYPE_TIMESTAMP) {
                        // The date portion of the timestamp is set to the current date
                        // Get current date
                        time_t now = time(NULL);
                        struct tm tm_info;
#ifdef WIN32
                        localtime_s(&tm_info, &now);
#else
                        localtime_r(&now, &tm_info);
#endif
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                "%04d-%02d-%02d %02hd:%02hd:%02hd",
                                tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
                                ptVal->hour, ptVal->minute, ptVal->second);
                    } else {
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),
                                "%02hd:%02hd:%02hd", ptVal->hour, ptVal->minute, ptVal->second);
                    }
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_NUMERIC:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)pParamData;
                    char *pNumData = pBindParamStrBuf->buf;
                    int num_data_len = sizeof(pBindParamStrBuf->buf);

                    // Convert the numeric to string
                    convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

                    // helpers
                    auto has_nonzero_fraction = [](const char* s) -> int {
                        const char* dot = strchr(s, '.');
                        if (!dot) {
                            return 0;
                        }
                        for (const char* p = dot + 1; *p; ++p) {
                            if (*p != '0') {
                                return 1;
                            }
                        }
                        return 0;
                    };
                    auto parse_int64 = [&](long long* out) -> int {
                        errno = 0;
                        char *endptr = NULL;
                        long long v = strtoll(pNumData, &endptr, 10);
                        if (endptr && *endptr && *endptr != '.') { // unexpected suffix
                            if (pConversionError) {
                                *pConversionError = TRUE;
                            }
                            if (pSqlstate) {
                                *pSqlstate = "22018"; // invalid character value for cast
                            }
                            return 0;
                        }
                        if (errno == ERANGE) {
                            if (pConversionError) {
                                *pConversionError = TRUE;
                            }
                            if (pSqlstate) {
                                *pSqlstate = "22003"; // out of range
                            }
                            return 0;
                        }
                        *out = v;
                        return 1;
                    };

                    if (hSQLType == SQL_BIGINT) {
                        // Warn only if we’ll actually truncate non-zero fractional part.
                        if (has_nonzero_fraction(pNumData)) {
                            if (pSqlstate) {
                                *pSqlstate = "01S07"; // fractional truncation
                            }
                        }
                        long long v;
                        if (!parse_int64(&v)) {
                            return NULL;
                        }
                        // No further range check needed: strtoll already validated to LLONG range.
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", (long long)v);
                    } else if (hSQLType == SQL_INTEGER) {
                        if (has_nonzero_fraction(pNumData)) {
                            if (pSqlstate) {
                                *pSqlstate = "01S07";
                            }
                        }
                        long long v;
                        if (!parse_int64(&v)) {
                            return NULL;
                        }
                        if (v > INT_MAX || v < INT_MIN) {
                            if (pConversionError) {
                                *pConversionError = TRUE;
                            }
                            if (pSqlstate) {
                                *pSqlstate = "22003";
                            }
                            return NULL;
                        }
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)v);
                    } else if (hSQLType == SQL_SMALLINT) {
                        if (has_nonzero_fraction(pNumData)) {
                            if (pSqlstate) {
                                *pSqlstate = "01S07";
                            }
                        }
                        long long v;
                        if (!parse_int64(&v)) {
                            return NULL;
                        }
                        if (v > SHRT_MAX || v < SHRT_MIN) {
                            if (pConversionError) {
                                *pConversionError = TRUE;
                            }
                            if (pSqlstate) {
                                *pSqlstate = "22003";
                            }
                            return NULL;
                        }
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", (short)v);
                    } else if (hSQLType == SQL_TINYINT) {
                        if (has_nonzero_fraction(pNumData)) {
                            if (pSqlstate) {
                                *pSqlstate = "01S07";
                            }
                        }
                        long long v;
                        if (!parse_int64(&v)) {
                            return NULL;
                        }
                        if (v > CHAR_MAX || v < CHAR_MIN) {
                            if (pConversionError) {
                                *pConversionError = TRUE;
                            }
                            if (pSqlstate) {
                                *pSqlstate = "22003";
                            }
                            return NULL;
                        }
                        snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", (int)v);
                    } else if (hSQLType == SQL_NUMERIC || hSQLType == SQL_DECIMAL) {
                        // Check that the numeric value doesn't exceed the target column's precision
                        if (iColumnSize > 0 && pnVal->precision > iColumnSize) {
                            if (pConversionError) 
                                *pConversionError = TRUE;
                            if (pSqlstate) 
                                *pSqlstate = "22003"; // Numeric value out of range
                            return NULL;
                        }
                    }

                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_BINARY:
            {

                switch(hSQLType){
                    case SQL_LONGVARBINARY:
                    case SQL_BINARY:
                    case SQL_VARBINARY:
                    {
                        if(iIndicator != SQL_NULL_DATA)
                        {
                            //c_binary to sql_binary is a default conversion
                            pBindParamStrBuf->pBuf= pParamData;
                        }
                        else
                        {
                            pBindParamStrBuf->pBuf = NULL;
                        }
                        break;
                    }
                    case SQL_DOUBLE:
                    {
                        if(iIndicator != SQL_NULL_DATA)
                        {
                            // Interpret binary data as double
                            if (iParamDataLen >= sizeof(double))
                            {
                                double dVal;
                                memcpy(&dVal, pParamData, sizeof(double));

                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%.38g", dVal);
                                pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                            }
                            else
                            {
                                // Not enough data for a double
                                if (pConversionError)
                                    *pConversionError = TRUE;
                                if (pSqlstate)
                                    *pSqlstate = "22003";
                                return NULL;
                            }
                        }
                        else
                        {
                            pBindParamStrBuf->pBuf = NULL;
                        }
                        break;
                    }
                    case SQL_FLOAT:
                    {
                        if(iIndicator != SQL_NULL_DATA)
                        {
                            // Interpret binary data as float
                            if (iParamDataLen >= sizeof(float))
                            {
                                float fVal;
                                memcpy(&fVal, pParamData, sizeof(float));
                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%f", fVal);
                                pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                            }
                            else
                            {
                                // Not enough data for a float
                                if (pConversionError) 
                                    *pConversionError = TRUE;
                                if (pSqlstate) 
                                    *pSqlstate = "22003";
                                return NULL;
                            }
                        }
                        else
                        {
                            pBindParamStrBuf->pBuf = NULL;
                        }
                        break;
                    }
                    case SQL_INTEGER:
                    {
                        if(iIndicator != SQL_NULL_DATA)
                        {
                            // Interpret binary data as int
                            if (iParamDataLen >= sizeof(int))
                            {
                                int iVal;
                                memcpy(&iVal, pParamData, sizeof(int));
                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%d", iVal);
                                pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                            }
                            else
                            {
                                // Not enough data for an int
                                if (pConversionError) 
                                    *pConversionError = TRUE;
                                if (pSqlstate) 
                                    *pSqlstate = "22003";
                                return NULL;
                            }
                        }
                        else
                        {
                            pBindParamStrBuf->pBuf = NULL;
                        }
                        break;
                    }
                    case SQL_BIGINT:
                    {
                        if(iIndicator != SQL_NULL_DATA)
                        {
                            // Interpret binary data as long long
                            if (iParamDataLen >= sizeof(long long))
                            {
                                long long llVal;
                                memcpy(&llVal, pParamData, sizeof(long long));
                                
                                snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", llVal);
                                pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                            }
                            else
                            {
                                // Not enough data for a long long
                                if (pConversionError) 
                                    *pConversionError = TRUE;
                                if (pSqlstate) 
                                    *pSqlstate = "22003";
                                return NULL;
                            }
                        }
                        else
                        {
                            pBindParamStrBuf->pBuf = NULL;
                        }
                        break;
                    }
                    default: {
                        // Binary to other non-binary type conversion not supported
                        if (pConversionError) 
                            *pConversionError = TRUE;
                        if (pSqlstate) 
                            *pSqlstate = "07006";
                        return NULL;
                    }
                }
                break;
            }
            default:
            {
                // Unsupported C type
                if (pConversionError) 
                    *pConversionError = TRUE;
                if (pSqlstate) 
                    *pSqlstate = "07006";

                pBindParamStrBuf->pBuf = NULL;
                break;
            }

        } // SQL Type
    }
    else
        pBindParamStrBuf->pBuf = NULL;

    return pBindParamStrBuf->pBuf;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the default C data type of the given SQL data type.
//
short getDefaultCTypeFromSQLType(short hSQLType, int *piConversionError)
{
    short hCType;
    int iConversionError = FALSE;

    switch(hSQLType)
    {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_NUMERIC:
        case SQL_DECIMAL:
        {
            hCType = SQL_C_CHAR;
            break;
        }

        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
        {
            hCType = SQL_C_WCHAR;
            break;
        }

        case SQL_SMALLINT:
        {
            hCType = SQL_C_SHORT;
            break;
        }

        case SQL_INTEGER:
        {
            hCType = SQL_C_LONG;
            break;
        }

        case SQL_BIGINT:
        {
            hCType = SQL_C_SBIGINT;
            break;
        }

        case SQL_REAL:
        {
            hCType = SQL_C_FLOAT;
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            hCType = SQL_C_DOUBLE;
            break;
        }

        case SQL_BIT:
        {
            hCType = SQL_C_BIT;
            break;
        }

        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        {
            hCType = SQL_C_BINARY;
            break;
        }

        case SQL_TINYINT:
        {
            hCType = SQL_C_TINYINT;
            break;
        }

        case SQL_TYPE_DATE:
        {
            hCType = SQL_C_TYPE_DATE;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        {
            hCType = SQL_C_TYPE_TIMESTAMP;
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            hCType = SQL_C_INTERVAL_YEAR_TO_MONTH;
            break;
        }

        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            hCType = SQL_C_INTERVAL_DAY_TO_SECOND;
            break;
        }

        case SQL_TYPE_TIME:
        {
            hCType = SQL_C_TYPE_TIME;
            break;
        }

        case SQL_DATE:
        {
            hCType = SQL_C_DATE;
            break;
        }

        case SQL_TIMESTAMP:
        {
            hCType = SQL_C_TIMESTAMP;
            break;
        }

        case SQL_TIME:
        {
            hCType = SQL_C_TIME;
            break;
        }

        default:
        {
            iConversionError = TRUE;
            hCType = SQL_C_DEFAULT;
            break;
        }

    } // SQL type

    if(piConversionError)
        *piConversionError = iConversionError;

    RS_LOG_DEBUG("RSUTIL",
                    "getDefaultCTypeFromSQLType hSQLType=%d hCType=%d",
                    hSQLType, hCType);

    return hCType;

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Count the bind parameters.
//
int countBindParams(RS_DESC_REC *pDescRecHead)
{
    RS_DESC_REC *pDescRec;
    int count = 0;

    for(pDescRec = pDescRecHead; pDescRec != NULL; pDescRec = pDescRec->pNext)
        count++;

    return count;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of given descriptor records.
//
void releaseDescriptorRecs(RS_DESC_INFO *pDesc)
{
    if(pDesc->iRecListType == RS_DESC_RECS_LINKED_LIST)
    {
        RS_DESC_REC *curr;

        // free descriptor recs
        curr = pDesc->pDescRecHead;
        while(curr != NULL)
        {
            RS_DESC_REC *next = curr->pNext;

            curr->pDataAtExec = freeDataAtExec(curr->pDataAtExec);
            curr = (RS_DESC_REC *)rs_free(curr);
            curr = next;
        }

        pDesc->pDescRecHead = NULL;
    }
    else
        pDesc->pDescRecHead = (RS_DESC_REC *)rs_free(pDesc->pDescRecHead);

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add descriptor record in the list.
//
void addDescriptorRec(RS_DESC_INFO *pDesc, RS_DESC_REC *pDescRec, int iAtFront)
{
    if(iAtFront)
    {
        // Put Desc record in front in the list
        pDescRec->pNext = pDesc->pDescRecHead;
        pDesc->pDescRecHead = pDescRec;
    }
    else
    {
        // Put Desc record at end in the list
        if(pDesc->pDescRecHead == NULL)
        {
            pDesc->pDescRecHead = pDescRec;
        }
        else
        {
            RS_DESC_REC *prev = NULL;
            RS_DESC_REC *cur  = pDesc->pDescRecHead;

            while(cur != NULL)
            {
                prev = cur;
                cur = cur->pNext;
            }

            prev->pNext = pDescRec;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources related to a descriptor record.
//
void releaseDescriptorRec(RS_DESC_INFO *pDesc, RS_DESC_REC *pDescRec)
{
    if(pDesc->iRecListType == RS_DESC_RECS_LINKED_LIST)
    {
        RS_DESC_REC *curr;
        RS_DESC_REC *prev;

        // Remove from Desc rec list
        curr  = pDesc->pDescRecHead;
        prev  = NULL;

        while(curr != NULL)
        {
            if(curr == pDescRec)
            {
                if(prev == NULL)
                    pDesc->pDescRecHead = pDesc->pDescRecHead->pNext;
                else
                    prev->pNext = curr->pNext;

                curr->pNext = NULL;

                // Free memory
                curr->pDataAtExec = freeDataAtExec(curr->pDataAtExec);
                curr = (RS_DESC_REC *)rs_free(curr);

                break;
            }

            prev = curr;
            curr = curr->pNext;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release the descriptor record related to given record number.
//
void releaseDescriptorRecByNum(RS_DESC_INFO *pDesc, short hRecNumber)
{
    if(pDesc->iRecListType == RS_DESC_RECS_LINKED_LIST)
    {
        RS_DESC_REC *curr;
        RS_DESC_REC *prev;

        // Remove from Desc rec list
        curr  = pDesc->pDescRecHead;
        prev  = NULL;

        while(curr != NULL)
        {
            if(curr->hRecNumber == hRecNumber)
            {
                if(prev == NULL)
                    pDesc->pDescRecHead = pDesc->pDescRecHead->pNext;
                else
                    prev->pNext = curr->pNext;

                curr->pNext = NULL;

                // Free memory
                curr->pDataAtExec = freeDataAtExec(curr->pDataAtExec);
                curr = (RS_DESC_REC *)rs_free(curr);

                break;
            }

            prev = curr;
            curr = curr->pNext;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Find the descriptor record related to given record number.
//
RS_DESC_REC *findDescRec(RS_DESC_INFO *pDesc, short hRecNumber)
{
    RS_DESC_REC *pDescRec;

    // Find from rec list
    pDescRec  = pDesc->pDescRecHead;

    while(pDescRec != NULL)
    {
        if(pDescRec->hRecNumber == hRecNumber)
            break;

        pDescRec = pDescRec->pNext;
    }

    return pDescRec;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Find the highest record number of the given descriptor.
//
short findHighestRecCount(RS_DESC_INFO *pDesc)
{
    RS_DESC_REC *pDescRec;
    short hHighestCount = 0;

    // Find highest from rec list
    pDescRec  = pDesc->pDescRecHead;

    while(pDescRec != NULL)
    {
        if(pDescRec->hRecNumber > hHighestCount)
            hHighestCount = pDescRec->hRecNumber;

        pDescRec = pDescRec->pNext;
    }

    return hHighestCount;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Allocate a descriptor record.
//
RS_DESC_INFO *allocateDesc(RS_CONN_INFO *pConn, int iType, int iImplicit)
{
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)new RS_DESC_INFO(pConn, iType, (iImplicit) ? SQL_DESC_ALLOC_AUTO : SQL_DESC_ALLOC_USER);

    return pDesc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of the given descriptor.
//
RS_DESC_INFO *releaseDescriptor(RS_DESC_INFO *pDesc, int iImplicit)
{
    if(pDesc)
    {
        RS_CONN_INFO *pConn = pDesc->phdbc;

        if((!iImplicit
                && (pDesc->iType == RS_APD
                    || pDesc->iType == RS_ARD
                    || pDesc->iType == RS_UNKNOWN_DESC_TYPE)
            )
            || iImplicit
        )
        {
            // Detach it from any/all statement(s) 
            if(pConn 
                && (pDesc->iType == RS_APD
                    || pDesc->iType == RS_ARD))
            {
                RS_STMT_INFO *pStmtCurr = pConn->phstmtHead;

                while(pStmtCurr != NULL)
                {
                    RS_STMT_INFO *pStmtNext = pStmtCurr->pNext;
                    RS_STMT_ATTR_INFO *pStmtAttr = pStmtCurr->pStmtAttr;

                    if(pStmtAttr)
                    {
                        if(pStmtAttr->pAPD 
                            && (pDesc->iType == RS_APD) 
                            && pStmtAttr->pAPD == pDesc)
                        {
                            pStmtAttr->pAPD = pStmtCurr->pAPD;
                        }
                        else
                        if(pStmtAttr->pARD 
                            && (pDesc->iType == RS_ARD) 
                            && pStmtAttr->pARD == pDesc)
                        {
                            pStmtAttr->pARD = pStmtCurr->pARD;
                        }
                    }

                    pStmtCurr = pStmtNext;
                } // Loop
            }

            // Invalidate header
            pDesc->pDescHeader.valid = false;
            releaseDescriptorRecs(pDesc);

            // Free any error info
            pDesc->pErrorList = clearErrorList(pDesc->pErrorList);

            // Free descriptor
            delete pDesc;
            pDesc = NULL;
        }
    }

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// If the descriptor record deson't exist then add it, otherwise return existance one.
//
RS_DESC_REC *checkAndAddDescRec(RS_DESC_INFO *pDesc, short hRecNumber, int iAtFront, int *pNewDecRec)
{
    RS_DESC_REC *pDescRec;

	if (pNewDecRec)
		*pNewDecRec = 0;

    // Find if rec already exist, then it's re-bind.
    pDescRec = findDescRec(pDesc, hRecNumber);
    if((pDescRec == NULL) && (pDesc->iRecListType !=  RS_DESC_RECS_ARRAY_LIST))
    {
        // If not create new one
        pDescRec = (RS_DESC_REC *)rs_calloc(1, sizeof(RS_DESC_REC));

        if(pDescRec)
        {
            pDescRec->hRecNumber = hRecNumber;

            // Initialize default values for date time interval code
            // to 0
            pDescRec->hDateTimeIntervalCode = 0;

            // Initialize default values based on descriptor type
            if (pDesc->iType == RS_ARD || pDesc->iType == RS_APD) {
                pDescRec->hType = SQL_C_DEFAULT;
                pDescRec->hConciseType = SQL_C_DEFAULT;
            } else {
                // For IRD and IPD, leave as 0 until populated from metadata
                pDescRec->hType = 0;
                pDescRec->hConciseType = 0;
            }

            if (pDesc->iType == RS_IPD){
                pDescRec->hInOutType = SQL_PARAM_INPUT;
            }

            // Add in the list
            addDescriptorRec(pDesc, pDescRec,  iAtFront);

			if (pNewDecRec)
				*pNewDecRec = 1;
        }
    }

    return pDescRec;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if given field identifier is a header field otherwise FALSE.
//
int isHeaderField(SQLSMALLINT hFieldIdentifier)
{
    int iIsHeaderField;

    if(hFieldIdentifier == SQL_DESC_ALLOC_TYPE
        || hFieldIdentifier == SQL_DESC_ARRAY_SIZE
        || hFieldIdentifier == SQL_DESC_ARRAY_STATUS_PTR
        || hFieldIdentifier == SQL_DESC_BIND_OFFSET_PTR
        || hFieldIdentifier == SQL_DESC_BIND_TYPE
        || hFieldIdentifier == SQL_DESC_COUNT
        || hFieldIdentifier == SQL_DESC_ROWS_PROCESSED_PTR)
    {
        iIsHeaderField = TRUE;
    }
    else
        iIsHeaderField = FALSE;

    return iIsHeaderField;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if given field identifier for the given descriptor is writable otherwise FALSE.
//
int isWritableField(RS_DESC_INFO *pDesc, SQLSMALLINT hFieldIdentifier)
{
    int iIsWritable = TRUE;
    int iDescType = pDesc->iType;

    if(iDescType != RS_UNKNOWN_DESC_TYPE)
    {
        switch(hFieldIdentifier)
        {
            case SQL_DESC_ALLOC_TYPE:
            case SQL_DESC_AUTO_UNIQUE_VALUE:
            case SQL_DESC_BASE_COLUMN_NAME:
            case SQL_DESC_BASE_TABLE_NAME:
            case SQL_DESC_CASE_SENSITIVE:
            case SQL_DESC_CATALOG_NAME:
            case SQL_DESC_DISPLAY_SIZE:
            case SQL_DESC_FIXED_PREC_SCALE:
            case SQL_DESC_LABEL:
            case SQL_DESC_LITERAL_PREFIX:
            case SQL_DESC_LITERAL_SUFFIX:
            case SQL_DESC_LOCAL_TYPE_NAME:
            case SQL_DESC_NULLABLE:
            case SQL_DESC_SCHEMA_NAME:
            case SQL_DESC_SEARCHABLE:
            case SQL_DESC_TABLE_NAME:
            case SQL_DESC_TYPE_NAME:
            case SQL_DESC_UNSIGNED:
            case SQL_DESC_UPDATABLE:
            {
                iIsWritable = FALSE;
                break;
            }

            case SQL_DESC_COUNT:
            case SQL_DESC_CONCISE_TYPE:
            case SQL_DESC_DATETIME_INTERVAL_CODE:
            case SQL_DESC_DATETIME_INTERVAL_PRECISION:
            case SQL_DESC_LENGTH:
            case SQL_DESC_NUM_PREC_RADIX:
            case SQL_DESC_OCTET_LENGTH:
            case SQL_DESC_PRECISION:
            case SQL_DESC_SCALE:
            case SQL_DESC_TYPE:
            {
                if(iDescType == RS_IRD)
                    iIsWritable = FALSE;

                break;
            }

            case SQL_DESC_ARRAY_SIZE:
            case SQL_DESC_BIND_OFFSET_PTR:
            case SQL_DESC_BIND_TYPE:
            case SQL_DESC_DATA_PTR:
            case SQL_DESC_INDICATOR_PTR:
            case SQL_DESC_OCTET_LENGTH_PTR:
            {
                if(iDescType == RS_IRD || iDescType == RS_IPD)
                    iIsWritable = FALSE;

                break;
            }

            case SQL_DESC_ROWS_PROCESSED_PTR:
            {
                if(iDescType == RS_ARD || iDescType == RS_APD)
                    iIsWritable = FALSE;

                break;
            }

            case SQL_DESC_NAME:
            case SQL_DESC_PARAMETER_TYPE:
            case SQL_DESC_UNNAMED:
            {
                if(iDescType != RS_IPD)
                    iIsWritable = FALSE;
                break;
            }


            case SQL_DESC_ARRAY_STATUS_PTR:
            {
                // Any desc type can write.
                break;
            }

            default:
            {
                break;
            }
        } // Switch
    }

    return iIsWritable;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if given field identifier for the given descriptor is readable otherwise FALSE.
//
// Unused treat as not readable.
 int isReadableField(RS_DESC_INFO *pDesc, SQLSMALLINT hFieldIdentifier)
{
    int iIsReadable = TRUE;
    int iDescType = pDesc->iType;

    if(iDescType != RS_UNKNOWN_DESC_TYPE)
    {
        switch(hFieldIdentifier)
        {
            case SQL_DESC_ALLOC_TYPE:
            case SQL_DESC_ARRAY_STATUS_PTR:
            case SQL_DESC_COUNT:
            case SQL_DESC_CONCISE_TYPE:
            case SQL_DESC_DATETIME_INTERVAL_CODE:
            case SQL_DESC_DATETIME_INTERVAL_PRECISION:
            case SQL_DESC_LENGTH:
            case SQL_DESC_NUM_PREC_RADIX:
            case SQL_DESC_OCTET_LENGTH:
            case SQL_DESC_PRECISION:
            case SQL_DESC_SCALE:
            case SQL_DESC_TYPE:
            {
                // Any desc type allow to read
                break;
            }

            case SQL_DESC_ARRAY_SIZE:
            case SQL_DESC_BIND_OFFSET_PTR:
            case SQL_DESC_BIND_TYPE:
            case SQL_DESC_DATA_PTR:
            case SQL_DESC_INDICATOR_PTR:
            case SQL_DESC_OCTET_LENGTH_PTR:
            {
                if(iDescType == RS_IRD || iDescType == RS_IPD)
                    iIsReadable = FALSE;

                break;
            }

            case SQL_DESC_ROWS_PROCESSED_PTR:
            case SQL_DESC_CASE_SENSITIVE:
            case SQL_DESC_FIXED_PREC_SCALE:
            case SQL_DESC_LOCAL_TYPE_NAME:
            case SQL_DESC_NAME:
            case SQL_DESC_NULLABLE:
            case SQL_DESC_TYPE_NAME:
            case SQL_DESC_UNNAMED:
            case SQL_DESC_UNSIGNED:
            {
                if(iDescType == RS_ARD || iDescType == RS_APD)
                    iIsReadable = FALSE;

                break;
            }

            case SQL_DESC_AUTO_UNIQUE_VALUE:
            case SQL_DESC_BASE_COLUMN_NAME:
            case SQL_DESC_BASE_TABLE_NAME:
            case SQL_DESC_CATALOG_NAME:
            case SQL_DESC_DISPLAY_SIZE:
            case SQL_DESC_LABEL:
            case SQL_DESC_LITERAL_PREFIX:
            case SQL_DESC_LITERAL_SUFFIX:
            case SQL_DESC_SCHEMA_NAME:
            case SQL_DESC_SEARCHABLE:
            case SQL_DESC_TABLE_NAME:
            case SQL_DESC_UPDATABLE:
            {
                if(iDescType != RS_IRD)
                    iIsReadable = FALSE;

                break;
            }

            case SQL_DESC_PARAMETER_TYPE:
            {
                if(iDescType != RS_IPD)
                    iIsReadable = FALSE;

                break;
            }

            default:
            {
                break;
            }
        } // Switch
    }

    return iIsReadable;
} 

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy IRD records from the result.
//
void copyIRDRecsFromResult(RS_RESULT_INFO *pResultHead, RS_DESC_INFO *pIRD)
{
    if(pResultHead && pIRD)
    {
        // Reset header info in pIRD
        pIRD->pDescHeader.hHighestCount = 0;

        // Release pIRD recs, if any
        releaseDescriptorRecs(pIRD);

        // Copy recs
        pIRD->pDescRecHead = pResultHead->pIRDRecs;
        pResultHead->pIRDRecs = NULL;
        pIRD->iRecListType = RS_DESC_RECS_ARRAY_LIST;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy IPD records from the prepare.
//
void copyIPDRecsFromPrepare(RS_PREPARE_INFO *pPrepareHead, RS_DESC_INFO *pIPD)
{
    if(pPrepareHead && pIPD)
    {
        // Reset header info in pIPD
        pIPD->pDescHeader.hHighestCount = 0;

        // Release pIPD recs, if any
        releaseDescriptorRecs(pIPD);

        // Copy recs
        pIPD->pDescRecHead = pPrepareHead->pIPDRecs;
        pPrepareHead->pIPDRecs = NULL;
        pIPD->iRecListType = RS_DESC_RECS_ARRAY_LIST;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if data-at-exec needed otherwise FALSE.
//
int needDataAtExec(RS_STMT_INFO *pStmt, RS_DESC_REC *pDescRecHead, long lParamProcessed,int executePrepared)
{
    int rc = FALSE;
    RS_DESC_REC *pDescRec = pDescRecHead;
    int iNoOfParams = (pStmt->pPrepareHead) ? getNumberOfParams(pStmt) : getParamMarkerCount(pStmt);

    while(pDescRec != NULL)
    {
        if(pDescRec->pcbLenInd != NULL 
            && (pDescRec->hParamSQLType == SQL_CHAR || pDescRec->hParamSQLType == SQL_VARCHAR
                || pDescRec->hType == SQL_C_CHAR || pDescRec->hType == SQL_C_WCHAR)
            && (pDescRec->hRecNumber <= iNoOfParams))
        {
            SQLLEN pcbLen = *(pDescRec->pcbLenInd + lParamProcessed);

            if((pcbLen == SQL_DATA_AT_EXEC 
                || pcbLen <= SQL_LEN_DATA_AT_EXEC_OFFSET)
                && (pDescRec->pDataAtExec == NULL))
            {
                pStmt->pAPDRecDataAtExec = pDescRec;
                rc = TRUE;
                break;
            }
        }

        pDescRec = pDescRec->pNext;
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Allocate and set data-at-exec structure.
//
RS_DATA_AT_EXEC *allocateAndSetDataAtExec(char *pDataPtr, long lStrLenOrInd)
{
    RS_DATA_AT_EXEC *pDataAtExec = (RS_DATA_AT_EXEC *) new RS_DATA_AT_EXEC();

    if(pDataAtExec)
    {
        if(lStrLenOrInd == SQL_NULL_DATA)
        {
            pDataAtExec->pValue = NULL;
            pDataAtExec->cbLen  = 0;
        }
        else
        {
            pDataAtExec->pValue = rs_strdup(pDataPtr, lStrLenOrInd);
            if(pDataAtExec->pValue)
                pDataAtExec->cbLen = strlen(pDataAtExec->pValue);
            else
                pDataAtExec->cbLen  = 0;
        }
    }

    return pDataAtExec;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Append data-at-exec value.
//
RS_DATA_AT_EXEC *appendDataAtExec(RS_DATA_AT_EXEC *pDataAtExec, char *pDataPtr, long lStrLenOrInd)
{
    if(pDataAtExec && pDataPtr)
    {
        char *pVal = pDataAtExec->pValue;
        SQLLEN cbLen = pDataAtExec->cbLen;

        if(lStrLenOrInd == SQL_NTS)
            lStrLenOrInd = (long)strlen(pDataPtr);

        pDataAtExec->pValue = (char *)rs_malloc(cbLen + lStrLenOrInd + 1);
        if(pDataAtExec->pValue)
        {
            if(pVal && cbLen > 0)
                memcpy(pDataAtExec->pValue, pVal, cbLen);
            if(lStrLenOrInd > 0)
                memcpy(pDataAtExec->pValue + cbLen, pDataPtr, lStrLenOrInd);
            pDataAtExec->cbLen = cbLen + lStrLenOrInd;
            pDataAtExec->pValue[pDataAtExec->cbLen] = '\0';
        }
        else
            pDataAtExec = NULL;

        pVal = (char *)rs_free(pVal);
    }

    return pDataAtExec;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources of given data-at-exec.
//
RS_DATA_AT_EXEC *freeDataAtExec(RS_DATA_AT_EXEC *pDataAtExec)
{
    if(pDataAtExec)
    {
        pDataAtExec->pValue = (char *)rs_free(pDataAtExec->pValue);
        pDataAtExec->cbLen = 0;
        delete pDataAtExec;
        pDataAtExec = NULL;
    }

    return pDataAtExec;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset data-at-exec status in the given statement and release resources of it.
//
void resetAndReleaseDataAtExec(RS_STMT_INFO *pStmt)
{
    RS_DESC_INFO *pDesc = pStmt->pStmtAttr->pAPD;

    // Reset previous values
    pStmt->pszCmdDataAtExec = NULL;
    pStmt->iExecutePreparedDataAtExec = 0;
    pStmt->lParamProcessedDataAtExec = 0;
    pStmt->pAPDRecDataAtExec = NULL;

    if(pDesc->iRecListType == RS_DESC_RECS_LINKED_LIST)
    {
        RS_DESC_REC *curr;

        curr = pDesc->pDescRecHead;
        while(curr != NULL)
        {
            RS_DESC_REC *next = curr->pNext;

            curr->pDataAtExec = freeDataAtExec(curr->pDataAtExec);
            curr = next;
        }
    }
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if given diag identifier has string data otherwise FALSE.
//
int isCharDiagIdentifier(SQLSMALLINT     hDiagIdentifier)
{
    int rc;

    if(hDiagIdentifier == SQL_DIAG_CLASS_ORIGIN
        || hDiagIdentifier == SQL_DIAG_SUBCLASS_ORIGIN
        || hDiagIdentifier == SQL_DIAG_CONNECTION_NAME
        || hDiagIdentifier == SQL_DIAG_SERVER_NAME
        || hDiagIdentifier == SQL_DIAG_MESSAGE_TEXT
        || hDiagIdentifier == SQL_DIAG_SQLSTATE)
    {
        rc = TRUE;
    }
    else
    {
        rc = FALSE;
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources related to previous SQL statement before executing new one.
//
void makeItReadyForNewQueryExecution(RS_STMT_INFO *pStmt, int executePrepared, int iReprepareForMultiInsert, int iResetMultiInsert)
{
	// Skip all results of streaming cursor
	libpqCheckAndSkipAllResultsOfStreamingCursor(pStmt, TRUE);

    if(!executePrepared)
    {
        // Release prepared stmt in the server
        libpqExecuteDeallocateCommand(pStmt, TRUE, FALSE);

        // Release prepare stmt related resources
        releasePrepares(pStmt);
    }

    // Release any open result(s)
    releaseResults(pStmt);

    // Reset last data-at-exec
    resetAndReleaseDataAtExec(pStmt);

    // Reset csc thread flag and streaming cursor values
    resetCscStatementConext(pStmt->pCscStatementContext);

	// Set streaming cursor row count
	libpqSetStreamingCursorRows(pStmt);

    if(!executePrepared)
    {

        // Reset multi insert flag
        if(iResetMultiInsert)
        {
            pStmt->iMultiInsert = 0;
            pStmt->iLastBatchMultiInsert = 0;
            releasePaStrBuf(pStmt->pszLastBatchMultiInsertCmd);
            pStmt->pszLastBatchMultiInsertCmd = (RS_STR_BUF *)rs_free(pStmt->pszLastBatchMultiInsertCmd);
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if cursor is scrollable otherwise FALSE.
//
int isScrollableCursor(RS_STMT_INFO *pStmt)
{
    int isScrollable;

    if(pStmt->pStmtAttr->iCursorType != SQL_CURSOR_FORWARD_ONLY
        && pStmt->pStmtAttr->iCursorScrollable == SQL_SCROLLABLE)
    {
        isScrollable = TRUE;
    }
    else
        isScrollable = FALSE;

    return isScrollable;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if cursor is updatable otherwise FALSE.
//
int isUpdatableCursor(RS_STMT_INFO *pStmt)
{
    int isUpdatable;

    if(pStmt->pStmtAttr->iConcurrency != SQL_CONCUR_READ_ONLY)
    {
        isUpdatable = TRUE;
    }
    else
        isUpdatable = FALSE;

    return isUpdatable;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources related to previously executed catalog query and set new catalog query in the buffer.
//
void setCatalogQueryBuf(RS_STMT_INFO *pStmt, char *szCatlogQuery)
{
    // Release previously allocated buf, if any
    releasePaStrBuf(pStmt->pCmdBuf);
    setParamMarkerCount(pStmt,0);

    resetPaStrBuf(pStmt->pCmdBuf);

    pStmt->pCmdBuf->pBuf = szCatlogQuery;
    pStmt->iCatalogQuery = TRUE;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Reset catalog query flag
//
void resetCatalogQueryFlag(RS_STMT_INFO *pStmt)
{
    pStmt->iCatalogQuery = FALSE;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set the thread execution status.
//
void setThreadExecutionStatus(RS_EXEC_THREAD_INFO *pExecThread, SQLRETURN rc)
{
    if(pExecThread && pExecThread->hThread)
    {
        pExecThread->pszCmd = (char *)rs_free(pExecThread->pszCmd);
        pExecThread->executePrepared = 0;
        pExecThread->rc = rc;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return status of the execution complete thread.
//
SQLRETURN checkExecutingThread(RS_STMT_INFO *pStmt)
{
    SQLRETURN rc;
    RS_EXEC_THREAD_INFO *pExecThread = pStmt->pExecThread;

    if(pExecThread)
        rc = pExecThread->rc;
    else
        rc = SQL_SUCCESS;

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Wait for executing thread to finish and then release resources of it.
//
void waitAndFreeExecThread(RS_STMT_INFO *pStmt, int iWaitFlag)
{
    RS_EXEC_THREAD_INFO *pExecThread = pStmt->pExecThread;

    if(pExecThread)
    {
        if(iWaitFlag)
        {
            rsJoinThread(pExecThread->hThread);
        }

        // Free the exec-thread info
        pExecThread->hThread = (THREAD_HANDLE)(long)NULL;
        pExecThread->pszCmd = (char *)rs_free(pExecThread->pszCmd);
        if (pStmt->pExecThread != NULL) {
          delete pStmt->pExecThread;
          pStmt->pExecThread = NULL;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set parameter marker count.
//
void setParamMarkerCount(RS_STMT_INFO *pStmt, int iNumOfParamMarkers)
{
    pStmt->iNumOfParamMarkers = iNumOfParamMarkers;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get parameter marker count.
//
int getParamMarkerCount(RS_STMT_INFO *pStmt)
{
    if(pStmt->iMultiInsert == 0)
        return pStmt->iNumOfParamMarkers;
    else
        return (pStmt->iNumOfParamMarkers/pStmt->iMultiInsert);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get prepare parameter count.
//
int getNumberOfParams(RS_STMT_INFO *pStmt)
{
    if(pStmt && pStmt->pPrepareHead)
    {
        if(pStmt->iMultiInsert == 0)
            return pStmt->pPrepareHead->iNumberOfParams;
        else
            return (pStmt->pPrepareHead->iNumberOfParams/pStmt->iMultiInsert);
    }
    else
        return 0;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Debugging function for test.
//
void Alert()
{
#ifdef WIN32
    MessageBox(NULL,"Debug", "Test", MB_OK);
#endif
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get short value and it's length.
//
void getShortVal(short hVal, short *phVal, SQLINTEGER *pcbLen)
{
    *phVal = hVal;
    if(pcbLen)
        *pcbLen = sizeof(short);

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get int value and it's length.
//
void getIntVal(int iVal, int *piVal, SQLINTEGER *pcbLen)
{
    *piVal = iVal;
    if(pcbLen)
        *pcbLen = sizeof(int);

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get SQLINTEGER value and it's length.
//
void getSQLINTEGERVal(long lVal, SQLINTEGER *piVal, SQLINTEGER *pcbLen)
{
    *piVal = (SQLINTEGER)lVal;
    if(pcbLen)
        *pcbLen = sizeof(SQLINTEGER);

}

// Get SQLLEN value and it's length.
void getSQLLENVal(long lVal, SQLLEN *plVal, SQLINTEGER *pcbLen) {
    *plVal = (SQLLEN)lVal;
    if(pcbLen) {
        *pcbLen = sizeof(SQLLEN);
    }
}

// Get SQLULEN value and it's length.
void getSQLULENVal(long lVal, SQLULEN *pulVal, SQLINTEGER *pcbLen) {
    *pulVal = (SQLULEN)lVal;
    if(pcbLen) {
        *pcbLen = sizeof(SQLULEN);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get pointer value and it's length.
//
void getPointerVal(void *ptrVal, void **ppVal, SQLINTEGER *pcbLen)
{
    *ppVal = ptrVal;
    if(pcbLen)
        *pcbLen = sizeof(void *);

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the datetime sub code.
//
short getDateTimeIntervalCode(short hDateTimeIntervalCode, short hType)
{
    if(hDateTimeIntervalCode == 0)
    {
        if(hType == SQL_TYPE_DATE || hType == SQL_DATE)
            hDateTimeIntervalCode = SQL_CODE_DATE;
        else
        if(hType == SQL_TYPE_TIMESTAMP || hType == SQL_TIMESTAMP)
            hDateTimeIntervalCode = SQL_CODE_TIMESTAMP;
        else
        if(hType == SQL_TYPE_TIME || hType == SQL_TIME)
            hDateTimeIntervalCode = SQL_CODE_TIME;
        else
        if(hType == SQL_INTERVAL_YEAR_TO_MONTH)
            hDateTimeIntervalCode = SQL_CODE_YEAR_TO_MONTH;
        else
        if(hType == SQL_INTERVAL_DAY_TO_SECOND)
            hDateTimeIntervalCode = SQL_CODE_DAY_TO_SECOND;
    }

    return hDateTimeIntervalCode;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get the C type from the given concise type.
//
short getCTypeFromConciseType(short hConciseType, short hDateTimeIntervalCode, short hType)
{
    if(hType == 0 || hType == SQL_C_DEFAULT)
    {
        if(hConciseType != 0)
        {
            if(hConciseType == SQL_C_DATE) 
            {
                if(hDateTimeIntervalCode == SQL_CODE_DATE)
                    hType = SQL_C_DATE;
                else
                if(hDateTimeIntervalCode == SQL_CODE_TIMESTAMP)
                    hType = SQL_C_TIMESTAMP;
                else
                    hType = SQL_C_DATE;
            }
            else
                hType = hConciseType;
        }
        else
            hType = SQL_C_DEFAULT;
    }

    return hType;
}

// Maps datetime concise types to their corresponding subtype codes
SQLSMALLINT mapDatetimeConciseTypeToCode(SQLSMALLINT conciseType) {
    switch (conciseType) {
        case SQL_TYPE_DATE:
        case SQL_DATE:
            return SQL_CODE_DATE;
        case SQL_TYPE_TIME:
        case SQL_TIME:
            return SQL_CODE_TIME;
        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
            return SQL_CODE_TIMESTAMP;
        default:
            return 0;
    }
}

// Maps datetime interval codes to their corresponding concise types
SQLSMALLINT mapDatetimeCodeToConciseType(SQLSMALLINT code, bool isIPD, bool isODBC2) {
    // IPD uses SQL types, application descriptors use C types
    switch (code) {
        case SQL_CODE_DATE:
            return isIPD ? (isODBC2 ? SQL_DATE : SQL_TYPE_DATE) : SQL_C_TYPE_DATE;
        case SQL_CODE_TIME:
            return isIPD ? (isODBC2 ? SQL_TIME : SQL_TYPE_TIME) : SQL_C_TYPE_TIME;
        case SQL_CODE_TIMESTAMP:
            return isIPD ? (isODBC2 ? SQL_TIMESTAMP : SQL_TYPE_TIMESTAMP) : SQL_C_TYPE_TIMESTAMP;
        default:
            return 0;
    }
}

// Maps interval concise types to their corresponding interval codes
SQLSMALLINT mapIntervalConciseTypeToCode(SQLSMALLINT conciseType) {
    switch (conciseType) {
        case SQL_INTERVAL_YEAR:
            return SQL_CODE_YEAR;
        case SQL_INTERVAL_MONTH:
            return SQL_CODE_MONTH;
        case SQL_INTERVAL_DAY:
            return SQL_CODE_DAY;
        case SQL_INTERVAL_HOUR:
            return SQL_CODE_HOUR;
        case SQL_INTERVAL_MINUTE:
            return SQL_CODE_MINUTE;
        case SQL_INTERVAL_SECOND:
            return SQL_CODE_SECOND;
        case SQL_INTERVAL_YEAR_TO_MONTH:
            return SQL_CODE_YEAR_TO_MONTH;
        case SQL_INTERVAL_DAY_TO_HOUR:
            return SQL_CODE_DAY_TO_HOUR;
        case SQL_INTERVAL_DAY_TO_MINUTE:
            return SQL_CODE_DAY_TO_MINUTE;
        case SQL_INTERVAL_DAY_TO_SECOND:
            return SQL_CODE_DAY_TO_SECOND;
        case SQL_INTERVAL_HOUR_TO_MINUTE:
            return SQL_CODE_HOUR_TO_MINUTE;
        case SQL_INTERVAL_HOUR_TO_SECOND:
            return SQL_CODE_HOUR_TO_SECOND;
        case SQL_INTERVAL_MINUTE_TO_SECOND:
            return SQL_CODE_MINUTE_TO_SECOND;
        default:
            return 0;
    }
}

// Maps interval codes to their corresponding concise types
SQLSMALLINT mapIntervalCodeToConciseType(SQLSMALLINT code) {
    switch (code) {
        case SQL_CODE_YEAR:
            return SQL_INTERVAL_YEAR;
        case SQL_CODE_MONTH:
            return SQL_INTERVAL_MONTH;
        case SQL_CODE_DAY:
            return SQL_INTERVAL_DAY;
        case SQL_CODE_HOUR:
            return SQL_INTERVAL_HOUR;
        case SQL_CODE_MINUTE:
            return SQL_INTERVAL_MINUTE;
        case SQL_CODE_SECOND:
            return SQL_INTERVAL_SECOND;
        case SQL_CODE_YEAR_TO_MONTH:
            return SQL_INTERVAL_YEAR_TO_MONTH;
        case SQL_CODE_DAY_TO_HOUR:
            return SQL_INTERVAL_DAY_TO_HOUR;
        case SQL_CODE_DAY_TO_MINUTE:
            return SQL_INTERVAL_DAY_TO_MINUTE;
        case SQL_CODE_DAY_TO_SECOND:
            return SQL_INTERVAL_DAY_TO_SECOND;
        case SQL_CODE_HOUR_TO_MINUTE:
            return SQL_INTERVAL_HOUR_TO_MINUTE;
        case SQL_CODE_HOUR_TO_SECOND:
            return SQL_INTERVAL_HOUR_TO_SECOND;
        case SQL_CODE_MINUTE_TO_SECOND:
            return SQL_INTERVAL_MINUTE_TO_SECOND;
        default:
            return 0;
    }
}

// Checks if a given type is a concise datetime type when performing input validation
// Note that since the value for SQL_DATE is equal to SQL_DATETIME (which is verbose type),
// we can't return true for SQL_DATE
bool isDatetimeType(SQLSMALLINT type) {
    return (type == SQL_TYPE_DATE || type == SQL_TYPE_TIME || type == SQL_TYPE_TIMESTAMP);
}

// Checks if a given type is an interval type
bool isIntervalType(SQLSMALLINT type) {
    return (type >= SQL_INTERVAL_YEAR && type <= SQL_INTERVAL_MINUTE_TO_SECOND);
}

// Checks if a given code is a valid interval code
bool isIntervalCode(SQLSMALLINT code) {
    return (code >= SQL_CODE_YEAR && code <= SQL_CODE_MINUTE_TO_SECOND);
}

// Checks if a given code is a valid datetime code
bool isDateTimeCode(SQLSMALLINT code) {
    return (code >= SQL_CODE_DATE && code <= SQL_CODE_TIMESTAMP);
}

// Checks if an interval code represents an interval with a seconds component
bool isIntervalSecondCode(SQLSMALLINT code) {
    return (code == SQL_CODE_SECOND || code == SQL_CODE_DAY_TO_SECOND || code == SQL_CODE_HOUR_TO_SECOND || code == SQL_CODE_MINUTE_TO_SECOND);
}

bool isValidOdbcNonDateTimeSQLType(SQLSMALLINT type) {
    switch (type) {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
        case SQL_DECIMAL:
        case SQL_NUMERIC:
        case SQL_SMALLINT:
        case SQL_INTEGER:
        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_BIGINT:
        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIME:
        case SQL_TYPE_TIMESTAMP:
        case SQL_DATETIME:
        case SQL_INTERVAL:
        case SQL_GUID:
            return true;
        default:
            return false;
    }
}

// Helper function to validate ODBC type
bool isValidOdbcNonDateTimeCType(SQLSMALLINT type) {
    switch (type) {
        // C types
        case SQL_C_CHAR:
        case SQL_C_WCHAR:
        case SQL_C_SHORT:
        case SQL_C_SSHORT:
        case SQL_C_USHORT:
        case SQL_C_LONG:
        case SQL_C_SLONG:
        case SQL_C_ULONG:
        case SQL_C_FLOAT:
        case SQL_C_DOUBLE:
        case SQL_C_BIT:
        case SQL_C_STINYINT:
        case SQL_C_UTINYINT:
        case SQL_C_SBIGINT:
        case SQL_C_UBIGINT:
        case SQL_C_BINARY:
        case SQL_C_TYPE_DATE:
        case SQL_C_TYPE_TIME:
        case SQL_C_TYPE_TIMESTAMP:
        case SQL_C_NUMERIC:
        case SQL_C_GUID:
            return true;
        default:
            return false;
    }
}

// Helper function to validate datetime/interval code
bool isValidDatetimeIntervalCode(SQLSMALLINT code, SQLSMALLINT type) {
    if (type == SQL_DATETIME) {
        return isDateTimeCode(code);
    } else if (type == SQL_INTERVAL) {
        return isIntervalCode(code);
    }
    return false;
}

// Helper function to check if type is numeric
bool isNumericType(SQLSMALLINT type) {
    switch (type) {
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            return true;
        default:
            return false;
    }
}


bool validateDescriptorConsistency(RS_DESC_REC *pDescRec, bool isODBC2) {
    // Check 1: SQL_DESC_TYPE must meet one of below condition:
    // 1. A valid ODBC C type
    // 2. A valid SQL type
    // 3. Must not be concise date time / interval type
    if (!isValidOdbcNonDateTimeCType(pDescRec->hType) &&
        !isValidOdbcNonDateTimeSQLType(pDescRec->hType) &&
        (isDatetimeType(pDescRec->hType) || isIntervalType(pDescRec->hType))) {
        RS_LOG_DEBUG("RSUTIL", "invalid odbc non-concise (verbose) data type");
        return false;
    }

    // Check 2: SQL_DESC_CONCISE_TYPE must meet one of below condition:
    // 1. A valid ODBC C type
    // 2. A valid SQL type
    // 3. Must not be verbose type (SQL_DATETIME(9) / SQL_INTERVAL(10))
    // Note that SQL_DATETIME(9) and SQL_INTERVAL(10) is overlapping with valid
    // ODBC 2 concise type SQL_DATE(9) and SQL_TIME(10) therefore we can only check
    // for ODBC 3 behavior here
    if (!isValidOdbcNonDateTimeCType(pDescRec->hConciseType) &&
        !isValidOdbcNonDateTimeSQLType(pDescRec->hConciseType) &&
        (!isODBC2 && (pDescRec->hConciseType == SQL_DATETIME || pDescRec->hConciseType == SQL_INTERVAL))) {
        RS_LOG_DEBUG("RSUTIL", "invalid odbc concise data type");
        return false;
    }

    // Check 3: For SQL_DATETIME or SQL_INTERVAL types, validate subtype code
    if (pDescRec->hType == SQL_DATETIME || pDescRec->hType == SQL_INTERVAL) {
        if (!isValidDatetimeIntervalCode(pDescRec->hDateTimeIntervalCode, pDescRec->hType)) {
            RS_LOG_DEBUG("RSUTIL", "invalid datetime interval code");
           return false;
        }
    }

    // Check 4: For numeric types, validate precision and scale
    if (isNumericType(pDescRec->hType)) {
        if (pDescRec->iPrecision < 0 || pDescRec->hScale < 0) {
            RS_LOG_DEBUG("RSUTIL", "invalid numeric precision / scale");
            return false;
        }
    }

    // Check 5: For time/timestamp types or interval types with seconds component,
    // validate seconds precision
    if ((pDescRec->hType == SQL_DATETIME &&
         (pDescRec->hDateTimeIntervalCode == SQL_CODE_TIME ||
          pDescRec->hDateTimeIntervalCode == SQL_CODE_TIMESTAMP)) ||
        (pDescRec->hType == SQL_INTERVAL &&
         isIntervalSecondCode(pDescRec->hDateTimeIntervalCode))) {
        if (pDescRec->iPrecision < 0 || pDescRec->iPrecision > 6) {
            RS_LOG_DEBUG("RSUTIL", "invalid second precision");
            return false;
        }
    }

    // Check 6: For interval types, validate leading precision
    if (pDescRec->hType == SQL_INTERVAL && !isIntervalSecondCode(pDescRec->hDateTimeIntervalCode)) {
        if (pDescRec->iDateTimeIntervalPrecision < 0 || pDescRec->iDateTimeIntervalPrecision > 9) {
            RS_LOG_DEBUG("RSUTIL", "invalid interval precision");
            return false;
        }
    }

    return true;
}

// Synchronizes descriptor fields when SQL_DESC_CONCISE_TYPE is set
void syncFieldsFromConciseType(RS_DESC_REC* pDescRec, SQLSMALLINT conciseType) {
    RS_LOG_TRACE("RSUTIL", "Synchronizing fields from CONCISE_TYPE=%d", conciseType);

    pDescRec->hConciseType = conciseType;

    if (isDatetimeType(conciseType)) {
        // Datetime types use verbose type SQL_DATETIME with specific subcode
        pDescRec->hType = SQL_DATETIME;
        pDescRec->hDateTimeIntervalCode = mapDatetimeConciseTypeToCode(conciseType);
    } else if (isIntervalType(conciseType)) {
        // Interval types use verbose type SQL_INTERVAL with specific subcode
        pDescRec->hType = SQL_INTERVAL;
        pDescRec->hDateTimeIntervalCode = mapIntervalConciseTypeToCode(conciseType);
    } else {
        // Non-datetime/interval types: TYPE = CONCISE_TYPE, interval code = 0
        pDescRec->hType = conciseType;
        pDescRec->hDateTimeIntervalCode = 0;
    }

    RS_LOG_TRACE("RSUTIL", "After sync: TYPE=%d, CONCISE_TYPE=%d, CODE=%d",
                 pDescRec->hType, pDescRec->hConciseType, pDescRec->hDateTimeIntervalCode);
}


// Synchronizes descriptor fields when SQL_DESC_TYPE is set
void syncFieldsFromType(RS_DESC_REC *pDescRec, SQLSMALLINT type, bool isIPD, bool isODBC2) {
    RS_LOG_TRACE("RSUTIL", "Synchronizing fields from TYPE=%d, isIPD=%s, isODBC2=%s",
                 type, isIPD ? "true":"false", isODBC2?"true":"false");

    pDescRec->hType = type;

    // If SQL_DESC_TYPE is set to the verbose datetime or interval data type
    // (SQL_DATETIME or SQL_INTERVAL) and the
    // SQL_DESC_DATETIME_INTERVAL_CODE field is set to the appropriate
    // subcode, the SQL_DESC_CONCISE TYPE field is set to the corresponding
    // concise type.
    if (type == SQL_DATETIME) {
        // Map the datetime interval code to the appropriate concise type
        // (SQL_TYPE_DATE, SQL_TYPE_TIME, or SQL_TYPE_TIMESTAMP)
        pDescRec->hConciseType = mapDatetimeCodeToConciseType(pDescRec->hDateTimeIntervalCode, isIPD, isODBC2);

        // Per ODBC spec: Set default precision based on datetime subtype
        // "When SQL_DESC_DATETIME_INTERVAL_CODE is set to SQL_CODE_DATE or SQL_CODE_TIME,
        // SQL_DESC_PRECISION is set to 0. When it is set to SQL_DESC_TIMESTAMP,
        // SQL_DESC_PRECISION is set to 6."
        if (pDescRec->hDateTimeIntervalCode == SQL_CODE_DATE || pDescRec->hDateTimeIntervalCode == SQL_CODE_TIME) {
            pDescRec->iPrecision = 0;
        } else if (pDescRec->hDateTimeIntervalCode == SQL_CODE_TIMESTAMP) {
            pDescRec->iPrecision = 6;
        }
    } else if (type == SQL_INTERVAL) {
        pDescRec->hConciseType = mapIntervalCodeToConciseType(pDescRec->hDateTimeIntervalCode);

        // Per ODBC spec: Set default interval leading precision
        // "When SQL_DESC_DATETIME_INTERVAL_CODE is set to an interval data type,
        // SQL_DESC_DATETIME_INTERVAL_PRECISION is set to 2 (the default interval
        // leading precision)"
        if (isIntervalCode(pDescRec->hDateTimeIntervalCode)) {
            pDescRec->iDateTimeIntervalPrecision = 2;
        }

        // Per ODBC spec: Set default seconds precision for intervals with seconds component
        // "When the interval has a seconds component, SQL_DESC_PRECISION is set to 6
        // (the default interval seconds precision)."
        if (isIntervalSecondCode(pDescRec->hDateTimeIntervalCode)) {
            pDescRec->iPrecision = 6;
        }
    } else {
        // Per ODBC spec: "If SQL_DESC_TYPE is set to a concise data type other than
        // an interval or datetime data type, the SQL_DESC_CONCISE_TYPE field is set
        // to the same value and the SQL_DESC_DATETIME_INTERVAL_CODE field is set to 0."
        pDescRec->hConciseType = type;
        pDescRec->hDateTimeIntervalCode = 0;

        if (type == SQL_CHAR || type == SQL_VARCHAR || type == SQL_C_CHAR) {
            // Character types: "SQL_DESC_LENGTH is set to 1. SQL_DESC_PRECISION is set to 0."
            pDescRec->iSize = 1;
            pDescRec->iPrecision = 0;
        } else if (type == SQL_DECIMAL || type == SQL_NUMERIC || type == SQL_C_NUMERIC) {
            // Numeric types: "SQL_DESC_SCALE is set to 0. SQL_DESC_PRECISION is set to
            // the implementation-defined precision for the respective data type."
            // Note: We leave precision unchanged as it's implementation-defined and
            // numeric is not a fixed-length type
            pDescRec->hScale = 0;
        } else if (type == SQL_FLOAT || type == SQL_C_FLOAT) {
            // Float types: "SQL_DESC_PRECISION is set to the implementation-defined
            // default precision for SQL_FLOAT"
            // SQL_FLOAT maps to SQL_REAL with default precision of 24 bits
            pDescRec->iPrecision = 24;
        }
    }

    RS_LOG_TRACE("RSUTIL", "After sync: TYPE=%d, CONCISE_TYPE=%d, CODE=%d",
                 pDescRec->hType, pDescRec->hConciseType, pDescRec->hDateTimeIntervalCode);
}

// Synchronizes descriptor fields when SQL_DESC_DATETIME_INTERVAL_CODE is set
void syncFieldsFromIntervalCode(RS_DESC_REC* pDescRec, SQLSMALLINT code, bool isIPD, bool isODBC2) {
    RS_LOG_TRACE("RSUTIL", "Synchronizing fields from CODE=%d, isIPD=%s, isODBC2=%s",
                 code, isIPD ? "true":"false", isODBC2?"true":"false");

    pDescRec->hDateTimeIntervalCode = code;

    // COMPLEX SYNCHRONIZATION LOGIC:
    // We can't determine TYPE and CONCISE_TYPE solely from the interval code because:
    // 1. The code could be a datetime code (SQL_CODE_DATE, etc.) or interval code (SQL_CODE_YEAR, etc.)
    // 2. We need to know if we're dealing with SQL_DATETIME or SQL_INTERVAL verbose type
    // 3. We need to check which field (TYPE or CONCISE_TYPE) is already set to guide synchronization

    // Case 1: SQL_DESC_TYPE is already set to SQL_DATETIME
    // Update CONCISE_TYPE to the corresponding datetime concise type
    if (pDescRec->hType == SQL_DATETIME) {
        pDescRec->hConciseType = mapDatetimeCodeToConciseType(code, isIPD, isODBC2);
    }
    // Case 2: SQL_DESC_TYPE is already set to SQL_INTERVAL
    // Update CONCISE_TYPE to the corresponding interval concise type
    else if (pDescRec->hType == SQL_INTERVAL) {
        pDescRec->hConciseType = mapIntervalCodeToConciseType(code);
    }
    // Case 3: SQL_DESC_CONCISE_TYPE is set to a datetime type
    // Update TYPE to SQL_DATETIME (verbose type)
    else if (isDatetimeType(pDescRec->hConciseType)) {
        pDescRec->hType = SQL_DATETIME;
    }
    // Case 4: SQL_DESC_CONCISE_TYPE is set to an interval type
    // Update TYPE to SQL_INTERVAL (verbose type)
    else if (isIntervalType(pDescRec->hConciseType)) {
        pDescRec->hType = SQL_INTERVAL;
    }
    // Note: If neither TYPE nor CONCISE_TYPE is set to a datetime/interval value,
    // we cannot determine the proper synchronization.

    RS_LOG_TRACE("RSUTIL", "After sync: TYPE=%d, CONCISE_TYPE=%d, CODE=%d",
                 pDescRec->hType, pDescRec->hConciseType, pDescRec->hDateTimeIntervalCode);
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Auto fetch the RefCursor data.
//
SQLRETURN checkAndAutoFetchRefCursor(RS_STMT_INFO *pStmt)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_RESULT_INFO *pResult;
    RS_RESULT_INFO *pPrevResult = NULL;

    // Walk through each result
    for(pResult = pStmt->pResultHead; pResult != NULL;)
    {
        int iMoveToNext = TRUE;

        // If any auto refcursor found, execute fetch all
        if(pResult->iRefCursorInResult)
        {
            // Remove this result from the list
            pResult->iRefCursorInResult = FALSE;

            // Check whether it has any result
            if((pResult->iCurRow >= -1) && (pResult->iCurRow <= (pResult->iNumberOfRowsInMem - 1)))
            {
                int  iDataLen;
                char *pData;
				int format;

                // Fetch Next
                pResult->iCurRow++;

                // Get the cursor name
                pData = libpqGetData(pResult, 0, &iDataLen, &format);

                if(pData && (iDataLen != SQL_NULL_DATA))
                {
                    char szCmd[SHORT_CMD_LEN + 1];
                    RS_RESULT_INFO *pSavNextResult;
                    RS_RESULT_INFO *pTempResult;
                    int iSavFunctionCall;

                    snprintf(szCmd,sizeof(szCmd),CURSOR_FETCH_ALL_CMD,  pData);

                    // Break the chain to get new result
                    if(pPrevResult == NULL)
                        pStmt->pResultHead = NULL;
                    else
                        pPrevResult->pNext=NULL;

                    // Save the next
                    pSavNextResult = pResult->pNext;

                    // Remove the node.
                    pResult->pNext = NULL;

                    // Release result node
                    releaseResult(pResult, (pResult == pStmt->pResultHead), pStmt);

                    iSavFunctionCall = pStmt->iFunctionCall;
                    pStmt->iFunctionCall = FALSE;

                    // Reset init msg loop flag.
                    resetCscStatementConext(pStmt->pCscStatementContext);

                    // Execute fetch all
                    rc = libpqExecuteDirectOrPreparedOnThread(pStmt, szCmd, FALSE, FALSE, FALSE);

                    pStmt->iFunctionCall = iSavFunctionCall;

                    // Join the rest of result in the chain
                    // Go to the end of the new list
                    for(pTempResult = pStmt->pResultHead; pTempResult && pTempResult->pNext != NULL; pTempResult = pTempResult->pNext);
                    
                    if(pTempResult)
                        pTempResult->pNext = pSavNextResult;
                    else
                        pStmt->pResultHead = pSavNextResult;

                    // Next node to look for
                    pResult = pSavNextResult;
                    iMoveToNext = FALSE;

                    if(rc == SQL_ERROR)
                        goto error;
                } // Cursor name exist
            } // Row exist
        }
        
        if(iMoveToNext)
        {
            pPrevResult = pResult;
            pResult = pResult->pNext;
        }

    } // Result loop

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release resources related to the given result.
//
void releaseResult(RS_RESULT_INFO *pResult, int iAtHeadResult, RS_STMT_INFO *pStmt)
{
    if(pResult)
    {
        // Free DescribeCol IRD recs, if any. Normally it move to pStmt->pIRD.
        pResult->pIRDRecs = (RS_DESC_REC *)rs_free(pResult->pIRDRecs);

		if(pStmt
			&& pStmt->pCscStatementContext 
			&& isStreamingCursorMode(pStmt)
			&& pResult->pgResult
			&& (PQresultStatus(pResult->pgResult) == PGRES_TUPLES_OK)
			&& !(libpqIsEndOfStreamingCursor(pStmt))
		)
		{
			int iSocketError = 0;

			if(IS_TRACE_ON())
			{
				RS_LOG_INFO("RSUTIL", "Skiping current result for streaming cursor...");
			}

			iSocketError = libpqSkipCurrentResultOfStreamingCursor(pStmt, pStmt->pCscStatementContext, pResult->pgResult, pStmt->phdbc->pgConn, TRUE);

			if(iSocketError)
				pResult->pgResult = NULL;

			if(IS_TRACE_ON())
			{
				RS_LOG_INFO("RSUTIL", "Skiping current result for streaming cursor done.iSocketError=%d", iSocketError);
			}
		}

        // Free the result
        libpqCloseResult(pResult);
        if(pResult != NULL) {
          delete pResult;
          pResult = NULL;
        }

		if(pStmt
			&& pStmt->pCscStatementContext 
			&& isStreamingCursorMode(pStmt)
			&& !(libpqIsEndOfStreamingCursorQuery(pStmt))
		)
		{
			pqResetConnectionResult(pStmt->phdbc->pgConn);
		}

        if(iAtHeadResult)
        {
            // Release DescribeCol result IRD recs
            pStmt->pIRD->pDescHeader.hHighestCount = 0;
            releaseDescriptorRecs(pStmt->pIRD);
            pStmt->pIRD->iRecListType = RS_DESC_RECS_LINKED_LIST;
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if given string is NULL or empty otherwise FALSE.
//
int isNullOrEmpty(SQLCHAR *pData)
{
    return (pData == NULL || pData[0] == '\0');
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Lock the mutex related to API.
//
// To use HENV mutex, pass non-null HENV.
// To use HDBC mutex, pass NULL to HENV and non-NULL to HDBC
// To use Global mutex, pass NULL to HENV and NULL to HDBC
//
void beginApiMutex(SQLHENV phenv, SQLHDBC phdbc)
{
    RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pEnv)
    {
      // Lock using HENV mutex
      rsLockMutex(pEnv->hApiMutex); 
    }
    else
    if(pConn) 
    { 
        if(pConn->pConnectProps && pConn->pConnectProps->iApplicationUsingThreads) 
        {
            // Lock using HDBC mutex
            rsLockMutex(pConn->hApiMutex); 
        }
    } 
    else 
    {
        // Lock using Global mutex
        rsLockMutex(gRsGlobalVars.hApiMutex); 
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unlock the mutex related to API.
//
// To use HENV mutex, pass non-null HENV.
// To use HDBC mutex, pass NULL to HENV and non-NULL to HDBC
// To use Global mutex, pass NULL to HENV and NULL to HDBC
//
void endApiMutex(SQLHENV phenv, SQLHDBC phdbc)
{
  RS_ENV_INFO *pEnv = (RS_ENV_INFO *)phenv;
  RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pEnv)
    {
      // Un-Lock using HENV mutex
      rsUnlockMutex(pEnv->hApiMutex); 
    }
    else
    if(pConn) 
    { 
        if(pConn->pConnectProps && pConn->pConnectProps->iApplicationUsingThreads) 
        {
            // Un-Lock using HDBC mutex
            rsUnlockMutex(pConn->hApiMutex); 
        }
    } 
    else 
    {
        // Un-Lock using Global mutex
        rsUnlockMutex(gRsGlobalVars.hApiMutex); 
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check the stmt handle and then add the error.
//
SQLRETURN checkHstmtHandleAndAddError(SQLHSTMT phstmt, SQLRETURN rc, char *pSqlState, char *pSqlErrMsg)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    addError(&pStmt->pErrorList,pSqlState, pSqlErrMsg, 0, NULL);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check the stmt handle and then add the error.
//
SQLRETURN checkHdbcHandleAndAddError(SQLHDBC phdbc, SQLRETURN rc, char *pSqlState, char *pSqlErrMsg)
{
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(!VALID_HDBC(phdbc))
    {
        rc = SQL_INVALID_HANDLE;
        return rc;
    }

    // Clear error list
    pConn->pErrorList = clearErrorList(pConn->pErrorList);

    addError(&pConn->pErrorList,pSqlState, pSqlErrMsg, 0, NULL);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether ODBC2 or ODBC3 version set by application/DM.
//
int isODBC2Behavior(RS_STMT_INFO *pStmt)
{
    int rc = FALSE;

    if(pStmt)
      rc = (pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion == SQL_OV_ODBC2);

    return rc;
}

int isODBC2BehaviorByDesc(RS_DESC_INFO *pDesc) {
    int isODBC2 = FALSE;

    if(pDesc && pDesc->phdbc && pDesc->phdbc->phenv && pDesc->phdbc->phenv->pEnvAttr) {
        isODBC2 = (pDesc->phdbc->phenv->pEnvAttr->iOdbcVersion == SQL_OV_ODBC2);
    }
    return isODBC2;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Map to ODBC2 SQLState, if needed.
//
void mapToODBC2SqlState(RS_ENV_INFO *pEnv,char *pszSqlState)
{
    if(pEnv && pszSqlState)
    {
       if(pEnv->pEnvAttr->iOdbcVersion == SQL_OV_ODBC2)
       {
            int i = 0;

            while(gMapToODBC2SqlState[i].pszOdbc3State.length())
            {
                if(strcmp(gMapToODBC2SqlState[i].pszOdbc3State.c_str(),pszSqlState) == 0)
                {
                    rs_strncpy(pszSqlState,gMapToODBC2SqlState[i].pszOdbc2State.c_str(),6);
                    break;
                }

                i++;
            }
       }
    }
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize ODBC
//
void initODBC(HMODULE hModule)
{
    initGlobals(hModule);
    // Read reg settings
    readAndSetTraceInfo();
    //Logger
    initTrace(false);
    initLibpq(NULL);
    logAllRsodbcVersions();
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Uninitialize ODBC
//
void uninitODBC()
{
    uninitLibpq();
    uninitTrace();
    releaseGlobals();
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get calling application name
//
void getApplicationName(SQLHDBC phdbc)
{
#ifdef WIN32

    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        GetModuleFileName(NULL, pConn->pConnAttr->szApplicationName, sizeof(pConn->pConnAttr->szApplicationName));
    }

#endif
#if defined LINUX 
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        size_t linknamelen;
        char file[256];

        snprintf(file, sizeof(file), "/proc/%d/exe", (int)getpid());
        linknamelen = readlink(file, pConn->pConnAttr->szApplicationName, sizeof(pConn->pConnAttr->szApplicationName) - 1);
        pConn->pConnAttr->szApplicationName[linknamelen + 1] = 0;
    }
#endif
}

//---------------------------------------------------------------------------------------------------------igarish
// Get driver binary path
//
char *getDriverPath()
{
  char moduleName[MAX_PATH];
  char *driverPath = NULL;

  moduleName[0] = '\0';

#ifdef WIN32

     GetModuleFileName(gRsGlobalVars.hModule, moduleName, sizeof(moduleName));

#endif
#if defined LINUX
    Dl_info info;
    int result = dladdr((void *) getDriverPath, &info);

    if (result && info.dli_fname)
    {
        if(rs_strncpy_safe(moduleName, info.dli_fname, MAX_PATH) == NULL)
        {
            moduleName[0] = '\0';
        }
    }

#endif

    char *lastPathSeparator = strrchr(moduleName, PATH_SEPARATOR_CHAR);
    if(lastPathSeparator)
    {
      *lastPathSeparator = '\0';

      driverPath = strdup(moduleName);
    }

    return driverPath;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get OS user name
//
void getOsUserName(SQLHDBC phdbc)
{
#ifdef WIN32

    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        DWORD len = sizeof(pConn->pConnAttr->szOsUserName);

        GetUserName(pConn->pConnAttr->szOsUserName, &len);
    }

#endif
#if defined LINUX 
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        getlogin_r(pConn->pConnAttr->szOsUserName, sizeof(pConn->pConnAttr->szOsUserName));
    }
#endif
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get client host name
//
void getClientHostName(SQLHDBC phdbc)
{
#ifdef WIN32

    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        DWORD len = sizeof(pConn->pConnAttr->szClientHostName);

        GetComputerNameEx(ComputerNameDnsHostname, pConn->pConnAttr->szClientHostName, &len);
    }

#endif
#if defined LINUX 
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        gethostname(pConn->pConnAttr->szClientHostName, sizeof(pConn->pConnAttr->szClientHostName));
    }
#endif
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get client domain name
//
void getClientDomainName(SQLHDBC phdbc)
{
#ifdef WIN32

    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        DWORD len = sizeof(pConn->pConnAttr->szClientDomainName);

        GetComputerNameEx(ComputerNameDnsDomain, pConn->pConnAttr->szClientDomainName, &len);
    }

#endif
#if defined LINUX  
    RS_CONN_INFO *pConn = (RS_CONN_INFO *)phdbc;

    if(pConn && pConn->pConnAttr)
    {
        getdomainname(pConn->pConnAttr->szClientDomainName, sizeof(pConn->pConnAttr->szClientDomainName));
        if(_stricmp(pConn->pConnAttr->szClientDomainName,"(none)") == 0)
        	pConn->pConnAttr->szClientDomainName[0] = 0;
    }
#endif

}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get client audit trail information
//
void getAuditTrailInfo(SQLHDBC phdbc)
{
    getApplicationName(phdbc);
    getOsUserName(phdbc);
    getClientHostName(phdbc);
    getClientDomainName(phdbc);
}

// Validates buffer size for SQL_NUMERIC/SQL_DECIMAL to character conversion.
// Ensures the application buffer can hold at least the whole digits portion.
// According to the ODBC spec, fractional truncation is allowed but whole digit
// truncation is an error.
SQLRETURN validateNumericBufferSize(RS_STMT_INFO *pStmt,
                                           const char *numStr, int numStrLen,
                                           SQLLEN cbLen, short hSQLType,
                                           short hType, bool isWideChar) {
    // Calculate minimum buffer size needed for whole digits + sign + null
    // terminator
    int requiredBufLen = calculateMinNumericBufferLength(numStr, numStrLen);

    // For wide character conversion, multiply by WCHAR size
    if (isWideChar) {
        requiredBufLen *= sizeof(WCHAR);
    }

    // Check if application buffer is too small for whole digits
    if (requiredBufLen > cbLen) {
        // ODBC spec requires SQLState 22003 when whole digits cannot fit
        addError(&pStmt->pErrorList, "22003",
                 "Buffer too small for numeric value", 0, NULL);
        RS_LOG_ERROR("RSUTIL",
                     "%s Buffer too small for numeric value. Required: %d, "
                     "Provided: %lld",
                     formatConversionPrefix(hSQLType, hType).c_str(),
                     requiredBufLen, cbLen);
        return SQL_ERROR;
    }

    return SQL_SUCCESS;
}
/*=====================================================================================*/
// Helper function to calculate minimum required buffer length for SQL_NUMERIC/SQL_DECIMAL to character conversion
// Returns the required buffer length including sign and null terminator
// For wide character conversion, multiply the result by sizeof(SQLWCHAR)
// Note: This function assumes that the input numeric string doesn't have scientific notation
// because PADB normalizes numeric to non-scientific format.
int calculateMinNumericBufferLength(const char* numStr, int strLen) {
    if (numStr == NULL || strLen <= 0) {
        return 1; // Just null terminator for empty string
    }

    int wholeDigits = 0;
    int hasSign = 0;

    // Check for sign
    if (numStr[0] == '-' || numStr[0] == '+') {
        hasSign = 1;
    }

    // Find the decimal point to determine whole digits
    const char* end = numStr + strLen;
    const char* decimalPoint = std::find(numStr, end, '.');
    if (decimalPoint != end) {
        wholeDigits = decimalPoint - numStr - hasSign;
    } else {
        wholeDigits = strLen - hasSign;
    }

    // Required buffer length = whole digits + sign (if any) + null terminator
    return wholeDigits + hasSign + 1;
}

//---------------------------------------------------------------------------------------------------------igarish
// Convert character string buffer of numeric to scaled integer in little endian mode.
//
void convertNumericStringToScaledInteger(char *pNumData, SQL_NUMERIC_STRUCT *pnVal)
{
    char *pDecimalPoint;
    int len;
    int len1, len2;
    char szBuf[MAX_NUMBER_BUF_LEN] = "";
    long long llVal = 0;
    int i; 
    char *pTemp;

    memset(pnVal,0,sizeof(SQL_NUMERIC_STRUCT));

    pnVal->sign = 1;

    if(*pNumData == '+')
        pNumData++;
    else
    if(*pNumData == '-')
    {
        pNumData++;
        pnVal->sign = 0;
    }

    pDecimalPoint = strchr(pNumData, '.');
    len = (int)strlen(pNumData);

    if(pDecimalPoint)
    {
        pnVal->precision = len - 1; // -1 for '.'
        pnVal->scale = (SQLSCHAR) strlen(pDecimalPoint + 1);
        len1 = len - 1 - pnVal->scale;
        len1 = redshift_min(len1,PADB_MAX_NUM_BUF_LEN);

        // Before decimal values
        memcpy(szBuf, pNumData, len1);

        // After decimal values
        len2 = redshift_min(pnVal->scale, PADB_MAX_NUM_BUF_LEN - len1);
        if(len2 > 0)
            memcpy(szBuf + len1 , pDecimalPoint + 1, len2);

        if((len1 + len2) < PADB_MAX_NUM_BUF_LEN)
            szBuf[len1 + len2] = '\0';
    }
    else
    {
        pnVal->precision = len;
        pnVal->scale = 0;
        len1 = redshift_min(len,PADB_MAX_NUM_BUF_LEN);
        memcpy(szBuf, pNumData, len1);
        if(len1 < PADB_MAX_NUM_BUF_LEN)
            szBuf[len1] = '\0';
    }

    // Is it 64 bit numeric?
    len = (int)strlen(szBuf);
    if(len <= 18)
    {
        sscanf(szBuf,"%lld", &llVal);

        pTemp = (char *)&llVal;
        len = sizeof(llVal);

        for(i = 0;i < len; i++)
            pnVal->val[i] = *pTemp++;
    }
    else
    {
        // 128 bit numeric
	    int	iTotalVal;
        int iOutLen;
        int iBit;
        int iOutVal;
        int iDigit;
        int iStart;

        // Loop for each digit in the string buffer
	    for (iOutVal = 0, iBit = 1L, iStart = 0, iOutLen = 0; iStart < len;)
	    {
            // Right shift each bit and calculate the carry. Store partial result back in the buf until it become 0.
		    for (iDigit = 0, i = iStart; i < len; i++)
		    {
			    iTotalVal = iDigit * 10 + szBuf[i] - '0';
			    iDigit = iTotalVal % 2;
			    szBuf[i] = iTotalVal / 2 + '0';
			    if (i == iStart && iTotalVal < 2)
				    iStart++;
		    }

            // Get the right most digit value and accumulate it in result byte
		    if (iDigit > 0)
			    iOutVal |= iBit;

            // Go to next bit
		    iBit <<= 1;

            // Is whole byte shift done?
		    if (iBit >= (1L << 8))
		    {
                // Output right shifted byte, which is LSB.
			    pnVal->val[iOutLen++] = iOutVal;
			    iOutVal = 0;
			    iBit = 1L;
			    if (iOutLen >= SQL_MAX_NUMERIC_LEN - 1)
			    {
				    pnVal->scale = iStart - pnVal->precision;
				    break;
			    }
		    } 
	    } // Loop

	    if (iOutVal && iOutLen < SQL_MAX_NUMERIC_LEN - 1)
		    pnVal->val[iOutLen++] = iOutVal;
    }
}

/**
 * @brief Parses a PRE-VALIDATED numeric string and builds an integer representation
 *
 * This function assumes the input has already been validated by
 * prepareStringForNumericConversion() and focuses on transformation logic:
 * - Rejects special IEEE 754 values (infinity, NaN) for integer conversion
 * - Parses sign, digits, decimal point, and scientific notation
 * - Builds unsigned integer magnitude
 * - Tracks fractional digit truncation
 * - Detects overflow conditions
 *
 * @param src               Pre-validated numeric string (from prepareStringForNumericConversion)
 * @param len               Length of the input string
 * @param isNeg             [OUT] True if the number is negative
 * @param magnitude         [OUT] Absolute value as unsigned long long
 * @param droppedFraction   [OUT] True if non-zero fractional digits were truncated
 *
 * @return PARSE_SUCCESS if parsing succeeded
 *         PARSE_INVALID_FORMAT for special values (infinity/NaN) that can't be integers
 *         PARSE_OVERFLOW on integer overflow
 *
 * @note INPUT MUST BE PRE-VALIDATED by prepareStringForNumericConversion
 * @note Supports decimal notation (123.45) and scientific notation (1.23e2)
 * @note For integer conversion only
 */
ParseReturnCode parseAndBuildInteger(const char *src, int len, bool &isNeg,
                   unsigned long long &magnitude, bool &droppedFraction) {
    // Step 1: Basic input validation

    // 1.1: Check for null pointer and invalid length
    if (!src || len <= 0) {
        return PARSE_INVALID_FORMAT;
    }

    // 1.2: Detect if the input is null-terminated and adjust effective length
    //      If last character is '\0', exclude it from parsing
    int effectiveLen = (src[len - 1] == '\0') ? len - 1 : len;
    if (effectiveLen <= 0) {
        return PARSE_INVALID_FORMAT;
    }

    // 1.3: Set up bounds for safe parsing
    const char *start = src;
    const char *end = src + effectiveLen;

    const char *numericStrPtr = start;

    // Step 2: Reject special IEEE 754 values for INTEGER conversion
    //   infinity, -infinity, NaN are valid for float/double but NOT for integers
    //   prepareStringForNumericConversion allows these through, so we check here

    // Check for infinity variants (case-insensitive)
    if ((_stricmp(numericStrPtr, "infinity") == 0) ||
        (_stricmp(numericStrPtr, "inf") == 0) ||
        (_stricmp(numericStrPtr, "+infinity") == 0) ||
        (_stricmp(numericStrPtr, "+inf") == 0) ||
        (_stricmp(numericStrPtr, "-infinity") == 0) ||
        (_stricmp(numericStrPtr, "-inf") == 0)) {
        return PARSE_OVERFLOW; // Cannot convert infinity to integer
    }

    // Check for NaN (case-insensitive)
    if (_stricmp(numericStrPtr, "nan") == 0) {
        return PARSE_OVERFLOW; // Cannot convert NaN to integer
    }

    // Step 3: Parse optional leading sign (+ or -)
    isNeg = false;
    if (numericStrPtr < end && (*numericStrPtr == '+' || *numericStrPtr == '-')) {
        isNeg = (*numericStrPtr == '-');
        ++numericStrPtr;
    }

    // Step 4: Collect all digits to prepare for exponent shift
    //   In this step, parsing follows the logic below:
    //   1. Get all numbers before decimal point
    //   2. Get all numbers after decimal point
    //   3. Store them in a single string for later processing
    //   Example: "123.456" -> digits="123456", fracDigits=3


    // 4.1: Initialize digit collection
    std::string digits;
    digits.reserve(end - numericStrPtr);
    bool sawDigit = false;

    // 4.2: Parse integer part digits (before decimal point)
    //      Example: "123.456" -> collects "123"
    while (numericStrPtr < end && *numericStrPtr >= '0' && *numericStrPtr <= '9') {
        digits.push_back(*numericStrPtr);
        sawDigit = true;
        ++numericStrPtr;
    }


    // 4.3: Parse fractional part after decimal point
    //      Example: "123.456" -> collects "456", fracDigits=3
    //      All digits are appended to the same 'digits' string
    int fracDigits = 0;
    if (numericStrPtr < end && *numericStrPtr == '.') {
        ++numericStrPtr;
        const char *fracStart = numericStrPtr;
        while (numericStrPtr < end && *numericStrPtr >= '0' && *numericStrPtr <= '9') {
            digits.push_back(*numericStrPtr); // Add to same digits string
            sawDigit = true;
            ++numericStrPtr;
        }
        fracDigits = static_cast<int>(numericStrPtr - fracStart);
    }

    // Step 5: Parse optional scientific notation exponent
    //   This step handles the exponent part (e/E) if present
    //   Examples: "1.23e5", "4.56E-3", "789e+2"
    //   The exponent determines how many positions to shift the decimal point
    int exp10 = 0;
    ParseReturnCode expResult = parseExponent(&numericStrPtr, end, &exp10);
    if (expResult != PARSE_SUCCESS) {
        return expResult;
    }

    // Step 7: Apply exponent shift to determine final integer value
    //   Calculate effective decimal shift after applying exponent
    //   This determines how many positions to move the decimal point
    //   Examples:
    //     "123.45e2"  -> shift = 2-2 = 0   -> "12345" (no net shift)
    //     "123.45e3"  -> shift = 3-2 = 1   -> "123450" (shift right 1)
    //     "123.45e1"  -> shift = 1-2 = -1  -> "1234.5" -> "1234" (shift left 1)
    //     "0.123e2"   -> shift = 2-3 = -1  -> "01.23" -> "01" -> "1"
    long long shift = static_cast<long long>(exp10) - static_cast<long long>(fracDigits);
    std::string intDigits;

    if (shift >= 0) {
        // 7.1: Positive shift - append zeros (result is pure integer)
        //      Example: "1.23e3" -> digits="123", shift=1 -> intDigits="1230"
        intDigits = digits;
        intDigits.append(static_cast<size_t>(shift), '0');
        droppedFraction = false;
    } else {
        // 7.2: Negative shift - some digits become fractional (may be dropped)
        //      Calculate how many digits to keep in integer part
        long long keep = static_cast<long long>(digits.size()) + shift; // shift < 0

        if (keep <= 0) {
            // 7.2.1: All digits shifted into fractional part; integer part is 0
            //        Example: "0.123e-5" -> all digits become fractional -> magnitude = 0
            magnitude = 0;

            // 7.2.2: Determine if any of the digits were non-zero
            droppedFraction = false;
            for (char digit : digits) {
                if (digit != '0') {
                    droppedFraction = true;
                    break;
                }
            }

            // Normalizes negative zero to positive when the integer and fractional part of number is 0
            if (!droppedFraction) {
                isNeg = false;
            }
            return PARSE_SUCCESS;
        }

        // 7.2.3: Split digits - keep some for integer, drop fractional part
        //        Example: "12345" with keep=3 -> intDigits="123", drop "45"
        size_t k = static_cast<size_t>(keep);
        intDigits.assign(digits.begin(), digits.begin() + k);

        // 7.2.4: Check if dropped fractional digits were non-zero
        droppedFraction = false;
        for (size_t t = k; t < digits.size(); ++t) {
            if (digits[t] != '0') {
                droppedFraction = true;
                break;
            }
        }
    }

    // Step 8: Strip leading zeros and convert to unsigned long long
    //   Remove leading zeros and convert the final integer string to numeric value
    //   Example: "000123" -> "123" -> magnitude = 123

    // 8.1: Skip all leading zeros
    size_t pos = 0;
    while (pos < intDigits.size() && intDigits[pos] == '0') {
        ++pos;
    }

    // 8.2: Handle case where all digits were zeros
    if (pos == intDigits.size()) {
        magnitude = 0;
        // Normalize negative zero to positive zero if integer and fraction part was all zero.
        if (!droppedFraction) {
            isNeg = false;
        }
        return PARSE_SUCCESS;
    }

    // 8.3: Convert remaining digits to unsigned long long with overflow check
    //      Process each digit and check for overflow before adding
    magnitude = 0;
    for (; pos < intDigits.size(); ++pos) {
        unsigned digitValue = static_cast<unsigned>(intDigits[pos] - '0');

        // Check for multiplication overflow
        if (magnitude > ULLONG_MAX / 10ULL) {
            return PARSE_OVERFLOW;
        }
        magnitude *= 10ULL;

        // Check for addition overflow
        if (magnitude > ULLONG_MAX - digitValue) {
            return PARSE_OVERFLOW;
        }
        magnitude += digitValue;
    }
    return PARSE_SUCCESS;
}

/**
 * Converts SQL_NUMERIC data to integer C data types with range validation.
 * 
 * @param pStmt         Statement handle for error reporting
 * @param pColData      Input numeric string data
 * @param iColDataLen   Length of input data
 * @param pBuf          Output buffer for converted value
 * @param pcbLenInd     Length/indicator buffer
 * @param hType         Target C data type (SQL_C_TINYINT, SQL_C_LONG, etc.)
 * 
 * @return SQL_SUCCESS on successful conversion
 *         SQL_SUCCESS_WITH_INFO if fractional digits were truncated
 *         SQL_ERROR on range overflow or invalid parameters
 *         SQL_INVALID_HANDLE if pStmt is NULL
 * 
 * Supports all ODBC integer types: BIT, TINYINT, SHORT, LONG, BIGINT (signed/unsigned).
 * Validates range limits and reports fractional truncation warnings.
 */
SQLRETURN convertStringNumericToIntegerCType(RS_STMT_INFO *pStmt,
                                        char *pColData,
                                        int iColDataLen,
                                        void *pBuf,
                                        SQLLEN *pcbLenInd,
                                        SQLSMALLINT hType) {
    // Step 1: Input parameter validation
    //   This step validates all required parameters before processing
    //   Ensures statement handle and all pointers are valid

    // 1.1: Check for valid statement handle
    if (!pStmt) {
        return SQL_INVALID_HANDLE;
    }

    // 1.2: Check for null pointers in required parameters
    if (!pBuf || !pColData) {
        if (pStmt->pErrorList) {
            addError(&pStmt->pErrorList, "HY009", "Invalid use of null pointer", 0, NULL);
        }
        RS_LOG_ERROR("RSUTIL", "HY009: Invalid null pointer");
        return SQL_ERROR;
    }

    // Handle NULL data case
    if (iColDataLen == SQL_NULL_DATA) {
        if (pcbLenInd) {
            *pcbLenInd = SQL_NULL_DATA;
        }
        return SQL_SUCCESS;
    }

    // Step 2: Define error handling lambdas
    //   These helper functions standardize error reporting and logging
    //   Used throughout the function for consistent error handling

    auto rangeError = [&](){
        addError(&pStmt->pErrorList, "22003", "Numeric value out of range", 0, NULL);
        RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
        return SQL_ERROR;
    };
    auto formatError = [&](){
        addError(&pStmt->pErrorList, "22018", "Invalid character value for cast specification", 0, NULL);
        RS_LOG_ERROR("RSUTIL", "22018: Invalid character value for cast specification");
        return SQL_ERROR;
    };
    auto addFracWarn = [&](){
        addWarning(&pStmt->pErrorList, "01S07", "Fractional truncation", 0, NULL);
        RS_LOG_WARN("RSUTIL", "01S07: Fractional truncation");
    };

    // Step 3: Parse numeric string into components
    //   Use parseAndBuildInteger to extract sign, magnitude, and precision information
    //   This handles decimal notation, scientific notation, and validation
    bool isNeg = false, dropped = false;
    unsigned long long mag = 0;
    ParseReturnCode parseResult = parseAndBuildInteger(pColData, iColDataLen, isNeg, mag, dropped);
    if (parseResult != PARSE_SUCCESS) {
        return (parseResult == PARSE_OVERFLOW) ? rangeError() : formatError();
    }

    // Step 4: Handle special case for SQL_C_BIT type
    //   BIT type can only hold values 0 or 1
    //   Negative values or values >= 2 are out of range

    if (hType == SQL_C_BIT) {
        // 4.1: Check range constraints for BIT type
        if (isNeg || mag >= 2ULL) {
            return rangeError();
        }

        // 4.2: Handle exact (non-fraction) vs non-exact values
        char b = static_cast<char>(mag);
        if (dropped) {
            // Non-exact value with fractional part (e.g., 0.5 -> 0, 1.7 -> 1)
            addFracWarn();
            getBooleanData(b, pBuf, pcbLenInd);
            return SQL_SUCCESS_WITH_INFO;
        }
        // Exact integer value (0 or 1)
        return getBooleanData(b, pBuf, pcbLenInd);
    }

    // Step 5: Process other integer types with range validation
    //   Each integer type has specific range limits that must be enforced
    //   Handle both signed and unsigned variants

    SQLRETURN rc = SQL_SUCCESS;

    switch (hType) {
        // 5.1: Unsigned 8-bit integer (0 to 255)
        case SQL_C_UTINYINT: {
            unsigned char result;
            rc = SAFE_CONVERT_MAG(unsigned char, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getUTinyIntData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.2: Signed 8-bit integer (-128 to 127)
        case SQL_C_TINYINT:
        case SQL_C_STINYINT: {
            signed char result;
            rc = SAFE_CONVERT_MAG(signed char, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getTinyIntData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.3: Unsigned 16-bit integer (0 to 65535)
        case SQL_C_USHORT: {
            unsigned short result;
            rc = SAFE_CONVERT_MAG(unsigned short, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getUShortData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.4: Signed 16-bit integer (-32768 to 32767)
        case SQL_C_SHORT:
        case SQL_C_SSHORT: {
            short result;
            rc = SAFE_CONVERT_MAG(short, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getShortData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.5: Unsigned 32-bit integer (0 to 4294967295)
        case SQL_C_ULONG: {
            unsigned int result;
            rc = SAFE_CONVERT_MAG(unsigned int, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getUIntData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.6: Signed 32-bit integer (-2147483648 to 2147483647)
        case SQL_C_LONG:
        case SQL_C_SLONG: {
            int result;
            rc = SAFE_CONVERT_MAG(int, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getIntData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.7: Unsigned 64-bit integer (0 to 18446744073709551615)
        case SQL_C_UBIGINT: {
            if (isNeg) {
                return rangeError();
            }
            rc = getUBigIntData(mag, pBuf, pcbLenInd);
            break;
        }

        // 5.8: Signed 64-bit integer (-9223372036854775808 to 9223372036854775807)
        case SQL_C_SBIGINT: {
            long long result;
            rc = SAFE_CONVERT_MAG(long long, isNeg, mag, &result, &pStmt->pErrorList);
            if (SQL_SUCCEEDED(rc)) {
                rc = getBigIntData(result, pBuf, pcbLenInd);
            }
            break;
        }

        // 5.9: Handle unsupported or invalid target types
        default:
            RS_LOG_ERROR("RSUTIL", "HY003: Invalid application buffer type");
            addError(&pStmt->pErrorList, "HY003", "Invalid application buffer type", 0, NULL);
            return SQL_ERROR;
    }

    // Now that we know we’re not returning an error, attach 01S07 iff we dropped fraction.
    if (rc == SQL_SUCCESS && dropped) {
        addFracWarn();
        return SQL_SUCCESS_WITH_INFO;
    }
    return rc;
}


/**
 * @brief Converts SQL_CHAR/SQL_VARCHAR data to floating point C data types
 * (SQL_C_FLOAT or SQL_C_DOUBLE)
 *
 * This function parses a string containing a numeric value and converts it to
 * either float or double. It handles special IEEE 754 values (infinity, NaN),
 * validates numeric format, and performs range checking.
 *
 * @param pStmt         Statement handle for error reporting
 * @param pColData      Input numeric string data
 * @param iColDataLen   Length of input data
 * @param pBuf          Output buffer for converted value
 * @param pcbLenInd     Length/indicator buffer
 * @param hType         Target C data type (SQL_C_FLOAT or SQL_C_DOUBLE)
 *
 * @return SQL_SUCCESS on successful conversion
 *         SQL_ERROR on range overflow or invalid parameters
 *         SQL_INVALID_HANDLE if pStmt is NULL
 *
 * Supports SQL_C_FLOAT and SQL_C_DOUBLE types with proper range validation.
 * Handles special values: infinity, -infinity, NaN (where appropriate).
 */
SQLRETURN convertStringNumericToFloatCType(RS_STMT_INFO *pStmt, char *pColData,
                                           int iColDataLen, void *pBuf,
                                           SQLLEN *pcbLenInd,
                                           SQLSMALLINT hType) {
    // Step 1: Input parameter validation
    if (!pStmt) {
        return SQL_INVALID_HANDLE;
    }

    if (!pBuf || !pcbLenInd || !pColData) {
        if (pStmt->pErrorList) {
            addError(&pStmt->pErrorList, "HY009", "Invalid use of null pointer",
                     0, NULL);
        }
        RS_LOG_ERROR("RSUTIL", "HY009: Invalid null pointer");
        return SQL_ERROR;
    }

    // Handle NULL data case
    if (iColDataLen == SQL_NULL_DATA) {
        *pcbLenInd = SQL_NULL_DATA;
        return SQL_SUCCESS;
    }

    // Step 2: Convert to numeric value using strtold
    char *endPtr;
    errno = 0;
    long double doubleVal = strtold(pColData, &endPtr);
    // Step 3: Validate conversion result
    if (endPtr == pColData || (*endPtr != '\0' && !isspace((unsigned char)*endPtr))) {
        const char *typeName = (hType == SQL_C_FLOAT) ? "FLOAT" : "DOUBLE";
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg),
                 "Invalid character value for cast to %s", typeName);
        addError(&pStmt->pErrorList, "22018", errMsg, 0, NULL);
        return SQL_ERROR;
    }

    // Step 4: Check for overflow/underflow during parsing
    if (errno == ERANGE) {
        const char *typeName = (hType == SQL_C_FLOAT) ? "FLOAT" : "DOUBLE";
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg),
                 "Numeric value out of range for %s", typeName);
        addError(&pStmt->pErrorList, "22003", errMsg, 0, NULL);
        return SQL_ERROR;
    }

    // Step 5: Perform type-specific conversion with range checking
    bool success = true;

    switch (hType) {
    case SQL_C_FLOAT: {
        float floatVal;
        convertFloatValue(doubleVal, &floatVal, &success, 
                         &pStmt->pErrorList, "FLOAT");
        if (!success) {
            return SQL_ERROR;
        }
        return getFloatData(floatVal, pBuf, pcbLenInd);
    }

    case SQL_C_DOUBLE: {
        double dblVal;
        convertFloatValue(doubleVal, &dblVal, &success, 
                         &pStmt->pErrorList, "DOUBLE");
        if (!success) {
            return SQL_ERROR;
        }
        return getDoubleData(dblVal, pBuf, pcbLenInd);
    }

    default:
        RS_LOG_ERROR("RSUTIL", "HY003: Invalid application buffer type");
        addError(&pStmt->pErrorList, "HY003", "Invalid application buffer type",
                 0, NULL);
        return SQL_ERROR;
    }
}

/**
 * @brief Converts a string representation of a number to a SQL_NUMERIC_STRUCT
 *
 * This function parses a string containing a decimal number (with optional sign, decimal point,
 * and scientific notation) and converts it to the binary representation required by SQL_NUMERIC_STRUCT.
 * The function handles normalization, scaling, and binary conversion with proper error detection.
 *
 * @param pStmt      Statement handle for error reporting, must not be NULL
 * @param pNumData   Input string containing the numeric value to convert (e.g., "123.45", "-67.8e2")
 * @param pnVal      Pointer to SQL_NUMERIC_STRUCT where the result will be stored
 *
 * @return SQL_SUCCESS if conversion is successful
 *         SQL_SUCCESS_WITH_INFO if conversion is successful but fractional digits were truncated
 *         SQL_ERROR if conversion fails (e.g., NULL pointers, numeric overflow, invalid format)
 *
 * @note The function handles:
 *       - Optional sign (+ or -)
 *       - Optional decimal point
 *       - Optional scientific notation (e.g., 1.23e4, 5.67E-2)
 *       - Precision up to 38 digits
 *       - Scale up to 37 decimal places
 *       - Binary conversion using different approaches for small vs. large numbers
 *       - Truncation detection and reporting
 */
SQLRETURN convertNumericStringToScaledIntegerExtended(RS_STMT_INFO *pStmt,
                                              char *pNumData, int iColDataLen,
                                              SQL_NUMERIC_STRUCT *pnVal)
{
    // Step 1: Input parameter validation
    //   This step validates all required parameters before processing
    //   Ensures all pointers are valid and not null
    if (!pStmt) {
        return SQL_INVALID_HANDLE;
    }

    if (!pNumData || !pnVal) {
        addError(&pStmt->pErrorList, "HY009", "Invalid parameter - NULL pointer", 0, NULL);
        return SQL_ERROR;
    }

    // Step 2: Initialize result structure
    //   Clear the output structure and set default values
    //   Sign: 1 = positive, 0 = negative

    memset(pnVal, 0, sizeof(SQL_NUMERIC_STRUCT));
    pnVal->sign = 1; // 1 = positive, 0 = negative

    // Step 3: Reject special IEEE 754 values
    //   SQL_NUMERIC_STRUCT cannot represent infinity or NaN values
    //   These must be rejected with appropriate error codes

    if (_stricmp(pNumData, "infinity") == 0 || _stricmp(pNumData, "inf") == 0 ||
        _stricmp(pNumData, "+infinity") == 0 || _stricmp(pNumData, "+inf") == 0 ||
        _stricmp(pNumData, "-infinity") == 0 || _stricmp(pNumData, "-inf") == 0 ||
        _stricmp(pNumData, "nan") == 0) {
        addError(&pStmt->pErrorList, "22003", "Numeric value out of range", 0, NULL);
        return SQL_ERROR;
    }

    auto setZeroResult = [&](bool truncationOccurred) -> SQLRETURN {
        pnVal->precision = 1;
        pnVal->scale = (SQLSCHAR)0;
        pnVal->sign = 1;
        memset(pnVal->val, 0, SQL_MAX_NUMERIC_LEN);
        return truncationOccurred ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
    };

    // Step 4: Set up parsing boundaries and handle whitespace
    //   Determine the effective length of input data and trim leading whitespace
    //   Handle empty or whitespace-only input as zero value

    const char *startPtr = pNumData;
    const char *endPtr   = pNumData + (iColDataLen >= 0 ? iColDataLen : (int)strlen(pNumData));

    // 4.1: Trim whitespace from both ends and check if empty
    if (trimWhitespace(&startPtr, &endPtr)) {
        addError(&pStmt->pErrorList, "22018", "Invalid character value for cast specification.", 0, NULL);
        return SQL_ERROR;
    }
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: input='%.*s', len=%d",
             (int)(endPtr - startPtr), startPtr, iColDataLen);

    // Step 5: Parse optional leading sign
    //   Determine if the number is positive or negative
    //   Update the sign field in the result structure

    if (*startPtr == '+') {
        ++startPtr;
    } else if (*startPtr == '-') {
        pnVal->sign = 0;
        ++startPtr;
    }
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: sign=%d", pnVal->sign);
    // Step 6: Collect mantissa digits and decimal point
    //   Parse all digits before and after decimal point into a single buffer
    //   Track the number of fractional digits for later scaling calculations
    //   Examples:
    //     "123.45" -> mantissaBuffer="12345", fracDigits=2
    //     "0.007"  -> mantissaBuffer="0007", fracDigits=3
    //     "42"     -> mantissaBuffer="42", fracDigits=0

    // 6.1: Initialize mantissa collection variables
    char mantissaBuffer[MAX_NUMBER_BUF_LEN + 1];
    int  mantissaLen   = 0;         // count of digits collected (no '.')
    int  fracDigits    = 0;         // digits after decimal point (before exponent)
    bool seenDot       = false;

    // 6.2: Parse digits and optional decimal point
    const char *currPtr = startPtr;
    while (currPtr < endPtr) {
        char c = *currPtr;
        if (c >= '0' && c <= '9') {
            // 6.2.1: Check for buffer overflow
            if (mantissaLen >= (int)sizeof(mantissaBuffer) - 1) {
                addError(&pStmt->pErrorList, "22003", "Numeric value too long", 0, NULL);
                return SQL_ERROR;
            }
            // 6.2.2: Add digit to mantissa buffer
            mantissaBuffer[mantissaLen++] = c;
            if (seenDot) {
                ++fracDigits;
            }
        } else if (c == '.' && !seenDot) {
            // 6.2.3: Handle decimal point (only one allowed)
            seenDot = true;
        } else {
            // 6.2.4: Stop at non-digit, non-decimal characters
            break;
        }
        ++currPtr;
    }

    // 6.3: Validate that at least one digit was found
    if (mantissaLen == 0) {
        addError(&pStmt->pErrorList, "22018", "Invalid numeric format", 0, NULL);
        return SQL_ERROR;
    }
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: mantissa='%.*s', len=%d, fracDigits=%d",
             mantissaLen, mantissaBuffer, mantissaLen, fracDigits);

    // Step 7: Parse optional scientific notation exponent
    //   Handle exponent in the form [eE][+-]?\d+
    //   This determines how many decimal places to shift the mantissa
    //   Examples:
    //     "1.23e2"  -> exponent10=2  (shift right 2: 123.0)
    //     "4.56e-3" -> exponent10=-3 (shift left 3: 0.00456)
    //     "789"     -> exponent10=0  (no exponent)
    int exponent10 = 0;

    ParseReturnCode expResult = parseExponent(&currPtr, endPtr, &exponent10);
    if (expResult == PARSE_INVALID_FORMAT) {
        RS_LOG_ERROR("RSUTIL", "Invalid numeric exponent format");
        addError(&pStmt->pErrorList, "22018", "Invalid numeric exponent", 0, NULL);
        return SQL_ERROR;
    } else if (expResult == PARSE_OVERFLOW) {
        RS_LOG_ERROR("RSUTIL", "Numeric exponent out of range");
        addError(&pStmt->pErrorList, "22003", "Numeric exponent out of range", 0, NULL);
        return SQL_ERROR;
    }
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: exponent=%d", exponent10);

    // Step 8: Validate trailing characters
    //   Only whitespace should remain after the numeric value
    //   Any other characters indicate invalid input format

    // 8.1: Check for invalid trailing characters
    if (currPtr != endPtr) {
        addError(&pStmt->pErrorList, "22018", "Invalid trailing characters in numeric literal", 0, NULL);
        return SQL_ERROR;
    }

    // Step 9: Strip leading zeros and handle zero values
    //   Remove leading zeros from mantissa to get significant digits
    //   Handle special case where all digits are zeros
    //   Examples:
    //     "000123" -> sigLen=3 (remove 3 leading zeros)
    //     "000000" -> sigLen=0 (all zeros, return as zero value)

    // 9.1: Count and skip leading zeros
    int leadingZeroCount = 0;
    while (leadingZeroCount < mantissaLen && mantissaBuffer[leadingZeroCount] == '0') {
        ++leadingZeroCount;
    }
    int sigLen = mantissaLen - leadingZeroCount; // significant length

    // Step 9.2: Handle case where value is zero
    if (sigLen == 0) {
        return setZeroResult(false);
    }
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: sigLen=%d, leadingZeros=%d",
             sigLen, leadingZeroCount);

    // Step 10: Calculate effective scale and normalize mantissa
    //   Apply exponent to determine final scale (decimal places)
    //   Handle negative scales by appending zeros to make integer
    //   Examples:
    //     "123.45" -> fracDigits=2, exponent10=0 -> scale=2-0=2 (2 decimal places)
    //     "1.23e2" -> fracDigits=2, exponent10=2 -> scale=2-2=0 (integer: 123)
    //     "4.5e-1" -> fracDigits=1, exponent10=-1 -> scale=1-(-1)=2 (0.45)

    // 10.1: Calculate effective scale after applying exponent
    int scale = fracDigits - exponent10;

    // 10.2: Move significant digits to start of buffer (remove leading zeros)
    memmove(mantissaBuffer, mantissaBuffer + leadingZeroCount, (size_t)sigLen);

    // 10.3: Handle negative scale by appending zeros
    //       Examples:
    //         "1.23e3" -> scale=-1, append 1 zero: "123" -> "1230", scale=0
    //         "4.5e4"  -> scale=-3, append 3 zeros: "45" -> "45000", scale=0
    if (scale < 0) {
        int zerosToAppend = -scale;
        // Check for buffer overflow
        if (sigLen + zerosToAppend >= (int)sizeof(mantissaBuffer)) {
            addError(&pStmt->pErrorList, "22003", "Numeric value too large", 0, NULL);
            return SQL_ERROR;
        }
        // Append zeros to make the number an integer
        memset(mantissaBuffer + sigLen, '0', (size_t)zerosToAppend);
        sigLen += zerosToAppend;
        scale = 0;
    }
    mantissaBuffer[sigLen] = '\0';
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: scale=%d, finalMantissa='%s'",
             scale, mantissaBuffer);

    // Step 11: Apply precision and scale constraints
    //   SQL_NUMERIC_STRUCT has maximum limits for precision and scale
    //   Truncate fractional digits if necessary, but preserve integer digits
    //   Examples (assuming MAX_PRECISION=38, MAX_SCALE=37):
    //     40-digit number -> truncate 2 rightmost fractional digits
    //     scale=40 -> reduce to scale=37, truncate 3 fractional digits
    //     eg. "12345678901234567890.1234567890123456789012345678901234567890" (20 int + 40 frac)
    //       -> sigLen=60, scale=40, truncate 22 fractional digits
    //       -> result: precision=38, scale=18

    bool truncationOccurred = false;

    // 11.1: Enforce maximum precision by trimming least-significant digits
    //       Only drop fractional digits to avoid overflow
    if (sigLen > MAX_PRECISION) {
        int drop = sigLen - MAX_PRECISION;
        if (drop > scale) {
            // Would need to drop integer digits -> overflow error
            addError(&pStmt->pErrorList, "22003",
                     "Numeric value out of range", 0, NULL);
            return SQL_ERROR;
        }
        // Drop fractional digits from the right
        sigLen -= drop;
        scale  -= drop;
        mantissaBuffer[sigLen] = '\0';
        truncationOccurred = true; // fractional truncation
    }

    // 11.2: Cap scale to MAX_SCALE by dropping additional fractional digits
    if (scale > MAX_SCALE) {
        int drop = MIN(scale - MAX_SCALE, sigLen);
        sigLen -= drop;
        scale   = MAX_SCALE;
        mantissaBuffer[sigLen] = '\0';
        truncationOccurred = true; // fractional truncation
    }

    // 11.3: Ensure ODBC constraint: scale <= precision
    int totalPrecision = MAX(sigLen, scale);
    if (totalPrecision > MAX_PRECISION) {
        int reduce = (totalPrecision - MAX_PRECISION > sigLen) ? sigLen : totalPrecision - MAX_PRECISION;
        sigLen -= reduce;
        scale -= reduce;
        mantissaBuffer[sigLen] = '\0';
        truncationOccurred = true;
        totalPrecision = MAX_PRECISION;
    }

    // Step 11.4: Handle case where everything got truncated to zero
    if (totalPrecision == 0) {
        return setZeroResult(truncationOccurred);
    }
    // 11.5: Set final precision and scale in result structure
    pnVal->precision = (SQLCHAR)totalPrecision;
    pnVal->scale     = (SQLSCHAR)scale;
    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: finalPrecision=%d, finalScale=%d, truncated=%s",
             pnVal->precision, pnVal->scale, truncationOccurred ? "true" : "false");

    // Step 12: Convert decimal string to binary representation
    //   SQL_NUMERIC_STRUCT stores the value as little-endian binary data
    //   Use different algorithms for small vs large numbers for efficiency
    //   Examples:
    //     "12345" (≤18 digits) -> use strtoull(), store as 8 bytes: [0x39, 0x30, 0x00, ...]
    //     "123456789012345678901" (>18 digits) -> use base-256 arithmetic

    const char *mantissaDigits = mantissaBuffer;

    if ((int)strlen(mantissaDigits) <= 18) {
        // 12.1: Small numbers - direct conversion using standard library
        //       Numbers with 18 or fewer digits fit in unsigned long long
        errno = 0;
        unsigned long long ullValue = strtoull(mantissaDigits, NULL, 10);
        if (errno == ERANGE) {
            addError(&pStmt->pErrorList, "22003", "Numeric value out of range", 0, NULL);
            return SQL_ERROR;
        }
        // 12.1.1: Store value in little-endian byte order
        for (int i = 0; i < 8 && i < SQL_MAX_NUMERIC_LEN; i++) {
            pnVal->val[i] = (SQLCHAR)(ullValue & 0xFF);
            ullValue >>= 8;
        }
    } else {
        // 12.2: Large numbers - base-256 magnitude build (little-endian)
        //       Store U = |value| * 10^scale in pnVal->val[] as unsigned LE integer
        //       Convert decimal digits using: U = U*10 + digit
        //       Implemented directly on byte array with multiply-by-10 and carry
        //  Overflow policy:
        //   only drop digits when overflow occurs in the fractional tail; overflow in the integer part return 22003.
        unsigned char littleEndianBytes[SQL_MAX_NUMERIC_LEN] = {0};
        size_t littleEndianBytesLen = 0;
        int fractionStartIndex = MAX(0, (int)strlen(mantissaDigits) - (int)pnVal->scale);

        for (size_t i = 0; i < strlen(mantissaDigits); i++) {
            char digit = mantissaDigits[i];
            // multiply existing by 10
            unsigned int carry = 0;
            for (size_t j = 0; j < littleEndianBytesLen; j++) {
                unsigned int product = littleEndianBytes[j] * 10u + carry;
                littleEndianBytes[j] = (unsigned char)(product & 0xFFu);
                carry  = product >> 8;
            }
            while (carry && littleEndianBytesLen < SQL_MAX_NUMERIC_LEN) {
                littleEndianBytes[littleEndianBytesLen++] = (unsigned char)(carry & 0xFFu);
                carry >>= 8;
            }

            // add current digit
            carry = (unsigned int)(digit - '0');
            for (size_t j = 0; j < littleEndianBytesLen && carry; j++) {
                unsigned int sum = littleEndianBytes[j] + carry;
                littleEndianBytes[j] = (unsigned char)(sum & 0xFFu);
                carry  = sum >> 8;
            }
            if (carry && littleEndianBytesLen < SQL_MAX_NUMERIC_LEN) {
                littleEndianBytes[littleEndianBytesLen++] = (unsigned char)carry;
                carry = 0;
            }

            // overflow check: if still carry or littleEndianBytesLen already at max
            if (carry && littleEndianBytesLen >= SQL_MAX_NUMERIC_LEN) {
                if (i >= fractionStartIndex) {
                    // We’re overflowing while processing digits that belong to the fractional part.
                    truncationOccurred = true; 
                    break;                     // stop adding more (equivalent to truncation)
                } else {
                    // Overflow on the integer side
                    addError(&pStmt->pErrorList, "22003",
                            "Numeric value too large", 0, NULL);
                    return SQL_ERROR;
                }
            }
        }

        // Copy to SQL_NUMERIC_STRUCT
        memcpy(pnVal->val, littleEndianBytes, littleEndianBytesLen);
    }

    // Step 13: Handle final result and truncation warnings
    //   Return appropriate status code based on whether truncation occurred
    //   Add warning if fractional digits were lost during conversion

    RS_LOG_DEBUG("RSUTIL", "convertNumericStringToScaledIntegerExtended: rc=%d, precision=%d, scale=%d, sign=%d",
             (truncationOccurred ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS), pnVal->precision, pnVal->scale, pnVal->sign);

    if (truncationOccurred) {
        addError(&pStmt->pErrorList, "01S07", "Fractional digits truncated", 0, NULL);
        return SQL_SUCCESS_WITH_INFO;
    }

    return SQL_SUCCESS;
}

//---------------------------------------------------------------------------------------------------------igarish
// Convert character string buffer of numeric to scaled integer in little endian mode.
//
void convertScaledIntegerToNumericString(SQL_NUMERIC_STRUCT *pnVal,char *pNumData, int num_data_len)
{
    // Call to convert the little endian mode data into numeric data.
    unsigned long long value=0;
    unsigned long long last=1;
    int i,current;
    int a=0,b=0;

    int sign=1;
    long double divisor;
    long double final_val;

    if(pnVal->precision < 19)
    {
        for(i=0; i < SQL_MAX_NUMERIC_LEN;i++)
        {
	        current = (int) pnVal->val[i];

            a = current % 16; //Obtain LSD
		    b = current / 16; //Obtain MSD
    				
		    value += last * a;	
		    last *= 16;	
		    value += last * b;
		    last *=  16; 
       }


        // The returned value in the above code is scaled to the value specified
        //in the scale field of the numeric structure. For example 25.212 would
        //be returned as 25212. The scale in this case is 3 hence the integer 
        //value needs to be divided by 1000.
       divisor = 1;
       if(pnVal->scale > 0)
       {
	     for (i=0;i< pnVal->scale; i++)	
             divisor = divisor * 10;
       }

        final_val =  (long double) value /(long double) divisor;

        // Examine the sign value returned in the sign field for the numeric
        //structure.
        //NOTE: The ODBC 3.0 spec required drivers to return the sign as 
        //1 for positive numbers and 2 for negative number. This was changed in the
        //ODBC 3.5 spec to return 0 for negative instead of 2.

        if(!pnVal->sign) 
            sign = -1;
        else 
            sign = 1;

	    final_val *= sign;

        if(pnVal->scale >= 0)
        {
            snprintf(pNumData, num_data_len, "%.*Lf",pnVal->scale,final_val);
        }
        else
        {
            int temp = 0;
            int iZeroes = -(pnVal->scale);

            temp = snprintf(pNumData, num_data_len, "%.*Lf",temp,final_val);

            while(iZeroes--)
                rs_strncat(&pNumData[temp++],"0",2);
        }
    }
    else
    {
        // 128 bit numeric
	    unsigned char	*val = (unsigned char *) pnVal->val;
	    unsigned char	pTempBuf[PADB_MAX_NUM_BUF_LEN];
	    int prec2ScaledIntegerLen[] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25, 27, 29, 32, 34, 37, 39};
        int ival;
        int iScaledIntegerLen;
        int len;
        int outputLen;
	    int	j;
        int k;
	    int	iCarry;

        // Convert precision to scaled integer len.
	    for (i = 0; i < SQL_MAX_NUMERIC_LEN && prec2ScaledIntegerLen[i] <= pnVal->precision; i++)
		    ;

	    iScaledIntegerLen = i;
	    len = 0;
	    memset(pTempBuf, 0, sizeof(pTempBuf));

	    for (i = iScaledIntegerLen - 1; i >= 0; i--)
	    {
		    for (j = len - 1; j >= 0; j--)
		    {
			    if (!pTempBuf[j])
				    continue;

                // Convert 4 byte integer to char buf val
			    ival = (((int)pTempBuf[j]) << 8);
			    pTempBuf[j] = (ival % 10);
			    ival /= 10;

			    pTempBuf[j + 1] += (ival % 10);
			    ival /= 10;

			    pTempBuf[j + 2] += (ival % 10);
			    ival /= 10;

			    pTempBuf[j + 3] += ival;

			    for (k = j;; k++)
			    {
				    iCarry = FALSE;
				    if (pTempBuf[k] > 0)
				    {
					    if (k >= len)
						    len = k + 1;

                        // Check for carry
					    while(pTempBuf[k] > 9)
					    {
						    pTempBuf[k + 1]++;
						    pTempBuf[k] -= 10;
						    iCarry = TRUE;
					    }
				    }
				    if (k >= j + 3 && !iCarry)
					    break;
			    }
		    }

		    ival = val[i];
		    if (!ival)
			    continue;

		    pTempBuf[0] += (ival % 10);
		    ival /= 10;

		    pTempBuf[1] += (ival % 10);
		    ival /= 10;

		    pTempBuf[2] += ival;

		    for (j = 0;; j++)
		    {
			    iCarry = FALSE;
			    if (pTempBuf[j] > 0)
			    {
				    if (j >= len)
					    len = j + 1;

				    while (pTempBuf[j] > 9)
				    {
					    pTempBuf[j + 1]++;
					    pTempBuf[j] -= 10;
					    iCarry = TRUE;
				    }
			    }

			    if (j >= 2 && !iCarry)
				    break;
		    }
	    }

        // Output the data according to scale
	    outputLen = 0;
	    if (pnVal->sign == 0)
		    pNumData[outputLen++] = '-';

	    if (i = len - 1, i < pnVal->scale)
		    i = pnVal->scale;

        // Output data before decimal digit. Guard i >= 0: a negative scale would
        // otherwise drive i below 0 and read pTempBuf[-1], pTempBuf[-2], ... which
        // is out of bounds. That uninitialized stack read produced platform-
        // dependent garbage (e.g. a 0xFF byte became '/' on Linux aarch64,
        // corrupting the output string), while x86/macOS happened to read zero
        // bytes and pass. The 128-bit path never supported negative scale.
	    for (; i >= pnVal->scale && i >= 0; i--)
		    pNumData[outputLen++] = pTempBuf[i] + '0';

	    if (pnVal->scale > 0)
	    {
		    pNumData[outputLen++] = '.';

            // Output data after decimal digit
		    for (; i >= 0; i--)
			    pNumData[outputLen++] = pTempBuf[i] + '0';
	    }

	    if (len == 0)
		    pNumData[outputLen++] = '0';

	    pNumData[outputLen] = '\0';

    } // 128 bit
}
/*=====================================================================================*/

#if defined LINUX 

//---------------------------------------------------------------------------------------------------------igarish
// Lower case the given string.
//
char *strlwr(char *str)
{
    char *pTemp = str;

    if(pTemp)
    {
        while(*pTemp)
        {
            *pTemp = tolower(*pTemp);
            pTemp++;
        }
    }

    return str;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Upper case the given string.
//
char *_strupr(char *str)
{
    char *pTemp = str;

    if(pTemp)
    {
        while(*pTemp)
        {
            *pTemp = toupper(*pTemp);
            pTemp++;
        }
    }

    return str;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Do initialization, which happens during DLL_ATTACH on Windows.
//
void sharedObjectAttach()
{
    if(!gInitGlobalVars)
    {
        gInitGlobalVars++;
        initODBC(NULL);
    }
    else
        gInitGlobalVars++;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Do release resources, which happens during DLL_DETACH on Windows.
//
void sharedObjectDetach()
{
    if(gInitGlobalVars)
    {
        gInitGlobalVars--;
        if(gInitGlobalVars == 0)
        {
            uninitODBC();
        }
    }
}

#endif // LINUX 

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read trace options from amazon.redshiftodbc.ini file in the driver directory.
// Return 1 on successful read otherwise 0.
//
int readTraceOptionsFromIniFile(char  *pszTraceLevel,int iTraceLevelBufLen, char *pszTraceFile, int iTraceFileBufLen)
{
    char iniFileName[MAX_PATH + _MAX_FNAME];
    int  readOptions = FALSE;
	char *driverPath;

    iniFileName[0] = '\0';

	 driverPath = getDriverPath();
	 if (driverPath != NULL && *driverPath != '\0')
	 {
		 snprintf(iniFileName, sizeof(iniFileName), "%s%s%s", driverPath, PATH_SEPARATOR, RSODBC_INI_FILE);

		 if (fileExists(iniFileName))
		 {
			 int count = 0;

			 if (pszTraceLevel)
			 {
				 count = RS_GetPrivateProfileString(DRIVER_SECTION_NAME, RS_LOG_LEVEL_OPTION_NAME, "", pszTraceLevel, iTraceLevelBufLen, iniFileName);
			 }

			 if (pszTraceFile)
			 {
				 *pszTraceFile = '\0';
				 char LogPath[MAX_PATH];

				 LogPath[0] = '\0';
				 count = RS_GetPrivateProfileString(DRIVER_SECTION_NAME, RS_LOG_PATH_OPTION_NAME, "", LogPath, MAX_PATH, iniFileName);
				 if (*LogPath)
				 {
					 snprintf(pszTraceFile, iTraceFileBufLen, "%s%s%s", LogPath, PATH_SEPARATOR, TRACE_FILE_NAME);
				 }
			 }

			 if (count != 0)
				 readOptions = TRUE;
		 }
	 }

	 if (driverPath)
		 free(driverPath);


     return readOptions;
}

//---------------------------------------------------------------------------------------------------------igarish
// Read DSN-less connection info from amazon.redshiftodbc.ini file in the driver directory.
// Return 1 on successful read otherwise 0.
//
int readDriverOptionFromIniFile(const char  *pszOptionName,char *pszOptionValBuf, int iOptionValBufLen)
{
    char iniFileName[MAX_PATH + _MAX_FNAME];
    int  readOptions = FALSE;
	char *driverPath;

	iniFileName[0] = '\0';

	driverPath = getDriverPath();

	if (driverPath != NULL && *driverPath != '\0')
	{
		snprintf(iniFileName, sizeof(iniFileName), "%s%s%s", driverPath, PATH_SEPARATOR, RSODBC_INI_FILE);

		if (fileExists(iniFileName))
		{
			int count = 0;

			if (pszOptionValBuf)
			{
				count = RS_GetPrivateProfileString(DRIVER_SECTION_NAME, pszOptionName, "", pszOptionValBuf, iOptionValBufLen, iniFileName);
			}

			if (count != 0)
				readOptions = TRUE;

		}
	}

	if (driverPath)
		free(driverPath);

     return readOptions;
}

void readCscOptionsForDsnlessConnection(RS_CONNECT_PROPS_INFO *pConnectProps)
{
	char optionVal[MAX_OPTION_VAL_LEN];
	int  readOptions;

	
	optionVal[0] = '\0';
	readOptions = readDriverOptionFromIniFile("CscEnable", optionVal, sizeof(optionVal));
    if(readOptions && optionVal[0] != '\0')
    {
        sscanf(optionVal,"%d",&pConnectProps->iCscEnable);
        if((pConnectProps->iCscEnable) && (pConnectProps->iCscEnable != 1))
            pConnectProps->iCscEnable = 0;
    }

	optionVal[0] = '\0';
	readOptions = readDriverOptionFromIniFile("CscMaxFileSize", optionVal, sizeof(optionVal));
    if(readOptions && optionVal[0] != '\0')
    {
        sscanf(optionVal,"%lld",&pConnectProps->llCscMaxFileSize);
    }

	optionVal[0] = '\0';
	readOptions = readDriverOptionFromIniFile("CscPath", optionVal, sizeof(optionVal));
    if(readOptions && optionVal[0] != '\0')
    {
        rs_strncpy_safe(pConnectProps->szCscPath, optionVal, MAX_PATH);
    }

	optionVal[0] = '\0';
	readOptions = readDriverOptionFromIniFile("CscThreshold", optionVal, sizeof(optionVal));
    if(readOptions && optionVal[0] != '\0')
    {
        sscanf(optionVal,"%lld",&pConnectProps->llCscThreshold);
    }

	optionVal[0] = '\0';
	readOptions = readDriverOptionFromIniFile("StreamingCursorRows", optionVal, sizeof(optionVal));
    if(readOptions && optionVal[0] != '\0')
    {
        sscanf(optionVal,"%d",&pConnectProps->iStreamingCursorRows);
    }
	if(pConnectProps->iCscEnable)
		pConnectProps->iStreamingCursorRows = 0;
	else
	if(pConnectProps->iStreamingCursorRows < 0)
		pConnectProps->iStreamingCursorRows = 0;

}


/*====================================================================================================================================================*/
//---------------------------------------------------------------------------------------------------------igarish
// Parse for INSERT command for multi insert conversion. Looking for INSERT...VALUES. If it found INSERT command and successfully convert 
// INSERT into multi INSERT, it will return new command, otherwise returns NULL.
//
char *parseForMultiInsertCommand(RS_STMT_INFO *pStmt, char *pCmd, SQLINTEGER cbLen, char **ppLastBatchMultiInsertCmd)
{
    char *pMultiInsertCmd = NULL;
    char *pLastBatchMultiInsertCmd = NULL;
    RS_CONN_INFO *pConn = pStmt->phdbc;
    RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;

    // Reset the flag for the new command
    pStmt->iMultiInsert = 0;
    pStmt->iLastBatchMultiInsert = 0;

    if(pConnectProps->iMultiInsertCmdConvertEnable && pCmd)
    {
        int i;
        char *pToken;
        int iTokenLen;
        char *pSrc = pCmd;

        cbLen = (INT_LEN(cbLen) == SQL_NTS) ? strlen(pCmd) : cbLen;
        i = 0;

        // Get first token
        pToken = getNextTokenForInsertCommand(&pSrc,cbLen,&i,0);
        if(pToken && (pToken != pSrc))
        {
           iTokenLen = (int)(pSrc - pToken);

           if(iTokenLen == strlen("INSERT")
               && _strnicmp(pToken, "INSERT", iTokenLen) == 0)
           {
                // Is it array binding?
                RS_DESC_HEADER &pAPDDescHeader = pStmt->pStmtAttr->pAPD->pDescHeader;

                // Bind array/single value
                long lArraySize = (pAPDDescHeader.valid == false || pAPDDescHeader.lArraySize <= 0) ? 1 : pAPDDescHeader.lArraySize;

                // Store insert command the and the respective state
                if (pCmd != pStmt->pszUserInsertCmd) { // we are re-prepareing
                    pStmt->resetMultiInsertInfo();
                    pStmt->pszUserInsertCmd = rs_strdup(pCmd, cbLen);
                }
                pStmt->lArraySizeMultiInsert = lArraySize;

                int  iArrayBinding = (lArraySize > 1);

                if(iArrayBinding)
                {
                   // INSERT command.
                   char *pTempCmd = rs_strdup(pCmd, cbLen);

                   if(pTempCmd)
                   {
                       int numOfParamMarkers =
                           ODBCEscapeClauseProcessor::countParamMarkers(
                               pTempCmd, SQL_NTS);

                       if(numOfParamMarkers > 0 && numOfParamMarkers <= PADB_MAX_PARAMETERS)
                       {
                           // Look for VALUES. We may reach to VALUES with more parsing in between tokens.
                           char *pTemp = findSQLClause(pTempCmd,"VALUES");
                           char *pLeftBracket = NULL;
                           char *pRightBracket = NULL;

                           if(pTemp)
                           {
                               pSrc = pTemp + strlen("VALUES");
                               i = (int)(pSrc - pTempCmd);

                               // Get next token to VALUES
                               pToken = getNextTokenForInsertCommand(&pSrc,cbLen,&i,'(');

                               if(pToken && (*pToken == '('))
                               {
                                  pLeftBracket = pToken;

                                  // This should not be multi-value command
                                  pToken = getNextTokenForInsertCommand(&pSrc,cbLen,&i,')');

                                  if(pToken && (*pToken == ')'))
                                  {
                                    pRightBracket = pToken;

                                    // User specified command in MULTI INSERT?
                                    pToken = getNextTokenForInsertCommand(&pSrc,cbLen,&i,0);
                                    if(pToken && (*pToken != ',') && (*pToken == '\0' || *pToken == ';'))
                                    {
                                        int iLastBatchTotalMultiTuples = 0;
                                        int iTotalMultiTuples = getTotalMultiTuples(numOfParamMarkers, lArraySize, &iLastBatchTotalMultiTuples);

                                        if(iTotalMultiTuples >= 1)
                                        {
                                            int iValueClauseLen = (int)(pRightBracket - pLeftBracket) + 1 + 1; // +1 for ','. '(' and ')' already included.
                                            int iMultiInsertCmdLen = (int)(cbLen + (int)(iValueClauseLen * iTotalMultiTuples) + 1); 
                                            int iLastPartLen;
                                            int iLastBatchMultiInsertCmdLen = (iLastBatchTotalMultiTuples > 0) ? (int)(cbLen + (int)(iValueClauseLen * iLastBatchTotalMultiTuples) + 1) : 0; 

                                            pMultiInsertCmd = (char *)rs_calloc(sizeof(char), iMultiInsertCmdLen);
                                            if(iLastBatchMultiInsertCmdLen > 0)
                                            {
                                                pLastBatchMultiInsertCmd = (char *)rs_calloc(sizeof(char), iLastBatchMultiInsertCmdLen);
                                            }

                                            if(pMultiInsertCmd)
                                            {
                                                int iCount;

                                                // Copy upto first '('
                                                i =  (int)(pLeftBracket - pTempCmd);
                                                memcpy(pMultiInsertCmd, pTempCmd,i);
                                                if(pLastBatchMultiInsertCmd)
                                                    memcpy(pLastBatchMultiInsertCmd, pTempCmd,i);

                                                // Copy values in the loop for array
                                                for(iCount = 0;iCount < iTotalMultiTuples; iCount++)
                                                {
                                                    if(iCount != 0)
                                                    {
                                                        memcpy(pMultiInsertCmd + i,",",1);

                                                        if(pLastBatchMultiInsertCmd  && (iCount < iLastBatchTotalMultiTuples))
                                                            memcpy(pLastBatchMultiInsertCmd + i,",",1);

                                                        i += 1;
                                                    }

                                                    memcpy(pMultiInsertCmd + i, pLeftBracket, pRightBracket - pLeftBracket + 1);
                                                    if(pLastBatchMultiInsertCmd  && (iCount < iLastBatchTotalMultiTuples))
                                                        memcpy(pLastBatchMultiInsertCmd + i, pLeftBracket, pRightBracket - pLeftBracket + 1);
                                                    i += ((int)(pRightBracket - pLeftBracket) + 1);

                                                } // Loop

                                                // Copy after ')'. Mostly this should be blanks with or without ';'. 
                                                iLastPartLen =  (int)(cbLen - ((int)(pRightBracket - pTempCmd) + 1));
                                                if(iLastPartLen > 0)
                                                {
                                                    memcpy(pMultiInsertCmd + i, pRightBracket + 1, iLastPartLen);
                                                    if(pLastBatchMultiInsertCmd)
                                                        memcpy(pLastBatchMultiInsertCmd + i, pRightBracket + 1, iLastPartLen);
                                                }

                                                // Convert INSERT into MULTI INSERT
                                                pStmt->iMultiInsert = iTotalMultiTuples;
                                                pStmt->iLastBatchMultiInsert = iLastBatchTotalMultiTuples;
                                            } // Allocated?
                                        } // multi-tuple>1

                                    } // User specified command in MULTI INSERT
                                  } // Right bracket
                               } // Left bracket
                           } // VALUES
                       } // #Params

                       pTempCmd = (char *)rs_free(pTempCmd);

                   } // Temp CMD
                } // ARRAY binding
           } // INSERT
           else {
                // Remove previous traces, if any.
                pStmt->resetMultiInsertInfo();
           }
        }
    }

    if(ppLastBatchMultiInsertCmd)
        *ppLastBatchMultiInsertCmd = pLastBatchMultiInsertCmd;

    return pMultiInsertCmd;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get next token for INSERT command
//
char *getNextTokenForInsertCommand(char **ppSrc, size_t cbLen, int *pi, char token)
{
    char *pToken = NULL;
    char *pSrc = *ppSrc;
    int i = *pi;
    int iQuote = 0;
    int iBracket = 0; // Increment on '(' and decrement on ')'.
    char cPrevChar = 0;


    // Trim leading space 
    while(isspace(*pSrc) && *pSrc && (i < (int)cbLen))
    {
         pSrc++; 
         i++;
    }

    // Get the second token
    if(!token)
        pToken = pSrc;

    while(*pSrc && (i < (int)cbLen))
    {
        // Looking for a specific token?
        if(token) 
        {
            if(*pSrc == SINGLE_QUOTE && (cPrevChar == 0 || cPrevChar != '\\'))
            {
                if(iQuote)
                    iQuote--;
                else
                    iQuote++;
            }
            else
            if(*pSrc == '(' && token == ')' && !iQuote)
            {
                iBracket++;
            }
            else
            if(*pSrc == ')' && token == ')' && !iQuote)
            {
                if(iBracket)
                {
                    iBracket--;

                    if(token)
                        cPrevChar = *pSrc;

                     pSrc++; 
                     i++;

                    continue;
                }
            }

            if(*pSrc == token && !iQuote && !iBracket)
            {
                pToken = pSrc;
                pSrc++; 
                i++;

                break;
            }
            else
            if(*pSrc == SEMI_COLON && !iQuote)
                break;

        }
        else
        if(isspace(*pSrc) || (*pSrc == SEMI_COLON))
            break;

        if(token)
            cPrevChar = *pSrc;

         pSrc++; 
         i++;
    } // Loop

    // Update the output var
    *ppSrc = pSrc;
    *pi = i;

    return pToken;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Calculate total multi tuples because limitations of MAX params.
//
int getTotalMultiTuples(int numOfParamMarkers, long lArraySize, int *piLastBatchTotalMultiTuples)
{
    int iTotalMultiTuples = (int)lArraySize;
    long lTotalParams = numOfParamMarkers * lArraySize;
    int iLastBatchTotalMultiTuples = 0;

    if(lTotalParams > PADB_MAX_PARAMETERS)
    {
        int iLoopCount;
        int iMaxParamsPerBatch;

        iTotalMultiTuples = PADB_MAX_PARAMETERS/numOfParamMarkers;
        iMaxParamsPerBatch = (numOfParamMarkers * iTotalMultiTuples);
        iLoopCount = lTotalParams/iMaxParamsPerBatch;
        iLastBatchTotalMultiTuples = lArraySize - (iTotalMultiTuples * iLoopCount);
    }

    if(piLastBatchTotalMultiTuples)
        *piLastBatchTotalMultiTuples = iLastBatchTotalMultiTuples;

    if(IS_TRACE_ON())
    {
        RS_LOG_INFO("RSUTIL", "iTotalMultiTuples=%d, numOfParamMarkers=%d, lArraySize=%ld, iLastBatchTotalMultiTuples=%d", 
                        iTotalMultiTuples, numOfParamMarkers,lArraySize, iLastBatchTotalMultiTuples);
    }

    return iTotalMultiTuples;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Find pClause in command. Check for it embedded in double quotes.
//
char *findSQLClause(char *pTempCmd, char *pClause)
{
   if (pTempCmd == nullptr || pClause == nullptr) {
       return nullptr;
   }

   char *pTemp = pTempCmd;
   int  iClauseLen = (int)strlen(pClause);

   while(pTemp != NULL)
   {
       pTemp = strcasestrwhole(pTemp,pClause);
       if(pTemp)
       {
            if(pTemp == pTempCmd)
                pTemp = NULL; // break
            else
            {
                // Is it embed in another word?
                if(isspace(*(pTemp - 1)) || *(pTemp - 1) == ')')
                {
                    // Is it embed in double quotes?
                    /*
                    TODO: This solution is not good for a super rare corner cases where input is bad like:
                        INSERT INTO VALUES\" (col1, col2) VALUES (1, 2), (3, 4)
                        then findSQLClause will return :
                        VALUES\" (col1, col2) VALUES (1, 2), (3, 4)
                        instead of:
                        VALUES (1, 2), (3, 4)
                    */
                    if(!DoesEmbedInDoubleQuotes(pTempCmd, pTemp))
                        break; 
                }
            }
       }

       if(pTemp)
            pTemp += iClauseLen; // Continue

   } // Loop

   return pTemp;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return > 0, if pEnd embed in double quotes, otherwise 0.
//
int DoesEmbedInDoubleQuotes(char *pStart, char *pEnd)
{
    int iDoubleQuote = 0;

    while(pStart != pEnd)
    {
        if(*pStart == DOUBLE_QUOTE)
        {
            if(iDoubleQuote)
                iDoubleQuote--;
            else
                iDoubleQuote++;
        }
        pStart++;
    }

    return iDoubleQuote;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create last batch insert command and store in Stmt
//
SQLRETURN createLastBatchMultiInsertCmd(RS_STMT_INFO *pStmt, char *pszLastBatchMultiInsertCmd)
{
    SQLRETURN rc = SQL_SUCCESS;
    char *pszCmd = NULL;

    if(pStmt && pszLastBatchMultiInsertCmd)
    {
        if(pStmt->pszLastBatchMultiInsertCmd == NULL)
            pStmt->pszLastBatchMultiInsertCmd = (RS_STR_BUF *)rs_calloc(1,sizeof(struct _RS_STR_BUF));

        if(pStmt->pszLastBatchMultiInsertCmd)
        {
            pszCmd = (char *)ODBCEscapeClauseProcessor::
                checkReplaceParamMarkerAndODBCEscapeClause(
                    NULL, (char *)pszLastBatchMultiInsertCmd, SQL_NTS,
                    pStmt->pszLastBatchMultiInsertCmd, TRUE);

            if(pszCmd == NULL || pszCmd != pszLastBatchMultiInsertCmd)
            {
                pszLastBatchMultiInsertCmd = (char *)rs_free(pszLastBatchMultiInsertCmd);
            }
        }
        else
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY009", "Couldn't create last batch multi-insert command", 0, NULL);
        }
    }

    return rc;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Re-prepare comamnd without storing any info. This is used by last batch multi-insert command.
//
SQLRETURN rePrepareMultiInsertCommand(RS_STMT_INFO *pStmt, char *pszCmd)
{
    SQLRETURN rc;

    // Release prepared stmt in the server
    rc = libpqExecuteDeallocateCommand(pStmt, FALSE, FALSE);
    if(rc == SQL_SUCCESS)
    {
        rc = libpqPrepareOnThreadWithoutStoringResults(pStmt, pszCmd);
    }

    return rc;
}

/*=====================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Skip all results
//
void checkAndSkipAllResultsOfStreamingCursor(RS_STMT_INFO *pStmt)
{
	if(pStmt
		&& pStmt->pCscStatementContext 
		&& isStreamingCursorMode(pStmt)
		&& pStmt->pResultHead
		&& pStmt->pResultHead->pgResult
		&& !isEndOfStreamingCursorQuery(pStmt->pCscStatementContext)
	)
	{
		if(IS_TRACE_ON())
		{
			RS_LOG_INFO("RSUTIL", "Skiping results for streaming cursor...");
		}

		// If user didn't fetch all result(s), fetch it
		pqSkipAllResultsOfStreamingCursor(pStmt->pCscStatementContext, pStmt->phdbc->pgConn);

		if(IS_TRACE_ON())
		{
			RS_LOG_INFO("RSUTIL", "Skiping results for streaming cursor done");
		}
	}
	else
	if(pStmt
		&& pStmt->pCscStatementContext 
		&& isStreamingCursorMode(pStmt)
	)
	{
        skipAllResultsOfStreamingRowsUsingConnection((RS_CONN_INFO *)pStmt->phdbc);
	}
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Skip all results using connection. Need lock?
//
void skipAllResultsOfStreamingRowsUsingConnection(RS_CONN_INFO *pConn)
{
	if(pConn && (pConn->pConnectProps->iStreamingCursorRows > 0))
	{
		RS_STMT_INFO *curr;
		int iStreamingCursorMode = FALSE;

		curr = pConn->phstmtHead;
		while(curr != NULL)
		{
			if(curr->pCscStatementContext 
				&& isStreamingCursorMode(curr)
			)
			iStreamingCursorMode = TRUE;

			if( curr->pCscStatementContext 
				&& isStreamingCursorMode(curr)
				&& curr->pResultHead	
				&& curr->pResultHead->pgResult
				&& !isEndOfStreamingCursorQuery(curr->pCscStatementContext)
			)
			{
				if(IS_TRACE_ON())
				{
					RS_LOG_INFO("RSUTIL", "Skiping results for streaming cursor...");
				}

				// If user didn't fetch all result(s), fetch it
				pqSkipAllResultsOfStreamingCursor(curr->pCscStatementContext, curr->phdbc->pgConn);

				if(IS_TRACE_ON())
				{
					RS_LOG_INFO("RSUTIL", "Skiping results for streaming cursor done");
				}

				break; // Only one statement active
			}

			curr = curr->pNext;
		} // Loop

		if(iStreamingCursorMode)
		{
			// For SET command kind of stuff, we don't have result. But we need idle state
			// before executing any query
			if(!pqIsIdle(pConn->pgConn))
			{
				curr = pConn->phstmtHead;
				while(curr != NULL)
				{
					if( curr->pCscStatementContext 
						&& isStreamingCursorMode(curr)
						&& !isEndOfStreamingCursorQuery(curr->pCscStatementContext)
					)
					{
						if(IS_TRACE_ON())
						{
							RS_LOG_INFO("RSUTIL", "Skiping results for streaming cursor...");
						}

						// If user didn't fetch all result(s), fetch it
						pqSkipAllResultsOfStreamingCursor(curr->pCscStatementContext, curr->phdbc->pgConn);

						if(IS_TRACE_ON())
						{
							RS_LOG_INFO("RSUTIL", "Skiping results for streaming cursor done");
						}

						break;
					}

					curr = curr->pNext;
				} // Loop
			} // Is IDLE?

			// Reset any other statement result before proceeding further
			pqResetConnectionResult(pConn->pgConn);
		}

	} // Is streaming cursor mode?
}

//---------------------------------------------------------------------------------------------------------igarish
// Does any other streaming cursor open
//
int doesAnyOtherStreamingCursorOpen(RS_CONN_INFO *pConn, RS_STMT_INFO *pStmt)
{
	int rc = FALSE;

	if(pConn && (pConn->pConnectProps->iStreamingCursorRows > 0))
	{
		RS_STMT_INFO *curr;

		curr = pConn->phstmtHead;
		while(curr != NULL)
		{
			if( curr->pCscStatementContext 
				&& isStreamingCursorMode(curr)
				&& !isEndOfStreamingCursorQuery(curr->pCscStatementContext)
				&& (getStreamingCursorBatchNumber(curr->pCscStatementContext) > 0)
			)
			{
				// Is it same statement on which new execution happening?
				if(pStmt != curr)
				{
					rc = TRUE;

					break; // Only one statement active
				}
			}

			curr = curr->pNext;
		} // Loop

	} // Is streaming cursor mode?

	return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get param types when there is at least one OUT parameter
//

std::vector<Oid> getParamTypes(int iNoOfBindParams, RS_DESC_REC *pDescRecHead,
                               RS_CONNECT_PROPS_INFO *pConnectProps) {
    std::vector<Oid> paramTypes(iNoOfBindParams, UNSPECIFIEDOID);

    RS_DESC_REC *pDescRec;
    int paramIndex = 0;

    for (pDescRec = pDescRecHead;
         pDescRec != NULL && paramIndex < iNoOfBindParams;
         pDescRec = pDescRec->pNext) {

        if (pDescRec->hInOutType == SQL_PARAM_OUTPUT)
            paramTypes[paramIndex] = VOIDOID;
        else {
            if (pDescRec->hParamSQLType == SQL_CHAR ||
                pDescRec->hParamSQLType == SQL_VARCHAR) {
                if (_stricmp(pConnectProps->szStringType, "unspecified") == 0)
                    paramTypes[paramIndex] = UNSPECIFIEDOID;
                else
                    paramTypes[paramIndex] = VARCHAROID;
            } else
                paramTypes[paramIndex] = UNSPECIFIEDOID;
        }
        paramIndex++;
    }

    return std::move(paramTypes);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Update OUT/IN_OUT parameter values
//
int updateOutBindParametersValue(RS_STMT_INFO *pStmt)
{
	int rc = SQL_SUCCESS;
	RS_RESULT_INFO *pResult = pStmt->pResultHead;

	// TODO: Make sure this is the last result and all previous results has been fetch
	if (pResult != NULL
		&& pResult->pNext == NULL)
	{
		// Fetch the row
		rc = RS_STMT_INFO::RS_SQLFetchScroll((SQLHSTMT)pStmt, SQL_FETCH_NEXT, 0);
		if (rc == SQL_SUCCESS)
		{
			// Getdata for each OUT/INOUT parameter
			RS_DESC_REC *pDescRec;
			int iNumBindParams = countBindParams(pStmt->pStmtAttr->pAPD->pDescRecHead);
			int *pOutParamRecNums = (int *)rs_calloc(sizeof(int), iNumBindParams); 

			if (pOutParamRecNums)
			{
				int iParamNumber = 0;
				int iOutParamResultIndex = 0;

				for (pDescRec = pStmt->pStmtAttr->pAPD->pDescRecHead; pDescRec != NULL; pDescRec = pDescRec->pNext)
				{
					if (pDescRec->hInOutType == SQL_PARAM_OUTPUT
						|| pDescRec->hInOutType == SQL_PARAM_INPUT_OUTPUT)
					{
						pOutParamRecNums[pDescRec->hRecNumber-1] = pDescRec->hRecNumber;
					}
				}


				for (pDescRec = pStmt->pStmtAttr->pAPD->pDescRecHead; 
								(iParamNumber < iNumBindParams && pDescRec != NULL); 
								iParamNumber++, pDescRec = pDescRec->pNext)
				{
					if(pOutParamRecNums[iParamNumber] != 0)
					{
                        SQLLEN pcbLenIndInternal = (std::numeric_limits<SQLLEN>::min)();
						rc = RS_STMT_INFO::RS_SQLGetData(pStmt, iOutParamResultIndex + 1, pDescRec->hType,
							pDescRec->pValue, pDescRec->cbLen, pDescRec->pcbLenInd, TRUE, pcbLenIndInternal);
						if (rc == SQL_ERROR)
						{
							pOutParamRecNums = (int *)rs_free(pOutParamRecNums);
							break;
						}
						iOutParamResultIndex++;
					}
				}

				pOutParamRecNums = (int *)rs_free(pOutParamRecNums);
			}
			else
				rc = SQL_ERROR;
		}
	}
	else
		rc = SQL_ERROR;

	return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// strncpy.
//
char *rs_strncpy(char *dest, const char *src, size_t n)
{
	int data_len = redshift_min(strlen(src), n-1);

	strncpy(dest, src, data_len);
	dest[data_len] = '\0';
	return dest;
}

/**
 * @brief Safely copies a null-terminated string into a fixed-size buffer.
 *
 * Copies at most (n - 1) characters from `src` to `dest` and always
 * null-terminates the result (if `n > 0`). This avoids buffer overflows
 * and ensures that `dest` is always a valid null-terminated C string.
 *
 * Unlike `strncpy()`, this function does not pad the rest of the buffer
 * with nulls, and it guarantees a trailing '\0' even when `src` is longer
 * than (n - 1).
 *
 * Defensive checks also reject:
 *   - NULL pointers
 *   - Zero-length buffers (n == 0)
 *   - The special ODBC sentinel value SQL_NTS ((size_t)-3)
 *
 * If `src == dest`, no copying is performed, but null-termination is still enforced.
 *
 * @param dest Destination buffer (must have at least `n` bytes)
 * @param src  Null-terminated input string
 * @param n    Size of destination buffer, including space for '\0'
 *
 * @return Pointer to `dest` on success, or NULL on invalid input
 */
char *rs_strncpy_safe(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0 || n == (size_t)SQL_NTS || n > INT_MAX) {
        return NULL;
    }

    const size_t cap = n - 1;       // bytes we can actually copy
    size_t len = rs_strnlen(src, cap);

    // Bound to capacity (defensive; len is already <= cap)
    if (len > cap) len = cap;

    if (len && src != dest) {
        memmove(dest, src, len);
    }
    dest[len] = '\0';
    return dest;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// strncpy.
//
char *rs_strncat(char *dest, const char *src, size_t n)
{
	strncat(dest, src, n-1);

	return dest;
}

/*====================================================================================================================================================*/

int date_out(int date, char *buf, int buf_len)
{
	char* result;
	struct pg_tm tt, *tm = &tt;
	int len;

	j2date(date + POSTGRES_EPOCH_JDATE, &(tm->tm_year), &(tm->tm_mon),
					&(tm->tm_mday));

	if ((tm->tm_mon < 1) || (tm->tm_mon > 12))
	{
		*buf = '\0'; // Error
		len = 0;
	}
	else
	{
		if (tm->tm_year > 0)
			len = snprintf(buf, buf_len, "%04d-%02d-%02d",
				tm->tm_year, tm->tm_mon, tm->tm_mday);
		else
			len = snprintf(buf, buf_len, "%04d-%02d-%02d %s",
				-(tm->tm_year - 1), tm->tm_mon, tm->tm_mday, "BC");
	}

	return len;
}

/*====================================================================================================================================================*/

/*====================================================================================================================================================*/

void
j2date(int jd, int *year, int *month, int *day)
{
	unsigned int julian;
	unsigned int quad;
	unsigned int extra;
	int			y;

	julian = jd;
	julian += 32044;
	quad = julian / 146097;
	extra = (julian - quad * 146097) * 4 + 3;
	julian += 60 + quad * 3 + extra / 146097;
	quad = julian / 1461;
	julian -= quad * 1461;
	y = julian * 4 / 1461;
	julian = ((y != 0) ? ((julian + 305) % 365) : ((julian + 306) % 366))
		+ 123;
	y += quad * 4;
	*year = y - 4800;
	quad = julian * 2141 / 65536;
	*day = julian - 7834 * quad / 256;
	*month = (quad + 10) % 12 + 1;

	return;
}	/* j2date() */

/*====================================================================================================================================================*/

int timestamp_out(long long timestamp, char *buf, int buf_len, char *session_timezone)
{
	struct pg_tm tt, *tm = &tt;
	long long fsec;
	int len;

	if (TIMESTAMP_NOT_FINITE(timestamp))
	{
		if (TIMESTAMP_IS_NOBEGIN(timestamp))
			len = snprintf(buf, buf_len, "%s", EARLY);
		else if (TIMESTAMP_IS_NOEND(timestamp))
			len = snprintf(buf, buf_len, "%s", LATE);
		else
		{
			buf[0] = '0';
			len = 0;
		}
	}
	else if (timestamp2tm(timestamp, NULL, tm, &fsec) == 0)
	{
		/* Compatible with ISO-8601 date formats */

		len = snprintf(buf, buf_len, "%04d-%02d-%02d %02d:%02d",
			((tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1)),
			tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);

		if (fsec != 0)
		{
			len += snprintf((buf + strlen(buf)), buf_len - len, ":%02d.%06lld", tm->tm_sec, fsec);
//			TrimTrailingZeros(buf,&len);
		}
		else
			len += snprintf((buf + strlen(buf)), buf_len - len,":%02d", tm->tm_sec);

		if (session_timezone)
		{
			// Server always send Binary data in UTC
			len += snprintf((buf + strlen(buf)), buf_len - len, " %s", "UTC"); // session_timezone
		}

		if (tm->tm_year <= 0)
			len += snprintf((buf + strlen(buf)), buf_len - len, " BC");
	}
	else
	{
		// Error timestamp out of range
		buf[0] = '0';
		len = 0;
	}

	return len;
}

/*====================================================================================================================================================*/

/**
 * @brief Convert a year-month interval to string representation.
 *
 * Produces the interval literal string format: [+-]Y-M
 * Examples: "5-3", "-2-7", "0-0"
 *
 * @param pInterval Pointer to SQL_INTERVAL_STRUCT containing year-month
 * interval data
 * @param buf       Output buffer to store the formatted string
 * @param buf_len   Size of the output buffer in bytes
 *
 * @return Number of characters written to the buffer (excluding null
 * terminator)
 *
 * @note The format is [sign]year-month where sign is '-' for
 *       negative intervals. All component values are absolute (unsigned).
 * @note The pInterval parameter must not be NULL (asserted)
 */
int intervaly2m_out(SQL_INTERVAL_STRUCT *pInterval, char *buf, int buf_len) {
    int len = 0;

    assert(pInterval != NULL);

    // Get absolute values from the struct
    int year = (int)pInterval->intval.year_month.year;
    int month = (int)pInterval->intval.year_month.month;

    // Format: [+-]Y-M
    if (pInterval->interval_sign == SQL_TRUE) {
        len = snprintf(buf, buf_len, "-%d-%d", year, month);
    } else {
        len = snprintf(buf, buf_len, "%d-%d", year, month);
    }

    return len;
}

/*====================================================================================================================================================*/

/**
 * @brief Convert a day-second interval to string representation.
 *
 * Produces the ODBC interval literal string format: [+-][D ]HH:MM:SS[.fraction]
 * Examples: "3 04:30:15.123000", "-1 02:45:30.456000", "00:00:00"
 *
 * @param pInterval Pointer to SQL_INTERVAL_STRUCT containing day-second
 * interval data
 * @param buf       Output buffer to store the formatted string
 * @param buf_len   Size of the output buffer in bytes
 *
 * @return Number of characters written to the buffer (excluding null
 * terminator)
 *
 * @note The format uses a single leading sign for negative
 * intervals, day component followed by space and time, fraction in microseconds
 * (6 digits).
 * @note The pInterval parameter must not be NULL (asserted)
 */
int intervald2s_out(SQL_INTERVAL_STRUCT *pInterval, char *buf, int buf_len) {
    int len = 0;

    assert(pInterval != NULL);

    // Get absolute values from the struct
    int day = (int)pInterval->intval.day_second.day;
    int hour = (int)pInterval->intval.day_second.hour;
    int minute = (int)pInterval->intval.day_second.minute;
    int second = (int)pInterval->intval.day_second.second;
    int fraction = (int)pInterval->intval.day_second.fraction;

    // Format: [+-][D ]HH:MM:SS[.fraction]
    // Single leading sign for negative, day component with space separator
    if (pInterval->interval_sign == SQL_TRUE) {
        if (day != 0) {
            len += snprintf(buf + rs_buf_offset(len, buf_len),
                            rs_buf_remaining(len, buf_len),
                            "-%d %02d:%02d:%02d", day, hour, minute, second);
        } else {
            len += snprintf(buf + rs_buf_offset(len, buf_len),
                            rs_buf_remaining(len, buf_len), "-%02d:%02d:%02d",
                            hour, minute, second);
        }
    } else {
        if (day != 0) {
            len += snprintf(buf + rs_buf_offset(len, buf_len),
                            rs_buf_remaining(len, buf_len), "%d %02d:%02d:%02d",
                            day, hour, minute, second);
        } else {
            len += snprintf(buf + rs_buf_offset(len, buf_len),
                            rs_buf_remaining(len, buf_len), "%02d:%02d:%02d",
                            hour, minute, second);
        }
    }

    if (fraction != 0) {
        len += snprintf(buf + rs_buf_offset(len, buf_len),
                        rs_buf_remaining(len, buf_len), ".%06d", fraction);
    }

    return len;
}

/*====================================================================================================================================================*/

/**
 * @brief Parse a year-month interval string and extract year/month fields
 *
 * @param buf     Input string containing the interval representation
 * @param buf_len Length of the input buffer
 *
 * @return SQL_INTERVAL_STRUCT with interval_type set to SQL_IS_YEAR_TO_MONTH,
 *         interval_sign indicating positive (SQL_FALSE) or negative (SQL_TRUE),
 *         and absolute values in intval.year_month.year and
 * intval.year_month.month
 *
 * @note Supports multiple formats: SQL standard ("Y-M" or "-Y-M"),
 *       Postgres ("1 year 2 mons"), and Postgres verbose ("@ 1 year 2 mons
 * ago")
 * @pre Input must be a well-formed interval string. The caller (getRsVal or
 *      convertCharToIntervalY2M) is responsible for ensuring the input has
 *      a valid format. Passing malformed or zero-length strings may produce
 *      a zeroed (invalid) result.
 */
SQL_INTERVAL_STRUCT parse_intervaly2m(const char *buf, int buf_len) {
    SQL_INTERVAL_STRUCT result;
    memset(&result, 0, sizeof(SQL_INTERVAL_STRUCT));
    result.interval_type = SQL_IS_YEAR_TO_MONTH;
    result.interval_sign = SQL_FALSE;  // Default to positive

    int year = 0, month = 0;
    bool is_negative = false;

    // Check for decimal points - not allowed in year-month intervals
    if (memchr(buf, '.', buf_len) != NULL) {
        return returnInvalidIntervalY2M();
    }

    bool is_sql_standard = true;
    for (int i = 0; i < buf_len; i++) {
        if (!isdigit(buf[i]) && buf[i] != '-') {
            is_sql_standard = false;
            break;
        }
    }
    if (is_sql_standard) {
        is_negative = (buf[0] == '-');
        if (is_negative) {
            buf++;
        }
        if (!isValidYearMonthFormat(buf)) {
            return returnInvalidIntervalY2M();
        }
        sscanf(buf, "%d-%d", &year, &month);

    } else {

        // Postgres or Postgres Verbose format
        // eg. 1 year 2 mons or @ 1 year 2 mons
        const char* search_pos = strstr(buf, "year");
        if (search_pos != NULL && search_pos >= buf + 2) {
            const char* c = search_pos-2;
            for (; c >= buf; c--) {
                if (!isdigit(*c) && *c != '-') {
                    break;
                }
            }
            sscanf(c + 1, "%d", &year);
            if (year < 0) {
                is_negative = true;
                year = -year;
            }
        }
        search_pos = strstr(search_pos ? search_pos : buf, "mon");
        if (search_pos != NULL && search_pos >= buf + 2) {
            const char* c = search_pos-2;
            for (; c >= buf; c--) {
                if (!isdigit(*c) && *c != '-') {
                    break;
                }
            }
            sscanf(c + 1, "%d", &month);
            if (month < 0) {
                is_negative = true;
                month = -month;
            }
        }
        // Check for "ago" suffix in Postgres verbose format
        // eg. @ 1 year 2 mons ago
        if (strstr(buf, "ago") != NULL) {
            is_negative = !is_negative;  // Toggle sign if "ago" is present
        }
    }

    if (year < 0 || month < 0 || month > MAX_DATE_MONTH-1) {
        return returnInvalidIntervalY2M();
    }

    // Set interval_sign: SQL_FALSE (0) for positive, SQL_TRUE (1) for negative
    result.interval_sign = is_negative ? SQL_TRUE : SQL_FALSE;
    // Store absolute values
    result.intval.year_month.year = (SQLUINTEGER)year;
    result.intval.year_month.month = (SQLUINTEGER)month;

    return result;
}

/*====================================================================================================================================================*/

/**
 * @brief Parse a day-second interval string and extract
 * day/hour/minute/second/fraction fields
 *
 * @param buf     Input string containing the interval representation
 * @param buf_len Length of the input buffer
 *
 * @return SQL_INTERVAL_STRUCT with interval_type set to SQL_IS_DAY_TO_SECOND,
 *         interval_sign indicating positive (SQL_FALSE) or negative (SQL_TRUE),
 *         and absolute values in intval.day_second fields
 *
 * @note Supports multiple formats:
 *       - SQL standard: "D H:M:S.F" or "-D H:M:S.F"
 *       - Postgres: "N days H:M:S.F" or "-N days -H:M:S.F"
 *       - Postgres verbose: "@ N days H hours M mins S.F secs [ago]"
 * @note The "ago" suffix in Postgres verbose format negates the interval sign
 * @note Returns zeroed result for invalid input (e.g., minutes/seconds out of
 * range)
 * @pre Input must be a well-formed interval string. The caller (getRsVal or
 *      convertCharToIntervalD2S) is responsible for ensuring the input has
 *      a valid format. Passing malformed or zero-length strings may produce
 *      a zeroed (invalid) result.
 */
SQL_INTERVAL_STRUCT parse_intervald2s(const char *buf, int buf_len) {
    SQL_INTERVAL_STRUCT result;
    memset(&result, 0, sizeof(SQL_INTERVAL_STRUCT));
    result.interval_type = SQL_IS_DAY_TO_SECOND;
    result.interval_sign = SQL_FALSE;  // Default to positive

    int day = 0, hour = 0, min = 0, sec = 0;
    char micr[] = "+000000000";
    bool is_negative = false;
    bool is_postgres_verbose = false;
    bool has_spaces = false;
    bool has_alphabets = false;
    // Scan input to determine format type
    for (int i = 0; i < buf_len; i++) {
        if (buf[i] == '@') {
            is_postgres_verbose = true;
        } else if (buf[i] == ' ') {
            has_spaces = true;
        }
        // Don't treat '+' or ':' or '-' or '.' as alphabetic
        else if (!isdigit(buf[i]) && buf[i] != '-' && buf[i] != '.' &&
                 buf[i] != '+' && buf[i] != ':') {
            has_alphabets = true;
        }
    }
    if (!has_alphabets) {
        // SQL Standard or simple numeric format
        const char *original_buf = buf;
        is_negative = (buf[0] == '-');
        if (buf[0] == '-' || buf[0] == '+') {
            buf++;
        }
        if (has_spaces) {
            // Sql Standard format: "D H:M:S.F"
            // Validate format before parsing using original buffer
            if (!isValidDayToSecondFormat(original_buf)) {
                return returnInvalidIntervalD2S();
            }
            sscanf(buf, "%d %d:%d:%d.%s", &day, &hour, &min, &sec, micr+1);
        } else {
            // Postgres but no days: "H:M:S.F"
            // Validate time format
            if (!isValidTimeFormat(buf)) {
                return returnInvalidIntervalD2S();
            }
            sscanf(buf, "%d:%d:%d.%s", &hour, &min, &sec, micr+1);
        }
    } else {
        // Postgres or Postgres Verbose format
        const char* prev_search_pos = buf;
        const char* search_pos = strstr(buf, "day");
        // Extract day component
        if (search_pos != NULL) {
            const char* c = search_pos-2;
            for (; c >= buf; c--) {
                if (!isdigit(*c) && *c != '-') break;
            }
            sscanf(c + 1, "%d", &day);
            if (day < 0) {
                is_negative = true;
                day = -day;
            }
        }
        if (is_postgres_verbose) {
            // Postgres verbose format: extract hour, minute, second with labels
            prev_search_pos = search_pos ? search_pos : prev_search_pos;
            search_pos = strstr(prev_search_pos, "hour");
            if (search_pos != NULL) {
                const char* c = search_pos-2;
                for (; c >= buf; c--) {
                    if (!isdigit(*c) && *c != '-') break;
                }
                int temp_hour = 0;
                sscanf(c + 1, "%d", &temp_hour);
                if (temp_hour < 0) {
                    is_negative = true;
                    hour = -temp_hour;
                } else {
                    hour = temp_hour;
                }
            }
            prev_search_pos = search_pos ? search_pos : prev_search_pos;
            search_pos = strstr(prev_search_pos, "min");
            if (search_pos != NULL) {
                const char* c = search_pos-2;
                for (; c >= buf; c--) {
                    if (!isdigit(*c) && *c != '-') break;
                }
                int temp_min = 0;
                sscanf(c + 1, "%d", &temp_min);
                if (temp_min < 0) {
                    is_negative = true;
                    min = -temp_min;
                } else {
                    min = temp_min;
                }
            }
            prev_search_pos = search_pos ? search_pos : prev_search_pos;
            search_pos = strstr(prev_search_pos, "sec");
            if (search_pos != NULL) {
                const char* c = search_pos-2;
                for (; c >= buf; c--) {
                    if (!isdigit(*c) && *c != '-' && *c != '.') break;
                }
                int temp_sec = 0;
                sscanf(c + 1, "%d.%s", &temp_sec, micr+1);
                if (temp_sec < 0) {
                    is_negative = true;
                    sec = -temp_sec;
                } else {
                    sec = temp_sec;
                }
            }
            // Check for "ago" suffix in Postgres verbose format
            search_pos = strstr(search_pos ? search_pos : prev_search_pos, "ago");
            if (search_pos != NULL) {
                is_negative = !is_negative;  // Toggle sign if "ago" is present
            }
        } else {
            // Postgres format: scan for time portion "H:M:S.F" or "-H:M:S.F"
            while (*search_pos != 0 && *search_pos != '-' && !isdigit(*search_pos))
                search_pos++;

            // Check if there's a dash before the time portion
            if (*search_pos == '-') {
                // Skip the dash - the overall sign is already determined by the day component
                // For negative intervals, postgres format uses "-N days -HH:MM:SS"
                search_pos++;
                // Skip any spaces after the dash
                while (*search_pos == ' ') search_pos++;
                is_negative = true;
            }
            // Now parse the time portion (without expecting a leading dash)
            if (isdigit(*search_pos)) {
                if (!isValidTimeFormat(search_pos)) {
                    return returnInvalidIntervalD2S();
                }
                sscanf(search_pos, "%d:%d:%d.%s", &hour, &min, &sec, micr + 1);
            }
        }
    }
    // Validate parsed time components per ODBC spec
    // For DAY TO SECOND intervals, hour is a non-leading field (0-23),
    // minute (0-59), second (0-59).
    // Note: Redshift normalizes hour > 23 into days on the server side,
    // so data from Redshift will always have hour 0-23.
    // For varchar-to-interval conversion, values with hour > 23 are
    // rejected as invalid interval literals per ODBC spec (SQLSTATE 22015).
    if (hour < 0 || hour > MAX_TIME_HOUR ||
        min < MIN_TIME_MINUTE || min > MAX_TIME_MINUTE ||
        sec < MIN_TIME_SECOND || sec > MAX_TIME_SECOND) {
        return returnInvalidIntervalD2S();
    }
    // Pad microseconds to 6 digits
    // micr is initialized as "+000000000", sscanf writes to micr+1
    // So micr+1 contains the fractional part string
    int fracLen = strlen(micr + 1); // Length of fractional part only
    for (int i = fracLen + 1; i < 7;
         i++) { // Start from position after last digit (+1 for sign)
        micr[i] = '0';
    }
    micr[7] = '\0';

    // Set interval_sign: SQL_FALSE (0) for positive, SQL_TRUE (1) for negative
    result.interval_sign = is_negative ? SQL_TRUE : SQL_FALSE;
    // Store absolute values
    result.intval.day_second.day = (SQLUINTEGER)day;
    result.intval.day_second.hour = (SQLUINTEGER)hour;
    result.intval.day_second.minute = (SQLUINTEGER)min;
    result.intval.day_second.second = (SQLUINTEGER)sec;
    result.intval.day_second.fraction =
        (SQLUINTEGER)atoi(micr + 1); // Skip the sign character

    return result;
}

/*====================================================================================================================================================*/

#ifdef WIN32

int date_out_wchar(int date, SQLWCHAR *buf, int buf_len)
{
	char* result;
	struct pg_tm tt, *tm = &tt;
	int len;

	j2date(date + POSTGRES_EPOCH_JDATE, &(tm->tm_year), &(tm->tm_mon),
		&(tm->tm_mday));

	if ((tm->tm_mon < 1) || (tm->tm_mon > 12))
	{
		*buf = L'\0'; // Error
		len = 0;
	}
	else
	{
		if (tm->tm_year > 0)
			len = swprintf(buf, buf_len, L"%04d-%02d-%02d",
				tm->tm_year, tm->tm_mon, tm->tm_mday);
		else
			len = swprintf(buf, buf_len, L"%04d-%02d-%02d %s",
				-(tm->tm_year - 1), tm->tm_mon, tm->tm_mday, "BC");
	}

	return len;
}

/*====================================================================================================================================================*/

int timestamp_out_wchar(long long timestamp, SQLWCHAR *buf, int buf_len, char *session_timezone)
{
	struct pg_tm tt, *tm = &tt;
	long long fsec;
	int len;

	if (TIMESTAMP_NOT_FINITE(timestamp))
	{
		if (TIMESTAMP_IS_NOBEGIN(timestamp))
			len = swprintf(buf, buf_len, L"%s", EARLY);
		else if (TIMESTAMP_IS_NOEND(timestamp))
			len = swprintf(buf, buf_len, L"%s", LATE);
		else
		{
			buf[0] = L'0';
			len = 0;
		}
	}
	else if (timestamp2tm(timestamp, NULL, tm, &fsec) == 0)
	{
		/* Compatible with ISO-8601 date formats */

		len = swprintf(buf, buf_len, L"%04d-%02d-%02d %02d:%02d",
			((tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1)),
			tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);

		if (fsec != 0)
		{
			len += swprintf((buf + wcslen(buf)), buf_len, L":%02d.%06lld", tm->tm_sec, fsec);
			//			TrimTrailingZeros(buf,&len);
		}
		else
			len += swprintf((buf + wcslen(buf)), buf_len, L":%02d", tm->tm_sec);

		if (session_timezone)
		{
			// Server always send Binary data in UTC

			len += swprintf((buf + wcslen(buf)), L" %s", "UTC"); // session_timezone
		}

		if (tm->tm_year <= 0)
			len += swprintf((buf + wcslen(buf)), buf_len, L" BC");
	}
	else
	{
		// Error timestamp out of range
		buf[0] = L'0';
		len = 0;
	}

	return len;
}

/*====================================================================================================================================================*/

int time_out_wchar(long long time, SQLWCHAR *buf, int buf_len, int *tzp)
{
	struct pg_tm tt, *tm = &tt;
	long long fsec;
	int len;

	if (time2tm(time, &tt, &fsec) == 0)
	{
		if ((tm->tm_hour < 0) || (tm->tm_hour > 24))
		{
			buf[0] = L'0';
			len = 0;
		}
		else
		{
			len = swprintf(buf, buf_len, L"%02d:%02d", tm->tm_hour, tm->tm_min);

			/*
			* Print fractional seconds if any.  The field widths here should be
			* at least equal to the larger of MAX_TIME_PRECISION and
			* MAX_TIMESTAMP_PRECISION.
			*/
			if (fsec != 0)
			{
				len += swprintf((buf + wcslen(buf)), buf_len, L":%02d.%06lld", tm->tm_sec, fsec);
				//				TrimTrailingZeros(buf,&len);
			}
			else
				len += swprintf((buf + wcslen(buf)), buf_len, L":%02d", tm->tm_sec);

			if (tzp != NULL)
			{
				int			hour,
					min;

				hour = -(*tzp / 3600);
				min = ((abs(*tzp) / 60) % 60);
				len += swprintf((buf + wcslen(buf)), buf_len, ((min != 0) ? L"%+03d:%02d" : L"%+03d"), hour, min);
			}
		}
	}
	else
	{
		// Error timestamp out of range
		buf[0] = L'0';
		len = 0;
	}

	return len;
}


#endif

/*====================================================================================================================================================*/

int time_out(long long time, char *buf, int buf_len, int *tzp) 
{
	struct pg_tm tt, *tm = &tt;
	long long fsec;
	int len;

	if (time2tm(time, &tt, &fsec) == 0)
	{
		if ((tm->tm_hour < 0) || (tm->tm_hour > 24))
		{
			buf[0] = '0';
			len = 0;
		}
		else
		{
			len = snprintf(buf, buf_len, "%02d:%02d", tm->tm_hour, tm->tm_min);

			/*
			* Print fractional seconds if any.  The field widths here should be
			* at least equal to the larger of MAX_TIME_PRECISION and
			* MAX_TIMESTAMP_PRECISION.
			*/
			if (fsec != 0)
			{
				len += snprintf((buf + strlen(buf)), buf_len - len, ":%02d.%06lld", tm->tm_sec, fsec);
//				TrimTrailingZeros(buf,&len);
			}
			else
				len += snprintf((buf + strlen(buf)), buf_len - len, ":%02d", tm->tm_sec);

			if (tzp != NULL)
			{
				int			hour,
					min;

				hour = -(*tzp / 3600);
				min = ((abs(*tzp) / 60) % 60);
				len += snprintf((buf + strlen(buf)), buf_len - len, ((min != 0) ? "%+03d:%02d" : "%+03d"), hour, min);
			}
		}
	}
	else
	{
		// Error timestamp out of range
		buf[0] = '0';
		len = 0;
	}

	return len;
}


/*====================================================================================================================================================*/

/*
* timestamp2tm() - Convert timestamp data type to POSIX time structure.
*
* Note that year is _not_ 1900-based, but is an explicit full value.
* Also, month is one-based, _not_ zero-based.
* Returns:
*   0 on success
*  -1 on out of range
*/
int timestamp2tm(long long dt, int* tzp, struct pg_tm* tm, long long* fsec)
{
	long long date;
	long long time;
	long long utime;

	time = dt;

	TMODULO(time, date, INT64CONST(86400000000));

	if (time < INT64CONST(0)) 
	{
		time += INT64CONST(86400000000);
		date -= 1;
	}

	/* add offset to go from J2000 back to standard Julian date */
	date += POSTGRES_EPOCH_JDATE;

	/* Julian day routine does not work for negative Julian days */
	if (date < 0 || date >(long long)INT_MAX)
		return -1;

	j2date((int)date, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
	dt2time(time, &tm->tm_hour, &tm->tm_min, &tm->tm_sec, fsec);

	tm->tm_isdst = -1;
	tm->tm_gmtoff = 0;
	tm->tm_zone = NULL;

	return 0;
}

/*====================================================================================================================================================*/

/*
* timestamp2tm() - Convert timestamp data type to POSIX time structure.
*
* Note that year is _not_ 1900-based, but is an explicit full value.
* Also, month is one-based, _not_ zero-based.
* Returns:
*   0 on success
*  -1 on out of range
*/

/*
 * interval2tm()
 * Convert a interval data type to a tm structure.
 */
int interval2tm(long long time, int months, struct pg_tm * tm, long long *fsec)
{
  tm->tm_year = months / 12;
  tm->tm_mon = months % 12;

  tm->tm_mday = (time / INT64CONST(86400000000));
  time -= (tm->tm_mday * INT64CONST(86400000000));
  tm->tm_hour = (time / INT64CONST(3600000000));
  time -= (tm->tm_hour * INT64CONST(3600000000));
  tm->tm_min = (time / INT64CONST(60000000));
  time -= (tm->tm_min * INT64CONST(60000000));
  tm->tm_sec = (time / INT64CONST(1000000));
  *fsec = (time - (tm->tm_sec * INT64CONST(1000000)));

  return 0;
}

/*====================================================================================================================================================*/

void dt2time(long long jd, int *hour, int *min, int *sec, long long *fsec)
{
	long long time;

	time = jd;

	*hour = (time / INT64CONST(3600000000));
	time -= ((*hour) * INT64CONST(3600000000));
	*min = (time / INT64CONST(60000000));
	time -= ((*min) * INT64CONST(60000000));
	*sec = (time / INT64CONST(1000000));
	*fsec = (time - (*sec * INT64CONST(1000000)));

	return;
}

/*====================================================================================================================================================*/

/* time2tm()
* Convert time data type to POSIX time structure.
* For dates within the system-supported time_t range, convert to the
*  local time zone. If out of this range, leave as GMT. - tgl 97/05/27
*/
int time2tm(long long time, struct pg_tm* tm, long long* fsec)
{
	// TIME value should not be negative or over MAX_TIME_VALUE.
	// Raise error in the caller function when appropriate.
	if (time < 0 || time >= MAX_TIME_VALUE) {
		memset(tm, '\0', sizeof(struct pg_tm));
		return -1;
	}

	tm->tm_hour = (time / INT64CONST(3600000000));
	time -= (tm->tm_hour * INT64CONST(3600000000));
	tm->tm_min = (time / INT64CONST(60000000));
	time -= (tm->tm_min * INT64CONST(60000000));
	tm->tm_sec = (time / INT64CONST(1000000));
	time -= (tm->tm_sec * INT64CONST(1000000));

	*fsec = time;

	return 0;
}

/*====================================================================================================================================================*/

void TrimTrailingZeros(char *str, int *plen)
{
	int			len = *plen;

	/* chop off trailing zeros... but leave at least 2 fractional digits */
	while ((*(str + len - 1) == '0')
		&& (*(str + len - 3) != '.'))
	{
		--len;
		*(str + len) = '\0';
	}

	*plen = len;
}

/*====================================================================================================================================================*/

int getInt32FromBinary(char *pColData, int idx)
{
	return
		((pColData[idx + 0] & 255) << 24)
		+ ((pColData[idx + 1] & 255) << 16)
		+ ((pColData[idx + 2] & 255) << 8)
		+ ((pColData[idx + 3] & 255));

}

/*====================================================================================================================================================*/

long long getInt64FromBinary(char *pColData, int idx)
{
	return ((long long)(pColData[idx + 0] & 255) << 56)
		+ ((long long)(pColData[idx + 1] & 255) << 48)
		+ ((long long)(pColData[idx + 2] & 255) << 40)
		+ ((long long)(pColData[idx + 3] & 255) << 32)
		+ ((long long)(pColData[idx + 4] & 255) << 24)
		+ ((long long)(pColData[idx + 5] & 255) << 16)
		+ ((long long)(pColData[idx + 6] & 255) << 8)
		+ (pColData[idx + 7] & 255);
}

StringMap createCaseInsensitiveMap() {
  static auto comp = [](std::string stringA, std::string stringB) {
    transform(stringA.begin(), stringA.end(), stringA.begin(), toupper);
    transform(stringB.begin(), stringB.end(), stringB.begin(), toupper);
    return stringA < stringB;
  };
  return StringMap(comp);
}

StringMap parseConnectionString(const std::string &connStr) {
  static std::map<std::string, StringMap> cache;
  if (auto it = cache.find(connStr); it != cache.end()) {
     return it->second;
  }
  StringMap res = createCaseInsensitiveMap();
  auto IsWstrWhitespace = [](const char in_char) -> bool {
    switch (in_char) {
      case ' ':
      case '\v':
      case '\n':
      case '\t':
      case '\r':
      case '\f': {
        return true;
      }
    }
    return false;
  };  // end of IsWstrWhitespace function

  auto SkipWhitespace = [&IsWstrWhitespace](const char *&p) -> void {
    while (IsWstrWhitespace(*p)) {
      ++p;
    }
  };  // end of SkipWhitespace function

  const std::string DRIVER_STR("DRIVER");
  const std::string DSN_STR("DSN");
  const char *currentPos = connStr.c_str();
  const char *keyIndex = NULL;
  uint16_t keyLength = 0;
  const char *usrInputValueIndex = NULL;
  uint16_t usrInputValueLength = 0;
  bool driverOrDSNFound = false;
  bool isDriverFirst = false;

  std::vector<char> valueBuff;  // kept for odbc1.x compatibility
  valueBuff.reserve(connStr.length() + 1);

  // Main while loop
  // Scan through the string.
  while ('\0' != *currentPos) {
    // Skip leading white space.
    SkipWhitespace(currentPos);

    if (';' == *currentPos) {
      // Empty key/value pair, skip it.
      ++currentPos;
      continue;
    }

    // Initialize key information.
    keyIndex = currentPos;
    keyLength = 0;

    // Measure key.
    while (('\0' != *currentPos) && ('=' != *currentPos)) {
      keyLength++;
      currentPos++;
    }

    // Check for an error.
    if (('\0' == *currentPos) || (0 == keyLength)) {
      RS_LOG_ERROR("RSUTIL", "Error parsing Connectionstring: Key parsing finished too early %d:%d",
                   (int)keyLength, (int)(*currentPos));
      return StringMap();
    }

    // Skip = sign.
    currentPos++;

    // Skip white space before value
    SkipWhitespace(currentPos);

    // Copy the key and value and convert to upper case.
    std::string keyStr(keyIndex, keyLength);
    trim(keyStr);

    if (!driverOrDSNFound) {
      if (keyStr == DRIVER_STR) {
        driverOrDSNFound = true;
        isDriverFirst = true;
      } else if (keyStr == DSN_STR) {
        driverOrDSNFound = true;
        isDriverFirst = false;
      }
    }

    // Initialize value information.
    usrInputValueIndex = currentPos;

    char charToLook = ';';
    usrInputValueLength = 0;
    valueBuff.clear();

    if ('{' == (*currentPos)) {
      charToLook = '}';
      ++usrInputValueLength;
      ++currentPos;
    }

    // Measure value.
    // There may be embedded blanks, allow them here, and
    // count them as part of the value length.
    while ('\0' != *currentPos) {
      if (('}' == charToLook) && ('}' == *currentPos) &&
          ('}' == *(currentPos + 1))) {
        valueBuff.push_back('}');
        currentPos += 2;
        usrInputValueLength += 2;
      } else if (charToLook == *currentPos) {
        break;
      } else {
        valueBuff.push_back(*currentPos);
        ++currentPos;
        ++usrInputValueLength;
      }
    }

    if ('}' == charToLook) {
      if ('\0' == *currentPos) {
        printf(
            "Error parsing Connectionstring: Braced value parsing "
            "finished too early\n");
        return StringMap();
      }
      ++usrInputValueLength;
      ++currentPos;
    }
    valueBuff.push_back('\0');

    // Save the key/value pair.
    /*
    To comply with Windows Driver Manager's behavior,
    the last repetition counts as the final value of an attribute.
    if (res.count(keyStr) == 0)
    */
    {
      std::string valueVariant(valueBuff.data(),
                               valueBuff.size() - 1);  // processed version
      res[keyStr] = trim(valueVariant);
    }

    // Skip whitespace.
    SkipWhitespace(currentPos);

    // Skip a semi-colon.
    if (';' == *currentPos) {
      currentPos++;
    }

    // Skip whitespace for early detection of EOL
    SkipWhitespace(currentPos);
  }  // mail while loop
  cache[connStr] = res;
  return res;
}

/*====================================================================================================================================================*/

#ifdef WIN32
char *base64Password(const unsigned char *input, int length) {
	const int pl = 4 * ((length + 2) / 3);
	char *output = (char *)calloc(pl + 1, 1); //+1 for the terminating null that EVP_EncodeBlock adds on
	const int ol = EVP_EncodeBlock((unsigned char *)output, input, length);
	if (ol != pl) 
	{ 
//		fprintf(stderr, "Encode predicted %d but we got %d\n", pl, ol); 
	}
	return output;
} 

/*====================================================================================================================================================*/

unsigned char *decode64Password(const char *input, int length) {
	const int pl = 3 * length / 4;
	unsigned char *output = (unsigned char *)calloc(pl + 1, 1);
	const int ol = EVP_DecodeBlock(output, (const unsigned char *)input, length);
	if (pl != ol) 
	{ 
//		fprintf(stderr, "Decode predicted %d but we got %d\n", pl, ol); 
	}
	return output;
}
#endif // WIN32
/*====================================================================================================================================================*/

bool isDatabaseMetadaCurrentOnly(RS_STMT_INFO *pStmt) {
    bool res = true; // true by default
    RS_CONN_INFO *pConn = pStmt->phdbc;
    if (pConn->pConnectProps) {
        res = (0 != pConn->pConnectProps->iDatabaseMetadataCurrentDbOnly);
    }
    return res;
}

bool getLibpqParameterStatus(RS_STMT_INFO *pStmt, const std::string &param,
                             const std::string &trueValue,
                             const std::vector<std::string> &validValues,
                             const bool defaultStatus) {

    RS_CONN_INFO *pConn = pStmt->phdbc;

    // sanity check
    auto it = std::find(validValues.begin(), validValues.end(), trueValue);
    if (it == validValues.end()) {
        throw ExceptionInvalidParameter(
            "Invalid expected parameter value for '" + param +
            "':" + trueValue);
    }

    // Try to get the param value
    char *paramValueStr = libpqParameterStatus(pConn, param.c_str());

    // param not available, return early
    if (!paramValueStr) {
        return defaultStatus;
    }

    // Is the value valid
    it = std::find(validValues.begin(), validValues.end(),
                   std::string(paramValueStr));
    if (it == validValues.end()) {
        throw ExceptionInvalidParameter("Invalid server parameter value for '" +
                                        param +
                                        "':" + std::string(paramValueStr));
    }
    // check and return the 'true'/'false' condition
    return *it == trueValue;
}

ExceptionInvalidParameter::ExceptionInvalidParameter(const std::string &message)
    : std::invalid_argument(message) {
    RS_LOG_ERROR("RSUTIL", "%s", message.c_str());
}

bool isEmptyString(SQLCHAR *str) { return str && *str == '\0'; }

bool isNullOrEmptyString(SQLCHAR *str) { return !str || *str == '\0'; }

std::string char2String(const unsigned char *str) {
    return std::string(reinterpret_cast<const char *>(str));
}

std::string_view char2StringView(const unsigned char *str) {
    return std::string_view(reinterpret_cast<const char *>(str));
}

int showDiscoveryVersion(RS_STMT_INFO *pStmt) {
    RS_CONN_INFO *pConn = pStmt->phdbc;
    char *paramValueStr = libpqParameterStatus(pConn, "show_discovery");

    // param not available, return early
    if (!paramValueStr) {
        RS_LOG_DEBUG("RSUTIL",
                    "showDiscoveryVersion version 0");
        return 0;
    } else {
        try {
            RS_LOG_DEBUG("RSUTIL",
                    "showDiscoveryVersion version %d",
                    std::stoi(paramValueStr));
            return std::stoi(paramValueStr);
        } catch (const std::invalid_argument &e) {
            throw ExceptionInvalidParameter(
                "Invalid server parameter value for 'show_discovery' : " +
                std::string(paramValueStr));
        }
    }
}

SQLRETURN validateStatementForCatalogFunction(SQLHSTMT phstmt) {
    // Validate statement handle
    if (!VALID_HSTMT(phstmt)) {
        return SQL_INVALID_HANDLE;
    }

    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;

    // Check if a cursor is currently open on the statement
    // A cursor is considered open if the statement is in EXECUTE state (RS_EXECUTE_STMT = 2)
    if (pStmt->iStatus == RS_EXECUTE_STMT) {
        // Add SQLSTATE 24000 - Invalid cursor state
        addError(&pStmt->pErrorList,
                 "24000",
                 "Invalid cursor state: A cursor is open on the statement",
                 0,
                 NULL);
        return SQL_ERROR;
    }
    return SQL_SUCCESS;
}

bool getCaseSensitive(RS_STMT_INFO *pStmt) {
    RS_CONN_INFO *pConn = pStmt->phdbc;
    char *paramValueStr = libpqParameterStatus(pConn, "case_sensitive");

    if (paramValueStr && (strcmp(paramValueStr, "on") == 0)) {
        return true;
    } else {
        return false;
    }
}

// Helper function to retrieve current connected database name
std::string getDatabase(RS_STMT_INFO *pStmt) {
    RS_CONNECT_PROPS_INFO *connection = pStmt->phdbc->pConnectProps;
    if (connection == NULL) {
        return NULL;
    }
    return connection->szDatabase;
}

/**
 * @brief Internal helper function to get column index from column name.
 *
 * This function is for metadata API internal use only.
 *
 * @param pStmt Pointer to the statement information structure
 * @param columnName Column name to look up (must be lowercase)
 * @return int Column index (1-based) if found, -1 if not found
 */
int getIndex(RS_STMT_INFO *pStmt, std::string columnName) {
    auto it = pStmt->pResultHead->columnNameIndexMap.find(columnName);
    if (it != pStmt->pResultHead->columnNameIndexMap.end()) {
        return it->second;
    }

    // If not found, try with uppercase
    std::string upperName = columnName;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    it = pStmt->pResultHead->columnNameIndexMap.find(upperName);
    if (it != pStmt->pResultHead->columnNameIndexMap.end()) {
        return it->second;
    }

    RS_LOG_ERROR("RSUTIL", "Column %s not found in column index map",
        columnName.c_str());
    return -1;
}

// Helper function to check if catalog name was SQL_ALL_CATALOGS
bool isSqlAllCatalogs(SQLCHAR *pCatalogName, SQLSMALLINT cbCatalogName){
    return pCatalogName && ((cbCatalogName == SQL_NTS && _stricmp((char *)pCatalogName, SQL_ALL_CATALOGS) == 0) || (cbCatalogName != SQL_NTS && _strnicmp((char *)pCatalogName, SQL_ALL_CATALOGS, cbCatalogName) == 0)); 	
}

// Helper function to check if schema name was SQL_ALL_SCHEMAS
bool isSqlAllSchemas(SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName){
    return pSchemaName && ((cbSchemaName == SQL_NTS && _stricmp((char *)pSchemaName, SQL_ALL_SCHEMAS) == 0) || (cbSchemaName != SQL_NTS && _strnicmp((char *)pSchemaName, SQL_ALL_SCHEMAS, cbSchemaName) == 0));
}

// Helper function to check if table type name was SQL_ALL_TABLE_TYPES
bool isSqlAllTableTypes(SQLCHAR *pTableType, SQLSMALLINT cbTableType){
    return pTableType && ((cbTableType == SQL_NTS && _stricmp((char *)pTableType, SQL_ALL_TABLE_TYPES) == 0) || (cbTableType != SQL_NTS && _strnicmp((char *)pTableType, SQL_ALL_TABLE_TYPES, cbTableType) == 0));
}

std::string escapedFilter(const std::string& input)
{
    std::string output;
    output.reserve(input.length() * 2); // Allocate extra space to avoid reallocations

    for (char c : input)
    {
        if (c == '\'')
        {
            output += "''"; // Double the single quote
        }
        else
        {
            output += c;
        }
    }

    return output;
}

/*====================================================================================================================================================*/

SQLRETURN copySqlwForClient(void *dst, const void *src, size_t totalCharsNeeded,
                            size_t cchLen, SQLLEN *pcbLen, size_t *copiedChars,
                            size_t charSize) {
    if (pcbLen) {
        if (totalCharsNeeded <= (std::numeric_limits<SQLLEN>::max)() / charSize) {
            *pcbLen = static_cast<SQLLEN>(totalCharsNeeded * charSize);
        } else {
            *pcbLen = (std::numeric_limits<SQLLEN>::max)();
            /*
                Even if it is length query we return error:
                - Client calls with dst=NULL to query required buffer size
                - Function detects overflow, caps *pcbLen to SQLLEN::max,
               returns SQL_SUCCESS
               - Client allocates SQLLEN::max bytes, believing it's
               sufficient
               - Client calls again with allocated buffer
               - Data still doesn't fit because actual size > SQLLEN::max
            */
            RS_LOG_ERROR(
                "RSUTIL",
                "totalCharsNeeded too large to copy to Client buffer:%zu",
                totalCharsNeeded);
            return SQL_ERROR;
        }
    }

    if (!dst || cchLen == 0) {
        if (copiedChars) {
            *copiedChars = 0;
        }
        return SQL_SUCCESS; // Length query
    }

    const bool hasRoomForAll =
        (totalCharsNeeded < SIZE_MAX && cchLen >= totalCharsNeeded + 1);
    const size_t srcCharsToCopy =
        hasRoomForAll ? totalCharsNeeded : (cchLen - 1);

    copyAndTerminateSqlwchar(dst, cchLen, src, srcCharsToCopy, charSize,
                             copiedChars);

    return hasRoomForAll ? SQL_SUCCESS : SQL_SUCCESS_WITH_INFO;
}

/*====================================================================================================================================================*/

void setNthSqlwcharNull(void *dst, size_t charIndex) {
    if (!dst)
        return;
    size_t size = sizeofSQLWCHAR();
    if (charIndex > SIZE_MAX / size) {
        RS_LOG_ERROR("RSUTIL", "setNthSqlwcharNull: charIndex out of range:%zu",
                     charIndex);
        return;
    }
    void *ptr = static_cast<char *>(dst) + (charIndex * size);
    std::memset(ptr, 0, size);
}

/*====================================================================================================================================================*/

void setFirstSqlwcharNull(void *dst) {
    if (!dst) {
        RS_LOG_WARN("RSUTIL", "setFirstSqlwcharNull: dst is NULL");
        return;
    }
    std::memset(dst, 0, sizeofSQLWCHAR());
}

/*====================================================================================================================================================*/

bool isFirstSqlwcharNull(const void *src) {
    if (!src)
        return true;
    size_t size = sizeofSQLWCHAR();
    if (size == 2) {
        return *static_cast<const uint16_t *>(src) == 0;
    } else if (size == 4) {
        return *static_cast<const uint32_t *>(src) == 0;
    }
    return std::memcmp(src, "\0\0\0\0", size) == 0;
}

/*====================================================================================================================================================*/

ConversionResult
convertWCharParamWithTruncCheck(SQLWCHAR *pwParam, SQLSMALLINT cchParam,
                                char *szParam, size_t bufLen,
                                const char *paramName, const char *logTag,
                                RS_STMT_INFO *pStmt, size_t *copiedChars) {
    if (!copiedChars || !pStmt || !szParam) {
        RS_LOG_ERROR(
            (logTag ? logTag : "RSUTIL"),
            "Insufficient data submitted for unicode conversion: %s %s %s",
            (copiedChars ? "" : "Invalid Char Count output, "),
            (pStmt ? "" : "Invalid Statement, "),
            (szParam ? "" : "Invalid String output"));
        if (pStmt) {
            addError(&pStmt->pErrorList, (char *)"HY000",
                     "Insufficient data submitted for unicode conversion", 0,
                     NULL);
        }
        return CONVERSION_ERROR;
    }

    // Handle NULL or empty input - these are valid for catalog functions
    if (!pwParam || cchParam == 0 || (cchParam == SQL_NTS && sqlwcsnlen_cap(pwParam, 1) == 0)) {
        *copiedChars = 0;
        if (bufLen > 0) {
            szParam[0] = 0;
        } else {
            RS_LOG_ERROR((logTag ? logTag : "RSUTIL"),
                         "Invalid Buffer length %zu for %s",
                         bufLen, (paramName ? paramName : "UNKNOWN_PARAM"));
            addError(&pStmt->pErrorList, (char *)"HY090",
                     "Invalid string or buffer length", 0, NULL);
            return CONVERSION_ERROR;
        }
        return CONVERSION_SUCCESS;
    }

    // At this point: pwParam is non-NULL and indicates non-empty content
    size_t totalNeeded = 0;
    *copiedChars =
        sqlwchar_to_utf8_char(pwParam, cchParam, szParam, bufLen, &totalNeeded);
    const char *paramName_ = (paramName ? paramName : "UNKNOWN_PARAM");
    // Detect conversion failure: non-empty input produced zero output
    if (*copiedChars == 0 && totalNeeded == 0) {
        RS_LOG_ERROR((logTag ? logTag : "RSUTIL"),
                     "Invalid Unicode sequence in %s", paramName_);
        char errorMsg[256] = {0};
        snprintf(errorMsg, sizeof(errorMsg), "Invalid Unicode sequence in %s",
                 paramName_);
        addError(&pStmt->pErrorList, (char *)"HY000", errorMsg, 0, NULL);
        return CONVERSION_ERROR;
    }

    // Check for truncation
    if (*copiedChars < totalNeeded) {
        RS_LOG_WARN((logTag ? logTag : "RSUTIL"),
                    "Buffer too small for %s. Truncated from %zu to %zu",
                    paramName_, totalNeeded, *copiedChars);
        char errorMsg[256] = {0};
        snprintf(errorMsg, sizeof(errorMsg),
                 "String data for %s, right truncated", paramName_);
        addError(&pStmt->pErrorList, (char *)"01004", errorMsg, 0, NULL);
        return CONVERSION_TRUNCATED;
    }

    return CONVERSION_SUCCESS;
}

DriverManagerInfo detectDriverManager() {
    RS_LOG_INFO("RSUTIL", "Detecting Driver Manager ... ");
    DriverManagerInfo info;

#ifdef WIN32
    HMODULE handle = GetModuleHandle(NULL);
    if (!handle)
        return info;

    const auto GetSymbol = [=](const char *symbol) {
        return GetProcAddress(handle, symbol);
    };
    const auto HasSymbol = [=](const char *symbol) {
        return !!GetSymbol(symbol);
    };

    if (HasSymbol("SQLDriverConnectW")) {
        info.family = DriverManagerInfo::WINDOWS;
        info.version = "Windows";
    }
#else
    void *handle = dlopen(NULL, RTLD_LAZY);
    if (!handle)
        return info;
    RS_LOG_TRACE("RSUTIL", "detectDriverManager !handle ");
    const auto GetSymbol = [=](const char *symbol) {
        return dlsym(handle, symbol);
    };
    const auto HasSymbol = [=](const char *symbol) {
        RS_LOG_TRACE("RSUTIL", "detectDriverManager HasSymbol ");
        return !!GetSymbol(symbol);
    };

    if (const char *version = static_cast<char *>(GetSymbol("iodbc_version"))) {
        info.family = DriverManagerInfo::IODBC;
        info.version = version;
    } else if (HasSymbol("uodbc_get_stats")) {
        info.family = DriverManagerInfo::UNIXODBC;
        info.version = HasSymbol("ODBCGetTryWaitValue") ? "2.3.x+" : "2.2.x";
    } else if (HasSymbol("odbcapi_symtab")) {
        info.family = DriverManagerInfo::IODBC;
        info.version = "Unknown";
    }
#endif

    return info;
}

bool isIODBC() {
    static std::optional<bool> isIODBC_;
    static std::once_flag flag;
#ifdef WIN32
    return false; // Windows doesn't use unixODBC
#else
    std::call_once(flag, [&]() {
        DriverManagerInfo dmInfo = detectDriverManager();
        RS_LOG_INFO("RSUTIL", "Driver Manager Family=%d", dmInfo.family);
        isIODBC_ = dmInfo.family == DriverManagerInfo::IODBC;
    });
    return *isIODBC_;
#endif
}

// Integer to integer conversions
SQLRETURN rsIntToTinyint(long long value, signed char* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<signed char>(value, result, errorList);
}

SQLRETURN rsIntToUTinyint(long long value, unsigned char* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<unsigned char>(value, result, errorList);
}

SQLRETURN rsIntToShort(long long value, short* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<short>(value, result, errorList);
}

SQLRETURN rsIntToUShort(long long value, unsigned short* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<unsigned short>(value, result, errorList);
}

SQLRETURN rsIntToInt(long long value, int* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<int>(value, result, errorList);
}

SQLRETURN rsIntToUInt(long long value, unsigned int* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<unsigned int>(value, result, errorList);
}

SQLRETURN rsIntToBigInt(long long value, long long* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<long long>(value, result, errorList);
}

SQLRETURN rsIntToUBigInt(long long value, unsigned long long* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<unsigned long long>(value, result, errorList);
}

SQLRETURN rsIntToFloat(long long value, float* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<float>(value, result, errorList);
}

SQLRETURN rsIntToDouble(long long value, double* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<double>(value, result, errorList);
}

// Float conversion functions
SQLRETURN rsFloatToTinyInt(double value, signed char* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<signed char>(value, result, errorList);
}

SQLRETURN rsFloatToUTinyInt(double value, unsigned char* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<unsigned char>(value, result, errorList);
}

SQLRETURN rsFloatToShort(double value, short* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<short>(value, result, errorList);
}

SQLRETURN rsFloatToUShort(double value, unsigned short* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<unsigned short>(value, result, errorList);
}

SQLRETURN rsFloatToInt(double value, int* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<int>(value, result, errorList);
}

SQLRETURN rsFloatToUInt(double value, unsigned int* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<unsigned int>(value, result, errorList);
}

SQLRETURN rsFloatToBigInt(double value, long long* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<long long>(value, result, errorList);
}

SQLRETURN rsFloatToUBigInt(double value, unsigned long long* result, RS_ERROR_INFO** errorList) {
    return rsFloatConvertWithTruncation<unsigned long long>(value, result, errorList);
}

SQLRETURN rsDoubleToFloat(double value, float* result, RS_ERROR_INFO** errorList) {
    return rsGenericConvert<float>(value, result, errorList);
}

// Helper function for SQL_C_BIT conversions from integer
SQLRETURN rsIntToBit(long long value, unsigned char* result, RS_ERROR_INFO** errorList) {
    if (!result) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "HY009: Invalid use of null pointer");
            addError(errorList, "HY009", "Invalid use of null pointer", 0, NULL);
        }
        return SQL_ERROR;
    }

    // Per ODBC spec for SQL_C_BIT from integer:
    // - If data is 0 or 1: Return data
    // - If data is < 0 or > 1: Numeric value out of range, SQLSTATE 22003
    if (value < 0 || value > 1) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
            addError(errorList, "22003", "Numeric value out of range", 0, NULL);
        }
        return SQL_ERROR;
    }

    *result = (value == 1) ? 1 : 0;
    return SQL_SUCCESS;
}

// Helper function for SQL_C_BIT conversions from floating point
SQLRETURN rsFloatToBit(double value, unsigned char* result, RS_ERROR_INFO** errorList) {
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

    // Per ODBC spec for SQL_C_BIT:
    // - If data is 0 or 1: Return data
    // - If data is > 0, < 1 or > 1, < 2: Truncate fractional part, SQLSTATE 01S07
    // - If data is < 0 or >= 2: Return SQL_ERROR with SQLSTATE 22003
    if (value < 0.0 || value >= 2.0) {
        if (errorList) {
            RS_LOG_ERROR("RSUTIL", "22003: Numeric value out of range");
            addError(errorList, "22003", "Numeric value out of range", 0, NULL);
        }
        return SQL_ERROR;
    }

    if (value == 0.0 || value == 1.0) {
        // Exact match - no truncation
        *result = (value == 1.0) ? 1 : 0;
        return SQL_SUCCESS;
    } else {
        // Value between 0 and 2, but not exactly 0 or 1
        // Truncate to integer value (0 or 1)
        *result = (value < 1.0) ? 0 : 1;
        if (errorList) {
            RS_LOG_DEBUG("RSUTIL", "01S07: Fractional truncation");
            addError(errorList, "01S07", "Fractional truncation", 0, NULL);
        }
        return SQL_SUCCESS_WITH_INFO;
    }
}

bool isTimePortionZero(const TIMESTAMP_STRUCT *ts) {
    if (!ts) {
        // NULL timestamp doesn't have a zero time portion
        return false;
    }
    return ts->hour == 0 && ts->minute == 0 && ts->second == 0 && ts->fraction == 0;
}
