/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#pragma once

#include "targetver.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#endif // WIN32

#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <map>
#include <unordered_map>
#include <set>
#include <list>
#include <string_view>
#include <vector>

#include "libpq-fe.h"


#ifdef WIN32
#include "win_port.h"
#endif // WIN32

#ifdef LINUX
#include "linux_port.h"
#endif

#include "rsiam.h"
#include "iam/RsSettings.h"
#include <rslog.h>

#define NULL_LEN		(-1)	/* pg_result len for NULL value */

// Handle checking macros
#define VALID_HENV(henv)    (henv != SQL_NULL_HENV)
#define VALID_HDBC(hdbc)    (hdbc != SQL_NULL_HDBC)
#define VALID_HSTMT(hstmt)  (hstmt != SQL_NULL_HSTMT)
#define VALID_HDESC(hdesc)  (hdesc != SQL_NULL_HDESC)

// Max limit macros
#define MAX_SQL_STATE_LEN    6
#define MAX_ERR_MSG_LEN        (256 + MAX_SQL_STATE_LEN + 1 + 64) // +1 for : after native sqlstate, 64: ODBC Error prefix
#ifndef MAX_IDEN_LEN
#define MAX_IDEN_LEN        (NAMEDATALEN + 1)
#endif
#define MAX_TEMP_BUF_LEN        256
#define MAX_NUMBER_BUF_LEN        64
#define MAX_REQUIRED_CONNECT_KEYWORDS 5
#define MAX_OPTIONAL_CONNECT_KEYWORDS 2
#define MAX_NAMEOID_SIZE NAMEDATALEN
#define PADB_MAX_NUM_BUF_LEN 40 // 38 + '.' + '\0'
#define PADB_MAX_PARAMETERS 32767
#define MAX_LARGE_TEMP_BUF_LEN        1024
#define MAX_SMALL_TEMP_BUF_LEN        32
#define MAX_REMARK_LEN     256  // Size of remarks was defined as 256 in padb side
#define MAX_SQL_LEN        1024
#define MAX_COLUMN_DEF_LEN 4000 // Size of column default was defined as 4000 in padb side
#define INT2_LEN    5
#define INT4_LEN    10
#define INT8_LEN    19


#define SET_SQL_FUNCTION_BIT(puhSupported, uwAPI) \
                (*(((UWORD*) (puhSupported)) + ((uwAPI) >> 4)) \
                    |= (1 << ((uwAPI) & 0x000F)) \
                )

#define  SQL_ODBC3_API_START    SQL_API_SQLALLOCHANDLE
#define  SQL_ODBC3_API_LAST     SQL_API_SQLFETCHSCROLL

#define  SQL_ODBC2_API_START    SQL_API_SQLALLOCCONNECT
#define  SQL_ODBC2_API_LAST     SQL_API_SQLBINDPARAMETER

#define  ODBC_INI    "odbc.ini"

#define  ODBCINI_ENV_VAR "ODBCINI"
#define  ODBC_SECTION_NAME "ODBC"

#define  DRV_CONNECT_DLG_ERROR        (-99)
#define  INITIAL_CONNECT_STR_LEN    2048
#define  ON_CONNECT_CMD_MAX_LEN        1024

#define RS_DRIVER_TYPE "ODBC" // ODBC, JDBC, .NET etc.
#define DEFAULT_PORT "5439"

#define ODBC_DRIVER_ERROR_PREFIX "[Redshift][ODBC Driver]"
#define PADB_ERROR_PREFIX         "[Redshift][ODBC Driver][Server]"

#define NETWORK_ERR_MSG_TEXT "could not connect to server"

#define REDSHIFT_ROOT_CERT_FILE    "root.crt"


// Implicit commands
#define BEGIN_CMD        "BEGIN"
#define COMMIT_CMD        "COMMIT"
#define ROLLBACK_CMD    "ROLLBACK"
#define DEALLOCATE_CMD  "DEALLOCATE" 
#define CURSOR_FETCH_ALL_CMD "FETCH ALL FROM \"%s\";"

#ifdef WIN32
#define IMPLICIT_CURSOR_NAME_PREFIX "SQL_CUR"
#endif
#if defined LINUX 
#define IMPLICIT_CURSOR_NAME_PREFIX "sql_cur"
#endif

#define RS_TEXT_FORMAT     0
#define RS_BINARY_FORMAT   1

/* Proprietary Connection/Env Attributes. from qesqlext.h file START */

/* SQLSetConnectOption extensions (1040 to 1139) */

#define SQL_CONOPT_START            1040

#define SQL_ATTR_APP_WCHAR_TYPE			(SQL_CONOPT_START+21)
#define SQL_ATTR_IGNORE_UNICODE_FUNCTIONS       (SQL_CONOPT_START+23)
#define SQL_ATTR_DRIVER_UNICODE_TYPE            (SQL_CONOPT_START+25)

#define SQL_DD_CP_ANSI                0
#define SQL_DD_CP_UCS2                1
#define SQL_DD_CP_UTF8                2
#define SQL_DD_CP_UTF16                SQL_DD_CP_UCS2
/* Determines whether the application/dbms is handling SQLWCHAR
 * as UCS2 or UTF8.
 */

/* Proprietary Connection/Env Attributes. END */

// OID values from catalog/pg_type.h
#define BOOLOID            16
#define BYTEAOID           17
#define CHAROID            18
#define NAMEOID            19
#define INT8OID            20
#define INT2OID            21
#define INT4OID            23
#define TEXTOID            25
#define OIDOID             26
#define FLOAT4OID          700
#define FLOAT8OID          701
#define UNKNOWNOID		   705
#define BPCHAROID          1042
#define VARCHAROID         1043
#define DATEOID            1082
#define TIMEOID            1083
#define TIMESTAMPOID       1114
#define TIMESTAMPTZOID     1184
#define INTERVALOID        1186
#define INTERVALY2MOID     1188
#define INTERVALD2SOID     1190
#define TIMETZOID          1266
#define NUMERICOID         1700
#define REFCURSOROID       1790
#define CSTRINGOID         2275
#define VOIDOID            2278
#define GEOMETRY		   3000
#define GEOGRAPHY		   3001
#define GEOMETRYHEX		   3999
#define SUPER			   4000
#define VARBYTE			   6551
#define UNSPECIFIEDOID     0

// PG catalog OID
#define INT2VECTOROID      22
#define REGPROCOID         24
#define TIDOID             27
#define XIDOID             28
#define CIDOID             29
#define OIDVECTOROID       30
#define ABSTIMEOID         702
#define NAMEARRAYOID       1003
#define INT2ARRAYOID       1005
#define INT4ARRAYOID       1007
#define TEXTARRAYOID       1009
#define FLOAT4ARRAYOID     1021
#define ACLITEMARRAYOID    1034
#define ANYARRAYOID        2277



// Redshift specific data type(s)
#define MAX_DATEOID_SIZE        10
#define MAX_TIMEOID_SIZE        15
#define MAX_TIMETZOID_SIZE		21 
#define MAX_TIMESTAMPOID_SIZE   29
#define MAX_TIMESTAMPTZOID_SIZE 35
#define MAX_INTERVALY2MOID_SIZE 32
#define MAX_INTERVALD2SOID_SIZE 64
#define MAX_SUPER_SIZE			4194304
#define MAX_VARBYTE_SIZE		1024000 
#define MAX_GEOMETRY_SIZE		1024000 
#define MAX_GEOGRAPHY_SIZE		1048316

// Classes forward reference
class RS_ENV_INFO;
class RS_CONN_INFO;
class RS_STMT_INFO;
class RS_RESULT_INFO;

class RS_PREPARE_INFO;
class RS_ERROR_INFO;

class RS_CONNECT_PROPS_INFO;

class RS_DESC_INFO;
class RS_DESC_HEADER;

class RS_ENV_ATTR_INFO;
class RS_CONN_ATTR_INFO;
class RS_STMT_ATTR_INFO;

class RS_DATA_AT_EXEC;
class RS_EXEC_THREAD_INFO;
// Data structures
struct _RS_DESC_REC; // Array of it
struct _RS_STR_BUF;
struct _CscStatementContext;


/*
 * Environment attributes info.
 */
class RS_ENV_ATTR_INFO
{
public:

    RS_ENV_ATTR_INFO() {
      iConnectionPooling = SQL_CP_DEFAULT;
      iConnectionPoolingMatch = SQL_CP_MATCH_DEFAULT;
      iOdbcVersion = SQL_OV_ODBC3;
      iOutputNts = SQL_TRUE;
    }

    int iConnectionPooling;
    int iConnectionPoolingMatch;
    int iOdbcVersion;
    int iOutputNts;
};

/*
 * Enviroment info.
 */
class RS_ENV_INFO
{
public:

