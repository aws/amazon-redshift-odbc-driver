/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsodbc.h"
#include "rs_pq_type.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "rstrace.h"
#include "rslock.h"
#include "rsdrvinfo.h"
#include "rsMetadataAPIPostProcessor.h"
#include <regex>

#ifdef LINUX
#include <sys/utsname.h>
#endif
#include <rslog.h>

#define MAX_CONNECT_PROPS 128

#ifdef __cplusplus
extern "C" {
#endif

static void getResultDescription(PGresult *pgResult, RS_RESULT_INFO *pResult, int iFetchRefCursor);
static RS_RESULT_INFO *createResultObject(RS_STMT_INFO *pStmt, PGresult *pgResult);

int getCscThreadCreatedFlag(void *_pCscStatementContext);
void setCscThreadCreatedFlag(void *_pCscStatementContext, int flag);
int pgCloseCsc(PGresult * pgResult);

SQLRETURN setResultInStmt(SQLRETURN rc, RS_STMT_INFO *pStmt, PGresult *pgResult, int readStatusFlag, ExecStatusType pqRc, 
                          int *piStop,int iArrayBinding);
void pgSetSkipAllResultsCsc(void *_pCscStatementContext, int val);
void pgWaitForCscThreadToFinish(PGconn *conn, int calledFromConnectionClose);
void initCscLib(FILE    *fpTrace);
void uninitCscLib();
int getUnknownTypeSize(Oid pgType);
void setStreamingCursorRows(void *_pCscStatementContext, int iStreamingCursorRows);

void setEndOfStreamingCursorQuery(void *_pCscStatementContext, int flag);
int isEndOfStreamingCursor(void *_pCscStatementContext);
int isEndOfStreamingCursorQuery(void *_pCscStatementContext);

int getMaxRowsForCsc(void *pCallerContext);
int getResultSetTypeForCsc(void *pCallerContext);
short RS_ResultHandlerCallbackFunc(void *pCallerContext, PGresult *pgResult);
int getResultConcurrencyTypeForCsc(void *pCallerContext);

#ifdef __cplusplus
}
#endif


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Connect using libpq with given connection attributes.
//
SQLRETURN libpqConnect(RS_CONN_INFO *pConn)
{
//    RS_ENV_INFO *pEnv = pConn->phenv;
    RS_CONNECT_PROPS_INFO *pConnectProps = pConn->pConnectProps;
    RS_CONN_ATTR_INFO *pConnAttr = pConn->pConnAttr;
    char **ppKeywords = (char **)rs_calloc(MAX_CONNECT_PROPS, sizeof(char *));
    char **ppValues = (char **)rs_calloc(MAX_CONNECT_PROPS, sizeof(char *));
    PGconn * pgConn = NULL;
    char *pError = NULL;
    int fail = FALSE;
    int iCount = 0;
    char szLoginTimeout[MAX_NUMBER_BUF_LEN];
    int  networkError = 0;
    int  connectRetryCount = pConnectProps->iConnectionRetryCount;
    char szCscEnable[MAX_NUMBER_BUF_LEN];
    char szCscMaxFileSize[MAX_NUMBER_BUF_LEN];
    char szCscThreshold[MAX_NUMBER_BUF_LEN];
    char szSslMode[MAX_IDEN_LEN];
    char szSslRootCert[MAX_PATH + 1];
    char szlibpqConnectionTraceFile[MAX_PATH + 1];
    char szStreamingCursorRows[MAX_NUMBER_BUF_LEN];
    char szSslDefaultCertPath [MAX_PATH + 1];
	char szClientProtocolVersion[MAX_NUMBER_BUF_LEN];
	char szOsVersion[MAX_TEMP_BUF_LEN];
	char szDriverVersion[MAX_TEMP_BUF_LEN];

    if(ppKeywords && ppValues)
    {
        ppKeywords[iCount] = "host";
        ppValues[iCount++] = pConnectProps->szHost;

        ppKeywords[iCount] = "user";
		ppValues[iCount++] = (pConnectProps->szUser[0] == '\0') ? NULL : pConnectProps->szUser;

        ppKeywords[iCount] = "password";
        ppValues[iCount++] = pConnectProps->szPassword;

        ppKeywords[iCount] = "port";
        ppValues[iCount++] = pConnectProps->szPort;

        ppKeywords[iCount] = "dbname";
        ppValues[iCount++] = pConnectProps->szDatabase;

        // CscEnable
        snprintf(szCscEnable,sizeof(szCscEnable),"%d",pConnectProps->iCscEnable);
        ppKeywords[iCount] = "CscEnable";
        ppValues[iCount++] = szCscEnable;

        if(pConnectProps->iCscEnable)
        {
            // CscMaxFileSize
            snprintf(szCscMaxFileSize,sizeof(szCscMaxFileSize), "%lld",pConnectProps->llCscMaxFileSize);
            ppKeywords[iCount] = "CscMaxFileSize";
            ppValues[iCount++] = szCscMaxFileSize;

            // CscPath
            ppKeywords[iCount] = "CscPath";
            ppValues[iCount++] = pConnectProps->szCscPath;

            // CscThreshold
            snprintf(szCscThreshold,sizeof(szCscThreshold),"%lld",pConnectProps->llCscThreshold);
            ppKeywords[iCount] = "CscThreshold";
            ppValues[iCount++] = szCscThreshold;
        }

        if(pConnAttr)
        {
            // Login timeout
            snprintf(szLoginTimeout,sizeof(szLoginTimeout), "%d",pConnAttr->iLoginTimeout);
            ppKeywords[iCount] = "connect_timeout";
            ppValues[iCount++] = szLoginTimeout;
        }

        // SSL parameters
        szSslMode[0] = '\0';
        szSslRootCert[0] = '\0';

        rs_strncpy(szSslMode, pConnectProps->szSslMode,sizeof(szSslMode));

        if(szSslMode[0] == '\0')
        {
          // sslmode
          if(pConnectProps->iEncryptionMethod == 0)
			  rs_strncpy(szSslMode,"disable", sizeof(szSslMode));
          else
		  if(pConnectProps->iEncryptionMethod == 1)
			  rs_strncpy(szSslMode, "verify-ca", sizeof(szSslMode));
		  else
		  if (pConnectProps->iEncryptionMethod == 2)
			  rs_strncpy(szSslMode,"verify-full", sizeof(szSslMode));
		  else
		  if (pConnectProps->iEncryptionMethod == 3)
			  rs_strncpy(szSslMode,"require", sizeof(szSslMode));
        }

        ppKeywords[iCount] = "sslmode";
        ppValues[iCount++] = szSslMode;

        if(_stricmp(szSslMode,"disable") != 0)
        {
          char *driverPath = getDriverPath();
          if (driverPath != NULL && *driverPath != '\0')
          {
            snprintf(szSslDefaultCertPath, sizeof(szSslDefaultCertPath), "%s%c%s",driverPath, PATH_SEPARATOR_CHAR, REDSHIFT_ROOT_CERT_FILE);
            // ssldefaultrootcert
            if(szSslDefaultCertPath[0] != '\0')
            {
                ppKeywords[iCount] = "ssldefaultrootcert";
                ppValues[iCount++] = szSslDefaultCertPath;
            }
          }

		  if (driverPath)
			  free(driverPath);


          if(pConnectProps->szTrustStore[0] != '\0')
              rs_strncpy(szSslRootCert, pConnectProps->szTrustStore,sizeof(szSslRootCert));
        }

        // sslrootcrt
        if (szSslRootCert[0] == '\0') {
            // try setting using CaFile or CaPath
            if (pConnectProps->szCaFile[0] != '\0') {
                rs_strncpy(szSslRootCert, pConnectProps->szCaFile,
                           sizeof(szSslRootCert));
            } else if (pConnectProps->szCaPath[0] != '\0') {
                snprintf(szSslRootCert, sizeof(szSslRootCert), "%s%c%s",
                         pConnectProps->szCaPath, PATH_SEPARATOR_CHAR,
                         REDSHIFT_ROOT_CERT_FILE);
            }
        }

        if(szSslRootCert[0] != '\0')
        {
            ppKeywords[iCount] = "sslrootcert";
            ppValues[iCount++] = szSslRootCert;
        }

        // Kerberos
        if(pConnectProps->szKerberosServiceName[0] != '\0')
        {
            ppKeywords[iCount] = "krbsrvname";
            ppValues[iCount++] = pConnectProps->szKerberosServiceName;
        }

#ifdef WIN32
        if(pConnectProps->szKerberosAPI[0] != '\0' && strcmp(pConnectProps->szKerberosAPI, "GSS") == 0)
        {
			// Set Kerberos API as GSS. Default is SSPI on Windows.
            ppKeywords[iCount] = "gsslib";
            ppValues[iCount++] = "gssapi";
        }
#endif
		if(IS_TRACE_LEVEL_DEBUG())
		{
			RS_LOG_DEBUG("RSLIBPQ", "pConnectProps->szKerberosServiceName=%s", pConnectProps->szKerberosServiceName);
			RS_LOG_DEBUG("RSLIBPQ", "pConnectProps->szKerberosAPI=%s", pConnectProps->szKerberosAPI);
		}

        if(pConnectProps->iStreamingCursorRows > 0)
        {
            // StreamingCursorRows
            snprintf(szStreamingCursorRows,sizeof(szStreamingCursorRows), "%d",pConnectProps->iStreamingCursorRows);
            ppKeywords[iCount] = "StreamingCursorRows";
            ppValues[iCount++] = szStreamingCursorRows;
		}

		// client_protocol_version
		if(pConnectProps->iClientProtocolVersion != -1)
			snprintf(szClientProtocolVersion, sizeof(szClientProtocolVersion), "%d", pConnectProps->iClientProtocolVersion);
		else
			snprintf(szClientProtocolVersion, sizeof(szClientProtocolVersion), "%d", EXTENDED_RESULT_METADATA_SERVER_PROTOCOL_VERSION);
		ppKeywords[iCount] = "client_protocol_version";
		ppValues[iCount++] = szClientProtocolVersion;

		// Driver version
		ppKeywords[iCount] = "driver_version";
		snprintf(szDriverVersion, sizeof(szDriverVersion),"Redshift ODBC Driver %s", ODBC_DRIVER_VERSION);
		ppValues[iCount++] = szDriverVersion;

		// OS version
#if defined(WIN32) || defined(_WIN64)
		DWORD dwVersion = 0;
		DWORD dwMajorVersion = 0;
		DWORD dwMinorVersion = 0;
		DWORD dwBuild = 0;
		dwVersion = GetVersion();
		dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
		dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
		if (dwVersion < 0x80000000)
		{
			dwBuild = (DWORD)(HIWORD(dwVersion));
		}


		// Processor Architecture
		SYSTEM_INFO siSysInfo;
		GetSystemInfo(&siSysInfo);
		char *w_arch = "Unknown";

		switch (siSysInfo.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_AMD64:
		{
			w_arch = "x64";
			break;
		}
		case PROCESSOR_ARCHITECTURE_ARM:
		{
			w_arch = "ARM";
			break;
		}
		case PROCESSOR_ARCHITECTURE_IA64:
		{
			w_arch = "Intel Itanium";
			break;
		}
		case PROCESSOR_ARCHITECTURE_UNKNOWN:
		default:
		{
			w_arch = "Unknown";
		}
		}

		snprintf(szOsVersion, sizeof(szOsVersion), "Windows  %ld.%ld(%ld) %s", dwMajorVersion, dwMinorVersion, dwBuild, w_arch);

#else
		struct utsname buffer;

		if (0 == uname(&buffer))
		{
			snprintf(szOsVersion, sizeof(szOsVersion),"%s %s %s %s", buffer.sysname, buffer.release, buffer.machine, buffer.version);
		}
		else
			snprintf(szOsVersion, sizeof(szOsVersion), "%s", "Unknown");

#endif // #if defined(WIN32) || defined(_WIN64)

		ppKeywords[iCount] = "os_version";
		ppValues[iCount++] = szOsVersion;

		// Plugin name
		char *plugin_name = (char *)((pConnectProps->isIAMAuth || pConnectProps->isNativeAuth)
			? ((pConnectProps->pIamProps->szPluginName[0] != '\0')
				? pConnectProps->pIamProps->szPluginName : "none")
			: "none");
		ppKeywords[iCount] = "plugin_name";
		ppValues[iCount++] = plugin_name;
		if(IS_TRACE_LEVEL_DEBUG()) {
			RS_LOG_DEBUG("RSLIBPQ", "using plugin_name=%s", plugin_name);
		}

		// TCP Proxy
		if (pConnectProps->pTcpProxyProps
			&&  pConnectProps->pTcpProxyProps->szHost[0] != '\0')
		{
			ppKeywords[iCount] = "proxy_host";
			ppValues[iCount++] = pConnectProps->pTcpProxyProps->szHost;
			ppKeywords[iCount] = "proxy_port";
			ppValues[iCount++] = pConnectProps->pTcpProxyProps->szPort;
			ppKeywords[iCount] = "proxy_user";
			ppValues[iCount++] = pConnectProps->pTcpProxyProps->szUser;
			ppKeywords[iCount] = "proxy_credentials";
			ppValues[iCount++] = pConnectProps->pTcpProxyProps->szPassword;
		}

		// Keep alive
		if (pConnectProps->szKeepAlive[0] != '\0')
		{
			ppKeywords[iCount] = "keepalives";
			ppValues[iCount++] = pConnectProps->szKeepAlive;
			if (pConnectProps->szKeepAliveIdle[0] != '\0')
			{
				ppKeywords[iCount] = "keepalives_idle";
				ppValues[iCount++] = pConnectProps->szKeepAliveIdle;
			}
			if (pConnectProps->szKeepAliveCount[0] != '\0')
			{
				ppKeywords[iCount] = "keepalives_count";
				ppValues[iCount++] = pConnectProps->szKeepAliveCount;
			}
			if (pConnectProps->szKeepAliveInterval[0] != '\0')
			{
				ppKeywords[iCount] = "keepalives_interval";
				ppValues[iCount++] = pConnectProps->szKeepAliveInterval;
			}
		}

		// Min TLS
		if (pConnectProps->szMinTLS[0] != '\0')
		{
			ppKeywords[iCount] = "min_tls";
			ppValues[iCount++] = pConnectProps->szMinTLS;
		}

		if (pConnectProps->szIdpType[0] != '\0')
		{
			ppKeywords[iCount] = "idp_type";
			ppValues[iCount++] = pConnectProps->szIdpType;
		}

		if(pConnectProps->pIamProps) {
            if(_stricmp(plugin_name, PLUGIN_BROWSER_IDC_AUTH) == 0) {
                ppKeywords[iCount] = RS_TOKEN_TYPE;
                ppValues[iCount++] = RS_TOKEN_TYPE_ACCESS_TOKEN;
            } else if(_stricmp(plugin_name, PLUGIN_IDP_TOKEN_AUTH) == 0 && pConnectProps->pIamProps->szTokenType[0] != '\0') {
				ppKeywords[iCount] = RS_TOKEN_TYPE;
				ppValues[iCount++] = pConnectProps->pIamProps->szTokenType;
			}
		}

		if (pConnectProps->pIamProps &&
			 (_stricmp(plugin_name, PLUGIN_IDP_TOKEN_AUTH) == 0) &&
			pConnectProps->pIamProps->szIdentityNamespace[0] != '\0') {
			ppKeywords[iCount] = RS_IDENTITY_NAMESPACE;
			ppValues[iCount++] = pConnectProps->pIamProps->szIdentityNamespace;
			if(IS_TRACE_LEVEL_DEBUG()) {
				RS_LOG_DEBUG("RSLIBPQ", "using identity_namespace=%s", pConnectProps->pIamProps->szIdentityNamespace);
			}
		}

		if (pConnectProps->szProviderName[0] != '\0')
		{
			ppKeywords[iCount] = "provider_name";
			ppValues[iCount++] = pConnectProps->szProviderName;
		}

		if (pConnectProps->pIamProps
			&& pConnectProps->pIamProps->pszJwt)
		{
			ppKeywords[iCount] = "web_identity_token";
			ppValues[iCount++] = pConnectProps->pIamProps->pszJwt;
		}

		if(pConnAttr->szApplicationName[0] == '\0')
			getApplicationName(pConn);
		if (pConnAttr->szApplicationName[0] != '\0')
		{
			ppKeywords[iCount] = "application_name";
			ppValues[iCount++] = pConnAttr->szApplicationName;
		}

		if (pConnAttr->szCompression[0] != '\0')
		{
			ppKeywords[iCount] = "compression";
			ppValues[iCount++] = pConnAttr->szCompression;
		}

		// This should be last parameter
        if (IS_TRACE_LEVEL_DEBUG())
        {
            RS_LOG_DEBUG("RSLIBPQ",
                            "pConnectProps->iStreamingCursorRows=%d",
                            pConnectProps->iStreamingCursorRows);
            RS_LOG_DEBUG("RSLIBPQ", "pConnectProps->iCscEnable=%d",
                            pConnectProps->iCscEnable);
            if (pConnectProps->iCscEnable) {
                RS_LOG_DEBUG(
                    "RSLIBPQ",
                    "Possible Cursor Mode (if rowset size exceed memory limit and may consume disk space): Client Side Cursor");
            } else if (pConnectProps->iStreamingCursorRows > 0) {
                RS_LOG_DEBUG("RSLIBPQ", "Possible Cursor Mode (if FWD only cursor): Streaming Cursor");
            } else {
                RS_LOG_DEBUG(
                    "RSLIBPQ",
                    "Possible Cursor Mode (may go OOM or slow down system depends on size of rows v/s actual physical memory): All-in-Memory Cursor");
            }
        }

		szlibpqConnectionTraceFile[0] = '\0';
		if(IS_TRACE_LEVEL_MSG_PROTOCOL())
		{
			char *pTraceFileName = getTraceFileName();
			if(pTraceFileName != NULL && pTraceFileName[0] != '\0')
			{
				snprintf(szlibpqConnectionTraceFile,sizeof(szlibpqConnectionTraceFile),"%s.1",pTraceFileName);

				// Enable tracing during connection. Keep this parameter last, so we can remove it in PQconnectdbParams.
				ppKeywords[iCount] = "libpqConnectionTracingFile";
				ppValues[iCount++] = szlibpqConnectionTraceFile;
			}
		}


        // Last terminated as NULL
        ppKeywords[iCount] = NULL; 

        do
        {
            networkError = FALSE;
            pgConn = PQconnectdbParams((const char **)ppKeywords, (const char **)ppValues, FALSE);

            // Release previous connection resources
            libpqFreeConnect(pConn);

            // Set new connection
            pConn->pgConn = pgConn;

            // Enable libpq tracing
            // libpqTrace(pConn); // Deprecated infavor of rslog

            pError = libpqErrorMsg(pConn);

            fail = (pError && *pError != '\0'); 

            if(pError) {
                RS_LOG_ERROR("RSLIBPQ", "pError=%s", pError);
            }
            else {
                RS_LOG_DEBUG("RSLIBPQ", "No error after PQconnectdbParams");
            }

            if(fail && pError && strstr(pError, NETWORK_ERR_MSG_TEXT))
            {
                if(connectRetryCount > 0)
                {
                    connectRetryCount--;
                    networkError = TRUE;
                    Sleep(pConnectProps->iConnectionRetryDelay * 1000);
                }
            }
        }while(networkError);

        if(fail)
            addError(&pConn->pErrorList,"HY000", pError, 0, pConn);

        if(!pgConn) 
            fail = TRUE;

        if(!fail)
        {
            char *pTemp = libpqParameterStatus(pConn,"padb_version"); // server_version

            // We will get and send the audit trail info as SET commands. 
            // getAuditTrailInfo(pConn);

            // Send audit trail info
            // onConnectAuditInfoExecute(pConn);

            // Check for on connect execute
            if(pConnectProps->pInitializationString && *(pConnectProps->pInitializationString))
            {
                SQLRETURN rc = onConnectExecute(pConn,pConnectProps->pInitializationString);
                fail = (rc != SQL_SUCCESS);
            }

			// Check for READONLY
			if (!fail 
				&& ((pConnAttr && pConnAttr->iAccessMode == SQL_MODE_READ_ONLY)
				|| (pConnectProps->iReadOnly == 1))
			)
			{
				SQLRETURN rc = onConnectExecute(pConn, "SET READONLY=1");
				fail = (rc != SQL_SUCCESS);
			}

            if(!fail)
                pConn->iStatus = RS_OPEN_CONNECTION;
        }
    }
    else
        fail = TRUE;

    ppKeywords = (char **)rs_free(ppKeywords);
    ppValues = (char **)rs_free(ppValues);

    return (fail) ? SQL_ERROR : SQL_SUCCESS;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Disconnect using libpq.
//
void libpqDisconnect(RS_CONN_INFO *pConn)
{
    if(pConn->pgConn != NULL)
    {
        // Wait for any csc thread executing, otherwise it may leads to IOException and some log on server side.
        pgWaitForCscThreadToFinish(pConn->pgConn, TRUE);

        pqCloseConnection(pConn->pgConn);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release libpq resources.
//
void libpqFreeConnect(RS_CONN_INFO *pConn)
{
    if(pConn->pgConn != NULL)
    {
        pqFreeConnection(pConn->pgConn);
        pConn->pgConn = NULL;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get connection parameter status using libpq.
//
char *libpqParameterStatus(RS_CONN_INFO *pConn, const char *paramName)
{
    return (char *) PQparameterStatus(pConn->pgConn,paramName); 
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get SQLState using libpq.
//
char *libpqGetNativeSqlState(RS_CONN_INFO *pConn)
{
    return (pConn) ? pqGetNativeSqlState(pConn->pgConn) : NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get error message.
//
char *libpqErrorMsg(RS_CONN_INFO *pConn)
{
    return PQerrorMessage(pConn->pgConn);
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Return TRUE if transaction status is idle otherwise FALSE.
//
int libpqIsTransactionIdle(RS_CONN_INFO *pConn)
{
    PGTransactionStatusType pgTxnStatus = PQtransactionStatus(pConn->pgConn);

    return(pgTxnStatus == PQTRANS_IDLE);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Execute BEGIN, END, COMMIT or ROLLBACK kind of transaction command.
//
SQLRETURN libpqExecuteTransactionCommand(RS_CONN_INFO *pConn, char *cmd, int iLockRequired)
{
    int fail = FALSE;
    PGresult *pgResult;


    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);

        // Wait for current csc thread to finish, if any.
        pgWaitForCscThreadToFinish(pConn->pgConn, FALSE);
    }

	// We have to release any result of streaming cursor before executing internal command
	skipAllResultsOfStreamingRowsUsingConnection(pConn);

    pgResult = PQexec(pConn->pgConn, cmd);

    if(PQresultStatus(pgResult) != PGRES_COMMAND_OK)
    {
        char *pError = libpqErrorMsg(pConn);

        fail = (pError && *pError != '\0'); 

        if(fail)
            addError(&pConn->pErrorList,"HY000", pError, 0, pConn);
    }

    PQclear(pgResult);
    pgResult = NULL;

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }

    return (fail) ? SQL_ERROR : SQL_SUCCESS;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Cancel the query.
//
// You don't have to lock on cancel request, but need to drain the pending result after sending cancel.
SQLRETURN libpqCancelQuery(RS_STMT_INFO *pStmt)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = pStmt->phdbc;
    PGcancel *pgCancelObj = PQgetCancel(pConn->pgConn);

    if(pgCancelObj)
    {
        char errBuf[MAX_ERR_MSG_LEN + 1];
        int valid; 
            
        errBuf[0] = '\0';
        errBuf[MAX_ERR_MSG_LEN] = '\0';
        valid = PQcancel(pgCancelObj, errBuf, MAX_ERR_MSG_LEN);
        if(!valid && errBuf[0] != '\0')
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY000", errBuf, 0, NULL);
        }

        PQfreeCancel(pgCancelObj);
        pgCancelObj = NULL;

        if(rc == SQL_SUCCESS)
        {
            // Skip all results
            pgSetSkipAllResultsCsc(pStmt->pCscStatementContext, TRUE);  
        }
    }
    else
    {
        char *pError = libpqErrorMsg(pConn);

        rc = SQL_ERROR;
        if(pError && *pError != '\0')
            addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Execute direct or prepare the SQL statement using libpq.
//
SQLRETURN libpqExecuteDirectOrPrepared(RS_STMT_INFO *pStmt, char *pszCmd, int executePrepared)
{
    SQLRETURN rc;
    int asyncEnable = isAsyncEnable(pStmt);

    if(!asyncEnable)
    {
        rc = libpqExecuteDirectOrPreparedOnThread(pStmt, pszCmd, executePrepared, TRUE, TRUE);
    }
    else
    {
        // Execute async on thread
        if(pStmt->pExecThread == NULL
            || pStmt->pExecThread->rc == SQL_NEED_DATA)
        {
            RS_EXEC_THREAD_INFO *pExecThread = pStmt->pExecThread;

            if(pExecThread == NULL)
            {
                pStmt->pExecThread = pExecThread = (RS_EXEC_THREAD_INFO *)new RS_EXEC_THREAD_INFO();
                if(pExecThread == NULL)
                {
                    rc = SQL_ERROR;
                    addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, NULL);
                    goto error;
                }
            }

            // Set thread execution context
            pExecThread->executePrepared = executePrepared;
            pExecThread->pszCmd = (char *)rs_free(pExecThread->pszCmd);
            pExecThread->pszCmd = rs_strdup(pszCmd, SQL_NTS);
            pExecThread->rc = SQL_STILL_EXECUTING;
            pExecThread->iPrepare = 0;

            // create thread
            pExecThread->hThread = rsCreateThread((void *)libpqExecuteDirectOrPreparedThreadProc, pStmt);
            if(pExecThread->hThread == (THREAD_HANDLE)(long) NULL)
            {
                // Release thread info
                waitAndFreeExecThread(pStmt, FALSE);

                // Execute on same thread
                rc = libpqExecuteDirectOrPreparedOnThread(pStmt, pszCmd, executePrepared, TRUE, TRUE);
            }
            else
                rc = SQL_STILL_EXECUTING;
        }
        else
        {
            rc = checkExecutingThread(pStmt);
            if(rc != SQL_STILL_EXECUTING)
                waitAndFreeExecThread(pStmt, FALSE);
        }
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Execute direct or prepare on a separate thread using this thread procedure.
//
#ifdef WIN32
void
#endif
#if defined LINUX 
void *
#endif
libpqExecuteDirectOrPreparedThreadProc(void *pArg)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pArg;
    RS_EXEC_THREAD_INFO *pExecThread = pStmt->pExecThread;
    SQLRETURN rc;

    rc = libpqExecuteDirectOrPreparedOnThread(pStmt, pExecThread->pszCmd, pExecThread->executePrepared, TRUE, TRUE);

    setThreadExecutionStatus(pExecThread, rc);

#ifdef WIN32
    return;
#endif
#if defined LINUX 
    return NULL;
#endif
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Execute direct or prepare on a separate thread.
//
// executePrepared = 1, if execute prepared. 0 means execute direct.
SQLRETURN libpqExecuteDirectOrPreparedOnThread(RS_STMT_INFO *pStmt, char *pszCmd, int executePrepared, int iCheckForRefCursor, int iLockRequired)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = pStmt->phdbc;
    int  iNumBindParams = 0;
    char **ppBindParamVals = NULL;
    int *piParamFormats = NULL;
    RS_BIND_PARAM_STR_BUF *pBindParamStrBuf = NULL;
    int iBindParam;
    int iBeginCommand = FALSE;
    int iCscThreadCreated = FALSE;
    int iLastBatchMultiInsertPrepare = FALSE;
    std::vector<Oid> paramTypes;
    // Use for legacy functions that need pointer and need to indicate null as
    // empty
    auto getParamTypesPtr = [&]() -> const Oid * {
        return paramTypes.empty() ? nullptr : paramTypes.data();
    };

    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);

        // Wait for current csc thread to finish, if any.
        pgWaitForCscThreadToFinish(pConn->pgConn, FALSE);
    }

    if((pszCmd && !executePrepared) || executePrepared)
    {
        PGresult *pgResult = NULL;
        ExecStatusType pqRc = PGRES_COMMAND_OK;
        int asyncEnable = isAsyncEnable(pStmt);
        int sendStatus = 1;

        // Set query timeout in the server
        rc = setQueryTimeoutInServer(pStmt);

        if(rc == SQL_SUCCESS)
        {
            RS_DESC_HEADER &pIPDDescHeader = pStmt->pIPD->pDescHeader;
            RS_DESC_HEADER &pAPDDescHeader = pStmt->pStmtAttr->pAPD->pDescHeader;

            // Bind array/single value
            long lParamsToBind = (pAPDDescHeader.lArraySize <= 0) ? 1 : pAPDDescHeader.lArraySize;
            long lParamProcessed = 0;
            int  iArrayBinding = (lParamsToBind > 1);
            int  iBindOffset = (pAPDDescHeader.plBindOffsetPtr) ?  *((int*)pAPDDescHeader.plBindOffsetPtr) : 0;
            int  iMultiInsert = pStmt->iMultiInsert;
            int  iLastBatchMultiInsert = pStmt->iLastBatchMultiInsert;
            int  iOffset = 0;

            for(lParamProcessed = 0; lParamProcessed < lParamsToBind; lParamProcessed++)
            {
                if(pStmt->pStmtAttr->pAPD->pDescRecHead)
                {
                    // Look for param at exec
                    if(!iMultiInsert && needDataAtExec(pStmt, pStmt->pStmtAttr->pAPD->pDescRecHead, lParamProcessed, executePrepared))
                    {
                        // Store arguments value for data-at-exec
                        pStmt->pszCmdDataAtExec = pszCmd;
                        pStmt->iExecutePreparedDataAtExec = executePrepared;
                        pStmt->lParamProcessedDataAtExec = lParamProcessed;
                        rc = SQL_NEED_DATA;
                    }
                    else
                    {
                        RS_DESC_REC *pDescRec;
                        RS_DESC_REC *pIPDRec;
                        int iConversionError;
                        int iNoOfParams = (pStmt->pPrepareHead) ? getNumberOfParams(pStmt) : getParamMarkerCount(pStmt);

                        iNumBindParams = countBindParams(pStmt->pStmtAttr->pAPD->pDescRecHead);

						if(!executePrepared && (pStmt->iNumOfOutOnlyParams > 0
													|| (!iArrayBinding && iNumBindParams > 0)))
							paramTypes = getParamTypes(iNumBindParams, pStmt->pStmtAttr->pAPD->pDescRecHead, pConn->pConnectProps);

                        // If user bind more than actually in query, we ignore unused.
                        if(iNoOfParams < iNumBindParams)
                            iNumBindParams = iNoOfParams;

                        if(!iMultiInsert)
                        {
                            if(iNumBindParams > 0)
                            {
                                ppBindParamVals = (char **)rs_calloc(iNumBindParams, sizeof(char *));
                                piParamFormats  = (int *)rs_calloc(iNumBindParams, sizeof(int));
                                pBindParamStrBuf = (RS_BIND_PARAM_STR_BUF *)rs_calloc(iNumBindParams, sizeof(RS_BIND_PARAM_STR_BUF));

                                if(ppBindParamVals == NULL 
                                    || piParamFormats == NULL
                                    || pBindParamStrBuf == NULL)
                                {
                                    rc = SQL_ERROR;
                                    goto error;
                                }
                            }
                        }
                        else
                        {
                            if((iNumBindParams > 0) && (ppBindParamVals == NULL))
                            {
                                ppBindParamVals = (char **)rs_calloc(iNumBindParams * iMultiInsert, sizeof(char *));
                                piParamFormats  = (int *)rs_calloc(iNumBindParams * iMultiInsert, sizeof(int));
                                pBindParamStrBuf = (RS_BIND_PARAM_STR_BUF *)rs_calloc(iNumBindParams * iMultiInsert, sizeof(RS_BIND_PARAM_STR_BUF));

                                if(ppBindParamVals == NULL 
                                    || piParamFormats == NULL
                                    || pBindParamStrBuf == NULL)
                                {
                                    rc = SQL_ERROR;
                                    goto error;
                                }
                            }
                        }

                        for(iBindParam = 0; iBindParam < iNumBindParams; iBindParam++)
                        {
                            // iValOffset is a stride = how many bytes to jump to reach the next row’s data or indicator.
                            SQLLEN iValOffset = 0;

                            pDescRec = findDescRec(pStmt->pStmtAttr->pAPD, iBindParam + 1);
                            pIPDRec  = findDescRec(pStmt->pIPD, iBindParam + 1);

                            if(!pDescRec)
                            {
                                rc = SQL_ERROR;
                                addError(&pStmt->pErrorList,"HY000", "Bind parameter not found.", 0, NULL);
                                goto error;
                            }

                            if (iArrayBinding) {
                                if (pAPDDescHeader.lBindType == SQL_BIND_BY_COLUMN) {
                                    // Column wise binding
                                    // If you are in column‑wise binding, iValOffset is handled per column (iOctetLen) which is the column size.
                                    iValOffset = pDescRec->iOctetLen;
                                }
                                else {
                                    // Row wise binding
                                    // If you are in row‑wise binding, lBindType is set to the size of your row structure 
                                    iValOffset = pAPDDescHeader.lBindType;   // how far to jump to get to next row
                                }
                                
                                if (iValOffset <= 0) {
                                    rc = SQL_ERROR;
                                    std::string err = "Invalid Array element length :" + std::to_string(iValOffset);
                                    RS_LOG_ERROR("rslibpq", "%s", err.data());
                                    addError(&pStmt->pErrorList, "HY000", err.data(), 0, NULL);
                                    goto error;
                                }
                            }

                            iOffset = (iMultiInsert) ? iOffset : iBindParam;

                            if (pDescRec->hInOutType == SQL_PARAM_OUTPUT)
                            {
                                ppBindParamVals[iOffset] = "null";
                            }
                            else
                            {
                                // -------- VALUE POINTER --------
                                char *pParamData = NULL;
                                if (pDescRec->pDataAtExec) {
                                    pParamData = pDescRec->pDataAtExec->pValue;
                                } else {
                                    if (pDescRec->pValue) {
                                        pParamData = (char *)pDescRec->pValue + (lParamProcessed * iValOffset) + iBindOffset;
                                    } else {
                                        pParamData = NULL;
                                    }
                                }
                                // -------- DATA LENGTH --------
                                SQLLEN iParamDataLen = 0;
                                if (pDescRec->pDataAtExec) {
                                    iParamDataLen = pDescRec->pDataAtExec->cbLen;
                                } else {//pcbLenInd
                                    iParamDataLen = pDescRec->cbLen;
                                }
                                // -------- INDICATOR POINTER --------
                                SQLLEN *plParamDataStrLenInd = NULL;
                                if (pDescRec->pDataAtExec) {
                                    plParamDataStrLenInd = (SQLLEN *)(void *)(&(
                                        pDescRec->pDataAtExec->cbLen));
                                } else if (pDescRec->pcbLenInd) {
                                    if (pAPDDescHeader.lBindType == SQL_BIND_BY_COLUMN) {
                                        plParamDataStrLenInd = (SQLLEN *)(pDescRec->pcbLenInd + lParamProcessed);
                                    }
                                    else {
                                        plParamDataStrLenInd = (SQLLEN *)((char *)pDescRec->pcbLenInd +
                                                                        iBindOffset +
                                                                        (lParamProcessed * iValOffset));
                                    }
                                }
                                // -------- CONVERT --------
                                short hPrepSQLType;
                                if (pIPDRec && pIPDRec->hType != 0) {
                                    hPrepSQLType = pIPDRec->hType;
                                } else {
                                    hPrepSQLType = pDescRec->hParamSQLType;
                                }
                                ppBindParamVals[iOffset] = convertCParamDataToSQLData(
                                    pStmt,
                                    pParamData,
                                    iParamDataLen,
                                    plParamDataStrLenInd,
                                    pDescRec->hType,
                                    pDescRec->hParamSQLType,
                                    hPrepSQLType,
                                    &(pBindParamStrBuf[iOffset]), 
                                    &iConversionError);
                                if (iConversionError)
                                {
                                    rc = SQL_ERROR;
                                    goto error;
                                }
                            }

                            piParamFormats[iOffset] = RS_TEXT_FORMAT;

                            iOffset++;

                        } // Bind param loop

                        // Put the param processed count
                        if(pIPDDescHeader.valid)
                        {
                            // Param processed count
                            if(pIPDDescHeader.plRowsProcessedPtr)
                                *(pIPDDescHeader.plRowsProcessedPtr) = lParamProcessed + 1;

                            // Param status
                            if(pIPDDescHeader.phArrayStatusPtr)
                            {
                                short hParamStatus;

                                if(rc == SQL_SUCCESS)
                                    hParamStatus = SQL_PARAM_SUCCESS;
                                else
                                if(rc == SQL_SUCCESS_WITH_INFO)
                                    hParamStatus = SQL_PARAM_SUCCESS_WITH_INFO;
                                else
                                if(rc == SQL_ERROR)
                                    hParamStatus = SQL_PARAM_ERROR;
                                else
                                    hParamStatus = SQL_PARAM_ERROR;

                                *(pIPDDescHeader.phArrayStatusPtr + lParamProcessed) = hParamStatus;
                            }

                    }

                    } /* !Data_At_Exec */
                }

                if(!iMultiInsert || (iMultiInsert && ((((lParamProcessed + 1) % iMultiInsert) == 0) 
                                                            || (iLastBatchMultiInsert && (lParamProcessed + 1 == lParamsToBind))
                                                      )
                                    )
                  )
                {
                    // Execute it using libpq
                    if(rc != SQL_NEED_DATA)
                    {
                        int iStopFlag = FALSE;
                        int nParams = 0;
						int iReadOutParamVals = pStmt->iFunctionCall;
                       
                        if(iMultiInsert)
                        {
                            if(iLastBatchMultiInsert && (lParamProcessed + 1 == lParamsToBind))
                            {
                                nParams = (iNumBindParams * iLastBatchMultiInsert);

                                if(!executePrepared)
                                    pszCmd = pStmt->pszLastBatchMultiInsertCmd->pBuf;
                                else
                                {
                                    iLastBatchMultiInsertPrepare = TRUE;
                                    // De-allocate previous multi-insert command and prepare last batch multi-insret command
                                    rc = rePrepareMultiInsertCommand(pStmt, pStmt->pszLastBatchMultiInsertCmd->pBuf);
                                    if(rc == SQL_ERROR)
                                         goto error;
                                }
                            }
                            else
                                nParams = (iNumBindParams * iMultiInsert);
                        }
                        else
                            nParams = iNumBindParams;

                        // Look for whether to execute BEGIN or not
                        if((pConn->pConnAttr->iAutoCommit == SQL_AUTOCOMMIT_OFF || pStmt->iFunctionCall == TRUE)
                            && libpqIsTransactionIdle(pConn) && (lParamProcessed == 0 || iMultiInsert))
                        {
                            iBeginCommand = TRUE;
                            rc = libpqExecuteTransactionCommand(pConn, BEGIN_CMD, FALSE);
                            if(rc == SQL_ERROR)
                                goto error;
                        }

                        if(asyncEnable)
                        {
                            sendStatus = (!executePrepared) ? ( (iNumBindParams) ? PQsendQueryParams( pConn->pgConn, pszCmd, nParams, getParamTypesPtr(),(const char *const * )ppBindParamVals,NULL, piParamFormats, RS_TEXT_FORMAT)
                                                                                  : PQsendQuery(pConn->pgConn, pszCmd) )
                                                            : PQsendQueryPrepared(pConn->pgConn, pStmt->szCursorName, nParams, (const char *const * )ppBindParamVals, NULL, piParamFormats, RS_TEXT_FORMAT);

                            if(sendStatus)
                            {
                                pgResult = pqGetResult(pConn->pgConn, pStmt->pCscStatementContext);
                                pqRc = PQresultStatus(pgResult);
                            }
                            else
                                pqRc = PGRES_FATAL_ERROR;
                        } else {
                          if (!executePrepared) {
                            if (iNumBindParams) {
                              pgResult = pqexecParams(
                                  pConn->pgConn, pszCmd, nParams, getParamTypesPtr(),
                                  (const char *const *)ppBindParamVals, NULL,
                                  piParamFormats, RS_TEXT_FORMAT,
                                  pStmt->pCscStatementContext);
                            } else {
                              pgResult = pqExec(pConn->pgConn, pszCmd,
                                                pStmt->pCscStatementContext);
                            }
                          } else {
                            pgResult = pqExecPrepared(
                                pConn->pgConn, pStmt->szCursorName, nParams,
                                (const char *const *)ppBindParamVals, NULL,
                                piParamFormats, RS_TEXT_FORMAT,
                                pStmt->pCscStatementContext);
                          }
                          pqRc = PQresultStatus(pgResult);
                        }

                        // Multi result loop
                        do
                        {
                            // Even one result in error, we are retuning error.
                            rc = setResultInStmt(rc, pStmt, pgResult, FALSE, pqRc,&iStopFlag, iArrayBinding);
                            if(iStopFlag)
                                break;

							if((isStreamingCursorMode(pStmt))
								&& (rc != SQL_ERROR)
							)
							{
								iReadOutParamVals = FALSE;
								break; // We are not looping for multiple result right now
							}

                            if(!getCscThreadCreatedFlag(pStmt->pCscStatementContext))
                            {
                                // Loop for next result
                                if(asyncEnable)
                                {
                                    // If command not send, no need to check for next result.
                                    if(!sendStatus)
                                        break;

                                    // Get next result
                                    pgResult = pqGetResult(pConn->pgConn, pStmt->pCscStatementContext);
                                    if(!pgResult)
                                        break;

                                    // Get result status
                                    pqRc = PQresultStatus(pgResult);
                                }
                                else
                                {
                                    // Get next result
                                    pgResult = pqGetResult(pConn->pgConn, pStmt->pCscStatementContext);
                                    if(!pgResult)
                                        break;

                                    // Get result status
                                    pqRc = PQresultStatus(pgResult);
                                }
                            }
                            else
                            {
                                // CSC processing will happen on a thread.
                                iCscThreadCreated = TRUE;

                                // Reset the flag as we are breaking the loop
                                setCscThreadCreatedFlag(pStmt->pCscStatementContext, FALSE);

								iReadOutParamVals = FALSE;

                                break;
                            }
                        }while(TRUE); // Results  loop

						if (rc == SQL_SUCCESS)
						{
							// Put the OUT parameter values, if any
							if (iReadOutParamVals
								&& (pStmt->iNumOfOutOnlyParams > 0
									|| pStmt->iNumOfInOutOnlyParams > 0))
							{
								int rc1 = updateOutBindParametersValue(pStmt);
								if (rc1 == SQL_ERROR)
								{
									rc = rc1;
									addError(&pStmt->pErrorList, "HY000", "OUT parameter processing issue", 0, NULL);
									goto error;
								}
							}
						}
                    } // !SQL_NEED_DATA
                } 

                // Clean param buffers
                if((iNumBindParams > 0) && (!iMultiInsert || ( iMultiInsert && ((((lParamProcessed + 1) % iMultiInsert) == 0)
                                                                                    || (iLastBatchMultiInsert && (lParamProcessed + 1 == lParamsToBind))
                                                                               )
                                                             )
                                           )
                  )
                {
                    ppBindParamVals = (char **)rs_free(ppBindParamVals);
                    piParamFormats  = (int *)rs_free(piParamFormats);

                    if(pBindParamStrBuf)
                    {
                        for(iBindParam = 0; iBindParam < iNumBindParams; iBindParam++)
                        {
                            if(pBindParamStrBuf[iBindParam].iAllocDataLen > 0)
                            {
                                pBindParamStrBuf[iBindParam].pBuf = (char *)rs_free(pBindParamStrBuf[iBindParam].pBuf);
                                pBindParamStrBuf[iBindParam].iAllocDataLen = 0;
                            }
                        }

                        pBindParamStrBuf = (RS_BIND_PARAM_STR_BUF *)rs_free(pBindParamStrBuf);
                    }

					paramTypes.clear();

                    iOffset = 0;
                }
            } // Array binding loop

            if(iLastBatchMultiInsertPrepare)
            {
                iLastBatchMultiInsertPrepare = FALSE;
                // De-allocate previous multi-insert command and prepare regular multi-insret command
                rc = rePrepareMultiInsertCommand(pStmt, pStmt->pCmdBuf->pBuf);
                if(rc == SQL_ERROR)
                     goto error;
            }
        } // SQL_SUCCESS
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid command buffer", 0, NULL);
        goto error;
    }

    // This will do recursion call, without lock
    if(iCheckForRefCursor && rc == SQL_SUCCESS && pConn->pConnectProps->iFetchRefCursor)
    {
        // TODO: We have to do RefCursor processing after CSC thread is done or do it for one result at a time.
        if(!iCscThreadCreated)
        {
            // Check for refcursor in result list, if exist execute fetch all and replce result node in-place.
            // Note that this may call this function again, so it become recursion.
            rc = checkAndAutoFetchRefCursor(pStmt);
            if(rc == SQL_ERROR)
                goto error;
        }
    }

    if(iBeginCommand)
    {
        if(pConn->pConnAttr->iAutoCommit != SQL_AUTOCOMMIT_OFF 
            && pStmt->iFunctionCall == TRUE
            && !libpqIsTransactionIdle(pConn))
        {
            // Send commit/rollback after function call, if auto commit is ON.
            libpqExecuteTransactionCommand(pConn, (char *)((rc != SQL_ERROR) ? COMMIT_CMD : ROLLBACK_CMD), FALSE);
        }
    }

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }

    return rc;

error:

    // Clean param buffers
    if(iNumBindParams > 0)
    {
        ppBindParamVals = (char **)rs_free(ppBindParamVals);
        piParamFormats  = (int *)rs_free(piParamFormats);

        if(pBindParamStrBuf)
        {
            for(iBindParam = 0; iBindParam < iNumBindParams; iBindParam++)
            {
                if(pBindParamStrBuf[iBindParam].iAllocDataLen > 0)
                {
                    pBindParamStrBuf[iBindParam].pBuf = (char *)rs_free(pBindParamStrBuf[iBindParam].pBuf);
                    pBindParamStrBuf[iBindParam].iAllocDataLen = 0;
                }
            }

            pBindParamStrBuf = (RS_BIND_PARAM_STR_BUF *)rs_free(pBindParamStrBuf);
        }
    }


    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release result related resources.
//
void libpqCloseResult(RS_RESULT_INFO *pResult)
{
    // Close csc, if any
    pgCloseCsc(pResult->pgResult);

    // Release buffers associated with result
    PQclear(pResult->pgResult);
    pResult->pgResult = NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Map Pg type to SQL type.
//
short mapPgTypeToSqlType(Oid pgType, short *phRsSpecialType)
{
    short sqlType;

    if(phRsSpecialType)
        *phRsSpecialType = 0;

    switch(pgType)
    {
        case BOOLOID:
        {
            sqlType = SQL_BIT;
            break;
        }

        case CHAROID:
        case BPCHAROID:
        {
            sqlType = SQL_CHAR;
            break;
        }

        case INT8OID:
		case XIDOID:
		{
            sqlType = SQL_BIGINT;
            break;
        }

        case INT2OID:
        {
            sqlType = SQL_SMALLINT;
            break;
        }

        case INT4OID:
        case OIDOID:
        case CIDOID:
        {
            sqlType = SQL_INTEGER;
            break;
        }

        case FLOAT4OID:
        {
            sqlType = SQL_REAL;
            break;
        }

        case FLOAT8OID:
        {
            sqlType = SQL_DOUBLE;
            break;
        }

        case VARCHAROID:
        case NAMEOID:
        case VOIDOID:
        case INT2VECTOROID: 
        case REGPROCOID:
        case TIDOID:
        case OIDVECTOROID:
        case ABSTIMEOID:
        case INT2ARRAYOID:
        case INT4ARRAYOID:
        case NAMEARRAYOID:
        case TEXTARRAYOID:
        case FLOAT4ARRAYOID:
        case ACLITEMARRAYOID:
        case ANYARRAYOID:
        case INTERVALOID:
        case TEXTOID:
        {
            sqlType = SQL_VARCHAR;
            break;
        }

        case DATEOID:
        {
            sqlType = SQL_TYPE_DATE; // SQL_DATE
            break;
        }

        case TIMESTAMPOID:
        {
            sqlType = SQL_TYPE_TIMESTAMP; // SQL_TIMESTAMP
            break;
        }

        case NUMERICOID:
        {
            sqlType = SQL_NUMERIC;
            break;
        }

        case REFCURSOROID:
        {
            sqlType = SQL_VARCHAR;
            break;
        }

        case TIMEOID:
        {
            sqlType = SQL_TYPE_TIME; // SQL_TIME
            break;
        }

        case TIMETZOID:
        {
            if(phRsSpecialType)
                *phRsSpecialType = TIMETZOID;

            sqlType = SQL_TYPE_TIME; 
            break;
        }

		case TIMESTAMPTZOID:
		{
			if (phRsSpecialType)
				*phRsSpecialType = TIMESTAMPTZOID;

			sqlType = SQL_TYPE_TIMESTAMP;
			break;
		}

		case INTERVALY2MOID:
		{
			sqlType = SQL_INTERVAL_YEAR_TO_MONTH;
			break;
		}

		case INTERVALD2SOID:
		{
			sqlType = SQL_INTERVAL_DAY_TO_SECOND;
			break;
		}

		case SUPER:
		{
			if (phRsSpecialType)
				*phRsSpecialType = SUPER;

			sqlType = SQL_LONGVARCHAR;
			break;
		}

		case VARBYTE:
		{
			if (phRsSpecialType)
				*phRsSpecialType = VARBYTE;

			sqlType = SQL_LONGVARBINARY;
			break;
		}

		case GEOGRAPHY:
		{
			if (phRsSpecialType)
				*phRsSpecialType = GEOGRAPHY;

			sqlType = SQL_LONGVARBINARY;
			break;
		}

		case GEOMETRY:
		{
			if (phRsSpecialType)
				*phRsSpecialType = GEOMETRY;

			sqlType = SQL_LONGVARBINARY;
			break;
		}

		case GEOMETRYHEX:
		{
			if (phRsSpecialType)
				*phRsSpecialType = GEOMETRYHEX;

			sqlType = SQL_LONGVARBINARY;
			break;
		}

		case UNKNOWNOID: // This happens when SELECT as parameter. e.g SELECT $1
		{
			sqlType = SQL_VARCHAR;
			break;
		}


        default:    // Some pg data type in catalog table as such not supported by PADB (e.g. aclitem[]), so map not supported as SQL_VARCHAR.
        {
            sqlType = SQL_UNKNOWN_TYPE; // SQL_UNKNOWN_TYPE SQL_VARCHAR
            break;
        }
    } // Switch

    return sqlType;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get column value.
//
char *libpqGetData(RS_RESULT_INFO *pResult, short hCol, int *piLenInd, int *piFormat)
{
    char *pData = PQgetvalue(pResult->pgResult, pResult->iCurRow, hCol);
	int format = PQfformat(pResult->pgResult, hCol);

    if(pData == NULL || *pData == '\0')
    {
        int isNull = PQgetisnull(pResult->pgResult, pResult->iCurRow, hCol);

        if(isNull && piLenInd)
            *piLenInd = SQL_NULL_DATA;
		else
		{
			if (piLenInd)
			{
				int len = IS_TEXT_FORMAT(format)
					? 0
					: PQgetlength(pResult->pgResult, pResult->iCurRow, hCol);
				*piLenInd = len;
			}
		}
    }
    else
    {
        if(piLenInd)
            *piLenInd = PQgetlength(pResult->pgResult, pResult->iCurRow, hCol);
    }

	*piFormat = format;

    return pData;
}

 
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Prepare SQL statement.
//
SQLRETURN libpqPrepare(RS_STMT_INFO *pStmt, char *pszCmd)
{
    SQLRETURN rc;
    int asyncEnable = isAsyncEnable(pStmt);

    if(!asyncEnable)
    {
        rc = libpqPrepareOnThread(pStmt, pszCmd);
    }
    else
    {
        // Prepare async on thread
        if(pStmt->pExecThread == NULL)
        {
            RS_EXEC_THREAD_INFO *pExecThread;

            pStmt->pExecThread = pExecThread = (RS_EXEC_THREAD_INFO *)new RS_EXEC_THREAD_INFO();
            if(pExecThread == NULL)
            {
                rc = SQL_ERROR;
                addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, NULL);
                goto error;
            }

            // Set thread execution context
            pExecThread->executePrepared = 0;
            pExecThread->pszCmd = (char *)rs_free(pExecThread->pszCmd);
            pExecThread->pszCmd = rs_strdup(pszCmd, SQL_NTS);
            pExecThread->rc = SQL_STILL_EXECUTING;
            pExecThread->iPrepare = 1;

            // create thread
            pExecThread->hThread =  rsCreateThread((void *)libpqPrepareThreadProc, pStmt);
            if(pExecThread->hThread == (THREAD_HANDLE)(long) NULL)
            {
                // Release thread info
                waitAndFreeExecThread(pStmt, FALSE);

                // Execute on same thread
                rc = libpqPrepareOnThread(pStmt, pszCmd);
            }
            else
                rc = SQL_STILL_EXECUTING;
        }
        else
        {
            rc = checkExecutingThread(pStmt);
            if(rc != SQL_STILL_EXECUTING)
                waitAndFreeExecThread(pStmt, FALSE);
        }
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Prepare SQL statement using thread procedure.
//
#ifdef WIN32
void
#endif
#if defined LINUX 
void *
#endif
libpqPrepareThreadProc(void *pArg)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pArg;
    RS_EXEC_THREAD_INFO *pExecThread = pStmt->pExecThread;
    SQLRETURN rc;

    rc = libpqPrepareOnThread(pStmt, pExecThread->pszCmd);

    setThreadExecutionStatus(pExecThread, rc);

#ifdef WIN32
    return;
#endif
#if defined LINUX 
    return NULL;
#endif
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Prepare SQL statement using thread.
//
SQLRETURN libpqPrepareOnThread(RS_STMT_INFO *pStmt, char *pszCmd)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = pStmt->phdbc;

    // Lock connection sem to protect multiple stmt execution at same time.
    rsLockSem(pConn->hSemMultiStmt);

    // Wait for current csc thread to finish, if any.
    pgWaitForCscThreadToFinish(pConn->pgConn, FALSE);

    if(pszCmd)
    {
        PGresult *pgResult = NULL;
        ExecStatusType pqRc = PGRES_COMMAND_OK;
        int asyncEnable = isAsyncEnable(pStmt);
        int sendStatus = 1;
        RS_PREPARE_INFO *pPrepare;
        // TODO: improve the code repetition in if and else
        if(asyncEnable)
        {
			int iNoOfBindParams = countBindParams(pStmt->pStmtAttr->pAPD->pDescRecHead);

			if (pStmt->iNumOfOutOnlyParams == 0 && iNoOfBindParams == 0)
				sendStatus = pqSendPrepareAndDescribe(pConn->pgConn, pStmt->szCursorName, pszCmd, 0, NULL);
            else {
                std::vector<Oid> paramTypes = getParamTypes(
                    iNoOfBindParams,
                    pStmt->pStmtAttr->pAPD->pDescRecHead,
                    pConn->pConnectProps);

                sendStatus = pqSendPrepareAndDescribe(
                    pConn->pgConn, pStmt->szCursorName, pszCmd,
                    iNoOfBindParams,
                    paramTypes.empty() ? nullptr
                                       : paramTypes.data());
            }

            if(sendStatus)
            {
                pgResult = PQgetResult(pConn->pgConn);
                pqRc = PQresultStatus(pgResult);
            }
            else
                pqRc = PGRES_FATAL_ERROR;
        }
        else
        {
			int iNoOfBindParams = countBindParams(pStmt->pStmtAttr->pAPD->pDescRecHead);
			if(pStmt->iNumOfOutOnlyParams == 0 && iNoOfBindParams == 0)
				pgResult = pqPrepare(pConn->pgConn, pStmt->szCursorName, pszCmd, 0, NULL);
            else {
                std::vector<Oid> paramTypes = getParamTypes(
                    iNoOfBindParams,
                    pStmt->pStmtAttr->pAPD->pDescRecHead,
                    pConn->pConnectProps);

                // OUT parameters in the stmt
                pgResult = pqPrepare(
                    pConn->pgConn, pStmt->szCursorName, pszCmd,
                    iNoOfBindParams,
                    paramTypes.empty() ? nullptr
                                       : paramTypes.data());
            }
            pqRc = PQresultStatus(pgResult);
        }

        // Multi prepare loop
        do
        {
            if (!(pqRc == PGRES_COMMAND_OK || pqRc == PGRES_TUPLES_OK))
            {
                char *pError = libpqErrorMsg(pConn);

                // Even one result in error, we are retuning error.
                rc = SQL_ERROR;

                if(pError && *pError != '\0')
                    addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);

                // Clear this result because we are not storing it
                PQclear(pgResult);
                pgResult = NULL;

                pPrepare = NULL;
            }
            else {
                PGresult *pgResultDescParam = PQgetResult(pConn->pgConn);

                if(pgResultDescParam == NULL)
                {
                    // 'Z' must be followed to 't' during above PQgetResult
                    // So read from resultForDescParam

                    pgResultDescParam = pqgetResultForDescribeParam(pConn->pgConn);
                }

                // Create prepare object
                pPrepare = (RS_PREPARE_INFO *)new RS_PREPARE_INFO(pStmt, pgResult);


                // Get describe param result and then get param information
                rc = libpqDescribeParams(pStmt, pPrepare, pgResultDescParam);

                // Add prepared to statement
                addPrepare(pStmt, pPrepare);

                if(pqRc == PGRES_TUPLES_OK)
                {
                    // SELECT kind of operation returning rows description

                    pgResult = PQgetResult(pConn->pgConn);

                    if(pgResult)
                    {
                        // Create result  object for description
                        RS_RESULT_INFO *pResult = createResultObject(pStmt, pgResult);

                        getResultDescription(pgResult, pResult, FALSE);

                        // Add result to statement
                        pPrepare->pResultForDescribeCol = pResult;
                    }
                } // SELECT

            } // Success

            if ((pConn && pConn->pgConn && PQstatus(pConn->pgConn) == CONNECTION_BAD))
            {
                // No need to loop any more.
                break;
            }

            // Loop for next result
            if(asyncEnable)
            {
                // If command not send, no need to check for next result.
                if(!sendStatus)
                    break;
            }

            // Get next result description
            pgResult = PQgetResult(pConn->pgConn);
            if(!pgResult)
            {
                // 'Z' must be followed to 'T' during above PQgetResult
                // So read from resultForDescRowPrep and release it.
                PGresult *pgResultDescRowPrep = pqgetResultForDescribeRowPrep(pConn->pgConn);
                if(pgResultDescRowPrep)
                {

                    // Create result  object for description
                    RS_RESULT_INFO *pResult = createResultObject(pStmt, pgResultDescRowPrep);

                    getResultDescription(pgResultDescRowPrep, pResult, FALSE);

                    pPrepare->pResultForDescribeCol = pResult;
                }

                break;
            }

            // Get result status
            pqRc = PQresultStatus(pgResult);

        }while(TRUE); // Results  loop
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid command buffer", 0, NULL);
        goto error;
    }

error:

    // Unlock connection sem
    rsUnlockSem(pConn->hSemMultiStmt);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release prepared statement related resource.
//
void libpqReleasePrepare(RS_PREPARE_INFO *pPrepare)
{
    // Release prepare pgResult
    PQclear(pPrepare->pgResult);
    pPrepare->pgResult = NULL;

    // Release param info
    PQclear(pPrepare->pgResultDescribeParam);
    pPrepare->pgResultDescribeParam = NULL;

    // Release col info
    releaseResult(pPrepare->pResultForDescribeCol, FALSE, NULL);
    pPrepare->pResultForDescribeCol = NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Trace libpq calls.
// Deprecated infavor of rslog
void libpqTrace(RS_CONN_INFO *pConn)
{
    return;
    if(IS_TRACE_LEVEL_MSG_PROTOCOL())
    {
        PQtrace(pConn->pgConn, NULL);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Describe parameter of a prepared SQL statement.
//
SQLRETURN libpqDescribeParams(RS_STMT_INFO *pStmt, RS_PREPARE_INFO *pPrepare, PGresult *pgResult)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = pStmt->phdbc;

    ExecStatusType pqRc = PGRES_COMMAND_OK;

    if(!(pqRc == PGRES_COMMAND_OK || pqRc == PGRES_TUPLES_OK))
    {
        char *pError = libpqErrorMsg(pConn);

        // Even one result in error, we are retuning error.
        rc = SQL_ERROR;

        if(pError && *pError != '\0')
            addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);

        // Clear this result because we are not storing it
        PQclear(pgResult);
        pgResult = NULL;
    }
    else
    {
        // Store libpq result handle of describe param
        pPrepare->pgResultDescribeParam = pgResult;

        // Get param info

        // # of params
        pPrepare->iNumberOfParams = PQnparams(pPrepare->pgResultDescribeParam);

        if(pPrepare->iNumberOfParams > 0)
        {
            int iParam;

            pPrepare->pIPDRecs = (RS_DESC_REC *)rs_calloc(pPrepare->iNumberOfParams,sizeof(RS_DESC_REC));

            if(pPrepare->pIPDRecs)
            {
                // Get param(s) infomation
                for(iParam = 0; iParam < pPrepare->iNumberOfParams; iParam++)
                {
                    RS_DESC_REC *pDescRec = &pPrepare->pIPDRecs[iParam];
                    Oid pgType;

                    pDescRec->hRecNumber = iParam + 1;

                    if(iParam)
                    {
                        // Link previous element to this one.
                        pPrepare->pIPDRecs[iParam - 1].pNext = pDescRec;
                    }


                    pgType = PQparamtype(pgResult, iParam);

                    pDescRec->hType = mapPgTypeToSqlType(pgType,&(pDescRec->hRsSpecialType));
                    pDescRec->iSize = getParamSize(pDescRec->hType);
                    pDescRec->hScale = getParamScale(pDescRec->hType);
                    pDescRec->hNullable = SQL_NULLABLE_UNKNOWN;
                } // Param loop
            } 
            else
            {
                rc = SQL_ERROR;

                addError(&pStmt->pErrorList,"HY001", "Memory allocation error", 0, pConn);

                // Clear this result because we are not storing it
                PQclear(pgResult);
                pgResult = NULL;
                pPrepare->iNumberOfParams = 0;
                pPrepare->pgResultDescribeParam = NULL;
            }
        } // params > 0
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get result description of a query.
//
static void getResultDescription(PGresult *pgResult, RS_RESULT_INFO *pResult, int iFetchRefCursor)
{
    // SELECT kind of operation returning rows
    int col;

    // #of cols
    pResult->iNumberOfCols = PQnfields(pgResult);

    pResult->pIRDRecs = (RS_DESC_REC *)rs_calloc(pResult->iNumberOfCols,sizeof(RS_DESC_REC));

    // Get column(s) infomation
    for(col = 0; col < pResult->iNumberOfCols; col++)
    {
        RS_DESC_REC *pDescRec = &pResult->pIRDRecs[col];
        Oid pgType;
        int pgMod;
        char *pName;
		char *pTemp;
		int case_sensitive;

        pDescRec->hRecNumber = col + 1;

        if(col)
        {
            // Link previous element to this one.
            pResult->pIRDRecs[col - 1].pNext = pDescRec;
        }

        // Column name is allocated in libpq
        pName = PQfname(pgResult, col);
        copyStrDataSmallLen(pName, SQL_NTS, pDescRec->szName, MAX_IDEN_LEN, NULL);
        if(pResult->phstmt && pResult->phstmt->iCatalogQuery)
        {
            // Make the column name upper case
            _strupr(pDescRec->szName);
        }

        pResult->columnNameIndexMap[pName] = col + 1;

        pgType = PQftype(pgResult, col);
        pDescRec->hType = mapPgTypeToSqlType(pgType,&(pDescRec->hRsSpecialType));

        if(iFetchRefCursor && (pgType == REFCURSOROID))
        {
            // Mark result for auto refcursor fetch
            pResult->iRefCursorInResult = TRUE;
        }

        pgMod = PQfmod(pgResult, col);
        if(pgMod < 0)
        {
            if(pgType == NAMEOID)
                pDescRec->iSize = MAX_NAMEOID_SIZE;
            else
            if(pgType == TIMETZOID)
                pDescRec->iSize = MAX_TIMETZOID_SIZE;
            else
            if(pgType == REFCURSOROID || pgType == VOIDOID)
                pDescRec->iSize = 65535;
            else
            if(pgType == TIMESTAMPTZOID)
                pDescRec->iSize = MAX_TIMESTAMPTZOID_SIZE;
            else
            if(pgType == INTERVALY2MOID)
                pDescRec->iSize = MAX_INTERVALY2MOID_SIZE;
            else
            if(pgType == INTERVALD2SOID)
                pDescRec->iSize = MAX_INTERVALD2SOID_SIZE;
            else
            if(pgType == SUPER)
                pDescRec->iSize = MAX_SUPER_SIZE;
            else
            if(pgType == VARBYTE)
                pDescRec->iSize = MAX_VARBYTE_SIZE;
			else
			if (pgType == GEOMETRY
					|| pgType == GEOMETRYHEX)
				pDescRec->iSize = MAX_GEOMETRY_SIZE;
			else
			if (pgType == GEOGRAPHY)
				pDescRec->iSize = MAX_GEOGRAPHY_SIZE;
			else
                pDescRec->iSize = 0; // Unknown
        }
        else
        {
            if(pDescRec->hType == SQL_NUMERIC
                || pDescRec->hType == SQL_DECIMAL)
            {
                // Upper 2 bytes are precision
                pDescRec->iSize = (pgMod >> 16) & 0xFFFF;
            }
            else
            if(pgType == TIMETZOID)
                pDescRec->iSize = MAX_TIMETZOID_SIZE;
            else
            {
                pDescRec->iSize = pgMod - 4; // 4 for sizeof(int) itself.
            }
        }

        if(pDescRec->hType == SQL_NUMERIC
            || pDescRec->hType == SQL_DECIMAL)
        {
            if(pgMod == -1)
                pDescRec->hScale = 0;
            else
            {
                // Lower 2 bytes are scale
                pDescRec->hScale = (pgMod & 0xFFFF) -4; // 4 for sizeof(int) itself.
            }
        }
        else
            pDescRec->hScale = 0; 

        pDescRec->hNullable = PQfnullable(pgResult, col);

        // Get col attributes
        pDescRec->cAutoInc = (PQfauto_increment(pgResult, col) == 1) ? SQL_TRUE : SQL_FALSE;

		// Catalog name is allocated in libpq
		pTemp = PQfcatalog_name(pgResult, col);
		if (pTemp)
		{
			copyStrDataSmallLen(pTemp, SQL_NTS, pDescRec->szCatalogName, MAX_IDEN_LEN, NULL);
			if (pResult->phstmt && pResult->phstmt->iCatalogQuery)
			{
				// Make the catalog name upper case
				_strupr(pDescRec->szCatalogName);
			}
		}
		else
			pDescRec->szCatalogName[0] = '\0';

		// Schema name is allocated in libpq
		pTemp = PQfschema_name(pgResult, col);
		if (pTemp)
		{
			copyStrDataSmallLen(pTemp, SQL_NTS, pDescRec->szSchemaName, MAX_IDEN_LEN, NULL);
			if (pResult->phstmt && pResult->phstmt->iCatalogQuery)
			{
				// Make the catalog name upper case
				_strupr(pDescRec->szSchemaName);
			}
		}
		else
			pDescRec->szSchemaName[0] = '\0';

		// Table name is allocated in libpq
		pTemp = PQftable_name(pgResult, col);
		if (pTemp)
		{
			copyStrDataSmallLen(pTemp, SQL_NTS, pDescRec->szTableName, MAX_IDEN_LEN, NULL);
			if (pResult->phstmt && pResult->phstmt->iCatalogQuery)
			{
				// Make the catalog name upper case
				_strupr(pDescRec->szTableName);
			}
		}
		else
			pDescRec->szTableName[0] = '\0';

		case_sensitive = PQfcase_sensitive(pgResult, col);
        pDescRec->cCaseSensitive = getCaseSensitive(pDescRec->hType, pDescRec->hRsSpecialType, case_sensitive);
        pDescRec->iDisplaySize = getDisplaySize(pDescRec->hType, pDescRec->iSize, pDescRec->hRsSpecialType);
        pDescRec->cFixedPrecScale = SQL_FALSE;
        getLiteralPrefix(pDescRec->hType, pDescRec->szLiteralPrefix, pDescRec->hRsSpecialType);
        getLiteralSuffix(pDescRec->hType, pDescRec->szLiteralSuffix, pDescRec->hRsSpecialType);
        getTypeName(pDescRec->hType, pDescRec->szTypeName, sizeof(pDescRec->szTypeName), pDescRec->hRsSpecialType);
        pDescRec->iNumPrecRadix = getNumPrecRadix(pDescRec->hType);
        pDescRec->iOctetLen = getOctetLen(pDescRec->hType, pDescRec->iSize, pDescRec->hRsSpecialType);
        pDescRec->iPrecision = getPrecision(pDescRec->hType, pDescRec->iSize, pDescRec->hRsSpecialType);
        pDescRec->iSearchable = getSearchable(pDescRec->hType, pDescRec->hRsSpecialType);
        pDescRec->iUnNamed = getUnNamed(pDescRec->szName);
        pDescRec->cUnsigned = getUnsigned(pDescRec->hType);
        pDescRec->iUpdatable = getUpdatable();
    } // Col loop
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Release prepared statement on server.
//
// DEALLOCATE stmt_name
SQLRETURN libpqExecuteDeallocateCommand(RS_STMT_INFO *pStmt, int iLockRequired, int calledFromDrop)
{
    int fail = FALSE;
    RS_CONN_INFO *pConn = pStmt->phdbc;

    if(pStmt->pPrepareHead && (calledFromDrop || pStmt->iStatus != RS_CANCEL_STMT))
    {
        char szCmd[SHORT_CMD_LEN + 1];
        PGresult *pgResult;

        if(iLockRequired)
        {
            // Lock connection sem to protect multiple stmt execution at same time.
            rsLockSem(pConn->hSemMultiStmt);

            // Wait for current csc thread to finish, if any.
            pgWaitForCscThreadToFinish(pConn->pgConn, FALSE);
        }

		// We have to release any result of streaming cursor before executing internal command
		checkAndSkipAllResultsOfStreamingCursor(pStmt);

        snprintf(szCmd,sizeof(szCmd),"%s %s", DEALLOCATE_CMD, pStmt->szCursorName);
        
        pgResult = PQexec(pConn->pgConn, szCmd);

        if(PQresultStatus(pgResult) != PGRES_COMMAND_OK)
        {
            char *pError = libpqErrorMsg(pConn);

            fail = (pError && *pError != '\0'); 

            if(fail)
                addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);
        }

        PQclear(pgResult);
        pgResult = NULL;

        if(iLockRequired)
        {
            // Unlock connection sem
            rsUnlockSem(pConn->hSemMultiStmt);
        }
    }

    return (fail) ? SQL_ERROR : SQL_SUCCESS;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Create Result object with given libpq result handle.
//
static RS_RESULT_INFO *createResultObject(RS_STMT_INFO *pStmt, PGresult *pgResult)
{
    // Create result  object for description
    RS_RESULT_INFO *pResult = (RS_RESULT_INFO *)new RS_RESULT_INFO(pStmt, pgResult);

    return pResult;
}

static void releaseResultObject(RS_RESULT_INFO* pResult) {
    if (pResult != nullptr) {
        // Clear PostgreSQL result if it exists
        if (pResult->pgResult != nullptr) {
            PQclear(pResult->pgResult);
            pResult->pgResult = nullptr;
        }

        // Delete the result object
        delete pResult;
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Callback function for result processing for CSC
//
short RS_ResultHandlerCallbackFunc(void *pCallerContext, PGresult *pgResult)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pCallerContext;
    SQLRETURN rc = SQL_SUCCESS;
    ExecStatusType pqRc = PGRES_COMMAND_OK;
    int iStopFlag = FALSE;

    // Mostly not needed bcoz we embed CSC in the pgResult when we return in memory result of the whole resultset of the first result.
    // We may have to see what to do for multi-result here.

    // Skip for the first result
    rc = setResultInStmt(rc, pStmt, pgResult, TRUE, pqRc, &iStopFlag, FALSE);

	return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get Max Rows from Stmt
//
int getMaxRowsForCsc(void *pCallerContext)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pCallerContext;
    int iMaxRows;

    if(pStmt)
    {
        iMaxRows = pStmt->pStmtAttr->iMaxRows;
    }
    else
        iMaxRows = 0;

    return iMaxRows;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get result set type from Stmt
//
int getResultSetTypeForCsc(void *pCallerContext)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pCallerContext;
    int iResultsetType;

    if(pStmt)
    {
        if(isScrollableCursor(pStmt))
            iResultsetType = (int) ~SQL_CURSOR_FORWARD_ONLY;
        else
            iResultsetType = SQL_CURSOR_FORWARD_ONLY;
    }
    else
       iResultsetType = SQL_CURSOR_FORWARD_ONLY;

    return iResultsetType;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get result concurrency type from Stmt
//
int getResultConcurrencyTypeForCsc(void *pCallerContext)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pCallerContext;
    int iResultConcurrencyType;

    if(pStmt)
    {
        if(isUpdatableCursor(pStmt))
            iResultConcurrencyType = (int) ~SQL_CONCUR_READ_ONLY;
        else
            iResultConcurrencyType = SQL_CONCUR_READ_ONLY;
    }
    else
       iResultConcurrencyType = SQL_CONCUR_READ_ONLY;

    return iResultConcurrencyType;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set result 
//
SQLRETURN setResultInStmt(SQLRETURN rc, RS_STMT_INFO *pStmt, PGresult *pgResult, int readStatusFlag, ExecStatusType pqRc, int *piStop, int iArrayBinding)
{
    RS_CONN_INFO *pConn = pStmt->phdbc;
    int iAddResultInList = TRUE;

    *piStop = FALSE;

    if(readStatusFlag)
        pqRc = PQresultStatus(pgResult);

    if(!(pqRc == PGRES_COMMAND_OK
            || pqRc == PGRES_TUPLES_OK
            || pqRc == PGRES_EMPTY_QUERY))
    {
        char *pError = libpqErrorMsg(pConn);

        // Even one result in error, we are retuning error.
        rc = SQL_ERROR;

        if(pError && *pError != '\0')
            addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);

        // Clear this result because we are not storing it
        PQclear(pgResult);
        pgResult = NULL;
    }
    else
    {
        // Create result object
        RS_RESULT_INFO *pResult = createResultObject(pStmt, pgResult);
        
        if(pqRc == PGRES_COMMAND_OK)
        {
            // non-SELECT command

            // Add rows affected count
            char *cmdStatus = PQcmdTuples(pgResult);

            if(cmdStatus)
            {
                long long llRowsUpdated = 0;

                sscanf(cmdStatus,"%lld",&llRowsUpdated);

                if((llRowsUpdated > 0) && (llRowsUpdated > LONG_MAX))
                    llRowsUpdated = LONG_MAX;

                if(iArrayBinding && pStmt->pResultHead != NULL && pStmt->pResultHead->lRowsUpdated > 0)
                {
                    // Accumulate the count for ARRAY processing of non-SELECT command.
                    iAddResultInList = FALSE;
                    pStmt->pResultHead->lRowsUpdated += (long)llRowsUpdated;

                    // Clear this result because we are not storing it
                    PQclear(pgResult);
                    pgResult = NULL;
                    if (pResult != NULL) {
                      delete pResult;
                      pResult = NULL;
                    }
                }
                else
                    pResult->lRowsUpdated = (long)llRowsUpdated;
            }
        }
        else
        if(pqRc == PGRES_TUPLES_OK)
        {
            // SELECT kind of operation returning rows

            // #of rows in memory
            pResult->iNumberOfRowsInMem = PQntuples(pgResult);

            getResultDescription(pgResult, pResult, pConn->pConnectProps->iFetchRefCursor);
        }

        // Add result to statement
        if(iAddResultInList)
            addResult(pStmt, pResult);
    } // Success

    // pqRc == CONNECTION_BAD
    if ((pConn && pConn->pgConn && PQstatus(pConn->pgConn) == CONNECTION_BAD))
    {
        // No need to loop any more.
        *piStop = TRUE;

        return rc;
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Initialize PQ lib like CSC etc.
//
void initLibpq(FILE    *fpTrace)
{
    initCscLib(fpTrace);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Uninitialize PQ lib like CSC etc.
//
void uninitLibpq()
{
    uninitCscLib();
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set query timeout in server
//
SQLRETURN setQueryTimeoutInServer(RS_STMT_INFO *pStmt)
{
    SQLRETURN rc = SQL_SUCCESS;

    if(pStmt)
    {
        RS_CONN_INFO *pConn = pStmt->phdbc;

        // If last set query timeout is not same as current statement timeout then set it with current statement value.
        if(pConn && pStmt->pStmtAttr && (pStmt->pStmtAttr->iQueryTimeout !=  pConn->iLastQueryTimeoutSetInServer))
        {
            char szSetCmd[SHORT_CMD_LEN];
            PGresult *pgResult = NULL;
            ExecStatusType pqRc;

            snprintf(szSetCmd,sizeof(szSetCmd),"set statement_timeout to %d;",(pStmt->pStmtAttr->iQueryTimeout * 1000));

            pgResult = PQexec(pConn->pgConn, szSetCmd);

            pqRc = PQresultStatus(pgResult);

            if(pqRc != PGRES_COMMAND_OK)
            {
                char *pError = libpqErrorMsg(pConn);

                // Even one result in error, we are retuning error.
                rc = SQL_ERROR;

                if(pError && *pError != '\0')
                    addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);
            }
            else
            {
                pConn->iLastQueryTimeoutSetInServer = pStmt->pStmtAttr->iQueryTimeout;
            }

            // Clear this result because we are not storing it
            PQclear(pgResult);
            pgResult = NULL;
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get size of unknown/unsupported datatype.
//
int getUnknownTypeSize(Oid pgType)
{
    if(pgType == REFCURSOROID 
        || pgType == VOIDOID 
        || pgType == INT2VECTOROID 
        || pgType == REGPROCOID
        || pgType == TIDOID
        || pgType == OIDVECTOROID
        || pgType == ABSTIMEOID 
        || pgType == INT2ARRAYOID
        || pgType == TEXTARRAYOID 
        || pgType == FLOAT4ARRAYOID
        || pgType == ACLITEMARRAYOID
        || pgType == ANYARRAYOID
       ) 
        return 65535;
    else
    if(pgType == INT4ARRAYOID
        || pgType == INTERVALOID)
        return 255;
    else
    if(pgType == TIMESTAMPTZOID)
        return 34;
    else
        return 0;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get connection status
//
ConnStatusType libpqConnectionStatus(RS_CONN_INFO *pConn)
{
	return PQstatus(pConn->pgConn);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Prepare SQL statement using thread.
//
SQLRETURN libpqPrepareOnThreadWithoutStoringResults(RS_STMT_INFO *pStmt, char *pszCmd)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_CONN_INFO *pConn = pStmt->phdbc;

    if(pszCmd)
    {
        PGresult *pgResult = NULL;
        ExecStatusType pqRc = PGRES_COMMAND_OK;
        int asyncEnable = isAsyncEnable(pStmt);
        int sendStatus = 1;

        if(asyncEnable)
        {
            sendStatus = pqSendPrepareAndDescribe(pConn->pgConn, pStmt->szCursorName, pszCmd, 0, NULL);

            if(sendStatus)
            {
                pgResult = PQgetResult(pConn->pgConn);
                pqRc = PQresultStatus(pgResult);
            }
            else
                pqRc = PGRES_FATAL_ERROR;
        }
        else
        {
            pgResult = pqPrepare(pConn->pgConn, pStmt->szCursorName, pszCmd, 0, NULL);
            pqRc = PQresultStatus(pgResult);
        }

        // Multi prepare loop
        do
        {
            if (!(pqRc == PGRES_COMMAND_OK || pqRc == PGRES_TUPLES_OK))
            {
                char *pError = libpqErrorMsg(pConn);

                // Even one result in error, we are retuning error.
                rc = SQL_ERROR;

                if(pError && *pError != '\0')
                    addError(&pStmt->pErrorList,"HY000", pError, 0, pConn);

                // Clear this result because we are not storing it
                PQclear(pgResult);
                pgResult = NULL;
            }
            else {
                PGresult *pgResultDescParam = PQgetResult(pConn->pgConn);

                if(pgResultDescParam == NULL)
                {
                    // 'Z' must be followed to 't' during above PQgetResult
                    // So read from resultForDescParam

                    pgResultDescParam = pqgetResultForDescribeParam(pConn->pgConn);
                }

                PQclear(pgResultDescParam);
                pgResultDescParam = NULL;

                if(pqRc == PGRES_TUPLES_OK)
                {
                    // SELECT kind of operation returning rows description

                    pgResult = PQgetResult(pConn->pgConn);
                    PQclear(pgResult);
                    pgResult = NULL;
                } // SELECT

            } // Success

            if ((pConn && pConn->pgConn && PQstatus(pConn->pgConn) == CONNECTION_BAD))
            {
                // No need to loop any more.
                break;
            }

            // Loop for next result
            if(asyncEnable)
            {
                // If command not send, no need to check for next result.
                if(!sendStatus)
                    break;
            }

            PQclear(pgResult);
            pgResult = NULL;

            // Get next result description
            pgResult = PQgetResult(pConn->pgConn);
            if(!pgResult)
            {
                // 'Z' must be followed to 'T' during above PQgetResult
                // So read from resultForDescRowPrep and release it.
                PGresult *pgResultDescRowPrep = pqgetResultForDescribeRowPrep(pConn->pgConn);

                PQclear(pgResultDescRowPrep);
                pgResultDescRowPrep = NULL;

                break;
            }

            // Get result status
            pqRc = PQresultStatus(pgResult);

        }while(TRUE); // Results  loop
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY000", "Invalid command buffer", 0, NULL);
        goto error;
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Free libpq buffer
//
void *libpqFreemem(void *ptr)
{
    if(ptr)
        PQfreemem(ptr);

    return NULL;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set streaming cursor rows
//
void libpqSetStreamingCursorRows(RS_STMT_INFO *pStmt)
{
	setStreamingCursorRows(pStmt->pCscStatementContext, pStmt->phdbc->pConnectProps->iStreamingCursorRows);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Set end of stream indicator
//
void libpqSetEndOfStreamingCursorQuery(RS_STMT_INFO *pStmt, int flag)
{
    setEndOfStreamingCursorQuery(pStmt->pCscStatementContext, flag);
}


//---------------------------------------------------------------------------------------------------------igarish
// Get end of stream indicator
//
int libpqIsEndOfStreamingCursor(RS_STMT_INFO *pStmt)
{
	return(isEndOfStreamingCursor(pStmt->pCscStatementContext));
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Get end of stream indicator
//
int libpqIsEndOfStreamingCursorQuery(RS_STMT_INFO *pStmt)
{
	return(isEndOfStreamingCursorQuery(pStmt->pCscStatementContext));
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Is it forward only curosr for given Stmt
//
int isForwardOnlyCursor(void *pCallerContext)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pCallerContext;
    int iResultsetType;

    if(pStmt)
    {
        if(isScrollableCursor(pStmt))
            iResultsetType = (int) ~SQL_CURSOR_FORWARD_ONLY;
        else
            iResultsetType = SQL_CURSOR_FORWARD_ONLY;
    }
    else
       iResultsetType = SQL_CURSOR_FORWARD_ONLY;

    return (iResultsetType == SQL_CURSOR_FORWARD_ONLY);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Is it streaming curosr for given Stmt
//
int isStreamingCursorMode(void *pCallerContext)
{
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)pCallerContext;
	int streamingCursorRows = (pStmt) ? pStmt->phdbc->pConnectProps->iStreamingCursorRows : 0;
	int iStreamingCursorMode = (streamingCursorRows > 0 && isForwardOnlyCursor(pCallerContext));

	return iStreamingCursorMode;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Lock and skip result of streaming cursor
//
void libpqCheckAndSkipAllResultsOfStreamingCursor(RS_STMT_INFO *pStmt, int iLockRequired)
{
    RS_CONN_INFO *pConn = pStmt->phdbc;

	if(iLockRequired)
	{
		iLockRequired = (pStmt->pCscStatementContext 
							&& isStreamingCursorMode(pStmt));
	}

    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);
	}

	// Skip all results of streaming cursor
	checkAndSkipAllResultsOfStreamingCursor(pStmt);

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Lock and skip result of streaming cursor
//
int libpqSkipCurrentResultOfStreamingCursor(RS_STMT_INFO *pStmt, void *_pCscStatementContext, PGresult *pgResult, PGconn *conn, int iLockRequired)
{
    RS_CONN_INFO *pConn = pStmt->phdbc;
	int rc;

	if(iLockRequired)
	{
		iLockRequired = (pStmt->pCscStatementContext 
							&& isStreamingCursorMode(pStmt));
	}

    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);
	}

	// Skip current result of streaming cursor
	rc = pqSkipCurrentResultOfStreamingCursor(_pCscStatementContext, pgResult, conn);

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }

	return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Lock and read next batch of result rows of streaming cursor
//
void libpqReadNextBatchOfStreamingRows(RS_STMT_INFO *pStmt, void *_pCscStatementContext, PGresult *pgResult, PGconn *conn,int *piError,int iLockRequired)
{
    RS_CONN_INFO *pConn = pStmt->phdbc;

	if(iLockRequired)
	{
		iLockRequired = (pStmt->pCscStatementContext 
							&& isStreamingCursorMode(pStmt));
	}

    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);
	}

	if(!libpqIsEndOfStreamingCursor(pStmt))
	{
		// Read next batch of result rows of streaming cursor
		pqReadNextBatchOfStreamingRows(_pCscStatementContext, pgResult, conn, piError);
	}

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }
}

//---------------------------------------------------------------------------------------------------------igarish
// Lock and read next result of streaming cursor
//
short libpqReadNextResultOfStreamingCursor(RS_STMT_INFO *pStmt, void *_pCscStatementContext, PGconn *conn, int iLockRequired)
{
    RS_CONN_INFO *pConn = pStmt->phdbc;
	short rc = SQL_SUCCESS;

	if(iLockRequired)
	{
		iLockRequired = (pStmt->pCscStatementContext 
							&& isStreamingCursorMode(pStmt));
	}

    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);
	}

	if(!libpqIsEndOfStreamingCursorQuery(pStmt))
	{
		// Read next result of streaming cursor
		rc = pqReadNextResultOfStreamingCursor(_pCscStatementContext, conn);
	}

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }

	return rc;
}

//---------------------------------------------------------------------------------------------------------igarish
// Lock and find any other streaming cursor open. TRUE if open.
//
int libpqDoesAnyOtherStreamingCursorOpen(RS_STMT_INFO *pStmt, int iLockRequired)
{
    RS_CONN_INFO *pConn = pStmt->phdbc;
	int rc;

	if(iLockRequired)
	{
		iLockRequired = (pStmt->pCscStatementContext 
							&& isStreamingCursorMode(pStmt));
	}

    if(iLockRequired)
    {
        // Lock connection sem to protect multiple stmt execution at same time.
        rsLockSem(pConn->hSemMultiStmt);
	}

	rc = doesAnyOtherStreamingCursorOpen(pConn, pStmt);

    if(iLockRequired)
    {
        // Unlock connection sem
        rsUnlockSem(pConn->hSemMultiStmt);
    }

	return rc;
}

/**
 * Allocates memory for the Implementation Row Descriptor (IRD) records.
 *
 * @param pStmt Pointer to the statement information structure.
 * @return true if allocation is successful, false otherwise.
 */
bool allocateIRDRecords(RS_STMT_INFO *pStmt) {
    if (!pStmt || pStmt->pIRD == NULL) {
        return false;
    }

    // Allocate memory for IRD records based on the number of columns
    pStmt->pIRD->pDescRecHead = (RS_DESC_REC *)rs_calloc(
        pStmt->pResultHead->iNumberOfCols, sizeof(RS_DESC_REC));
    if (!pStmt->pIRD->pDescRecHead) {
        return false;
    }
    pStmt->pIRD->iRecListType = RS_DESC_RECS_ARRAY_LIST;
    return true;
}

/**
 * Initializes the Implementation Row Descriptor (IRD) records for all columns.
 *
 * @param pStmt Pointer to the statement information structure.
 * @param colNum Number of columns in the result set.
 * @return true if initialization is successful for all records, false
 * otherwise.
 */
bool initializeIRDRecords(RS_STMT_INFO *pStmt, int colNum) {
    RS_DESC_REC *iRD = pStmt->pIRD->pDescRecHead;
    RS_RESULT_INFO *pResult = pStmt->pResultHead;
    if (!pResult) {
        return false;
    }

    // Iterate through all columns and initialize their IRD records
    for (int i = 0; i < colNum; i++) {
        RS_DESC_REC *pDescRec = &iRD[i];

        // Initialize individual IRD record
        if (!initializeIRDRecord(pStmt, pResult, pDescRec, i)) {
            return false;
        }

        // Link the current record to the previous one (except for the first
        // record)
        if (i > 0) {
            iRD[i - 1].pNext = pDescRec;
        }
    }
    return true;
}

/**
 * Initializes a single Implementation Row Descriptor (IRD) record.
 *
 * @param pStmt Pointer to the statement information structure.
 * @param pResult Pointer to the result information structure.
 * @param pDescRec Pointer to the descriptor record to be initialized.
 * @param i Column index.
 * @return true if initialization is successful, false otherwise.
 */
bool initializeIRDRecord(RS_STMT_INFO *pStmt, RS_RESULT_INFO *pResult,
                         RS_DESC_REC *pDescRec, int i) {
    if (!pResult || !pDescRec) {
        return false;
    }

    // Retrieve the column name
    char *pName = PQfname(pResult->pgResult, i);
    if (!pName) {
        handleIRDInitializationError(
            pStmt, "Fail to retrieve column name from result object");
        return false;
    }

    // Retrieve the Data type OID
    Oid pgType = PQftype(pResult->pgResult, i);
    if (pgType == InvalidOid) {
        handleIRDInitializationError(
            pStmt, "Fail to retrieve data type oid from result object");
        return false;
    }

    // Set the record number (1-based index)
    pDescRec->hRecNumber = i + 1;

    // Copy the column name to the descriptor record
    copyStrDataSmallLen(pName, SQL_NTS, pDescRec->szName, MAX_IDEN_LEN, NULL);

    // Map Data type OID to SQL type
    pDescRec->hType = mapPgTypeToSqlType(pgType, &(pDescRec->hRsSpecialType));
    if (pDescRec->hType == SQL_UNKNOWN_TYPE) {
        handleIRDInitializationError(pStmt,
                                     "Fail to convert pg type oid to sql type");
        return false;
    }

    // Set additional attributes for the descriptor record
    setDescRecAttributes(pStmt, pDescRec, pgType, pName);
    return true;
}

/**
 * Sets various attributes for a descriptor record based on the Data type OID
 * and column name.
 *
 * @param pStmt Pointer to the statement information structure.
 * @param pDescRec Pointer to the descriptor record to be updated.
 * @param pgType Data type OID.
 * @param pName Column name.
 */
void setDescRecAttributes(RS_STMT_INFO *pStmt, RS_DESC_REC *pDescRec,
                          Oid pgType, const char *pName) {

    if (!pStmt || !pDescRec || !pName) {
        RS_LOG_ERROR("setDescRecAttributes", "Input validation failed");
        return;
    }

    // Set size and case sensitivity based on Data type OID
    if (pgType == INT2OID || pgType == INT4OID || pgType == INT8OID) {
        pDescRec->iSize = (pgType == INT2OID) ? INT2_LEN : 
                          (pgType == INT4OID) ? INT4_LEN : INT8_LEN;
        pDescRec->cCaseSensitive = SQL_FALSE;
    } else {
        // Set size for special column names or use default
        if (strcmp(pName, "REMARKS") == 0) {
            pDescRec->iSize = MAX_REMARK_LEN;
        } else if (strcmp(pName, "COLUMN_DEF") == 0) {
            pDescRec->iSize = MAX_COLUMN_DEF_LEN;
        } else {
            pDescRec->iSize = NAMEDATALEN;
        }
        pDescRec->cCaseSensitive = getCaseSensitive(pStmt);
    }

    // Set various other attributes
    pDescRec->hScale = 0;
    pDescRec->hNullable = SQL_TRUE;
    pDescRec->cAutoInc = SQL_FALSE;
    pDescRec->iDisplaySize = getDisplaySize(
        pDescRec->hType, pDescRec->iSize, pDescRec->hRsSpecialType);
    pDescRec->cFixedPrecScale = SQL_FALSE;
    getLiteralPrefix(pDescRec->hType, pDescRec->szLiteralPrefix,
                        pDescRec->hRsSpecialType);
    getLiteralSuffix(pDescRec->hType, pDescRec->szLiteralSuffix,
                        pDescRec->hRsSpecialType);
    getTypeName(pDescRec->hType, pDescRec->szTypeName,
                sizeof(pDescRec->szTypeName), pDescRec->hRsSpecialType);
    pDescRec->iNumPrecRadix = getNumPrecRadix(pDescRec->hType);
    pDescRec->iOctetLen = getOctetLen(pDescRec->hType, pDescRec->iSize,
                                        pDescRec->hRsSpecialType);
    pDescRec->iPrecision = getPrecision(
        pDescRec->hType, pDescRec->iSize, pDescRec->hRsSpecialType);
    pDescRec->iSearchable =
        getSearchable(pDescRec->hType, pDescRec->hRsSpecialType);
    pDescRec->iUnNamed = getUnNamed(pDescRec->szName);
    pDescRec->cUnsigned = getUnsigned(pDescRec->hType);
    pDescRec->iUpdatable = getUpdatable();
}

/**
 * Handles errors during IRD record initialization.
 *
 * @param pStmt Pointer to the statement information structure.
 * @param errorMessage Error message to be logged and added to the error list.
 */
void handleIRDInitializationError(RS_STMT_INFO *pStmt, char *errorMessage) {
    if (!pStmt) {
        RS_LOG_ERROR("handleIRDInitializationError", "Invalid statement handler");
        return;
    }

    if (pStmt->pIRD != NULL) {
        pStmt->pIRD = releaseDescriptor(pStmt->pIRD, TRUE);
    }
    addError(&pStmt->pErrorList, "HY000", errorMessage, 0, NULL);
}

/**
 * Helper function to handle error cleanup and reporting in libpqInitializeResultSetField.
 *
 * @param pStmt Pointer to the statement information structure.
 * @param column Pointer to column attributes to be cleaned up.
 * @param colNum Number of columns.
 * @param result PGresult to be cleared (can be NULL).
 * @param errorMessage Error message to be added to the error list.
 * @return SQL_ERROR.
 */
SQLRETURN handleInitializeResultSetError(RS_STMT_INFO *pStmt,
                                               PGresAttDesc *column,
                                               int colNum,
                                               PGresult *result,
                                               char *errorMessage) {
    if (column) {
        PQcleanupCustomizeAttrs(column, colNum);
    }

    if (result) {
        PQclear(result);
    }

    if (pStmt && pStmt->pResultHead) {
        releaseResultObject(pStmt->pResultHead);
        pStmt->pResultHead = NULL;
    }

    if (pStmt && errorMessage) {
        addError(&pStmt->pErrorList, "HY000", errorMessage, 0, NULL);
    }

    return SQL_ERROR;
}

/**
 * Initializes a custom PGResult object for a result set.
 *
 * @param pStmt Pointer to the statement information structure.
 * @param colName Array of column names.
 * @param colNum Number of columns.
 * @param colDatatype Array of column data types.
 * @return SQL_SUCCESS if successful, SQL_ERROR or SQL_INVALID_HANDLE otherwise.
 */
SQLRETURN libpqInitializeResultSetField(RS_STMT_INFO *pStmt, char **colName,
                                        int colNum, int *colDatatype) {

    RS_LOG_TRACE("libpqInitializeResultSetField",
                 "Start creating customized result set object");

    // Validate input parameters
    if (!pStmt) {
        // Log the error instead of adding into error list since
        // statement is invalid
        RS_LOG_ERROR("libpqInitializeResultSetField", "Invalid statement handler");
        return SQL_INVALID_HANDLE;
    }
    if (!colName || !colDatatype) {
        addError(&pStmt->pErrorList, "HY000",
                "Column name / Column data type should be provided", 0, NULL);
        return SQL_ERROR;
    }

    SQLRETURN rc = SQL_SUCCESS;
    PGresAttDesc *column = NULL;
    PGresult *result = NULL;

    // Create customize column attribute description
    column = PQcreateCustomizeAttrs(colName, colNum, colDatatype);
    if (!column) {
        addError(&pStmt->pErrorList, "HY000",
                "Failed to create column attributes", 0, NULL);
        return SQL_ERROR;
    }

    // Create a newly allocated, initialized PGresult with given status
    result = PQmakeEmptyPGresult(pStmt->phdbc->pgConn, PGRES_TUPLES_OK);
    if (!result) {
        PQcleanupCustomizeAttrs(column, colNum);
        addError(&pStmt->pErrorList, "HY000",
                "Failed to create PGresult", 0, NULL);
        return SQL_ERROR;
    }

    // Create new RS_RESULT_INFO object with initialized PGresult
    pStmt->pResultHead = createResultObject(pStmt, result);
    if (!pStmt->pResultHead) {
        return handleInitializeResultSetError(pStmt, column, colNum, result, "Failed to create result object");
    }

    RS_RESULT_INFO *pResult = pStmt->pResultHead;

    // Set the column number in RS_RESULT_INFO object
    pResult->iNumberOfCols = colNum;

    // Populate column (PGresAttDesc*) into pResult->pgResult (PGresult *)
    if (!PQsetResultAttrs(pResult->pgResult, colNum, column)) {
        return handleInitializeResultSetError(pStmt, column, colNum, result, "Failed to set result attributes");
    }

    // Allocate IRD records
    if (!allocateIRDRecords(pStmt)) {
        return handleInitializeResultSetError(pStmt, column, colNum, result, "Failed to allocate IRD records");
    }

    // Initialized IRD records
    if (!initializeIRDRecords(pStmt, colNum)) {
        return handleInitializeResultSetError(pStmt, column, colNum, result, "Failed to initialize IRD records");
    }

    PQcleanupCustomizeAttrs(column, colNum);
    RS_LOG_TRACE("rslibpq", "Successfully create empty PGresult object");
    return rc;
}

SQLRETURN libpqCreateSQLGetTypeInfoCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<RS_TYPE_INFO> &typeInfo) {

    const int typeInfoSize = typeInfo.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Update the cursor state
    pStmt->iStatus = RS_EXECUTE_STMT;

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = typeInfoSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (typeInfoSize == 0) {
        return SQL_SUCCESS;
    }

    for (int i = 0; i < typeInfoSize; i++) {
        PQsetvalue(res, i, kSQLGetTypeInfo_TYPE_NAME_COL_NUM,
                   (char *)typeInfo[i].szTypeName.c_str(),
                   typeInfo[i].szTypeName.size());
        std::string dataType = std::to_string(typeInfo[i].hType);
        PQsetvalue(res, i, kSQLGetTypeInfo_DATA_TYPE_COL_NUM, dataType.data(),
                   dataType.size());
        std::string columnSize = std::to_string(typeInfo[i].iColumnSize);
        PQsetvalue(res, i, kSQLGetTypeInfo_COLUMN_SIZE_COL_NUM,
                   columnSize.data(), columnSize.size());
        PQsetvalue(res, i, kSQLGetTypeInfo_LITERAL_PREFIX_COL_NUM,
                   (char *)typeInfo[i].szLiteralPrefix.c_str(),
                   typeInfo[i].szLiteralPrefix.size());
        PQsetvalue(res, i, kSQLGetTypeInfo_LITERAL_SUFFIX_COL_NUM,
                   (char *)typeInfo[i].szLiteralSuffix.c_str(),
                   typeInfo[i].szLiteralSuffix.size());
        PQsetvalue(res, i, kSQLGetTypeInfo_CREATE_PARAMS_COL_NUM,
                   (char *)typeInfo[i].szCreateParams.c_str(),
                   typeInfo[i].szCreateParams.size());
        std::string nullable = std::to_string(typeInfo[i].hNullable);
        PQsetvalue(res, i, kSQLGetTypeInfo_NULLABLE_COL_NUM, nullable.data(),
                   nullable.size());
        std::string caseSensitive = std::to_string(typeInfo[i].iCaseSensitive);
        PQsetvalue(res, i, kSQLGetTypeInfo_CASE_SENSITIVE_COL_NUM,
                   caseSensitive.data(), caseSensitive.size());
        std::string searchable = std::to_string(typeInfo[i].iSearchable);
        PQsetvalue(res, i, kSQLGetTypeInfo_SEARCHABLE_COL_NUM,
                   searchable.data(), searchable.size());
        std::string unsignedAttr = std::to_string(typeInfo[i].iUnsigned);
        PQsetvalue(res, i, kSQLGetTypeInfo_UNSIGNED_ATTRIBUTE_COL_NUM,
                   unsignedAttr.data(), unsignedAttr.size());
        std::string precScale = std::to_string(typeInfo[i].iFixedPrecScale);
        PQsetvalue(res, i, kSQLGetTypeInfo_FIXED_PREC_SCALE_COL_NUM,
                   precScale.data(), precScale.size());
        std::string autoInc = std::to_string(typeInfo[i].iAutoInc);
        PQsetvalue(res, i, kSQLGetTypeInfo_AUTO_UNIQUE_VAL_COL_NUM,
                   autoInc.data(), autoInc.size());
        PQsetvalue(res, i, kSQLGetTypeInfo_LOCAL_TYPE_NAME_COL_NUM,
                   (char *)typeInfo[i].szLocalTypeName.c_str(),
                   typeInfo[i].szLocalTypeName.size());
        std::string minScale = std::to_string(typeInfo[i].hMinScale);
        PQsetvalue(res, i, kSQLGetTypeInfo_MINIMUM_SCALE_COL_NUM,
                   minScale.data(), minScale.size());
        std::string maxScale = std::to_string(typeInfo[i].hMaxScale);
        PQsetvalue(res, i, kSQLGetTypeInfo_MAXIMUM_SCALE_COL_NUM,
                   maxScale.data(), maxScale.size());
        std::string sqlDataType = std::to_string(typeInfo[i].hSqlDataType);
        PQsetvalue(res, i, kSQLGetTypeInfo_SQL_DATA_TYPE_COL_NUM,
                   sqlDataType.data(), sqlDataType.size());
        std::string sqlDateTimeSub =
            std::to_string(typeInfo[i].hSqlDateTimeSub);
        PQsetvalue(res, i, kSQLGetTypeInfo_SQL_DATETIME_SUB_COL_NUM,
                   sqlDateTimeSub.data(), sqlDateTimeSub.size());
        std::string numPrexRadix = std::to_string(typeInfo[i].iNumPrexRadix);
        PQsetvalue(res, i, kSQLGetTypeInfo_NUM_PREC_RADIX_COL_NUM,
                   numPrexRadix.data(), numPrexRadix.size());
        std::string intervalPrecision =
            std::to_string(typeInfo[i].iIntervalPrecision);
        PQsetvalue(res, i, kSQLGetTypeInfo_INTERVAL_PRECISION_COL_NUM,
                   intervalPrecision.data(), intervalPrecision.size());
    }

    return SQL_SUCCESS;
}

// Helper function to create result set for SQLTables special call to get
// catalog list
SQLRETURN libpqCreateSQLCatalogsCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<std::string> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    for (int i = 0; i < intermediateRSSize; i++) {
        PQsetvalue(res, i, kSQLTables_TABLE_CATALOG_COL_NUM,
                   (char *)intermediateRS[i].c_str(), intermediateRS[i].size());
        PQsetvalue(res, i, kSQLTables_TABLE_SCHEM_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_NAME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_TYPE_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_REMARKS_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_OWNER_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_LAST_ALTERED_TIME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_LAST_MODIFIED_TIME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_DIST_STYLE_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_SUBTYPE_COL_NUM, NULL, NULL_LEN);
    }

    return SQL_SUCCESS;
}

// Helper function to create result set for SQLTables special call to get schema
// list
SQLRETURN libpqCreateSQLSchemasCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<SHOWSCHEMASResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    for (int i = 0; i < intermediateRSSize; i++) {
        PQsetvalue(res, i, kSQLTables_TABLE_CATALOG_COL_NUM,
                   (char *)intermediateRS[i].database_name,
                   intermediateRS[i].database_name_Len);
        PQsetvalue(res, i, kSQLTables_TABLE_SCHEM_COL_NUM,
                   (char *)intermediateRS[i].schema_name,
                   intermediateRS[i].schema_name_Len);
        PQsetvalue(res, i, kSQLTables_TABLE_NAME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_TYPE_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_REMARKS_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_OWNER_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_LAST_ALTERED_TIME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_LAST_MODIFIED_TIME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_DIST_STYLE_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_SUBTYPE_COL_NUM, NULL, NULL_LEN);
    }

    return SQL_SUCCESS;
}

// Helper function to create result set for SQLTables special call to get table
// type list
SQLRETURN libpqCreateSQLTableTypesCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<std::string> &tableTypeList) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = tableTypeList.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    for (int i = 0; i < intermediateRSSize; i++) {
        PQsetvalue(res, i, kSQLTables_TABLE_CATALOG_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_SCHEM_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_NAME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_TYPE_COL_NUM,
                   (char *)tableTypeList[i].c_str(), tableTypeList[i].size());
        PQsetvalue(res, i, kSQLTables_REMARKS_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_OWNER_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_LAST_ALTERED_TIME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_LAST_MODIFIED_TIME_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_DIST_STYLE_COL_NUM, NULL, NULL_LEN);
        PQsetvalue(res, i, kSQLTables_TABLE_SUBTYPE_COL_NUM, NULL, NULL_LEN);
    }

    return SQL_SUCCESS;
}

// Helper function to create result set for SQLTables and apply table type
// filter
SQLRETURN libpqCreateSQLTablesCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum, const std::string &tableType,
    const std::vector<SHOWTABLESResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;
        return SQL_SUCCESS;
    }

    int finalRowNum = 0;

    // Create table type filter
    std::unordered_set<std::string> typeSet;
    if (!tableType.empty()) {
        std::stringstream ss(tableType);
        std::string temp;
        while (std::getline(ss, temp, ',')) {
            if (temp.find('\'') != std::string::npos) {
                // Remove the surrounding single quotes
                temp = temp.substr(1, temp.length() - 2);
            }
            typeSet.insert(temp);
        }
    }

    for (int i = 0; i < intermediateRSSize; i++) {
        if (tableType.empty() ||
            typeSet.find(char2String(intermediateRS[i].table_type)) !=
                typeSet.end()) {
            PQsetvalue(res, finalRowNum, kSQLTables_TABLE_CATALOG_COL_NUM,
                       (char *)intermediateRS[i].database_name,
                       intermediateRS[i].database_name_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_TABLE_SCHEM_COL_NUM,
                       (char *)intermediateRS[i].schema_name,
                       intermediateRS[i].schema_name_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_TABLE_NAME_COL_NUM,
                       (char *)intermediateRS[i].table_name,
                       intermediateRS[i].table_name_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_TABLE_TYPE_COL_NUM,
                       (char *)intermediateRS[i].table_type,
                       intermediateRS[i].table_type_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_REMARKS_COL_NUM,
                       (char *)intermediateRS[i].remarks,
                       intermediateRS[i].remarks_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_OWNER_COL_NUM,
                       (char *)intermediateRS[i].owner,
                       intermediateRS[i].owner_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_LAST_ALTERED_TIME_COL_NUM,
                       (char *)intermediateRS[i].last_altered_time,
                       intermediateRS[i].last_altered_time_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_LAST_MODIFIED_TIME_COL_NUM,
                       (char *)intermediateRS[i].last_modified_time,
                       intermediateRS[i].last_modified_time_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_DIST_STYLE_COL_NUM,
                       (char *)intermediateRS[i].dist_style,
                       intermediateRS[i].dist_style_Len);
            PQsetvalue(res, finalRowNum, kSQLTables_TABLE_SUBTYPE_COL_NUM,
                       (char *)intermediateRS[i].table_subtype,
                       intermediateRS[i].table_subtype_Len);
            finalRowNum += 1;
        }
    }

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = finalRowNum;

    return SQL_SUCCESS;
}

// Helper function to create result set for SQLColumns
SQLRETURN libpqCreateSQLColumnsCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<SHOWCOLUMNSResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    SQLINTEGER ODBCVer = pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion;
    RS_LOG_DEBUG("RSLIBPQ", "ODBC spec version: %d", ODBCVer);

    int columnSize = 0, bufferLen = 0, charOctetLen = 0;
    short sqlType = 0, sqlDataType = 0, sqlDateSub = 0, precisions = 0,
          decimalDigit = 0, num_pre_radix = 0;

    std::string numStr = {0};

    bool dateTimeCustomizePrecision = false;

    for (int i = 0; i < intermediateRSSize; i++) {
        // Reset the variable
        columnSize = 0;
        bufferLen = 0;
        charOctetLen = 0;
        sqlType = 0;
        sqlDataType = 0;
        sqlDateSub = 0;
        precisions = 0;
        decimalDigit = 0;
        num_pre_radix = 0;
        dateTimeCustomizePrecision = false;
        std::string rsType;
        std::string dataType = char2String(intermediateRS[i].data_type);

        ProcessedTypeInfo processedType = RsMetadataAPIHelper::processDataTypeInfo(
            dataType,
            ODBCVer
        );

        if (processedType.typeInfoResult.found) {
            sqlType = processedType.typeInfoResult.typeInfo.sqlType;
            sqlDataType = processedType.typeInfoResult.typeInfo.sqlDataType;
            sqlDateSub = processedType.typeInfoResult.typeInfo.sqlDateSub;
            rsType = processedType.typeInfoResult.typeInfo.typeName;
        } else {
            rsType = processedType.cleanedTypeName;
        }

        if (processedType.dateTimeInfo.hasCustomPrecision) {
            precisions = processedType.dateTimeInfo.precision;
            dateTimeCustomizePrecision = true;
        }

        columnSize = RsMetadataAPIHelper::getColumnSize(
            rsType, intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);

        bufferLen = RsMetadataAPIHelper::getBufferLen(
            rsType, intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);

        decimalDigit = RsMetadataAPIHelper::getDecimalDigit(
            rsType, intermediateRS[i].numeric_scale, precisions,
            dateTimeCustomizePrecision);

        num_pre_radix = RsMetadataAPIHelper::getNumPrecRadix(rsType);

        charOctetLen = RsMetadataAPIHelper::getCharOctetLen(
            rsType, intermediateRS[i].character_maximum_length);

        // Catalog name
        PQsetvalue(res, i, kSQLColumns_TABLE_CAT_COL_NUM,
                   (char *)intermediateRS[i].database_name,
                   intermediateRS[i].database_name_Len);

        // Schema name
        PQsetvalue(res, i, kSQLColumns_TABLE_SCHEM_COL_NUM,
                   (char *)intermediateRS[i].schema_name,
                   intermediateRS[i].schema_name_Len);

        // Table name
        PQsetvalue(res, i, kSQLColumns_TABLE_NAME_COL_NUM,
                   (char *)intermediateRS[i].table_name,
                   intermediateRS[i].table_name_Len);

        // Column name
        PQsetvalue(res, i, kSQLColumns_COLUMN_NAME_COL_NUM,
                   (char *)intermediateRS[i].column_name,
                   intermediateRS[i].column_name_Len);

        // SQL type (concise data type)
        numStr = std::to_string(sqlType);
        PQsetvalue(res, i, kSQLColumns_DATA_TYPE_COL_NUM, numStr.data(),
                   numStr.size());

        // Redshift type name
        PQsetvalue(res, i, kSQLColumns_TYPE_NAME_COL_NUM, (char *)rsType.c_str(),
                   rsType.size());

        // Column size
        if (columnSize == kNotApplicable) {
            PQsetvalue(res, i, kSQLColumns_COLUMN_SIZE_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(columnSize);
            PQsetvalue(res, i, kSQLColumns_COLUMN_SIZE_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Buffer length
        if (bufferLen == kNotApplicable) {
            PQsetvalue(res, i, kSQLColumns_BUFFER_LENGTH_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(bufferLen);
            PQsetvalue(res, i, kSQLColumns_BUFFER_LENGTH_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Decimal digits
        if (decimalDigit == kNotApplicable) {
            // Return NULL where DECIMAL_DIGITS is not applicable
            PQsetvalue(res, i, kSQLColumns_DECIMAL_DIGITS_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(decimalDigit);
            PQsetvalue(res, i, kSQLColumns_DECIMAL_DIGITS_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Num prec radix
        if (num_pre_radix == kNotApplicable) {
            // Return NULL where NUM_PREC_RADIX is not applicable
            PQsetvalue(res, i, kSQLColumns_NUM_PREC_RADIX_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(num_pre_radix);
            PQsetvalue(res, i, kSQLColumns_NUM_PREC_RADIX_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Nullable
        numStr = std::to_string(RsMetadataAPIHelper::getNullable(
            char2String(intermediateRS[i].is_nullable)));
        PQsetvalue(res, i, kSQLColumns_NULLABLE_COL_NUM, numStr.data(),
                   numStr.size());

        // Remarks
        PQsetvalue(res, i, kSQLColumns_REMARKS_COL_NUM,
                   (char *)intermediateRS[i].remarks,
                   intermediateRS[i].remarks_Len);

        // Column default
        PQsetvalue(res, i, kSQLColumns_COLUMN_DEF_COL_NUM,
                   (char *)intermediateRS[i].column_default,
                   intermediateRS[i].column_default_Len);

        // SQL Data type (non-concise data type)
        numStr = std::to_string(sqlDataType);
        PQsetvalue(res, i, kSQLColumns_SQL_DATA_TYPE_COL_NUM, numStr.data(),
                   numStr.size());

        // SQL Date data type subtype code
        if (sqlDateSub == kNotApplicable) {
            PQsetvalue(res, i, kSQLColumns_SQL_DATETIME_SUB_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(sqlDateSub);
            PQsetvalue(res, i, kSQLColumns_SQL_DATETIME_SUB_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // char octet length
        if (charOctetLen == kNotApplicable) {
            PQsetvalue(res, i, kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(charOctetLen);
            PQsetvalue(res, i, kSQLColumns_CHAR_OCTET_LENGTH_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Ordinal position
        numStr = std::to_string(intermediateRS[i].ordinal_position);
        PQsetvalue(res, i, kSQLColumns_ORDINAL_POSITION_COL_NUM, numStr.data(),
                   numStr.size());

        // Is nullable
        PQsetvalue(res, i, kSQLColumns_IS_NULLABLE_COL_NUM,
                   (char *)intermediateRS[i].is_nullable,
                   intermediateRS[i].is_nullable_Len);

        // Sort key type
        PQsetvalue(res, i, kSQLColumns_SORT_KEY_TYPE_COL_NUM,
                   (char *)intermediateRS[i].sort_key_type,
                   intermediateRS[i].sort_key_type_Len);

        // Sort key
        if (intermediateRS[i].sort_key_Len == SQL_NULL_DATA) {
            PQsetvalue(res, i, kSQLColumns_SORT_KEY_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(intermediateRS[i].sort_key);
            PQsetvalue(res, i, kSQLColumns_SORT_KEY_COL_NUM, numStr.data(),
                    numStr.size());
        }

        // Dist key
        if (intermediateRS[i].dist_key_Len == SQL_NULL_DATA) {
            PQsetvalue(res, i, kSQLColumns_DIST_KEY_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(intermediateRS[i].dist_key);
            PQsetvalue(res, i, kSQLColumns_DIST_KEY_COL_NUM, numStr.data(),
                    numStr.size());
        }
        // Encoding
        PQsetvalue(res, i, kSQLColumns_ENCODING_COL_NUM,
                   (char *)intermediateRS[i].encoding,
                   intermediateRS[i].encoding_Len);

        // Collation
        PQsetvalue(res, i, kSQLColumns_COLLATION_COL_NUM,
                   (char *)intermediateRS[i].collation,
                   intermediateRS[i].collation_Len);
    }

    return SQL_SUCCESS;
}

SQLRETURN libpqCreateSQLPrimaryKeysCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<SHOWCONSTRAINTSPRIMARYKEYSResult>& intermediateRS) {

    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLPrimaryKeysCustomizedResultSet", "Statement handle allocation failed");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    for (int i = 0; i < intermediateRSSize; i++) {
        const auto& row = intermediateRS[i];

        // TABLE_CAT (catalog name)
        PQsetvalue(res, i, kSQLPrimaryKeys_TABLE_CAT_COL_NUM,
                   (char *)row.database_name,
                   row.database_name_Len);

        // TABLE_SCHEM (schema name)
        PQsetvalue(res, i, kSQLPrimaryKeys_TABLE_SCHEM_COL_NUM,
                   (char *)row.schema_name,
                   row.schema_name_Len);

        // TABLE_NAME
        PQsetvalue(res, i, kSQLPrimaryKeys_TABLE_NAME_COL_NUM,
                   (char *)row.table_name,
                   row.table_name_Len);

        // COLUMN_NAME
        PQsetvalue(res, i, kSQLPrimaryKeys_COLUMN_NAME_COL_NUM,
                   (char *)row.column_name,
                   row.column_name_Len);

        // KEY_SEQ (sequence number within primary key)
        std::string keySeqStr = std::to_string(row.key_seq);
        if (PQsetvalue(res, i, kSQLPrimaryKeys_KEY_SEQ_COL_NUM,
                    keySeqStr.data(), keySeqStr.length()) != 1) {
            RS_LOG_ERROR("libpqCreateSQLPrimaryKeysCustomizedResultSet", "Failed to set KEY_SEQ");
            return SQL_ERROR;
        }

        // PK_NAME (primary key name)
        PQsetvalue(res, i, kSQLPrimaryKeys_PK_NAME_COL_NUM,
                   (char *)row.pk_name,
                   row.pk_name_Len);
    }

    return SQL_SUCCESS;
}

// Helper function to create result set for SQLForeignKeys
SQLRETURN libpqCreateSQLForeignKeysCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<SHOWCONSTRAINTSFOREIGNKEYSResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLForeignKeysCustomizedResultSet", "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    std::string numStr = {0};
    for (int i = 0; i < intermediateRSSize; i++) {
        // pk Catalog name
        PQsetvalue(res, i, kSQLForeignKeys_PKTABLE_CAT_COL_NUM,
                   (char *)intermediateRS[i].pk_table_cat,
                   intermediateRS[i].pk_table_cat_Len);

        // pk Schema name
        PQsetvalue(res, i, kSQLForeignKeys_PKTABLE_SCHEM_COL_NUM,
                   (char *)intermediateRS[i].pk_table_schem,
                   intermediateRS[i].pk_table_schem_Len);

        // pk Table name
        PQsetvalue(res, i, kSQLForeignKeys_PKTABLE_NAME_COL_NUM,
                   (char *)intermediateRS[i].pk_table_name,
                   intermediateRS[i].pk_table_name_Len);

        // pk Column name
        PQsetvalue(res, i, kSQLForeignKeys_PKCOLUMN_NAME_COL_NUM,
                   (char *)intermediateRS[i].pk_column_name,
                   intermediateRS[i].pk_column_name_Len);

        // fk Catalog name
        PQsetvalue(res, i, kSQLForeignKeys_FKTABLE_CAT_COL_NUM,
                   (char *)intermediateRS[i].fk_table_cat,
                   intermediateRS[i].fk_table_cat_Len);

        // fk Schema name
        PQsetvalue(res, i, kSQLForeignKeys_FKTABLE_SCHEM_COL_NUM,
                   (char *)intermediateRS[i].fk_table_schem,
                   intermediateRS[i].fk_table_schem_Len);

        // fk Table name
        PQsetvalue(res, i, kSQLForeignKeys_FKTABLE_NAME_COL_NUM,
                   (char *)intermediateRS[i].fk_table_name,
                   intermediateRS[i].fk_table_name_Len);

        // fk Column name
        PQsetvalue(res, i, kSQLForeignKeys_FKCOLUMN_NAME_COL_NUM,
                   (char *)intermediateRS[i].fk_column_name,
                   intermediateRS[i].fk_column_name_Len);

        // Key sequence number
        numStr = std::to_string(intermediateRS[i].key_seq);
        PQsetvalue(res, i, kSQLForeignKeys_KEY_SEQ_COL_NUM, numStr.data(),
                   numStr.size());

        // Update role
        numStr = intermediateRS[i].update_rule_Len == SQL_NULL_DATA
                       ? std::to_string(SQL_NO_ACTION)
                       : std::to_string(intermediateRS[i].update_rule);
        PQsetvalue(res, i, kSQLForeignKeys_UPDATE_RULE_COL_NUM, numStr.data(),
                   numStr.size());

        // Delete role
        numStr = intermediateRS[i].delete_rule_Len == SQL_NULL_DATA
                       ? std::to_string(SQL_NO_ACTION)
                       : std::to_string(intermediateRS[i].delete_rule);
        PQsetvalue(res, i, kSQLForeignKeys_DELETE_RULE_COL_NUM, numStr.data(),
                   numStr.size());

        // fk name
        PQsetvalue(res, i, kSQLForeignKeys_FK_NAME_COL_NUM,
                   (char *)intermediateRS[i].fk_name,
                   intermediateRS[i].fk_name_Len);

        // pk name
        PQsetvalue(res, i, kSQLForeignKeys_PK_NAME_COL_NUM,
                   (char *)intermediateRS[i].pk_name,
                   intermediateRS[i].pk_name_Len);

        // deferrability
        numStr = std::to_string(intermediateRS[i].deferrability_Len == SQL_NULL_DATA
                                       ? SQL_NOT_DEFERRABLE
                                       : intermediateRS[i].deferrability);
        PQsetvalue(res, i, kSQLForeignKeys_DEFERRABILITY_COL_NUM,
                   numStr.data(), numStr.size());
    }
    return SQL_SUCCESS;
}

SQLRETURN libpqCreateSQLSpecialColumnsCustomizedResultSet(
    RS_STMT_INFO *pStmt,
    short columnNum,
    const std::vector<SHOWCOLUMNSResult> &intermediateRS,
    SQLUSMALLINT identifierType) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    SQLINTEGER ODBCVer = pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion;
    RS_LOG_DEBUG("RSLIBPQ", "ODBC spec version: %d", ODBCVer);

    int columnSize = 0, bufferLen = 0, charOctetLen = 0;
    short sqlType = 0, sqlDataType = 0, sqlDateSub = 0, precisions = 0,
          decimalDigit = 0, num_pre_radix = 0;

    std::string strValue = {0};

    bool dateTimeCustomizePrecision = false;

    for (int i = 0; i < intermediateRSSize; i++) {
        // Reset variables
        sqlType = 0;
        columnSize = 0;
        bufferLen = 0;
        precisions = 0;
        decimalDigit = 0;
        dateTimeCustomizePrecision = false;
        std::string rsType;
        std::string dataType = char2String(intermediateRS[i].data_type);

        ProcessedTypeInfo processedType = RsMetadataAPIHelper::processDataTypeInfo(
            dataType,
            ODBCVer
        );

        if (processedType.typeInfoResult.found) {
            sqlType = processedType.typeInfoResult.typeInfo.sqlType;
            sqlDataType = processedType.typeInfoResult.typeInfo.sqlDataType;
            sqlDateSub = processedType.typeInfoResult.typeInfo.sqlDateSub;
            rsType = processedType.typeInfoResult.typeInfo.typeName;
        } else {
            rsType = processedType.cleanedTypeName;
        }

        if (processedType.dateTimeInfo.hasCustomPrecision) {
            precisions = processedType.dateTimeInfo.precision;
            dateTimeCustomizePrecision = true;
        }

        columnSize = RsMetadataAPIHelper::getColumnSize(
            rsType, intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);

        bufferLen = RsMetadataAPIHelper::getBufferLen(
            rsType, intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);

        decimalDigit = RsMetadataAPIHelper::getDecimalDigit(
            rsType, intermediateRS[i].numeric_scale, precisions,
            dateTimeCustomizePrecision);

        // SCOPE 
        strValue = std::to_string(identifierType == SQL_BEST_ROWID ? 2 : 0);
        PQsetvalue(res, i, kSQLSpecialColumns_SCOPE,
                  strValue.data(),
                  strValue.length());

        // COLUMN_NAME
        PQsetvalue(res, i, kSQLSpecialColumns_COLUMN_NAME,
                  (char *)intermediateRS[i].column_name,
                  intermediateRS[i].column_name_Len);

        // DATA_TYPE
        strValue = std::to_string(sqlType);
        PQsetvalue(res, i, kSQLSpecialColumns_DATA_TYPE,
                  strValue.data(),
                  strValue.length());

        // TYPE_NAME
        PQsetvalue(res, i, kSQLSpecialColumns_TYPE_NAME,
                  rsType.data(),
                  rsType.length());

        // COLUMN_SIZE
        columnSize = RsMetadataAPIHelper::getColumnSize(
            rsType,
            intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);
        
        if (columnSize == kNotApplicable) {
            PQsetvalue(res, i, kSQLSpecialColumns_COLUMN_SIZE, NULL, NULL_LEN);
        } else {
            strValue = std::to_string(columnSize);
            PQsetvalue(res, i, kSQLSpecialColumns_COLUMN_SIZE,
                      strValue.data(),
                      strValue.length());
        }

        // BUFFER_LENGTH
        bufferLen = RsMetadataAPIHelper::getBufferLen(
            rsType,
            intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);
        
        if (bufferLen == kNotApplicable) {
            PQsetvalue(res, i, kSQLSpecialColumns_BUFFER_LENGTH, NULL, NULL_LEN);
        } else {
            strValue = std::to_string(bufferLen);
            PQsetvalue(res, i, kSQLSpecialColumns_BUFFER_LENGTH,
                      strValue.data(),
                      strValue.length());
        }

        // DECIMAL_DIGITS
        decimalDigit = RsMetadataAPIHelper::getDecimalDigit(
            rsType,
            intermediateRS[i].numeric_scale,
            precisions,
            dateTimeCustomizePrecision);
        
        if (decimalDigit == kNotApplicable) {
            PQsetvalue(res, i, kSQLSpecialColumns_DECIMAL_DIGITS, NULL, NULL_LEN);
        } else {
            strValue = std::to_string(decimalDigit);
            PQsetvalue(res, i, kSQLSpecialColumns_DECIMAL_DIGITS,
                      strValue.data(),
                      strValue.length());
        }

        // PSEUDO_COLUMN
        int pseudoColumnValue = (identifierType == SQL_BEST_ROWID) ? 1 :
                              (identifierType == SQL_ROWVER) ? 2 : 0;
        strValue = std::to_string(pseudoColumnValue);
        PQsetvalue(res, i, kSQLSpecialColumns_PSEUDO_COLUMN,
                  strValue.data(),
                  strValue.length());
    }

    return SQL_SUCCESS;
}


SQLRETURN libpqCreateSQLTablePrivilegesCustomizedResultSet(
    RS_STMT_INFO *pStmt, 
    short columnNum,
    const std::vector<SHOWGRANTSTABLEResult>& intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLTablePrivilegesCustomizedResultSet", "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    std::string isGrantable = "NO";
    for (int i = 0; i < intermediateRSSize; i++) {
        // TABLE_CAT (catalog name)
        PQsetvalue(res, i, kSQLTablePrivileges_TABLE_CAT_COL_NUM,
                  (char *)intermediateRS[i].table_cat,
                  intermediateRS[i].table_cat_Len);

        // TABLE_SCHEM (schema name)
        PQsetvalue(res, i, kSQLTablePrivileges_TABLE_SCHEM_COL_NUM,
                  (char *)intermediateRS[i].table_schem,
                  intermediateRS[i].table_schem_Len);

        // TABLE_NAME
        PQsetvalue(res, i, kSQLTablePrivileges_TABLE_NAME_COL_NUM,
                  (char *)intermediateRS[i].table_name,
                  intermediateRS[i].table_name_Len);

        // GRANTOR
        PQsetvalue(res, i, kSQLTablePrivileges_GRANTOR_COL_NUM,
                  (char *)intermediateRS[i].grantor,
                  intermediateRS[i].grantor_Len);

        // GRANTEE
        PQsetvalue(res, i, kSQLTablePrivileges_GRANTEE_COL_NUM,
                  (char *)intermediateRS[i].grantee,
                  intermediateRS[i].grantee_Len);

        // PRIVILEGE
        PQsetvalue(res, i, kSQLTablePrivileges_PRIVILEGE_COL_NUM,
                  (char *)intermediateRS[i].privilege,
                  intermediateRS[i].privilege_Len);

        // IS_GRANTABLE
        isGrantable = (intermediateRS[i].admin_option_len != SQL_NULL_DATA &&
               intermediateRS[i].admin_option != 0) ? "YES" : "NO";
        PQsetvalue(res, i, kSQLTablePrivileges_IS_GRANTABLE_COL_NUM,
                  isGrantable.data(),
                  isGrantable.size());
    }

    return SQL_SUCCESS;
}

SQLRETURN libpqCreateSQLColumnPrivilegesCustomizedResultSet(
    RS_STMT_INFO *pStmt,
    short columnNum,
    const std::vector<SHOWGRANTSCOLUMNResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLColumnPrivilegesCustomizedResultSet", "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    std::string isGrantable = "NO";

    for (int i = 0; i < intermediateRSSize; i++) {
        // TABLE_CAT
        PQsetvalue(res, i, kSQLColumnPrivileges_TABLE_CAT_COL_NUM,
                  (char *)intermediateRS[i].table_cat,
                  intermediateRS[i].table_cat_Len);

        // TABLE_SCHEM
        PQsetvalue(res, i, kSQLColumnPrivileges_TABLE_SCHEM_COL_NUM,
                  (char *)intermediateRS[i].table_schem,
                  intermediateRS[i].table_schem_Len);

        // TABLE_NAME
        PQsetvalue(res, i, kSQLColumnPrivileges_TABLE_NAME_COL_NUM,
                  (char *)intermediateRS[i].table_name,
                  intermediateRS[i].table_name_Len);

        // COLUMN_NAME
        PQsetvalue(res, i, kSQLColumnPrivileges_COLUMN_NAME_COL_NUM,
                  (char *)intermediateRS[i].column_name,
                  intermediateRS[i].column_name_Len);

        // GRANTOR
        PQsetvalue(res, i, kSQLColumnPrivileges_GRANTOR_COL_NUM,
                  (char *)intermediateRS[i].grantor,
                  intermediateRS[i].grantor_Len);

        // GRANTEE
        PQsetvalue(res, i, kSQLColumnPrivileges_GRANTEE_COL_NUM,
                  (char *)intermediateRS[i].grantee,
                  intermediateRS[i].grantee_Len);

        // PRIVILEGE
        PQsetvalue(res, i, kSQLColumnPrivileges_PRIVILEGE_COL_NUM,
                  (char *)intermediateRS[i].privilege,
                  intermediateRS[i].privilege_Len);

        // IS_GRANTABLE
        isGrantable = (intermediateRS[i].admin_option_len != SQL_NULL_DATA &&
               intermediateRS[i].admin_option != 0) ? "YES" : "NO";
        PQsetvalue(res, i, kSQLColumnPrivileges_IS_GRANTABLE_COL_NUM,
                  isGrantable.data(),
                  isGrantable.size());
    }

    return SQL_SUCCESS;
}

SQLRETURN libpqCreateSQLProceduresCustomizedResultSet(
    RS_STMT_INFO *pStmt,
    short columnNum,
    const std::vector<SHOWPROCEDURESFUNCTIONSResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProceduresCustomizedResultSet", "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    std::string numStr = {0};

    for (int i = 0; i < intermediateRSSize; i++) {
        // PROCEDURE_CAT
        PQsetvalue(res, i, kSQLProcedures_PROCEDURE_CAT_COL_NUM,
                  (char *)intermediateRS[i].object_cat,
                  intermediateRS[i].object_cat_Len);

        // PROCEDURE_SCHEM
        PQsetvalue(res, i, kSQLProcedures_PROCEDURE_SCHEM_COL_NUM,
                  (char *)intermediateRS[i].object_schem,
                  intermediateRS[i].object_schem_Len);

        // PROCEDURE_NAME
        PQsetvalue(res, i, kSQLProcedures_PROCEDURE_NAME_COL_NUM,
                  (char *)intermediateRS[i].object_name,
                  intermediateRS[i].object_name_Len);

        // NUM_INPUT_PARAMS (reserved for future use)
        PQsetvalue(res, i, kSQLProcedures_NUM_INPUT_PARAMS_COL_NUM, NULL, NULL_LEN);

        // NUM_OUTPUT_PARAMS (reserved for future use)
        PQsetvalue(res, i, kSQLProcedures_NUM_OUTPUT_PARAMS_COL_NUM, NULL, NULL_LEN);

        // NUM_RESULT_SETS (reserved for future use)
        PQsetvalue(res, i, kSQLProcedures_NUM_RESULT_SETS_COL_NUM, NULL, NULL_LEN);

        // REMARKS - Always empty string since Redshift doesn't support
        // comment on procedure or function
        PQsetvalue(res, i, kSQLProcedures_REMARKS_COL_NUM, "", 1);

        // PROCEDURE_TYPE
        numStr = std::to_string(intermediateRS[i].object_type);
        PQsetvalue(res, i, kSQLProcedures_PROCEDURE_TYPE_COL_NUM, numStr.data(),
                  numStr.size());
    }

    return SQL_SUCCESS;
}


// Helper function to create result set for SQLProcedureColumns
SQLRETURN libpqCreateSQLProcedureColumnsCustomizedResultSet(
    RS_STMT_INFO *pStmt, short columnNum,
    const std::vector<SHOWCOLUMNSResult> &intermediateRS) {

    // Result handler validation
    if (pStmt == NULL || pStmt->pResultHead == NULL || pStmt->pResultHead->pgResult == NULL) {
        RS_LOG_ERROR("libpqCreateSQLProcedureColumnsCustomizedResultSet",
            "Invalid statement handler or Result head");
        return SQL_INVALID_HANDLE;
    }

    const int intermediateRSSize = intermediateRS.size();
    PGresult *res = pStmt->pResultHead->pgResult;

    // Set the total column number
    PQsetNumAttributes(res, columnNum);

    // Set the total row number
    pStmt->pResultHead->iNumberOfRowsInMem = intermediateRSSize;

    // Reset the current row number for fetching result
    pStmt->pResultHead->iCurRow = -1;

    // Return early if intermediate result set is empty
    if (intermediateRSSize == 0) {
        return SQL_SUCCESS;
    }

    SQLINTEGER ODBCVer = pStmt->phdbc->phenv->pEnvAttr->iOdbcVersion;
    RS_LOG_DEBUG("RSLIBPQ", "ODBC spec version: %d", ODBCVer);

    int columnSize = 0, bufferLen = 0, charOctetLen = 0;
    short sqlType = 0, sqlDataType = 0, sqlDateSub = 0, precisions = 0,
          decimalDigit = 0, num_pre_radix = 0;

    std::string numStr = {0};

    bool dateTimeCustomizePrecision = false;

    for (int i = 0; i < intermediateRSSize; i++) {
        // Reset the variable
        columnSize = 0;
        bufferLen = 0;
        charOctetLen = 0;
        sqlType = 0;
        sqlDataType = 0;
        sqlDateSub = 0;
        precisions = 0;
        decimalDigit = 0;
        num_pre_radix = 0;
        dateTimeCustomizePrecision = false;
        std::string rsType;
        std::string dataType = char2String(intermediateRS[i].data_type);

        ProcessedTypeInfo processedType = RsMetadataAPIHelper::processDataTypeInfo(
            dataType,
            ODBCVer
        );

        if (processedType.typeInfoResult.found) {
            sqlType = processedType.typeInfoResult.typeInfo.sqlType;
            sqlDataType = processedType.typeInfoResult.typeInfo.sqlDataType;
            sqlDateSub = processedType.typeInfoResult.typeInfo.sqlDateSub;
            rsType = processedType.typeInfoResult.typeInfo.typeName;
        } else {
            rsType = processedType.cleanedTypeName;
        }

        if (processedType.dateTimeInfo.hasCustomPrecision) {
            precisions = processedType.dateTimeInfo.precision;
            dateTimeCustomizePrecision = true;
        }

        columnSize = RsMetadataAPIHelper::getColumnSize(
            rsType, intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);

        bufferLen = RsMetadataAPIHelper::getBufferLen(
            rsType, intermediateRS[i].character_maximum_length,
            intermediateRS[i].numeric_precision);

        decimalDigit = RsMetadataAPIHelper::getDecimalDigit(
            rsType, intermediateRS[i].numeric_scale, precisions,
            dateTimeCustomizePrecision);

        num_pre_radix = RsMetadataAPIHelper::getNumPrecRadix(rsType);

        charOctetLen = RsMetadataAPIHelper::getCharOctetLen(
            rsType, intermediateRS[i].character_maximum_length);

        // Catalog name
        PQsetvalue(res, i, kSQLProcedureColumns_PROCEDURE_CAT_COL_NUM,
                   (char *)intermediateRS[i].database_name,
                   intermediateRS[i].database_name_Len);

        // Schema name
        PQsetvalue(res, i, kSQLProcedureColumns_PROCEDURE_SCHEM_COL_NUM,
                   (char *)intermediateRS[i].schema_name,
                   intermediateRS[i].schema_name_Len);

        // Procedure name
        PQsetvalue(res, i, kSQLProcedureColumns_PROCEDURE_NAME_COL_NUM,
                   (char *)intermediateRS[i].procedure_function_name,
                   intermediateRS[i].procedure_function_name_Len);

        // Column name
        PQsetvalue(res, i, kSQLProcedureColumns_COLUMN_NAME_COL_NUM,
                   (char *)intermediateRS[i].column_name,
                   intermediateRS[i].column_name_Len);

        // Column type
        numStr = std::to_string(
            RsMetadataAPIHelper::getProcedureFunctionColumnType(char2String(intermediateRS[i].parameter_type))
        );
        PQsetvalue(res, i, kSQLProcedureColumns_COLUMN_TYPE_COL_NUM, numStr.data(),
                   numStr.size());

        // SQL type (concise data type)
        numStr = std::to_string(sqlType);
        PQsetvalue(res, i, kSQLProcedureColumns_DATA_TYPE_COL_NUM, numStr.data(),
                   numStr.size());

        // Redshift type name
        PQsetvalue(res, i, kSQLProcedureColumns_TYPE_NAME_COL_NUM, (char *)rsType.c_str(),
                   rsType.size());

        // Column size
        if (columnSize == kNotApplicable) {
            PQsetvalue(res, i, kSQLProcedureColumns_COLUMN_SIZE_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(columnSize);
            PQsetvalue(res, i, kSQLProcedureColumns_COLUMN_SIZE_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Length
        if (bufferLen == kNotApplicable) {
            PQsetvalue(res, i, kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(bufferLen);
            PQsetvalue(res, i, kSQLProcedureColumns_BUFFER_LENGTH_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Decimal digit
        if (decimalDigit == kNotApplicable) {
            // Return NULL where Scale is not applicable
            PQsetvalue(res, i, kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(decimalDigit);
            PQsetvalue(res, i, kSQLProcedureColumns_DECIMAL_DIGITS_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Radix
        if (num_pre_radix == kNotApplicable) {
            // Return NULL where RADIX is not applicable
            PQsetvalue(res, i, kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(num_pre_radix);
            PQsetvalue(res, i, kSQLProcedureColumns_NUM_PREC_RADIX_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Nullable return unknown based on old hardcoded query
        numStr = std::to_string(SQL_NULLABLE_UNKNOWN);
        PQsetvalue(res, i, kSQLProcedureColumns_NULLABLE_COL_NUM, numStr.data(),
                   numStr.size());

        // Remarks
        PQsetvalue(res, i, kSQLProcedureColumns_REMARKS_COL_NUM, (char *)"",0);

        // Column default
        PQsetvalue(res, i, kSQLProcedureColumns_COLUMN_DEF_COL_NUM, NULL, NULL_LEN);

        // SQL Data type (non-concise data type)
        numStr = std::to_string(sqlDataType);
        PQsetvalue(res, i, kSQLProcedureColumns_SQL_DATA_TYPE_COL_NUM, numStr.data(),
                   numStr.size());

        // SQL Date data type subtype code
        if (sqlDateSub == kNotApplicable) {
            PQsetvalue(res, i, kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(sqlDateSub);
            PQsetvalue(res, i, kSQLProcedureColumns_SQL_DATETIME_SUB_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // char octet length
        if (charOctetLen == kNotApplicable) {
            PQsetvalue(res, i, kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM, NULL, NULL_LEN);
        } else {
            numStr = std::to_string(charOctetLen);
            PQsetvalue(res, i, kSQLProcedureColumns_CHAR_OCTET_LENGTH_COL_NUM, numStr.data(),
                       numStr.size());
        }

        // Ordinal position
        numStr = std::to_string(intermediateRS[i].ordinal_position);
        PQsetvalue(res, i, kSQLProcedureColumns_ORDINAL_POSITION_COL_NUM, numStr.data(),
                   numStr.size());

        // Is nullable return empty string based on old hardcoded query
        PQsetvalue(res, i, kSQLProcedureColumns_IS_NULLABLE_COL_NUM, (char *)"",0);
    }

    return SQL_SUCCESS;
}
