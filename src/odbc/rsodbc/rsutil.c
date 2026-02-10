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
#include <vector>
#include <cctype>
#include <regex>
#include <string>
#include <iostream>
#include <codecvt>
#include <optional>
#include <mutex>

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
    }
    static thread_local char buf[32];
    snprintf(buf, sizeof(buf), "unknown(%d)", value);
    return buf;
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
        NAME(SQL_C_FLOAT)
        NAME(SQL_C_DOUBLE)
        NAME(SQL_C_BIT)
        NAME(SQL_C_TINYINT)
        NAME(SQL_C_STINYINT)
        NAME(SQL_C_UTINYINT)
        NAME(SQL_C_SBIGINT)
        NAME(SQL_C_UBIGINT)
        NAME(SQL_C_BINARY)
        NAME(SQL_C_TYPE_DATE)
        NAME(SQL_C_TYPE_TIME)
        NAME(SQL_C_TYPE_TIMESTAMP)
        NAME(SQL_C_NUMERIC)
        NAME(SQL_C_DEFAULT)
        NAME(SQL_C_DATE)
        NAME(SQL_C_TIME)
        NAME(SQL_C_TIMESTAMP)
        NAME(SQL_C_INTERVAL_YEAR_TO_MONTH)
        NAME(SQL_C_INTERVAL_DAY_TO_SECOND)
    }
    static thread_local char buf[32];
    snprintf(buf, sizeof(buf), "unknown(%d)", value);
    return buf;
#undef NAME
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

#ifdef WIN32
int intervald2s_out_wchar(INTERVALD2S_STRUCT* d2s, SQLWCHAR *buf, int buf_len);
int intervaly2m_out_wchar(INTERVALY2M_STRUCT* y2m, SQLWCHAR *buf, int buf_len);
int intervaly2m_out_wchar(INTERVALD2S_STRUCT* d2s, SQLWCHAR *buf, int buf_len);
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
        strncpy(pError->szSqlState,pNativeSqlState,MAX_SQL_STATE_LEN - 1);
    else
    if(pSqlState != NULL)
        strncpy(pError->szSqlState,pSqlState,MAX_SQL_STATE_LEN - 1);

    pError->szSqlState[MAX_SQL_STATE_LEN - 1] = '\0';

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

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Copy small string data.
//
SQLRETURN copyStrDataSmallLen(const char *pSrc, SQLINTEGER iSrcLen, char *pDest, SQLSMALLINT cbLen, SQLSMALLINT *pcbLen)
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
            }
        }
        else
        {
            if(cbLen > 0)
                pDest[0] = '\0';
        }
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

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