    RS_ENV_INFO() {
      phdbcHead = NULL;
      pErrorList = NULL;
      hApiMutex = NULL;
      pEnvAttr = (RS_ENV_ATTR_INFO *)new RS_ENV_ATTR_INFO();
    }

    // List of connection
    RS_CONN_INFO *phdbcHead;

    // Environment attributes
    RS_ENV_ATTR_INFO *pEnvAttr;

    // Error list
    RS_ERROR_INFO *pErrorList;

    // Thread safe API mutex
    MUTEX_HANDLE hApiMutex; 

    // Methods
    static SQLRETURN  SQL_API RS_SQLAllocEnv(SQLHENV *pphenv);
    static SQLRETURN  SQL_API RS_SQLAllocConnect(SQLHENV phenv,
                                            SQLHDBC *pphdbc);
    static SQLRETURN  SQL_API RS_SQLFreeEnv(SQLHENV phenv);
};

/*
 * Connection info.
 */
class RS_CONN_INFO
{
public:

#define RS_CLOSE_CONNECTION 0
#define RS_OPEN_CONNECTION  1

    RS_CONN_INFO(RS_ENV_INFO *_phenv) {
      // Initialize HDBC
      phenv = _phenv;
      pgConn = NULL;
      pConnectProps = NULL;
      pConnAttr = NULL;
      iStatus = RS_CLOSE_CONNECTION;
      iBrowseIteration = 0;
      iInternal = FALSE;
      phstmtHead = NULL;
      pErrorList = NULL;
      pCmdBuf = NULL;
      phdescHead = NULL;
      hSemMultiStmt = NULL;
      hApiMutex = NULL;
      iLastQueryTimeoutSetInServer = 0;
      pNext = NULL;

//      memset(&iamSettings, '\0', sizeof(iamSettings));
    }

    RS_ENV_INFO *phenv; // Parent HENV, who created this connection.
    PGconn * pgConn; // libpq connection handle.

    // Connection properties
    RS_CONNECT_PROPS_INFO *pConnectProps;

    // Connection attributes
    RS_CONN_ATTR_INFO *pConnAttr;

    int iStatus; // Open, Close.
    int iBrowseIteration;
    int iInternal; // 0 means external, 1 means calls internally

    // List of statements
    RS_STMT_INFO *phstmtHead;

    // Error list
    RS_ERROR_INFO *pErrorList;

    // Query
    struct _RS_STR_BUF *pCmdBuf;

    // Explicit Descriptor list
    class RS_DESC_INFO *phdescHead;

    // Semaphore to protect multiple statements execution at same time
    SEM_HANDLE hSemMultiStmt;

    // Thread safe API mutex
    MUTEX_HANDLE hApiMutex; 

    // Last query timeout set in the server
    int iLastQueryTimeoutSetInServer;

    // IAM stuff
    RsSettings iamSettings;


    // Next element
    RS_CONN_INFO *pNext;

    // Methods
   void resetConnectProps();
   void setConnectStr(char *szInitStr);
   void appendConnectStr(char *szInitStr);
   void appendConnectAttribueStr(const char *szName, const char *szVal);
   char *getConnectStr();
   void readMoreConnectPropsFromRegistry(int readUser);
   void readIamConnectPropsFromRegistry();
   int parseConnectString(char *szConnStrIn, size_t cbConnStrIn, int append,int onlyDSN);
   int readAuthProfile(int append);


   int isConnectionOpen();
   int isConnectionDead();

   static int doesNeedMoreBrowseConnectStr(char *_szConnStrIn,char *szConnStrOut, SWORD cbConnStrOut, SWORD *pcbConnStrOut,
                                           int iBrowseIteration, RS_CONN_INFO *pConn);
   static void readIntValFromDsn(char *szDsn, char *pKey, int *piVal);
   static void readLongValFromDsn(char *szDSN, char *pKey, long *plVal);
   static void readLongLongValFromDsn(char *szDSN, char *pKey, long long *pllVal);
   static void readBoolValFromDsn(char *szDSN, char *pKey, bool *pbVal);
   static bool convertToBoolVal(const char *pVal);

   static SQLRETURN doConnection(RS_CONN_INFO *pConn);


   static SQLRETURN  SQL_API RS_SQLConnect(SQLHDBC phdbc,
       SQLCHAR *szDSN,
       SQLSMALLINT cchDSN,
       SQLCHAR *szUID,
       SQLSMALLINT cchUID,
       SQLCHAR *szAuthStr,
       SQLSMALLINT cchAuthStr);

   static SQLRETURN SQL_API RS_SQLDriverConnect(SQLHDBC            phdbc,
                                       SQLHWND           hwnd,
                                       SQLCHAR           *szConnStrIn,
                                       SQLSMALLINT       cbConnStrIn,
                                       SQLCHAR           *szConnStrOut,
                                       SQLSMALLINT       cbConnStrOut,
                                       SQLSMALLINT       *pcbConnStrOut,
                                       SQLUSMALLINT       hDriverCompletion);

   static SQLRETURN SQL_API RS_SQLBrowseConnect(SQLHDBC          phdbc,
                                       SQLCHAR       *szConnStrIn,
                                       SQLSMALLINT   cbConnStrIn,
                                       SQLCHAR       *szConnStrOut,
                                       SQLSMALLINT   cbConnStrOut,
                                       SQLSMALLINT   *pcbConnStrOut);

   static SQLRETURN  SQL_API RS_SQLFreeConnect(SQLHDBC phdbc);

   static SQLRETURN  SQL_API RS_SQLAllocStmt(SQLHDBC phdbc,
                                       SQLHSTMT *pphstmt);

};


/*
 * Statement info.
 */
class RS_STMT_INFO
{
  public:
// Statement states
#define RS_ALLOCATE_STMT 0
#define RS_PREPARE_STMT  1
#define RS_EXECUTE_STMT  2
#define RS_EXECUTE_STMT_NEED_DATA  3
#define RS_CANCEL_STMT   4
#define RS_CLOSE_STMT     5
#define RS_DROP_STMT     6

    RS_STMT_INFO(RS_CONN_INFO *_phdbc) {
      // Initialize HSTMT

      phdbc = _phdbc;
      pErrorList = NULL;
      iStatus = RS_ALLOCATE_STMT;

      pAPD = NULL;
      pIPD = NULL;
      pARD = NULL;
      pIRD = NULL;

      pStmtAttr = NULL;
      pCmdBuf = NULL;

      iNumOfParamMarkers = 0;
      pResultHead = NULL;

	  iNumOfOutOnlyParams = 0;
	  iNumOfInOutOnlyParams = 0;

      szCursorName[0] = '\0';
      pPrepareHead = NULL;

      pAPDRecDataAtExec = NULL;
      pszCmdDataAtExec = NULL;
      iExecutePreparedDataAtExec = 0;
      lParamProcessedDataAtExec = 0;

      iFunctionCall = 0;
      iCatalogQuery = 0;

      pExecThread = NULL;
      pCscStatementContext = NULL;
      iMultiInsert = 0;
      iLastBatchMultiInsert = 0;
      pszLastBatchMultiInsertCmd = NULL;

      pszUserInsertCmd = NULL;
      pNext = NULL;
    }

    RS_CONN_INFO *phdbc; // Parent HDBC, who created this statement.

    // Error list
    RS_ERROR_INFO *pErrorList;

    int iStatus; // Allocate, Prepare, Execute, Close.

    // Descriptors
    RS_DESC_INFO *pAPD;
    RS_DESC_INFO *pIPD;
    RS_DESC_INFO *pARD;
    RS_DESC_INFO *pIRD;

    // Statement attributes
    RS_STMT_ATTR_INFO *pStmtAttr;

    // Query
    struct _RS_STR_BUF *pCmdBuf;

    // Number of param mark in query
    int iNumOfParamMarkers;

	// Number of OUT params for a stored proc
	int iNumOfOutOnlyParams;

	// Number of INOUT params for a stored proc
	int iNumOfInOutOnlyParams;

    // List of results. Head is alway active result bcoz one can't go backward.
    // So as we move next result, we will move Head.
    RS_RESULT_INFO *pResultHead;

    // Implicit or explicit cursor name.
    char    szCursorName[MAX_IDEN_LEN];

    // Prepare statement info from libpq
    RS_PREPARE_INFO *pPrepareHead;

    // Data at exec stuff
    struct _RS_DESC_REC *pAPDRecDataAtExec;
    char *pszCmdDataAtExec;
    int iExecutePreparedDataAtExec;
    long lParamProcessedDataAtExec;

    // Is it contain function/procedure call?
    int iFunctionCall;

    // Is it catalog query?
    int iCatalogQuery;

    // Thread info
    RS_EXEC_THREAD_INFO *pExecThread;

    // Statement context for CSC in libpq
    struct _CscStatementContext *pCscStatementContext;

    // > 0 means INSERT converted to multi INSERT. It shows mutlipier factor.
    int iMultiInsert;
    // Records the lArraySize when iMultiInsert was updated
    int lArraySizeMultiInsert;

    // > 0 means Multi-Insert has remainder which is less than iMultiInsert.
    int iLastBatchMultiInsert;

    // If iLastBatchMultiInsert > 0 then this will contain last batch multi-insert command.
    struct _RS_STR_BUF *pszLastBatchMultiInsertCmd;

    // Contains user INSERT command pass to SQLPrepare, which we can convert into multi-insert if API sequence calls not in order.
    char *pszUserInsertCmd;

    // Next element
    RS_STMT_INFO *pNext;

    // Methods
    static SQLRETURN  SQL_API RS_SQLFreeStmt(SQLHSTMT phstmt,
                                        SQLUSMALLINT uhOption,
                                        int iInternalFlag);

    static SQLRETURN  SQL_API RS_SQLDescribeCol(SQLHSTMT phstmt,
                                           SQLUSMALLINT hCol,
                                           SQLCHAR *pColName,
                                           SQLSMALLINT cbLen,
                                           SQLSMALLINT *pcbLen,
                                           SQLSMALLINT *pDataType,
                                           SQLULEN *pColSize,
                                           SQLSMALLINT *pDecimalDigits,
                                           SQLSMALLINT *pNullable);

    static SQLRETURN  SQL_API RS_SQLBindCol(SQLHSTMT phstmt,
                                SQLUSMALLINT hCol, 
                                SQLSMALLINT hType,
                                SQLPOINTER pValue, 
                                SQLLEN cbLen, 
                                SQLLEN *pcbLenInd);

    static SQLRETURN  SQL_API RS_SQLGetData(RS_STMT_INFO *pStmt,
                                      SQLUSMALLINT hCol,
                                      SQLSMALLINT hType,
                                      SQLPOINTER pValue,
                                      SQLLEN cbLen,
                                      SQLLEN *pcbLenInd,
                                      int iInternal);

    static SQLRETURN  SQL_API RS_SQLColAttributeW(SQLHSTMT     phstmt,
                                            SQLUSMALLINT hCol,
                                            SQLUSMALLINT hFieldIdentifier,
                                            SQLPOINTER   pwValue,
                                            SQLSMALLINT  cbLen,
                                            SQLSMALLINT *pcbLen,
                                            SQLLEN        *plValue);

    static SQLRETURN  SQL_API RS_SQLColAttribute(SQLHSTMT        phstmt,
                                           SQLUSMALLINT hCol,
                                           SQLUSMALLINT hFieldIdentifier,
                                           SQLPOINTER    pcValue,
                                           SQLSMALLINT    cbLen,
                                           SQLSMALLINT *pcbLen,
                                           SQLLEN       *plValue);

    static SQLRETURN  SQL_API RS_SQLFetchScroll(SQLHSTMT phstmt,
                                          SQLSMALLINT hFetchOrientation,
                                          SQLLEN iFetchOffset);

    SQLRETURN  SQL_API InternalSQLCloseCursor();
    SQLRETURN  SQL_API InternalSQLPrepare(
                                        SQLCHAR* pCmd,
                                        SQLINTEGER cbLen,
                                        int iInternal,
                                        int iSQLPrepareW,
                                        int iReprepareForMultiInsert,
                                        int iLockRequired);
    bool shouldRePrepareArrayBinding();
    void resetMultiInsertInfo();
};

/*
 * Error info.
 */
class RS_ERROR_INFO
{
public:

    RS_ERROR_INFO() {
      szSqlState[0] = '\0';
      szErrMsg[0] = '\0';
      lNativeErrCode = 0;

      pNext = NULL;
    }

    char szSqlState[MAX_SQL_STATE_LEN];
    char szErrMsg[MAX_ERR_MSG_LEN];
    long lNativeErrCode;

    // Next element
    RS_ERROR_INFO *pNext;
};

/*
 * Global state.
 */
typedef struct _RS_GLOBAL_VARS
{
    HMODULE hModule; // DLL hinstance
    MUTEX_HANDLE hApiMutex; // Thread safe API mutex
}RS_GLOBAL_VARS;

/*
 * DATA-AT-EXEC values set by SQLPutData.
 */
class RS_DATA_AT_EXEC
{
public:

    RS_DATA_AT_EXEC() {
      pValue = NULL;
      cbLen = 0;
    }

    char *pValue; 
    SQLLEN cbLen;      
};

/*
 * Descriptor header info.
 */
class RS_DESC_HEADER
{
public:

    RS_DESC_HEADER(short _hAllocType) {
      hAllocType = _hAllocType;
      lArraySize = 1;

      /*
      TODO:
      We don't see this being ever initialized.
      it is usually used in conjuction with lArraySize (via lParamsToBind and
      lRowsToFetch) but corresponding logic are never executed due to
      phArrayStatusPtr's NULL check; which makes it useless. Before considering
      removing the variable and all the respective logic around it, find where
      this status array was supposed to be used. If found a valid/missing use
      case, fix the ptr manipulation and use the collected status wherever
      necessary.
      */
      phArrayStatusPtr = NULL;

      /*
      TODO:
      Cannot find real non-null usage of plBindOffsetPtr
      */
      plBindOffsetPtr = NULL;

      lBindType = SQL_PARAM_BIND_TYPE_DEFAULT;
      hHighestCount = 0;

      /*
      TODO:
      Cannot find real non-null usage of plRowsProcessedPtr
      */
      plRowsProcessedPtr = NULL;

      valid = true;
    }

    short hAllocType; // SQL_DESC_ALLOC_AUTO or USER
    long  lArraySize; // ARD: Number of rows to return, APD: Number of values for each param.
    short *phArrayStatusPtr; // Status array
    SQLLEN *plBindOffsetPtr; // Bind offset
    long  lBindType; // Columnwise or Rowwise
    short hHighestCount; // Highest column bound
    long *plRowsProcessedPtr; // Rows returned
    bool valid; // to replace malloc based null checks(defaults to true)
};

/*
 * Descriptor field info.
 */