/*====================================================================================================================================================*/

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

    int iConversion =
        getRsVal(pColData, iColDataLen, hSQLType, &rsVal, hType, format,
                    pDescRec, hRsSpecialType, FALSE);

    RS_LOG_TRACE("RSUTIL",
                 "convertSQLDataToCData C-Type=%s SQL-Type=%s "
                 "format=%d iColDataLen=%d iConversion=%d",
                 cTypeNameMap(hType), sqlTypeNameMap(hSQLType), format,
                 iColDataLen, iConversion);

    switch(hType)
    {
        case SQL_C_CHAR:
        {
            switch(hSQLType)
            {
                case SQL_CHAR:
                {
                    rc = copyStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    break;
                }

				case SQL_VARCHAR:
				{
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

				case SQL_LONGVARCHAR:
				{
					rc = copyStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
						if (iColDataLen > 0)
							len = timestamp_out(rsVal.llVal, (char *)pBuf, cbLen, NULL);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;

					}

					break;
				}

				case SQL_INTERVAL_YEAR_TO_MONTH:
				case SQL_INTERVAL_DAY_TO_SECOND:
				{
					if (IS_TEXT_FORMAT(format)) {
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					}
					else
					{
						if (iColDataLen > 0)
							if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH)
								len = intervaly2m_out(&(rsVal.y2mVal), (char *)pBuf, cbLen);
							else
								len = intervald2s_out(&(rsVal.d2sVal), (char *)pBuf, cbLen);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
					}

					break;
				}

				case SQL_NUMERIC:
				case SQL_DECIMAL:
				{
					if (IS_TEXT_FORMAT(format))
					{
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
					{
						rc = copyStrDataBigLen(pStmt, pColData, iColDataLen,(char *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					}
					else
					{
						if (iColDataLen > 0)
							len = time_out(rsVal.llVal, (char *)pBuf, cbLen, NULL);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
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
				{
                    rc = copyWStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen,(SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
                    break;
                }

                case SQL_VARCHAR: {
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


				case SQL_LONGVARCHAR:
				{
					rc = copyWStrDataBigLen(pStmt, rsVal.pcVal, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen,(SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
					else
					{
#ifdef WIN32
						if (iColDataLen > 0)
							if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH)
								len = intervaly2m_out_wchar(&(rsVal.y2mVal), (SQLWCHAR *)pBuf, cbLen);
							else
								len = intervald2s_out_wchar(&(rsVal.d2sVal), (SQLWCHAR *)pBuf, cbLen);
						else
							len = 0;

						if (pcbLenInd)
							*pcbLenInd = len;
#endif
#if defined LINUX 
						if (iColDataLen > 0) {
							char tempBuf[MAX_TEMP_BUF_LEN];

							if (hSQLType == SQL_INTERVAL_YEAR_TO_MONTH)
								len = intervaly2m_out(&(rsVal.y2mVal), (char *)tempBuf, sizeof(tempBuf));
							else
								len = intervald2s_out(&(rsVal.d2sVal), (char *)tempBuf, sizeof(tempBuf));
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

				case SQL_NUMERIC:
				case SQL_DECIMAL:
				{
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					if (IS_TEXT_FORMAT(format))
						rc = copyWStrDataBigLen(pStmt, pColData, iColDataLen, (SQLWCHAR *)pBuf, cbLen, cbLenOffset, pcbLenInd);
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
					rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_CHAR:
                case SQL_VARCHAR:
				{
					// Convert char to short
					getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put short into app buf
					rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);

					break;
				}

                case SQL_BIT:
                case SQL_TINYINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to short
						getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}

					// Now put short into app buf
					rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to short
						getRsVal(pColData, iColDataLen, SQL_SMALLINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);
						rsVal.hVal = (short)atoi(pNumData);
					}

                    // Now put short into app buf
                    rc = getShortData(rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    rc = getShortData((short)rsVal.iVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_BIGINT:
                {
                    rc = getShortData((short)rsVal.llVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_REAL:
                {
                    rc = getShortData((short)rsVal.fVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    rc = getShortData((short)rsVal.dVal, pBuf, pcbLenInd);

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
                    rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_VARCHAR:
				{
					// Convert char to int
					getRsVal(pColData, iColDataLen, SQL_INTEGER, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put int into app buf
					rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);

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
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to int
						getRsVal(pColData, iColDataLen, SQL_INTEGER, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}

					// Now put int into app buf
					rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);

					break;
				}

                case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to int
						getRsVal(pColData, iColDataLen, SQL_INTEGER, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);
						rsVal.iVal = atoi(pNumData);
					}

                    // Now put int into app buf
                    rc = getIntData(rsVal.iVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_SMALLINT:
                {
                    rc = getIntData((int)rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_BIGINT:
                {
                    rc = getIntData((int)rsVal.llVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_REAL:
                {
                    rc = getIntData((int)rsVal.fVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    rc = getIntData((int)rsVal.dVal, pBuf, pcbLenInd);

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
                    rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_VARCHAR:
				{
					// Convert char to long long
					getRsVal(pColData, iColDataLen, SQL_BIGINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put long long into app buf
					rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);

					break;
				}

                case SQL_BIT:
                case SQL_TINYINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to long long
						getRsVal(pColData, iColDataLen, SQL_BIGINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}

					// Now put long long into app buf
					rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to long long
						getRsVal(pColData, iColDataLen, SQL_BIGINT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
						SQL_NUMERIC_STRUCT *pnVal = (SQL_NUMERIC_STRUCT *)&(rsVal.nVal);
						char tempBuf[MAX_TEMP_BUF_LEN];
						char *pNumData = tempBuf;
						int num_data_len = sizeof(tempBuf);

						convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);
						sscanf(pNumData, "%lld", &(rsVal.llVal));
					}

                    // Now put long long into app buf
                    rc = getBigIntData(rsVal.llVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_SMALLINT:
                {
                    rc = getBigIntData((long long)rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    rc = getBigIntData((long long)rsVal.iVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_REAL:
                {
                    rc = getBigIntData((long long)rsVal.fVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    rc = getBigIntData((long long)rsVal.dVal, pBuf, pcbLenInd);

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
                    rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_VARCHAR:
				{
					// Convert char to float
					getRsVal(pColData, iColDataLen, SQL_REAL, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put float into app buf
					rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_BIT:
                case SQL_TINYINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to float
						getRsVal(pColData, iColDataLen, SQL_REAL, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}

					// Now put float into app buf
					rc = getFloatData(rsVal.fVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
					if (IS_TEXT_FORMAT(format))
					{
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
                    rc = getFloatData((float)rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    rc = getFloatData((float)rsVal.iVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_BIGINT:
                {
                    rc = getFloatData((float)rsVal.llVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    rc = getFloatData((float)rsVal.dVal, pBuf, pcbLenInd);

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
                    rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_VARCHAR:
				{
					// Convert char to double
					getRsVal(pColData, iColDataLen, SQL_DOUBLE, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put double into app buf
					rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_BIT:
                case SQL_TINYINT:
				{
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to double
						getRsVal(pColData, iColDataLen, SQL_DOUBLE, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}

					// Now put double into app buf
					rc = getDoubleData(rsVal.dVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
					if (IS_TEXT_FORMAT(format))
					{
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
                    rc = getDoubleData((double)rsVal.hVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    rc = getDoubleData((double)rsVal.iVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_BIGINT:
                {
                    rc = getDoubleData((double)rsVal.llVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_REAL:
                {
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
                case SQL_TINYINT:
                {
                    rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);
                    break;
                }

                case SQL_CHAR:
                case SQL_VARCHAR:
				{
					// Convert char to bit
					getRsVal(pColData, iColDataLen, SQL_BIT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put bit into app buf
					rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);

					break;
				}

				case SQL_NUMERIC:
                case SQL_DECIMAL:
                {
					if (IS_TEXT_FORMAT(format))
					{
						// Convert char to bit
						getRsVal(pColData, iColDataLen, SQL_BIT, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);
					}
					else
					{
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
					}

                    // Now put bit into app buf
                    rc = getBooleanData(rsVal.bVal, pBuf, pcbLenInd);

                    break;
                }

                case SQL_SMALLINT:
                {
                    rc = getBooleanData((rsVal.hVal != 0) ? 1 : 0, pBuf, pcbLenInd);

                    break;
                }

                case SQL_INTEGER:
                {
                    rc = getBooleanData((rsVal.iVal != 0) ? 1 : 0, pBuf, pcbLenInd);
                    break;
                }

                case SQL_BIGINT:
                {
                    rc = getBooleanData((rsVal.llVal != 0) ? 1 : 0, pBuf, pcbLenInd);
                    break;
                }

                case SQL_REAL:
                {
                    rc = getBooleanData((rsVal.fVal != 0) ? 1 : 0, pBuf, pcbLenInd);
                    break;
                }

                case SQL_FLOAT:
                case SQL_DOUBLE:
                {
                    rc = getBooleanData((rsVal.dVal != 0) ? 1 : 0, pBuf, pcbLenInd);
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

                    break;
                }

                case SQL_BIT:
                case SQL_CHAR:
                case SQL_VARCHAR:
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

                    tsVal.year  = 0;
                    tsVal.month = 0;
                    tsVal.day = 0;
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

				case SQL_VARCHAR:
				{
					if (hRsSpecialType == TIMESTAMPTZOID)
					{
						char *pTemp;
						char tempBuf[MAX_TEMP_BUF_LEN];
						char szFraction[32]; // Billionth of a second
						int fractionLen;

						rsVal.tsVal.fraction = 0;
						szFraction[0] = '\0';


						if (IS_TEXT_FORMAT(format))
						{
							pTemp = pColData;
						}
						else
						{
							char *pTimeZone = libpqParameterStatus(pStmt->phdbc, "TimeZone");
							len = timestamp_out(rsVal.llVal, (char *)tempBuf, MAX_TEMP_BUF_LEN, pTimeZone);

							pTemp = tempBuf;
						}

						// Not using  timezone value.
						sscanf(pColData, "%4hd-%2hd-%2hd %2hd:%2hd:%2hd.%s", &(rsVal.tsVal.year), 
										&(rsVal.tsVal.month), &(rsVal.tsVal.day),
							&(rsVal.tsVal.hour), &(rsVal.tsVal.minute), &(rsVal.tsVal.second),
							szFraction);

						if (szFraction[0] != '\0')
							rsVal.tsVal.fraction = atoi(szFraction);

						rc = getTimeStampData(&(rsVal.tsVal), pBuf, pcbLenInd);
					}
					else
						iConversionError = TRUE;

					break;
				}


                case SQL_BIT:
                case SQL_TINYINT:
                case SQL_CHAR:
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
                    rc = getIntervalY2MData(&(rsVal.y2mVal), pBuf, pcbLenInd);
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
                    rc = getIntervalD2SData(&(rsVal.d2sVal), pBuf, pcbLenInd);
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

				case SQL_VARCHAR:
				{
					if (hRsSpecialType == TIMETZOID)
					{
						char *pTemp;
						char tempBuf[MAX_TEMP_BUF_LEN];

						if (IS_TEXT_FORMAT(format))
						{
							pTemp = pColData;
						}
						else
						{
							len = time_out(rsVal.tzVal.time, (char *)tempBuf, MAX_TEMP_BUF_LEN, &(rsVal.tzVal.zone));
							pTemp = tempBuf;
						}

						// Not using fraction and  timezone value.
						sscanf(pTemp, "%2hd:%2hd:%2hd", &(rsVal.tVal.sqltVal.hour),
							&(rsVal.tVal.sqltVal.minute),
							&(rsVal.tVal.sqltVal.second));
						rc = getTimeData(&(rsVal.tVal), pBuf, pcbLenInd);
					}
					else
						iConversionError = TRUE;

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


					break;
				}

                case SQL_BIT:
                case SQL_TINYINT:
                case SQL_CHAR:
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
                    rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);
                    break;
                }

				case SQL_CHAR:
				case SQL_VARCHAR:
				{
					// Convert char to Numeric
					getRsVal(pColData, iColDataLen, SQL_NUMERIC, &rsVal, hType, format, pDescRec, hRsSpecialType, TRUE);

					// Now put numeric into app buf
					rc = getNumericData(&(rsVal.nVal), pBuf, pcbLenInd);

					break;
				}

				case SQL_BIT:
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
						rc = copyBinaryDataBigLen(rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, pcbLenInd);
					}
					else
					{
						// Convert HEX format to Binary
						rc = copyHexToBinaryDataBigLen(rsVal.pcVal, iColDataLen, (char *)pBuf, cbLen, pcbLenInd, cbLenOffset);
					}
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
                    rc = getIntervalY2MData(&(rsVal.y2mVal), pBuf, pcbLenInd);
                    break;
                }
                case SQL_INTERVAL_DAY_TO_SECOND:
                {
                    rc = getIntervalD2SData(&(rsVal.d2sVal), pBuf, pcbLenInd);
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

    if(iConversionError)
    {
        RS_LOG_ERROR("RSUTIL",
                     "Fetch data type conversion is not supported from "
                     "hCType=%d hSQLType=%d "
                     "format=%d iColDataLen%d iConversion=%d",
                     hType, hSQLType, format, iColDataLen, iConversion);
        char szErrMsg[MAX_ERR_MSG_LEN];

        snprintf(szErrMsg, sizeof(szErrMsg), "Fetch data type conversion is not supported from %hd SQL type to %hd C type.", hSQLType,hType);

        rc = SQL_ERROR;
        if(pStmt)
            addError(&pStmt->pErrorList,"HY000", szErrMsg, 0, NULL);
        goto error;
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
			{
				pRsVal->pcVal = pColData;
				break;
			}

            case SQL_VARCHAR:
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

			case SQL_LONGVARCHAR:
			{
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
					if (IS_TEXT_FORMAT(format) || isTextData)
					{
						if (pColData[iColDataLen - 1] == '\0')
						{
							sscanf(pColData, "%4hd-%2hd-%2hd", &(pRsVal->dtVal.year), &(pRsVal->dtVal.month), &(pRsVal->dtVal.day));
						}
						else
						{
							makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);

							sscanf(szNumBuf, "%4hd-%2hd-%2hd", &(pRsVal->dtVal.year), &(pRsVal->dtVal.month), &(pRsVal->dtVal.day));
						}
					}
					else
					{
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
                    if (IS_TEXT_FORMAT(format) || isTextData)
                    {
                        if (pColData[iColDataLen - 1] == '\0')
                        {
                            pRsVal->y2mVal = parse_intervaly2m(pColData, iColDataLen);
                        }
                        else
                        {
                            makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);
                            pRsVal->y2mVal = parse_intervaly2m(szNumBuf, strlen(szNumBuf));
                        }
                    }
                    else
                    {
                        int month = getInt32FromBinary(pColData, 0);
                        struct pg_tm tm = {0};
                        long long fsec = 0;
                        interval2tm(0, month, &tm, &fsec);
                        pRsVal->y2mVal = {(SQLUINTEGER)tm.tm_year,
                                          (SQLUINTEGER)tm.tm_mon};
                    }
                }
                else
                {
                    memset(&(pRsVal->y2mVal), '\0', sizeof(INTERVALY2M_STRUCT));
                }

                break;
            }
            case SQL_INTERVAL_DAY_TO_SECOND:
            {
                if(iColDataLen > 0)
                {
                    if (IS_TEXT_FORMAT(format) || isTextData)
                    {
                        if (pColData[iColDataLen - 1] == '\0')
                        {
                            pRsVal->d2sVal = parse_intervald2s(pColData, iColDataLen);
                    }
                    else
                    {
                        makeNullTerminateIntVal(pColData, iColDataLen, szNumBuf, MAX_NUMBER_BUF_LEN + 1);
                            pRsVal->d2sVal = parse_intervald2s(szNumBuf, strlen(szNumBuf));
                        }
                    }
                    else
                    {
                        long long time = getInt64FromBinary(pColData, 0);
                        struct pg_tm tm = {0};
                        long long fsec = 0;
                        interval2tm(time, 0, &tm, &fsec);
                        pRsVal->d2sVal = {
                            (SQLUINTEGER)tm.tm_mday, (SQLUINTEGER)tm.tm_hour,
                            (SQLUINTEGER)tm.tm_min, (SQLUINTEGER)tm.tm_sec,
                            (SQLUINTEGER)fsec};
                    }
                }
                else
                {
                    memset(&(pRsVal->d2sVal), '\0', sizeof(INTERVALD2S_STRUCT));
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

//---------------------------------------------------------------------------------------------------------igarish
// Get long data.
//
SQLRETURN getLongData(long lVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(long *)pBuf = lVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(long);

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

//---------------------------------------------------------------------------------------------------------lkumayus
// Get interval year to month data.
//
SQLRETURN getIntervalY2MData(INTERVALY2M_STRUCT *py2mVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(INTERVALY2M_STRUCT *)pBuf = *py2mVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(INTERVALY2M_STRUCT);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------lkumayus
// Get interval day to second data.
//
SQLRETURN getIntervalD2SData(INTERVALD2S_STRUCT *pd2sVal, void *pBuf,  SQLLEN *pcbLenInd)
{
    SQLRETURN rc;

    if(pBuf)
    {
        *(INTERVALD2S_STRUCT *)pBuf = *pd2sVal;
        rc = SQL_SUCCESS;
    }
    else
        rc = SQL_SUCCESS_WITH_INFO;

    if(pcbLenInd)
        *pcbLenInd = sizeof(INTERVALD2S_STRUCT);

    return rc;
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
		case SQL_LONGVARBINARY:
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
            lSize = 24; // 7
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            lSize = 53; // 15
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
            lSize = 32;
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            lSize = 64;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            lSize = 15; // hh:mm:ss + '.' + microseconds (6)
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
        case SQL_TYPE_TIME:
        case SQL_TIME:
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
		case SQL_LONGVARCHAR:
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
        {
            iDisplaySize = (hRsSpecialType == TIMETZOID) ? MAX_TIMETZOID_SIZE : iSize;
            break;
        }

		case SQL_LONGVARCHAR:
		{
			iDisplaySize =  iSize;
			break;
		}

		case SQL_LONGVARBINARY:
		{
			iDisplaySize = iSize;
			break;
		}

        case SQL_CHAR:
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
            iDisplaySize = 13;
            break;
        }

        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iDisplaySize = 22;
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
            iDisplaySize = 32;
            break;
        }

        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iDisplaySize = 64;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            iDisplaySize = 15;
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
		case SQL_LONGVARCHAR:
		{
            if(hRsSpecialType == TIMETZOID)
                pBuf[0] = '\0';
            else
                rs_strncpy(pBuf,"'",2);

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
		case SQL_LONGVARCHAR:
		{
            if(hRsSpecialType == TIMETZOID)
                pBuf[0] = '\0';
            else
                rs_strncpy(pBuf,"'",2);

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
            rs_strncpy(pBuf,"INTERVALY2M", bufLen);
            break;    
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            rs_strncpy(pBuf,"INTERVALD2S", bufLen);
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
        {
			rs_strncpy(pBuf,"CHARACTER", bufLen);
            break;
        }

        case SQL_VARCHAR:
        {
			rs_strncpy(pBuf,"CHARACTER VARYING", bufLen);
            break;
        }

		case SQL_LONGVARCHAR:
		{
			if (hRsSpecialType == SUPER)
				rs_strncpy(pBuf, "SUPER", bufLen);
			else
				rs_strncpy(pBuf, "CHARACTER VARYING", bufLen);
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
        {
            iNumPrexRadix = 10;
            break;
        }

        case SQL_REAL:
        case SQL_FLOAT:
        case SQL_DOUBLE:
        {
            iNumPrexRadix = 2;
            break;
        }

		case SQL_LONGVARBINARY:
		{
			iNumPrexRadix = 2;
			break;
		}

        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
		case SQL_LONGVARCHAR:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_SECOND:
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
		case SQL_LONGVARCHAR:
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
            iOctetSize = 8; // size of SQL_INTERVALY2M_STRUCT
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iOctetSize = 20; // size of SQL_INTERVALD2S_STRUCT
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
            iOctetSize = (iSize > 0) ? iSize + 2 : sizeof(SQL_NUMERIC_STRUCT);
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
            iOctetSize = 8; // size of SQL_INTERVALY2M_STRUCT
            break;
        }
        case SQL_C_INTERVAL_DAY_TO_SECOND:
        {
            iOctetSize = 20; // size of SQL_INTERVALD2S_STRUCT
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
		case SQL_LONGVARCHAR:
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
        case SQL_FLOAT:
        {
            iPrec = 15;
            break;
        }

        case SQL_DOUBLE:
        {
            iPrec = 53;
            break;
        }

        case SQL_TYPE_DATE:
        case SQL_DATE:
        {
            iPrec = 10;
            break;
        }

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
        {
            iPrec = 26; // 19 + dot + millionth of seconds (6)
            break;
        }

        case SQL_INTERVAL_YEAR_TO_MONTH:
        {
            iPrec = 32;
            break;
        }
        case SQL_INTERVAL_DAY_TO_SECOND:
        {
            iPrec = 64;
            break;
        }

        case SQL_TYPE_TIME:
        case SQL_TIME:
        {
            iPrec = 15; // 8 +  dot + millionth of seconds (6)
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
		case SQL_LONGVARCHAR:
		{
            iSearchable = (hRsSpecialType == TIMETZOID) ? SQL_PRED_BASIC : SQL_PRED_SEARCHABLE;
            break;
        }

        case SQL_CHAR:
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
        case SQL_VARCHAR:
        case SQL_BIT:
        case SQL_TINYINT:
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIMESTAMP:
        case SQL_TYPE_TIME:
        case SQL_DATE:
        case SQL_TIMESTAMP:
        case SQL_TIME:
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
        case SQL_VARCHAR:
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
        case SQL_VARCHAR:
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
// Convert C data into SQL data.
//
char *convertCParamDataToSQLData(RS_STMT_INFO *pStmt, char *pParamData, SQLLEN iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, 
                                  short hSQLType, short hPrepSQLType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, int *piConversionError)
{
    int iConversionError = FALSE;
    char *pcVal; 
    short hType;
    
    if(hCType == SQL_C_DEFAULT)
        hType = getDefaultCTypeFromSQLType(hSQLType, &iConversionError);
    else
        hType = hCType;

    if(iConversionError)
    {
        pcVal = NULL;
        goto error;
    }

    // This could happen when Bind parameter occurs using descriptor
    if(hSQLType == 0)
        hSQLType = hPrepSQLType;

    pcVal = getParamVal(pParamData, iParamDataLen, plParamDataStrLenInd, hType, pBindParamStrBuf, hSQLType);

    switch(hSQLType)
    {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
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
                    // We already convert these types to string
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
                    break;
                }

            } // C Type

            break;
        } // SQL_CHAR

		case SQL_LONGVARCHAR: // SUPER
		{
			switch (hType)
			{
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			{
				// We already convert these types to string
				break;
			}

			default:
			{
				iConversionError = TRUE;
				break;
			}

			} // C Type

			break;
		} // SQL_LONGVARCHAR (SUPER)

		case SQL_LONGVARBINARY: // GEOMETRY, VARBYTE, GEOGRAPHY
		{
			switch (hType)
			{
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_BINARY:
			{
				// We already convert these types to string
				break;
			}

			default:
			{
				iConversionError = TRUE;
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
                    // We already convert these types to string
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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                {
                    // We already convert these types to string
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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                {
                    // We already convert these types to string
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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                    // We already convert these types to string

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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                {
                    // We already convert these types to string

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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                    // We already convert these types to string
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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                    // We already convert these types to string
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    if(pcVal)
                    {
                        if(*pcVal == '{' && *(pcVal + 1) == 'd')
                        {
                            char *pTemp = strchr(pcVal, '}');

                            if(pTemp)
                            {
                                pcVal +=2;
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
                case SQL_C_TYPE_TIME:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                {
                    // We already convert these types to string
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    if(pcVal)
                    {
                        if(*pcVal == '{' && *(pcVal + 1) == 't' && *(pcVal + 2) == 's')
                        {
                            char *pTemp = strchr(pcVal, '}');

                            if(pTemp)
                            {
                                pcVal +=3;
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
                case SQL_C_TYPE_TIME:
                case SQL_C_TIME:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                    // We already convert these types to string
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
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
                    // We already convert these types to string
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
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
                    // We already convert these types to string
                    break;
                }

                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                {
                    if(pcVal)
                    {
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
                case SQL_C_TYPE_TIMESTAMP:
                case SQL_C_DATE:
                case SQL_C_TIMESTAMP:
                {
                    iConversionError = TRUE;
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                    // We already convert these types to string
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
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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
                    // We already convert these types to string
                    break;
                }

                default:
                {
                    iConversionError = TRUE;
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

    return pcVal;

error:

    if(iConversionError)
    {
        char szErrMsg[MAX_ERR_MSG_LEN];

        if(hType == SQL_C_DEFAULT){
            snprintf(szErrMsg, sizeof(szErrMsg), "SQL_C_DEFAULT type conversion is not supported for %hd SQL type", hSQLType);
        }
        else{
            snprintf(szErrMsg, sizeof(szErrMsg), "Parameter type conversion is not supported from %hd SQL type to %hd C type.", hSQLType,hCType);
        }
        if(pStmt)
            addError(&pStmt->pErrorList,"HY000", szErrMsg, 0, NULL);
    }

    if(piConversionError)
        *piConversionError = iConversionError;

    return NULL;
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
// Get parameter value as string from C buffer using given C data type.
//
char *getParamVal(char *pParamData, SQLLEN iParamDataLen, SQLLEN *plParamDataStrLenInd, short hCType, RS_BIND_PARAM_STR_BUF *pBindParamStrBuf, short hSQLType)
{
    int iIndicator = int((plParamDataStrLenInd) ? *plParamDataStrLenInd : 0);
    pBindParamStrBuf->iAllocDataLen = 0;

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
            case SQL_C_USHORT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%hd", *(short *)pParamData);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_LONG:
            case SQL_C_SLONG:
            case SQL_C_ULONG:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    int iVal = *(int *)pParamData;

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

            case SQL_C_SBIGINT:
            case SQL_C_UBIGINT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%lld", *(long long *)pParamData);
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
                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%f", *(float *)pParamData);
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
                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%.38g", *(double *)pParamData);
                    pBindParamStrBuf->pBuf = pBindParamStrBuf->buf;
                }
                else
                    pBindParamStrBuf->pBuf = NULL;

                break;
            }

            case SQL_C_BIT:
            case SQL_C_TINYINT:
            case SQL_C_STINYINT:
            case SQL_C_UTINYINT:
            {
                if(iIndicator != SQL_NULL_DATA)
                {
                    char val = *(char *)pParamData;

                    if(hCType == SQL_C_BIT)
                    {
                        if(val == '1' || val == 1 || val == 't' || val == 'T')
                            val = '1';
                        else
                            val = '0';
                    }

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%c", val);
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

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf),"%04hd-%02hd-%02hd", pdtVal->year, pdtVal->month, pdtVal->day);
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

                    // Convert billionth of a fraction to micro second.
                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%04hd-%02hd-%02hd %02hd:%02hd:%02hd.%06d", ptsVal->year, ptsVal->month, ptsVal->day,
                                                                    ptsVal->hour, ptsVal->minute, ptsVal->second,
                                                                    (int)(ptsVal->fraction/1000));
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
                    INTERVALY2M_STRUCT *pivlVal = (INTERVALY2M_STRUCT *)pParamData;
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
                    INTERVALD2S_STRUCT *pivlVal = (INTERVALD2S_STRUCT *)pParamData;

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

                    snprintf(pBindParamStrBuf->buf, sizeof(pBindParamStrBuf->buf), "%02hd:%02hd:%02hd", ptVal->hour, ptVal->minute, ptVal->second);
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

                    convertScaledIntegerToNumericString(pnVal, pNumData, num_data_len);

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
                            //c_binary to sql_binary is a defauly conversion
                            pBindParamStrBuf->pBuf= pParamData;
                        }
                        else
                        {
                            pBindParamStrBuf->pBuf = NULL;
                        }
                        break;
                    } 
                }
                break;
            }



            default:
            {
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
            strncpy(pDataAtExec->pValue, pVal, cbLen);
            strncpy(pDataAtExec->pValue + cbLen, pDataPtr, lStrLenOrInd);
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
// Get concise type of the given SQL type.
//
short getConciseType(short hConciseType, short hType)
{
    if(hConciseType == 0)
    {
		switch (hType)
		{
			case SQL_DATE:
			case SQL_TYPE_DATE:
				hConciseType = SQL_TYPE_DATE;
				break;

			case SQL_TYPE_TIME:
			case SQL_TIME:
				hConciseType = SQL_TYPE_TIME;
				break;

			case SQL_TIMESTAMP:
			case SQL_TYPE_TIMESTAMP:
				hConciseType = SQL_TYPE_TIMESTAMP; // SQL_DATETIME
				break;

			default:
				hConciseType = hType;
				break;

		}
    }

    return hConciseType;
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

    if (result)
    {
        strncpy(moduleName,info.dli_fname,MAX_PATH);
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

/*=====================================================================================*/

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

/*=====================================================================================*/

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
    long long divisor;
    double final_val;

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

        final_val =  (double) value /(double) divisor;

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
            snprintf(pNumData, num_data_len, "%.*f",pnVal->scale,final_val);
        }
        else
        {
            int temp = 0;
            int iZeroes = -(pnVal->scale);

            temp = snprintf(pNumData, num_data_len, "%.*f",temp,final_val);

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

        // Output data before decimal digit
	    for (; i >= pnVal->scale; i--)
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
        strncpy(pConnectProps->szCscPath, optionVal,MAX_PATH - 1);
        pConnectProps->szCscPath[MAX_PATH - 1] = '\0';
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

/*
 * intervaly2m_out()
 * Convert an year-month interval to string.
 */
int intervaly2m_out(INTERVALY2M_STRUCT* y2m, char *buf, int buf_len) {
    int len = 0;

    assert(y2m != NULL);
    len += snprintf(buf + len, buf_len - len, "%d years %d mons", y2m->year, y2m->month);

    return len;
}

/*====================================================================================================================================================*/

/*
 * intervald2s_out()
 * Convert a day-second interval to string.
 */
int intervald2s_out(INTERVALD2S_STRUCT* d2s, char *buf, int buf_len) {
    int len = 0;

    assert(d2s != NULL);
    int second = (int)d2s->second;
    int fraction = (int)d2s->fraction;
    second += fraction/1000000;
    fraction %= 1000000;
    if (second > 0 && fraction < 0) {
        second -= 1;
        fraction += 1000000;
    } else if (second < 0 && fraction > 0) {
        second += 1;
        fraction -= 1000000;
    }
    // At this stage it is guaranteed that second and fraction have the same sign.
    d2s->second = second;
    d2s->fraction = fraction;
    len += snprintf(buf + len, buf_len - len, "%d days %d hours %d mins %s%d.%06d secs",
                    d2s->day, d2s->hour, d2s->minute,
                    (second == 0 && fraction < 0) ? "-" : "", second,
                    fraction < 0 ? -fraction : fraction);

    return len;
}

/*====================================================================================================================================================*/

/*
 * parse_intervaly2m()
 * Parse an interval year to month string to extract year/month fields.
 */
INTERVALY2M_STRUCT parse_intervaly2m(const char *buf, int buf_len) {
    int year = 0, month = 0;

    bool is_sql_standard = true;
    for (int i = 0; i < buf_len; i++) {
        if (!isdigit(buf[i]) && buf[i] != '-') {
            is_sql_standard = false;
            break;
        }
    }
    if (is_sql_standard) {
        // Sql Standard
        bool is_neg = (buf[0] == '-');
        if (is_neg) buf++;
        sscanf(buf, "%d-%d", &year, &month);
        if (is_neg) {
            year = -year;
            month = -month;
        }
    } else {
        // Postgres or Postgres Verbose
        const char* search_pos = strstr(buf, "year");
        if (search_pos != NULL) {
            const char* c = search_pos-2;
            for (; c >= buf; c--) {
                if (!isdigit(*c) && *c != '-') break;
            }
            sscanf(c + 1, "%d", &year);
        } else {
            search_pos = buf;
        }
        search_pos = strstr(search_pos, "mon");
        if (search_pos != NULL) {
            const char* c = search_pos-2;
            for (; c >= buf; c--) {
                if (!isdigit(*c) && *c != '-') break;
            }
            sscanf(c + 1, "%d", &month);
        }
    }

    return {(SQLUINTEGER)year, (SQLUINTEGER)month};
}

/*====================================================================================================================================================*/

/*
 * parse_intervald2s()
 * Parse an interval day to second string to extract day/hour/minute/second/microsecond fields.
 */
INTERVALD2S_STRUCT parse_intervald2s(const char *buf, int buf_len) {
    int day = 0, hour = 0, min = 0, sec = 0;
    char micr[] = "+000000000";
    bool is_postgres_verbose = false;
    bool has_spaces = false;
    bool has_alphabets = false;
    bool has_time = false;
    for (int i = 0; i < buf_len; i++) {
        if (buf[i] == '@') is_postgres_verbose = true;
        else if (buf[i] == ' ') has_spaces = true;
        else if (buf[i] == ':') has_time = true;
        else if (!isdigit(buf[i]) && buf[i] != '-' && buf[i] != '.')
          has_alphabets = true;
    }
    if (!has_alphabets) {
        bool is_neg = (buf[0] == '-');
        if (is_neg) buf++;
        if (has_spaces) {
            // Sql Standard
            sscanf(buf, "%d %d:%d:%d.%s", &day, &hour, &min, &sec, micr+1);
        } else {
            // Postgres but no days.
            sscanf(buf, "%d:%d:%d.%s", &hour, &min, &sec, micr+1);
        }
        if (is_neg) {
            day = day > 0 ? -day : day;
            hour = hour > 0 ? -hour : hour;
            min = min > 0 ? -min : min;
            sec = sec > 0 ? -sec : -sec;
            micr[0] = '-';
        }
    } else {
        // Postgres or Postgres Verbose.
        const char* prev_search_pos = buf;
        const char* search_pos = strstr(buf, "day");
        if (search_pos != NULL) {
            const char* c = search_pos-2;
            for (; c >= buf; c--) {
                if (!isdigit(*c) && *c != '-') break;
            }
            sscanf(c + 1, "%d", &day);
        } else {
            search_pos = prev_search_pos;
        }
        if (is_postgres_verbose) {
            prev_search_pos = search_pos;
            search_pos = strstr(search_pos, "hour");
            if (search_pos != NULL) {
                const char* c = search_pos-2;
                for (; c >= buf; c--) {
                    if (!isdigit(*c) && *c != '-') break;
                }
                sscanf(c + 1, "%d", &hour);
            } else {
                search_pos = prev_search_pos;
            }
            prev_search_pos = search_pos;
            search_pos = strstr(search_pos, "min");
            if (search_pos != NULL) {
                const char* c = search_pos-2;
                for (; c >= buf; c--) {
                    if (!isdigit(*c) && *c != '-') break;
                }
                sscanf(c + 1, "%d", &min);
            } else {
                search_pos = prev_search_pos;
            }
            prev_search_pos = search_pos;
            search_pos = strstr(search_pos, "sec");
            if (search_pos != NULL) {
                const char* c = search_pos-2;
                for (; c >= buf; c--) {
                    if (!isdigit(*c) && *c != '-' && *c != '.') break;
                }
                sscanf(c + 1, "%d.%s", &sec, micr+1);
                if (sec < 0) micr[0] = '-';
            } else {
                search_pos = prev_search_pos;
            }
            search_pos = strstr(search_pos, "ago");
            if (search_pos != NULL) {
                // negate everything.
                day = -day;
                hour = -hour;
                min = -min;
                sec = -sec;
                micr[0] = '-';
            }
        } else {
            // Either scan something of the form "-%d:%d:%d" or scan nothing.
            while (*search_pos != 0 && *search_pos != '-' && !isdigit(*search_pos))
                search_pos++;
            if (isdigit(*search_pos)) {
                sscanf(search_pos, "%d:%d:%d.%s", &hour, &min, &sec, micr+1);
            } else if (*search_pos == '-') {
                sscanf(search_pos, "-%d:%d:%d.%s", &hour, &min, &sec, micr+1);
                hour = -hour;
                min = -min;
                sec = -sec;
                micr[0] = '-';
            }
        }
    }

    for (int i = strlen(micr); i < 7; i++) { micr[i] = '0'; }
    micr[7] = 0;

    return {(SQLUINTEGER)day, (SQLUINTEGER)hour, (SQLUINTEGER)min,
            (SQLUINTEGER)sec, (SQLUINTEGER)atoi(micr)};
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

/*
 * intervaly2m_out_wchar()
 * Convert an year-month interval to string.
 */
int intervaly2m_out_wchar(INTERVALY2M_STRUCT* y2m, SQLWCHAR *buf, int buf_len) {
    int len = 0;

    if (y2m->year != 0) {
        len += swprintf(buf + len, buf_len, L"%d years", y2m->year);
    }
    if (y2m->month != 0 || y2m->year == 0) {
        len += swprintf(buf + len, buf_len, L"%s%d mons",
                        y2m->year != 0 ? " " : "", y2m->month);
    }

    return len;
}

/*====================================================================================================================================================*/

/*
 * intervald2s_out_wchar()
 * Convert a day-second interval to string.
 */
int intervald2s_out_wchar(INTERVALD2S_STRUCT* d2s, SQLWCHAR *buf, int buf_len) {
    int len = 0;

    if (d2s->day != 0) {
        len += swprintf(buf + len, buf_len, L"%d days ", d2s->day);
    }
    len += swprintf(buf + len, buf_len, L"%02d:%02d:%02d",
                    d2s->hour, d2s->minute, d2s->second);
    if (d2s->fraction != 0) {
        len += swprintf(buf + len, buf_len, L".%06d", d2s->fraction);
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
    RS_LOG_ERROR("%s", message.c_str());
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