typedef struct _RS_DESC_REC
{
/* public:
    RS_DESC_REC() {
      hRecNumber = 0;
      szName[0] = '\0';
      hType = 0;
      iSize = 0;
      hScale = 0;
      hNullable = 0;

      cAutoInc = 0;
      szTableName[0] = '\0';
      cCaseSensitive = 0;
      szCatalogName[0] = '\0';
      iDisplaySize = 0;
      cFixedPrecScale = 0;

      szLiteralPrefix[0] = '\0';
      szLiteralSuffix[0] = '\0';
      szTypeName[0] = '\0';

      iNumPrecRadix = 0;
      iOctetLen = 0;
      iPrecision = 0;
      szSchemaName[0] = '\0';
      iSearchable = 0;
      iUnNamed = 0;
      cUnsigned = 0;
      iUpdatable = 0;

      pValue = NULL;
      cbLen = 0;
      pcbLenInd = NULL;

      hInOutType = 0;
      hParamSQLType = 0;

      hConciseType = 0;
      plOctetLen = NULL;

      hDateTimeIntervalCode = 0;
      iDateTimeIntervalPrecision = 0;

      pDataAtExec = NULL;

      hPaSpecialType = 0;

      pNext = NULL;
    }
*/

    short hRecNumber; // ARD: Record Number. Col number or param number. Start from 1.

    // COL_INFO/PARAM_INFO/BIND_PARAM
    char szName[MAX_IDEN_LEN + 1];    // IRD: SQL_DESC_BASE_COLUMN_NAME, R. IRD: SQL_DESC_LABEL, R. IRD: SQL_DESC_NAME, R. IPD: SQL_DESC_NAME, R/W. 
    short hType;            // SQL or C Type. IRD: SQL_DESC_TYPE, R. IPD: SQL_DESC_TYPE, R/W. APD: SQL_DESC_TYPE R/W. ARD: SQL_DESC_TYPE, R/W.  
    int  iSize;                // IRD: SQL_DESC_LENGTH, R. IPD: SQL_DESC_LENGTH, R/W. APD: SQL_DESC_LENGTH, R/W. 
    short hScale;            // IRD: SQL_DESC_SCALE, R. IPD: SQL_DESC_SCALE, R/W. APD: SQL_DESC_SCALE, R/W. ARD: SQL_DESC_SCALE, R/W.
    short hNullable;        // IRD: SQL_DESC_NULLABLE, R. IPD: SQL_DESC_NULLABLE, R.

    // COL_ATTR
    char  cAutoInc;                  // IRD: SQL_DESC_AUTO_UNIQUE_VALUE, R.
    char  szTableName[MAX_IDEN_LEN + 1]; // "" . IRD: SQL_DESC_BASE_TABLE_NAME, R. IRD: SQL_DESC_TABLE_NAME, R.
    char  cCaseSensitive;          // IRD: SQL_DESC_CASE_SENSITIVE, R. IPD: SQL_DESC_CASE_SENSITIVE, R.
    char  szCatalogName[MAX_IDEN_LEN + 1]; // "". IRD: SQL_DESC_CATALOG_NAME, R.
    int   iDisplaySize;        // IRD: SQL_DESC_DISPLAY_SIZE, R.
    char  cFixedPrecScale;  // IRD: SQL_DESC_FIXED_PREC_SCALE, R. IPD: SQL_DESC_FIXED_PREC_SCALE, R.
    char  szLiteralPrefix[2]; // '. IRD: SQL_DESC_LITERAL_PREFIX, R.
    char  szLiteralSuffix[2]; // '. IRD: SQL_DESC_LITERAL_SUFFIX, R.
    char  szTypeName[MAX_IDEN_LEN + 1]; // IRD: SQL_DESC_LOCAL_TYPE_NAME, R. IPD: SQL_DESC_LOCAL_TYPE_NAME, R. IRD: SQL_DESC_TYPE_NAME, R. IPD: SQL_DESC_TYPE_NAME, R.
    int   iNumPrecRadix;            // IRD: SQL_DESC_NUM_PREC_RADIX, R. IPD: SQL_DESC_NUM_PREC_RADIX, R/W. APD: SQL_DESC_NUM_PREC_RADIX, R/W. ARD: SQL_DESC_NUM_PREC_RADIX, R/W.
    int   iOctetLen;                // IRD: SQL_DESC_OCTET_LENGTH, R. IPD: SQL_DESC_OCTET_LENGTH, R/W. APD: SQL_DESC_OCTET_LENGTH, R/W. ARD: SQL_DESC_OCTET_LENGTH, R/W.
    int   iPrecision;                // IRD: SQL_DESC_PRECISION, R. IPD: SQL_DESC_PRECISION, R/W. APD: SQL_DESC_PRECISION, R/W. ARD: SQL_DESC_PRECISION, R/W.
    char  szSchemaName[MAX_IDEN_LEN + 1];     // "". IRD: SQL_DESC_SCHEMA_NAME, R.
    int   iSearchable;           // IRD: SQL_DESC_SEARCHABLE, R.
    int   iUnNamed;               // IRD: SQL_DESC_UNNAMED, R, IPD: SQL_DESC_UNNAMED, R/W.
    char  cUnsigned;           // IRD: SQL_DESC_UNSIGNED, R. IPD: SQL_DESC_UNSIGNED, R.
    int   iUpdatable;           // IRD: SQL_DESC_UPDATABLE, R.

    // BIND_COL/BIND_PARAM
    SQLPOINTER pValue = NULL; // App buffer. ARD: SQL_DESC_DATA_PTR, R/W. APD: SQL_DESC_DATA_PTR, R/W.
    SQLLEN cbLen;      // Buffer length. ARD: SQL_DESC_LENGTH, R/W. APD: SQL_DESC_LENGTH, R/W.
    SQLLEN *pcbLenInd; // Actual data  len or indicator. ARD: SQL_DESC_INDICATOR_PTR, R/W. APD: SQL_DESC_INDICATOR_PTR, R/W.

    // BIND_PARAM/PARAM_INFO
    short hInOutType; // Input/Output type. IPD: SQL_DESC_PARAMETER_TYPE, R/W. APD: SQL_DESC_PARAMETER_TYPE, R/W.
    short hParamSQLType; // This is because SQLBindParameter  set both value type and param type. Use in APD, R/W. 

    // DESC specific.
    short hConciseType;            // SQL_DESC_CONCISE_TYPE: SQL Type. IRD: R. IPD: R/W. APD: R/W. ARD:  R/W.  
    SQLINTEGER *plOctetLen; // ARD: SQL_DESC_OCTET_LENGTH_PTR, R/W. APD: SQL_DESC_OCTET_LENGTH_PTR, R/W.

    // Not supported. But allow get and set.
    short hDateTimeIntervalCode; // SQL_DESC_DATETIME_INTERVAL_CODE: ARD: R/W, APD: R/W, IRD: R, IPD: R/W.
    int iDateTimeIntervalPrecision; // SQL_DESC_DATETIME_INTERVAL_PRECISION: ARD: R/W, APD: R/W, IRD: R, IPD: R/W.

    RS_DATA_AT_EXEC *pDataAtExec; // Actual value given by app for DATA_AT_EXEC

    short   hRsSpecialType; // Redshift specific SQL type. e.g. RS_TIMETZ.

    // Next element
    struct _RS_DESC_REC *pNext;
}RS_DESC_REC;

/*
 * Descriptor infomation.
 */
class RS_DESC_INFO
{
public:
// Desc types    
#define RS_UNKNOWN_DESC_TYPE 0
#define RS_APD                1
#define RS_ARD                2
#define RS_IPD                3
#define RS_IRD                4

// Desc recs storage as list or array    
#define RS_DESC_RECS_LINKED_LIST 0
#define RS_DESC_RECS_ARRAY_LIST  1

    RS_DESC_INFO(RS_CONN_INFO *_phdbc, int _iType, short _hAllocType) 
    : pDescHeader(_hAllocType)
    {
      // Initialize HDESC
      phdbc = _phdbc;
      iType = _iType;
      iRecListType = RS_DESC_RECS_LINKED_LIST;
      pDescRecHead = NULL;
      pErrorList = NULL;
      pNext = NULL;
    }

    ~RS_DESC_INFO() {
        phdbc = NULL;
        pDescRecHead = NULL;
        pErrorList = NULL;
        pNext = NULL;
    }

    int        iType; // Desc type as shown above.
    RS_CONN_INFO *phdbc; // Parent HDBC, who created this descriptor.

    // Header fields
    RS_DESC_HEADER pDescHeader;

    // Record list
    int iRecListType; // 0 means linked list, 1 means array
    struct _RS_DESC_REC *pDescRecHead;

    // Error list
    RS_ERROR_INFO *pErrorList;

    // Next element
    RS_DESC_INFO *pNext;

    // Methods
    static SQLRETURN  SQL_API RS_SQLAllocDesc(SQLHDBC phdbc,
                                        SQLHDESC *pphdesc);

    static SQLRETURN  SQL_API RS_SQLFreeDesc(SQLHDESC phdesc);
};

/* ----------------
 * DATA_TYPE_INFO
 *
 * Structure to store the data type info mapping
 * ----------------
 */
struct DATA_TYPE_INFO {
    short sqlType;
    short sqlDataType;
    short sqlDateSub;
    std::string typeName;
};

/* ----------------
 * TypeInfoResult
 *
 * Contains both the type information and a flag indicating if the type was found
 * ----------------
 */
struct TypeInfoResult {
    DATA_TYPE_INFO typeInfo;    // Holds the actual type information
    bool found;                 // Indicates if the type was successfully found
    
    TypeInfoResult(DATA_TYPE_INFO i, bool f) : typeInfo(i), found(f) {}
    static TypeInfoResult notFound() { 
        return TypeInfoResult(DATA_TYPE_INFO(), false); 
    }
};

typedef struct _tuple {
    int len = 0;
    char *value;
} Tuple;

// Define structure to store SHOW SCHEMAS result for post-processing
struct SHOWSCHEMASResult {
    SQLCHAR database_name[NAMEDATALEN] = {0};
    SQLLEN database_name_Len = 0;
    SQLCHAR schema_name[NAMEDATALEN] = {0};
    SQLLEN schema_name_Len = 0;
};

// Define structure to store SHOW TABLES result for post-processing
struct SHOWTABLESResult {
    SQLCHAR database_name[NAMEDATALEN] = {0};
    SQLLEN database_name_Len = 0;
    SQLCHAR schema_name[NAMEDATALEN] = {0};
    SQLLEN schema_name_Len = 0;
    SQLCHAR table_name[NAMEDATALEN] = {0};
    SQLLEN table_name_Len = 0;
    SQLCHAR table_type[NAMEDATALEN] = {0};
    SQLLEN table_type_Len = 0;
    SQLCHAR remarks[MAX_REMARK_LEN] = {0};
    SQLLEN remarks_Len = 0;
};

// Define structure to store SHOW COLUMNS result for post-processing
struct SHOWCOLUMNSResult {
    SQLCHAR database_name[NAMEDATALEN] = {0};
    SQLLEN database_name_Len = 0;
    SQLCHAR schema_name[NAMEDATALEN] = {0};
    SQLLEN schema_name_Len = 0;
    SQLCHAR table_name[NAMEDATALEN] = {0};
    SQLLEN table_name_Len = 0;
    SQLCHAR column_name[NAMEDATALEN] = {0};
    SQLLEN column_name_Len = 0;
    SQLSMALLINT ordinal_position = 0;
    SQLLEN ordinal_position_Len = 0;
    SQLCHAR column_default[MAX_COLUMN_DEF_LEN] = {0};
    SQLLEN column_default_Len = 0;
    SQLCHAR is_nullable[NAMEDATALEN] = {0};
    SQLLEN is_nullable_Len = 0;
    SQLCHAR data_type[NAMEDATALEN] = {0};
    SQLLEN data_type_Len = 0;
    SQLSMALLINT character_maximum_length = 0;
    SQLLEN character_maximum_length_Len = 0;
    SQLSMALLINT numeric_precision = 0;
    SQLLEN numeric_precision_Len = 0;
    SQLSMALLINT numeric_scale = 0;
    SQLLEN numeric_scale_Len = 0;
    SQLCHAR remarks[MAX_REMARK_LEN] = {0};
    SQLLEN remarks_Len = 0;
};

#define SHORT_NAME_KEYWORD 0
#define LONG_NAME_KEYWORD  1

// Connection options name
#define RS_DRIVER             "DRIVER"
#define RS_DSN                "DSN"
#define RS_HOST_NAME          "HostName"
#define RS_HOST               "Host"
#define RS_SERVER             "Server"
#define RS_PORT_NUMBER        "PortNumber"
#define RS_PORT               "Port"
#define RS_DATABASE           "Database"
#define RS_DB                 "DB"
#define RS_LOGON_ID           "LogonID"
#define RS_UID                "UID"
#define RS_USER               "User"
#define RS_PASSWORD           "Password"
#define RS_PWD                "PWD"
#define RS_LOGIN_TIMEOUT      "LoginTimeout"
#define RS_ENABLE_DESCRIBE_PARAM      "EnableDescribeParam"
#define RS_EXTENDED_COLUMN_METADATA   "ExtendedColumnMetaData"
#define RS_APPLICATION_USING_THREADS  "ApplicationUsingThreads"
#define RS_FETCH_REF_CURSOR           "FetchRefCursor"
#define RS_TRANSACTION_ERROR_BEHAVIOR "TransactionErrorBehavior"
#define RS_CONNECTION_RETRY_COUNT     "ConnectionRetryCount"
#define RS_CONNECTION_RETRY_DELAY     "ConnectionRetryDelay"
#define RS_QUERY_TIMEOUT              "QueryTimeout"
#define RS_INITIALIZATION_STRING      "InitializationString"
#define RS_TRACE                      "Trace"
#define RS_TRACE_FILE                 "TraceFile"
#define RS_TRACE_LEVEL                "TraceLevel"
#define RS_CSC_ENABLE                 "CscEnable"
#define RS_CSC_THRESHOLD              "CscThreshold"
#define RS_CSC_PATH                   "CscPath"
#define RS_CSC_MAX_FILE_SIZE          "CscMaxFileSize"
#define RS_SSL_MODE                   "SSLMode"
#define RS_ENCRYPTION_METHOD          "EncryptionMethod"
#define RS_VALIDATE_SERVER_CERTIFICATE  "ValidateServerCertificate"
//#define RS_HOST_NAME_IN_CERTIFICATE     "HostNameInCertificate"
#define RS_TRUST_STORE                  "TrustStore"
#define RS_MULTI_INSERT_CMD_CONVERT_ENABLE  "MultiInsertCmdConvertEnable"
#define RS_KERBEROS_SERVICE_NAME            "KerberosServiceName"
#define RS_KERBEROS_API                     "KerberosAPI"
#define RS_STREAMING_CURSOR_ROWS            "StreamingCursorRows"
#define RS_DATABASE_METADATA_CURRENT_DB_ONLY     "DatabaseMetadataCurrentDbOnly"
#define RS_READ_ONLY							"ReadOnly"
#define RS_CLIENT_PROTOCOL_VERSION              "client_protocol_version"
#define RS_STRING_TYPE							"StringType"
#define RS_APPLICATION_NAME						"ApplicationName"
#define RS_COMPRESSION						"Compression"



// TCP Proxy connection options
#define RS_TCP_PROXY_HOST       "ProxyHost"
#define RS_TCP_PROXY_PORT       "ProxyPort"
#define RS_TCP_PROXY_USER_NAME  "ProxyUid"
#define RS_TCP_PROXY_PASSWORD   "ProxyPwd"

// Keep alive connection options
#define RS_KEEP_ALIVE "KeepAlive"  
#define RS_KEEP_ALIVE_COUNT "KeepAliveCount"  
#define RS_KEEP_ALIVE_IDLE "KeepAliveIdle"  
#define RS_KEEP_ALIVE_INTERVAL "KeepAliveInterval" 

// Min TLS 1.1/1.2
#define RS_MIN_TLS "Min_TLS" // Default is 1.0


// IAM connection options
#define RS_IAM                    "IAM"
#define RS_HTTPS_PROXY_HOST       "https_proxy_host"
#define RS_HTTPS_PROXY_PORT       "https_proxy_port"
#define RS_HTTPS_PROXY_USER_NAME  "https_proxy_username"
#define RS_HTTPS_PROXY_PASSWORD   "https_proxy_password"
#define RS_IDP_USE_HTTPS_PROXY    "idp_use_https_proxy"
#define RS_AUTH_TYPE              "AuthType"
#define RS_CLUSTER_ID             "ClusterId"
#define RS_REGION                 "Region"
#define RS_END_POINT_URL          "EndPointUrl"
#define RS_DB_USER                "DbUser"
#define RS_DB_GROUPS              "DbGroups"
#define RS_DB_GROUPS_FILTER       "dbgroups_filter"
#define RS_AUTO_CREATE            "AutoCreate"
#define RS_SERVERLESS             "Serverless"
#define RS_WORKGROUP              "Workgroup"
#define RS_FORCE_LOWER_CASE       "ForceLowercase"
#define RS_IDP_RESPONSE_TIMEOUT   "idp_response_timeout"
#define RS_IAM_DURATION           "IAMDuration"
#define RS_ACCESS_KEY_ID          "AccessKeyID"
#define RS_SECRET_ACCESS_KEY      "SecretAccessKey"
#define RS_SESSION_TOKEN          "SessionToken"
#define RS_PROFILE                "Profile"
#define RS_INSTANCE_PROFILE        "InstanceProfile"
#define RS_PLUGIN_NAME            "plugin_name"
#define RS_PREFERRED_ROLE         "preferred_role"
#define RS_IDP_HOST               "idp_host"
#define RS_IDP_PORT               "idp_port"
#define RS_SSL_INSECURE           "ssl_insecure"
#define RS_GROUP_FEDERATION       "group_federation"
#define RS_LOGIN_TO_RP            "loginToRp"
#define RS_IDP_TENANT             "idp_tenant"
#define RS_IDP_PARTITION          "idp_partition"
#define RS_CLIENT_ID              "client_id"
#define RS_CLIENT_SECRET          "client_secret"
#define RS_LOGIN_URL              "login_url"
#define RS_LISTEN_PORT            "listen_port"
#define RS_PARTNER_SPID           "partner_spid"
#define RS_APP_ID                 "app_id"
#define RS_APP_NAME               "app_name"
#define RS_WEB_IDENTITY_TOKEN     "web_identity_token"
#define RS_ROLE_ARN               "role_arn"
#define RS_DURATION               "duration"
#define RS_ROLE_SESSION_NAME      "role_session_name"
#define RS_IAM_CA_PATH            "CaPath"
#define RS_IAM_CA_FILE            "CaFile"
#define RS_IAM_STS_ENDPOINT_URL    "StsEndpointUrl"
//	#define IAM_KEY_PROVIDER_NAME       "provider_name"
#define RS_IAM_AUTH_PROFILE        "AuthProfile"
#define RS_DISABLE_CACHE           "DisableCache"
// #define RS_IAM_STS_ENDPOINT_URL    "StsEndpointUrl"
#define RS_IAM_STS_CONNECTION_TIMEOUT  "StsConnectionTimeout"
#define RS_SCOPE                       "scope"
#define RS_BASIC_AUTH_TOKEN            "token"
#define RS_IDENTITY_NAMESPACE          "identity_namespace" 
#define RS_TOKEN_TYPE                  "token_type"
#define RS_ISSUER_URL              "issuer_url"
#define RS_IDC_REGION              "idc_region"
#define RS_IDC_CLIENT_DISPLAY_NAME "idc_client_display_name"
#define RS_NATIVE_KEY_PROVIDER_NAME	"provider_name"
#define RS_VPC_ENDPOINT_URL         "vpc_endpoint_url"



// Connection options value
#define RS_AUTH_TYPE_STATIC   "Static"
#define RS_AUTH_TYPE_PROFILE  "Profile"
#define RS_AUTH_TYPE_PLUGIN   "Plugin"
#define RS_IDP_TYPE_AWS_IDC   "AwsIdc"
#define RS_TOKEN_TYPE_ACCESS_TOKEN  "ACCESS_TOKEN"

/* Predefined external plug-in */
#define IAM_PLUGIN_ADFS             "ADFS"
#define IAM_PLUGIN_AZUREAD          "AzureAD"
#define IAM_PLUGIN_BROWSER_AZURE    "BrowserAzureAD"
#define IAM_PLUGIN_BROWSER_SAML     "BrowserSAML"
#define IAM_PLUGIN_PING             "Ping"
#define IAM_PLUGIN_OKTA             "Okta"
#define IAM_PLUGIN_JWT              "JWT"   // used for federated native IdP auth
#define IAM_PLUGIN_BROWSER_AZURE_OAUTH2    "BrowserAzureADOAuth2"   // used for federated native IdP auth
#define JWT_IAM_AUTH_PLUGIN         "JwtIamAuthPlugin"   // used for federated Jwt IAM auth
#define PLUGIN_IDP_TOKEN_AUTH       "IdpTokenAuthPlugin"
#define PLUGIN_BROWSER_IDC_AUTH     "BrowserIdcAuthPlugin"

#define RS_LOG_LEVEL_OPTION_NAME "LogLevel"
#define RS_LOG_PATH_OPTION_NAME  "LogPath"


struct RS_TCP_PROXY_CONN_PROPS_INFO {
    char szHost[MAX_IAM_BUF_VAL] = {0}; // proxy host
    char szPort[MAX_IDEN_LEN] = {0};    // proxy port
    char szUser[MAX_IDEN_LEN] = {0};
    char szPassword[MAX_IDEN_LEN] = {0};
};

/*
 * Connect props info.
 * Note: Login Timeout–Maximum number of seconds for PADB to respond to a connection request before
 *        abandoning the connection. A value of zero indicates an infinite wait. This we read in connection attr.
 *
 */
class RS_CONNECT_PROPS_INFO
{
public:

    RS_CONNECT_PROPS_INFO() {
      szHost[0] = '\0';
      iHostNameKeyWordType = 0;
      szPort[0] = '\0';
      iPortKeyWordType = 0;
      szDatabase[0] = '\0';
      iDatabaseKeyWordType = 0;
      szUser[0] = '\0';
      iUserKeyWordType = 0;
      szPassword[0] = '\0';
      iPasswordKeyWordType = 0;
      szDSN[0] = '\0';
      szDriver[0] = '\0';

//      iEnableDescribeParam = 0;
//      iExtendedColumnMetaData = 0;
      iApplicationUsingThreads = 0;
      iFetchRefCursor = 0;
      iTransactionErrorBehavior = 0;
      iConnectionRetryCount = 0;
      iConnectionRetryDelay = 0;
      iQueryTimeout = 0;
      pInitializationString = NULL;

      iTraceLevel = 0;
      iCscEnable = 0;

      llCscMaxFileSize = 0LL;

      szCscPath[0] = '\0';

      llCscThreshold = 0LL;

	  strncpy(szSslMode,"verify-ca",sizeof(szSslMode));
      iEncryptionMethod = 1;
      iValidateServerCertificate = 1;

//      szHostNameInCertificate[0] = '\0';
      szTrustStore[0] = '\0';

      iMultiInsertCmdConvertEnable = 0;

      szKerberosServiceName[0] = '\0';
      szKerberosAPI[0] = '\0';

      iStreamingCursorRows = 0;
	  iDatabaseMetadataCurrentDbOnly = 1;
	  iReadOnly = 0;

	  szKeepAlive[0] = '\0';
	  szKeepAliveIdle[0] = '\0';
	  szKeepAliveCount[0] = '\0';
	  szKeepAliveInterval[0] = '\0';

	  szMinTLS[0] = '\0';

      pConnectStr = NULL;
      cbConnectStr = 0;
      isIAMAuth = false;
      isNativeAuth = false;
      pIamProps = NULL;
      pHttpsProps = NULL;
	  pTcpProxyProps = NULL;

	  szIdpType[0] = '\0';
	  szProviderName[0] = '\0';

	  iClientProtocolVersion = -1;

	  strncpy(szStringType, "varchar", sizeof(szStringType)); // "unspecified"
    }

    char szHost[MAX_IDEN_LEN] = {0};
    int  iHostNameKeyWordType; // SHORT_NAME or LONG_NAME?
    char szPort[MAX_IDEN_LEN] = {0};
    int  iPortKeyWordType;
    char szDatabase[MAX_IDEN_LEN] = {0};
    int  iDatabaseKeyWordType;
    char szUser[MAX_IDEN_LEN] = {0};
    int  iUserKeyWordType;
    char szPassword[PADB_MAX_PARAMETERS] = {0};
    int  iPasswordKeyWordType;
    char szDSN[MAX_IDEN_LEN] = {0};
    char szDriver[MAX_IDEN_LEN] = {0};

/*  If checked, retrieves parameter metadata from the PADB server when a query
    contains parameters (for use with the SQLDescribeParam() ODBC API function). Note that returned
    metadata may not be completely accurate for NUMERIC, VARCHAR, or CHAR data types because the
    parameter metadata does not include precision or scale information 
*/
//    int     iEnableDescribeParam; // 1: Means enable, which is default.


/*  Metadata–If checked, retrieves schema, table name, and column attribute information
    when a query returns data (for use with the SQLDescribeColumn() or SQLColAttribute() ODBC API
    functions). Enabling this option can slow performance.
*/
//    int  iExtendedColumnMetaData; // 1: Means get extended col meta data. 0 is the default.

/*  If checked, calls all ODBC API functions in a thread-safe manner. Enabling this option
    can slow performance. 
*/
    int  iApplicationUsingThreads; // 1: Means enbale thread safety. Default is 1.

/*  If checked, automatically fetches REFCURSOR return values as if the function
    execution is a regular data-returning statement. Requires an enclosing transaction, and does not always
    successfully return column metadata from a REFCURSOR.
*/
    int  iFetchRefCursor;          // 1: Means fetch the ref cursor. Default is 1.

/*  If checked, automatically issues a ROLLBACK statement upon encountering an
    error. If unchecked, the application will need to explicitly issue the ROLLBACK statement.
*/
    int  iTransactionErrorBehavior; // 1: Means rollback on error. Default is 1.

/*  Specifies the number of retry attempts if a network connection cannot be established
    with PADB.
*/
    int     iConnectionRetryCount;     // Default is 0.

/* Number of seconds to wait before each connection retry attempt if Connection Retries is
   greater than zero.
*/
    int  iConnectionRetryDelay;        // Default is 3.

/* Maximum number of seconds for the ODBC driver to wait for PADB to respond to a
   query request. A value of zero indicates an infinite wait.
*/
    int  iQueryTimeout;                // Default is 0.

/*  Specifies one or more queries to automatically execute immediately after PADB
    accepts a connection. Use this parameter to adjust session settings, such as set datestyle to 'ISO
    MDY'. Separate multiple queries with a semi-colon.
*/
    char *pInitializationString;    // On connect execute commands. Default is empty or NULL means no command on connect.

    int     iTraceLevel; // Trace level
    int iLogLevel = -1; // Log level. -1 to mark as Not Set! Good for overriding DSN and/or registry
    char szLogPath[MAX_IDEN_LEN * 10] = {0};

/* Enables or disables the client side cursor. To enable the client side cursor, you must
   set both cscenable and PreloadReader to true. Default is false. 
*/
    int iCscEnable;

/*  The maximum disk cursor size (in MB) per statement for the client side cursor. An
    error occurs if the result set for a single query exceeds this limit. A value of 0 means
    no limit. Negative values are treated as 0. Default is 4000.
*/
    long long llCscMaxFileSize;

/*  The directory in which to store the client side cursor files. If the provided path does
    not exist, the value is changed to the default. Default is TMP/TEMP.
*/
    char szCscPath[MAX_PATH + 1] = {0};

/*  Threshold memory cursor size (in MB). If the client side cursor is enabled and the
    result set for a query exceeds the memory amount in cscthreshold, the excess data
    gets saved in a file in the directory specified in cscpath. A value of 0 means no
    threshold. Negative values are treated as 0. Default is 1.
*/
    long long llCscThreshold;

/*
 * SSLMode set by user. If it's not set then derived from other parameters such as EncryptionMethod,
 * ValidateServerCertificate, szHostNameInCertificate.
 *
 */
    char szSslMode[MAX_IDEN_LEN] = {0};
    char szCaPath[2*MAX_IDEN_LEN] = {0};
    char szCaFile[2*MAX_IDEN_LEN] = {0};


/*  The method the driver uses to encrypt data sent between the driver and the database server. 
    If the specified encryption method is not supported by the database server, the connection fails and the driver returns an error.
    This connection option can affect performance. 
    If set to 0 (No Encryption), data is not encrypted.
    If set to 1 (SSL), data is encrypted using SSL. 
    Default is zero.
*/
    int iEncryptionMethod;

/*  Determines whether the driver validates the certificate that is sent by the database server when SSL encryption is enabled (Encryption Method=1). 
    If set to 1 (Enabled), the driver validates the certificate that is sent by the database server. 
    If set to 0 (Disabled), the driver does not validate the certificate that is sent by the database server. 
    Default is 0.
*/
    int iValidateServerCertificate;

/*  A host name for certificate validation when SSL encryption is enabled (Encryption Method=1) and 
    validation is enabled (Validate Server Certificate=1). 
*/
//    char szHostNameInCertificate[MAX_IDEN_LEN];

/*  The directory that contains the truststore file and the truststore file name to be used when SSL is enabled (Encryption Method=1) 
    and server authentication is used. The truststore file contains a list of the valid Certificate Authorities (CAs) that are trusted 
    by the client machine for SSL server authentication. 
*/
    char szTrustStore[MAX_PATH + 1] = {0};

    // Multi-Insert conversion enable
    int iMultiInsertCmdConvertEnable;

/* Kerberos authentication support specifying the service name 
*/
    char szKerberosServiceName[MAX_IDEN_LEN] = {0};

/* Kerberos authentication support specifying API to use. SSPI or GSS. SSPI only avail on Windows.
*/
    char szKerberosAPI[MAX_IDEN_LEN] = {0};

/* Streaming Cursor size
*/
    int iStreamingCursorRows; // Default is 0
	int iDatabaseMetadataCurrentDbOnly; // Default is 1. 0 means datashare i.e. across multiple databases.
	int iReadOnly; // Default is 0. 1 means READ ONLY session.

    char *pConnectStr;                // Rest of connection option store in this one.
    size_t cbConnectStr;

    // IAM connection options
    bool isIAMAuth;     // The connection key to determine if IAM Authentication is enabled.
    RS_IAM_CONN_PROPS_INFO *pIamProps;
    RS_PROXY_CONN_PROPS_INFO *pHttpsProps;

    bool isNativeAuth;     // The connection property to determine if native auth needs to be used.

	// Redshift TCP Proxy
	RS_TCP_PROXY_CONN_PROPS_INFO *pTcpProxyProps;

	// TCP keep alives
	char szKeepAlive[MAX_NUMBER_BUF_LEN] = {0};
	char szKeepAliveIdle[MAX_NUMBER_BUF_LEN] = {0};
	char szKeepAliveCount[MAX_NUMBER_BUF_LEN] = {0};
	char szKeepAliveInterval[MAX_NUMBER_BUF_LEN] = {0};

	// Min TLS
	char szMinTLS[MAX_NUMBER_BUF_LEN] = {0};

	// Redshift native auth
	char szIdpType[MAX_IDEN_LEN] = {0};
	char szProviderName[MAX_IDEN_LEN] = {0};

	// Bind OID for VARCHAR
	// The type to bind String parameters as (usually 'varchar', 'unspecified' allows implicit casting to other types)
	char szStringType[MAX_SMALL_TEMP_BUF_LEN] = {0};

	int iClientProtocolVersion; // If user sets, the driver uses it otherwise there will be default value hardcoded.
};



/*
 * Connection attributes info.
 */
class RS_CONN_ATTR_INFO
{
public:

    RS_CONN_ATTR_INFO() {

      iAccessMode = SQL_MODE_DEFAULT;
      iAsyncEnable = SQL_ASYNC_ENABLE_DEFAULT;
      iAutoIPD = FALSE; // Read Only
      iAutoCommit = SQL_AUTOCOMMIT_DEFAULT;
      iConnectionTimeout = 0;
      pCurrentCatalog = NULL;
      iLoginTimeout = 0;
      iMetaDataId = SQL_FALSE;
      iOdbcCursors = SQL_CUR_USE_DRIVER;
      iPacketSize = 0;
      hQuietMode = NULL;
      iTrace = SQL_OPT_TRACE_DEFAULT;
      pTraceFile = NULL; // rs_strdup(SQL_OPT_TRACE_FILE_DEFAULT, SQL_NTS);
      pTranslateLib = NULL;
      iTranslateOption = 0;
      iTxnIsolation = SQL_TXN_SERIALIZABLE;

      szApplicationName[0] = '\0';
      szOsUserName[0] = '\0';
      szClientHostName[0] = '\0';
      szClientDomainName[0] = '\0';
      szCompression[0] = '\0';
    }

    int iAccessMode;
    int iAsyncEnable;
    int iAutoIPD;
    int iAutoCommit;
    int iConnectionTimeout;
    char *pCurrentCatalog;
    int iLoginTimeout;
    int iMetaDataId;
    int iOdbcCursors;
    int iPacketSize;
    HANDLE hQuietMode;
    int iTrace;
    char *pTraceFile;
    char *pTranslateLib;
    int  iTranslateOption;
    int  iTxnIsolation;

    // Audit trail information
    char szApplicationName[MAX_TEMP_BUF_LEN];
    char szCompression[MAX_TEMP_BUF_LEN];
    char szOsUserName[MAX_TEMP_BUF_LEN];
    char szClientHostName[MAX_TEMP_BUF_LEN];
    char szClientDomainName[MAX_TEMP_BUF_LEN];
};

/*
 * Statement attributes info.
 */
class RS_STMT_ATTR_INFO
{
public:

    RS_STMT_ATTR_INFO(RS_CONN_INFO *pConn, RS_STMT_INFO *pStmt) {
      pAPD = pStmt->pAPD;
      pARD = pStmt->pARD;
      iAsyncEnable = pConn->pConnAttr->iAsyncEnable; // SQL_ASYNC_ENABLE_DEFAULT;
      iConcurrency = SQL_CONCUR_DEFAULT;
      iCursorScrollable = SQL_NONSCROLLABLE;
      iCursorSensitivity = SQL_UNSPECIFIED;
      iCursorType = SQL_CURSOR_TYPE_DEFAULT;
      iAutoIPD = FALSE; // Read Only
      pFetchBookmarkPtr = NULL;
      iKeysetSize = 0;
      iMaxLength = 0;
      iMaxRows = 0;
      iMetaDataId = pConn->pConnAttr->iMetaDataId; // SQL_FALSE;
      iNoScan = SQL_NOSCAN_DEFAULT;
      iQueryTimeout = pConn->pConnectProps->iQueryTimeout;
      iRetrieveData = SQL_RD_DEFAULT;
      iRowNumber = 0;
      iSimulateCursor = SQL_SC_NON_UNIQUE;
      iUseBookmark = SQL_UB_DEFAULT;
    }

    RS_DESC_INFO *pAPD; /* APD */
    RS_DESC_INFO *pARD; /* ARD */
    int iAsyncEnable;
    int iConcurrency;
    int iCursorScrollable;
    int iCursorSensitivity;
    int iCursorType;
    int iAutoIPD;
    void *pFetchBookmarkPtr;
    int iKeysetSize;
    int iMaxLength;
    int iMaxRows;
    int iMetaDataId;
    int iNoScan;
    int iQueryTimeout;                  
    int iRetrieveData;
    int iRowNumber;                      /* Read Only */
    int iSimulateCursor;
    int iUseBookmark;
};

/*
 * Result info.
 */
class RS_RESULT_INFO
{
public:

    RS_RESULT_INFO(RS_STMT_INFO *_phstmt, PGresult *_pgResult) : cbLenOffsets() {
      phstmt = _phstmt;
      pgResult = _pgResult;

      iNumberOfCols = 0;
      lRowsUpdated = -1L;
      iNumberOfRowsInMem = 0;
      iCurRow = -1;
      iRowOffset = 0;

      iRefCursorInResult = 0;
      pIRDRecs = NULL;

      iPrevhCol = 0;
      pNext = NULL;
    }

    RS_STMT_INFO *phstmt; // Parent HSTMT, who created this result.
    PGresult * pgResult;          // libpq result handle.
    int        iNumberOfCols;      // > 0: means SELECT kind of operation, 0: means INSERT UPDATE DELETE kind of operation.
    long    lRowsUpdated;
    int        iNumberOfRowsInMem; // Number of rows in memory
    int     iCurRow; // -1 means before cursor: 0..n-1 in cursor: >=n means after cursor.
    int     iRowOffset;    // Offset of row 0 in the actual resultset
    int     iRefCursorInResult; // 1 means result contains auto refcursor, 0 means no auto refcursor.

    // Record list as array
    struct _RS_DESC_REC *pIRDRecs; // IRD Records

    int iPrevhCol; // Track column number for SQLGetData. If same col number get called in same row then return SQL_NO_DATA.
    std::map<int, SQLLEN> cbLenOffsets; // keeping per-column state of how much data was processed last time.
    std::unordered_map<std::string, int> columnNameIndexMap; 
    // Next element
    RS_RESULT_INFO *pNext;

    // Methods
    static SQLRETURN setFetchAtFirstRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult);
    static SQLRETURN setFetchAtLastRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult);
    static SQLRETURN setAbsolute(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult, SQLLEN iFetchOffset);
    static int isBeforeFirstRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult);
    static int isAfterLastRow(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult);
    SQLLEN& getColumnReadOffset(int iCol) { return cbLenOffsets[iCol]; }
};


/*
 * Prepare info.
 */
class RS_PREPARE_INFO
{
public:

    RS_PREPARE_INFO(RS_STMT_INFO *_phstmt, PGresult *_pgResult) {
      phstmt = _phstmt;
      pgResult = _pgResult;

      pgResultDescribeParam = NULL;
      iNumberOfParams = 0;
      pIPDRecs = NULL;
      pResultForDescribeCol = NULL;
      pNext = NULL;
    }

    RS_STMT_INFO *phstmt; // Parent HSTMT, who created this prepare.
    PGresult *pgResult;          // libpq prepare handle.
    PGresult *pgResultDescribeParam; // libpq describe prepare handle. 
    int        iNumberOfParams;         // > 0: means parametric command. 0 Means no params.

    // Record list as array
    struct _RS_DESC_REC *pIPDRecs; // IPD Records

    RS_RESULT_INFO *pResultForDescribeCol; // Describe col info after prepare.

    // Next element
    RS_PREPARE_INFO *pNext;
};

SQLRETURN setQueryTimeoutInServer(RS_STMT_INFO *pStmt);

/* Type information
*/
typedef struct _RS_TYPE_INFO
{
    char  szTypeName[MAX_IDEN_LEN];
    short hType;
    int   iColumnSize;
    char  szLiteralPrefix[10]; // ', 0x, {d ', {ts ' etc.
    char  szLiteralSuffix[10]; // ', '} etc.
    char  szCreateParams[MAX_IDEN_LEN]; // length, max length, (precision,scale) etc.
    short hNullable;
    int   iCaseSensitive;
    int   iSearchable;
    int   iUnsigned;
    int   iFixedPrecScale;
    int   iAutoInc;
    char  szLocalTypeName[MAX_IDEN_LEN];
    short hMinScale;
    short hMaxScale;
    short hSqlDataType;
    short hSqlDateTimeSub;
    int   iNumPrexRadix;
    int   iIntervalPrecision;
}RS_TYPE_INFO;

/*
 * Execution thread info.
 */
class RS_EXEC_THREAD_INFO
{
public:

    RS_EXEC_THREAD_INFO() {
      pszCmd = NULL;
      executePrepared = 0;
      hThread = NULL;
      rc = 0;
      iPrepare = 0;
      iExecFromParamData = 0;
    }

    char *pszCmd; // Command. This is allocated buffer using rs_strdup because of async operation.
    int executePrepared; // Execute prepared one or execute direct.
    THREAD_HANDLE hThread; // Thread handle
    SQLRETURN rc;   // Return code
    int iPrepare; // 1 means SQLPrepare
    int iExecFromParamData;
};

// Golbal var
extern RS_GLOBAL_VARS gRsGlobalVars;

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

// Functions prototype
SQLRETURN libpqConnect(RS_CONN_INFO *pConn);
void libpqDisconnect(RS_CONN_INFO *pConn);
void libpqFreeConnect(RS_CONN_INFO *pConn);
char *libpqParameterStatus(RS_CONN_INFO *pConn, const char *paramName);
char *libpqGetNativeSqlState(RS_CONN_INFO *pConn);
char *libpqErrorMsg(RS_CONN_INFO *pConn);
int libpqIsTransactionIdle(RS_CONN_INFO *pConn);
SQLRETURN libpqExecuteTransactionCommand(RS_CONN_INFO *pConn, char *cmd, int iLockRequired);
SQLRETURN libpqCancelQuery(RS_STMT_INFO *pStmt);

SQLRETURN libpqExecuteDirectOrPrepared(RS_STMT_INFO *pStmt, char *pszCmd, int executePrepared);
SQLRETURN libpqExecuteDirectOrPreparedOnThread(RS_STMT_INFO *pStmt, char *pszCmd, int executePrepared, int iCheckForRefCursor, int iLockRequired);

#ifdef WIN32
void
#endif
#if defined LINUX 
void *
#endif
libpqExecuteDirectOrPreparedThreadProc(void *pArg);

void libpqCloseResult(RS_RESULT_INFO *pResult);
short mapPgTypeToSqlType(Oid pgType,short *phPaSpecialType);
char *libpqGetData(RS_RESULT_INFO *pResult, short hCol, int *piLenInd, int *piFormat);
SQLRETURN libpqInitializeResultSetField(RS_STMT_INFO *pStmt, char** tableCol, int colNum, int* tableColDatatype);
SQLRETURN libpqCreateEmptyResultSet(RS_STMT_INFO *pStmt, short columnNum, const int* columnDataType);
SQLRETURN libpqCreateSQLCatalogsCustomizedResultSet(RS_STMT_INFO *pStmt, short columnNum, const std::vector<std::string>& intermediateRS);
SQLRETURN libpqCreateSQLSchemasCustomizedResultSet(RS_STMT_INFO *pStmt, short columnNum, const std::vector<SHOWSCHEMASResult>& intermediateRS);
SQLRETURN libpqCreateSQLTableTypesCustomizedResultSet(RS_STMT_INFO *pStmt, short columnNum, const std::vector<std::string> &tableTypeList);
SQLRETURN libpqCreateSQLTablesCustomizedResultSet(RS_STMT_INFO *pStmt, short columnNum, const std::string &pTableType, const std::vector<SHOWTABLESResult>& intermediateRS);
SQLRETURN libpqCreateSQLColumnsCustomizedResultSet(RS_STMT_INFO *pStmt, short columnNum, const std::vector<SHOWCOLUMNSResult>& intermediateRS);


SQLRETURN libpqPrepare(RS_STMT_INFO *pStmt, char *pszCmd);
SQLRETURN libpqPrepareOnThread(RS_STMT_INFO *pStmt, char *pszCmd);
SQLRETURN libpqPrepareOnThreadWithoutStoringResults(RS_STMT_INFO *pStmt, char *pszCmd);

#ifdef WIN32
void
#endif
#if defined LINUX 
void *
#endif
libpqPrepareThreadProc(void *pArg);

void libpqReleasePrepare(RS_PREPARE_INFO *pPrepare);
void libpqTrace(RS_CONN_INFO *pConn); // Deprecated
SQLRETURN libpqDescribeParams(RS_STMT_INFO *pStmt, RS_PREPARE_INFO *pPrepare, PGresult *pgResult);
SQLRETURN libpqExecuteDeallocateCommand(RS_STMT_INFO *pStmt, int iLockRequired, int calledFromDrop);
void initLibpq(FILE    *fpTrace);
void uninitLibpq();

void *libpqFreemem(void *ptr);

ConnStatusType libpqConnectionStatus(RS_CONN_INFO *pConn);

void libpqSetStreamingCursorRows(RS_STMT_INFO *pStmt);
void libpqSetEndOfStreamingCursorQuery(RS_STMT_INFO *pStmt, int flag);
int libpqIsEndOfStreamingCursor(RS_STMT_INFO *pStmt);
int libpqIsEndOfStreamingCursorQuery(RS_STMT_INFO *pStmt);
int isForwardOnlyCursor(void *pCallerContext);
int isStreamingCursorMode(void *pCallerContext);
void libpqCheckAndSkipAllResultsOfStreamingCursor(RS_STMT_INFO *pStmt, int iLockRequired);
int libpqSkipCurrentResultOfStreamingCursor(RS_STMT_INFO *pStmt, void *_pCscStatementContext, PGresult *pgResult, PGconn *conn, int iLockRequired);
void libpqReadNextBatchOfStreamingRows(RS_STMT_INFO *pStmt, void *_pCscStatementContext, PGresult *pgResult, PGconn *conn,int *piError,int iLockRequired);
short libpqReadNextResultOfStreamingCursor(RS_STMT_INFO *pStmt, void *_pCscStatementContext, PGconn *conn, int iLockRequired);
int libpqDoesAnyOtherStreamingCursorOpen(RS_STMT_INFO *pStmt, int iLockRequired);

SQLRETURN  SQL_API RS_SQLAllocHandle(SQLSMALLINT hHandleType,
                                    SQLHANDLE pInputHandle, 
                                    SQLHANDLE *ppOutputHandle);

SQLRETURN  SQL_API RS_SQLFreeHandle(SQLSMALLINT hHandleType, 
                                    SQLHANDLE pHandle);

int  RS_SQLGetPrivateProfileString(const char *pSectionName, char *pKey, char *pDflt, char *pReturn, int iSize, char *pFile);
int RS_GetPrivateProfileString(const char *pSectionName, const char *pKey, const char *pDflt, char *pReturn, int iSize, char *pFile);

#ifdef __cplusplus
}
#endif /* C++ */



