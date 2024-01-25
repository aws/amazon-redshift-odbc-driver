/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

/*-------
 * Setup functions for configuring a Redshift ODBC Data Source in the
 * ODBC.INI portion of the registry.
 *
 *	Uses a Property Sheet control for the DSN dialog.
 *	NOTE: The usual Property Sheet context-sensitive Help button
 *	has been hijacked to provide a Test Connection button; each
 *	page of the property sheet provides its own Help button.
 */

#include  <windows.h>
#include  <windowsx.h>
#include <commctrl.h>
#include  <string.h>
#include  <stdlib.h>
#include  <stdio.h>
#include <stdarg.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <odbcinst.h>
#include <pshpack1.h>
#include <prsht.h>
#include  "resource.h"
#include "version.h"
#include <openssl/evp.h>

#define ODBC_INI "ODBC.INI"

 // Copied from rsodbc.h
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
#define RS_LOGIN_TO_RP            "loginToRp"
#define RS_IDP_TENANT             "idp_tenant"
#define RS_CLIENT_ID              "client_id"
#define RS_CLIENT_SECRET          "client_secret"
#define RS_LOGIN_URL              "login_url"
#define RS_LISTEN_PORT            "listen_port"
#define RS_PARTNER_SPID           "partner_spid"
#define RS_APP_ID                 "app_id"
#define RS_APP_NAME               "app_name"
#define RS_WEB_IDENTITY_TOKEN     "web_identity_token"
#define RS_PROVIDER_NAME          "provider_name"
#define RS_ROLE_ARN               "role_arn"
#define RS_DURATION               "duration"
#define RS_ROLE_SESSION_NAME      "role_session_name"
#define RS_IAM_CA_PATH            "CaPath"
#define RS_IAM_CA_FILE            "CaFile"
#define RS_SSL_MODE               "SSLMode"
#define RS_IAM_STS_ENDPOINT_URL   "StsEndpointUrl"
#define RS_IAM_AUTH_PROFILE       "AuthProfile"
#define RS_SCOPE                  "scope"
#define RS_TOKEN                  "token"	
#define RS_TOKEN_TYPE             "token_type"
#define RS_IDENTITY_NAMESPACE     "identity_namespace"
#define RS_SERVERLESS				"Serverless"
#define RS_WORKGROUP				"Workgroup"


 // Connection options value
#define RS_AUTH_TYPE_STATIC   "Static"
#define RS_AUTH_TYPE_PROFILE  "Profile"
#define RS_AUTH_TYPE_PLUGIN   "Plugin"

 /* Predefined external plug-in */
#define IAM_PLUGIN_ADFS             "ADFS"
#define IAM_PLUGIN_AZUREAD          "AzureAD"
#define IAM_PLUGIN_BROWSER_AZURE    "BrowserAzureAD"
#define IAM_PLUGIN_BROWSER_SAML     "BrowserSAML"
#define IAM_PLUGIN_PING             "Ping"
#define IAM_PLUGIN_OKTA             "Okta"
#define IAM_PLUGIN_JWT              "JWT"    // used for federated native IdP auth
#define IAM_PLUGIN_BROWSER_AZURE_OAUTH2    "BrowserAzureADOAuth2"
#define PLUGIN_JWT_IAM_AUTH         "JwtIamAuthPlugin"    // used for federated IAM auth
#define PLUGIN_IDP_TOKEN_AUTH              "IdpTokenAuthPlugin"


#define MAX_JWT					(16 * 1024)

#define RS_DATABASE_METADATA_CURRENT_DB_ONLY    "DatabaseMetadataCurrentDbOnly"
#define RS_READ_ONLY							"ReadOnly"

#define RS_TCP_PROXY_HOST       "ProxyHost"
#define RS_TCP_PROXY_PORT       "ProxyPort"
#define RS_TCP_PROXY_USER_NAME  "ProxyUid"
#define RS_TCP_PROXY_PASSWORD   "ProxyPwd"


#define RS_HTTPS_PROXY_HOST       "https_proxy_host"
#define RS_HTTPS_PROXY_PORT       "https_proxy_port"
#define RS_HTTPS_PROXY_USER_NAME  "https_proxy_username"
#define RS_HTTPS_PROXY_PASSWORD   "https_proxy_password"
#define RS_IDP_USE_HTTPS_PROXY    "idp_use_https_proxy"

#define RS_LOG_LEVEL_OPTION_NAME "LogLevel"
#define RS_LOG_PATH_OPTION_NAME  "LogPath"

#define LOG_LEVEL_OFF 0
#define LOG_LEVEL_FATAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_TRACE 6

#define DFLT_ASVR ""
//#define DFLT_AUT "1"
#define DFLT_CR "0"
#define DFLT_CRC "0"
#define DFLT_CRD "3"
#define DFLT_DB ""
#define DFLT_DSN ""
//#define DFLT_EDP "1"
#define DFLT_EM "1"
//#define DFLT_ECMD "0"
#define DFLT_FG "0"
#define DFLT_FM "0"
#define DFLT_FP "0"
#define DFLT_FRC "1"
#define DFLT_FTSWTZAT "0"
#define DFLT_FTWFSAT "0"
#define DFLT_HOST ""
//#define DFLT_HNIC ""
#define DFLT_IS ""
#define DFLT_KP ""
#define DFLT_KS ""
#define DFLT_KSP ""
#define DFLT_LBT "0"
#define DFLT_LB "0"
#define DFLT_LT "0"
#define DFLT_UID ""
#define DFLT_MXPS "100"
#define DFLT_MNPS "0"
#define DFLT_PWD ""
#define DFLT_POOL "0"
#define DFLT_PORT "5439"
#define DFLT_QT "0"
#define DFLT_RCCE "0"
#define DFLT_TEB "1"
#define DFLT_TS ""
#define DFLT_TSP ""
//#define DFLT_VSC "1"
#define DFLT_XDT "-10"
#define DFLT_CSC_ENABLE "0"
#define DFLT_CSC_THRESHOLD "1"
#define DFLT_CSC_PATH ""
#define DFLT_CSC_MAX_FILE_SIZE "4096"
#define DFLT_MICC_ENABLE "1" // MultiInsertCmdConvertEnable
//#define DFLT_KSN ""  // Kerberos Service Name
//#define DFLT_KSA "SSPI"  // Kerberos API
#define DFLT_SC_ROWS "100"
#define DFLT_SSL_MODE "verify-ca"

// IDP default values
#define DFLT_IAM "0"
#define DFLT_AUTH_TYPE ""
#define DFLT_CLUSTER_ID  ""
#define DFLT_REGION ""
#define DFLT_DB_USER ""
#define DFLT_DB_GROUPS ""
#define DFLT_DB_GROUPS_FILTER  ""
#define DFLT_AUTO_CREATE "0"
#define DFLT_SERVERLESS "0"
#define DFLT_WORKGROUP ""
#define DFLT_FORCE_LOWER_CASE "0"
#define DFLT_IDP_RESPONSE_TIMEOUT ""
#define DFLT_IAM_DURATION ""
#define DFLT_ACCESS_KEY_ID ""
#define DFLT_SECRET_ACCESS_KEY ""
#define DFLT_SESSION_TOKEN ""
#define DFLT_PROFILE ""
#define DFLT_INSTANCE_PROFILE "0"
#define DFLT_PLUGIN_NAME  ""
#define DFLT_PREFERRED_ROLE  ""
#define DFLT_IDP_HOST ""
#define DFLT_IDP_PORT ""
#define DFLT_SSL_INSECURE  "0"
#define DFLT_LOGIN_TO_RP  ""
#define DFLT_IDP_TENANT ""
#define DFLT_CLIENT_ID  ""
#define DFLT_CLIENT_SECRET ""
#define DFLT_LOGIN_URL ""
#define DFLT_LISTEN_PORT ""
#define DFLT_PARTNER_SPID ""
#define DFLT_APP_ID ""
#define DFLT_APP_NAME ""
#define DFLT_WEB_IDENTITY_TOKEN ""
#define DFLT_PROVIDER_NAME ""
#define DFLT_ROLE_ARN ""
#define DFLT_DURATION ""
#define DFLT_ROLE_SESSION_NAME ""
#define DFLT_ENDPOINT_URL ""
#define DFLT_STS_ENDPOINT_URL ""
#define DFLT_AUTH_PROFILE ""
#define DFLT_SCOPE ""
#define DFLT_TOKEN ""
#define DFLT_TOKEN_TYPE ""
#define DFLT_IDENTITY_NAMESPACE ""

#define DFLT_DATABASE_METADATA_CURRENT_DB_ONLY "1"
#define DFLT_READ_ONLY "0"

#define DFLT_PROXY_HOST ""
#define DFLT_PROXY_PORT ""
#define DFLT_PROXY_UID ""
#define DFLT_PROXY_PWD ""
#define DFLT_HTTPS_PROXY_HOST ""
#define DFLT_HTTPS_PROXY_PORT ""
#define DFLT_HTTPS_PROXY_UID ""
#define DFLT_HTTPS_PROXY_PWD ""
#define DFLT_IDP_USE_HTTPS_PROXY "0"

#define DFLT_LOG_LEVEL "0"
#define DFLT_LOG_PATH  ""


#define MIN(x,y)	  ((x) < (y) ? (x) : (y))

#define MAXKEYLEN		(32+1)	/* Max keyword length */
#define MAXVALLEN		(1023+1) /* Max value length */
#define MAXDSNAME		(32+1)	/* Max data source name length */

/* 8 -> 4 for CSC + 1 for MICC + 1 for KSN + 1 for KSA  + 1 for SCR */
/* 46 IAM/IDP */ 
/* 2 Advanced */ 
/* 9 Proxy */
/* 1 Serverless*/
/* 1 Workgroup*/

#define DD_DSN_ATTR_COUNT 95

#define ODBC_GLB_ATTR_COUNT (2 + 1) // LogLevel, LogPath

#define rs_DSN_GET_CHECKBOX(hdlg, ctxt, item, attr) \
	rs_dsn_set_attr(ctxt, attr, IsDlgButtonChecked(hdlg, item) ? "1" : "0");


typedef struct {
	char attr_code[MAXKEYLEN];
	char attr_val[MAXVALLEN];
	char *large_attr_val;
} rs_dsn_attr_t;

typedef struct {
	char attr_code[MAXKEYLEN];
	char attr_val[MAXVALLEN];
} rs_odbc_glb_attr_t;

typedef struct {
	HWND		hwndParent;				/* Parent window handle */
	char		driver_desc[MAXVALLEN];		/* Driver description */
	rs_dsn_attr_t	attrs[DD_DSN_ATTR_COUNT];	/* map of attributes-to-value pairs */
	rs_odbc_glb_attr_t	odbc_glb_attrs[ODBC_GLB_ATTR_COUNT];	/* map of attributes-to-value pairs */

	char		dsn_name[MAXDSNAME];	/* Original data source name */
	BOOL		is_new_dsn_name;		/* New data source flag */
	BOOL		is_default_dsn;			/* Default data source flag */
	char		password[MAXDSNAME];	/* password for connect test */
	BOOL		password_cancelled;		/* set true if CANCEL on password dialog */
/*
 *	PropSheet doesn't init a tab until its actually opened, so
 *	if we try to test connection before opening all tabs, we'd force
 *	the settings for uninit'd tabs to be NULL-ish
 *	SO: keep a flag for each tab to indicate its been init'd, and
 *	on test_connect, only read dialog values for init'd tabs
 */
	BOOL		connect_inited;			/* TRUE => Connect tab has been init'd */
	BOOL		advanced_inited;		/* TRUE => Advanced tab has been init'd */
	BOOL		failover_inited;		/* TRUE => Failover tab has been init'd */
	BOOL		pooling_inited;			/* TRUE => Pooling tab has been init'd */
	BOOL		csc_inited;			    /* TRUE => Cursor tab has been init'd */
	BOOL		ssl_inited;			    /* TRUE => SSL tab has been init'd */
	BOOL		proxy_inited;			/* TRUE => Proxy tab has been init'd */
}	rs_dsn_setup_t, *rs_dsn_setup_ptr_t;

typedef struct {
	int control;
	int type;
	char *dsn_entry;
} rs_dialog_controls;

static BOOL rs_dsn_write_attrs(HWND hwndParent, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static BOOL rs_dsn_read_attrs(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static BOOL rs_odbc_glb_write_attrs(HWND hwndParent, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static BOOL rs_odbc_glb_read_attrs(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);

static void rs_dsn_read_connect_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_advanced_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_failover_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_pooling_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_csc_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_ssl_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_proxy_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static BOOL rs_dsn_setup_prop_sheet(HWND hwndOwner, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static BOOL rs_dsn_test_connect(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_open_help_doc(char *anchor);

static void rs_hide_show_authtype_controls(HWND hwndDlg, char *curAuthType);
static void rs_show_controls(HWND hwndDlg, int controlId[]);
static void rs_hide_controls(HWND hwndDlg, int controlId[]);
static void rs_enable_controls(HWND hwndDlg, int controlId[], BOOL bEnable);
static char*get_attr_val(rs_dsn_attr_t *attr, BOOL bEncrypt);
static void set_attr_val(rs_dsn_attr_t *attr, char *val);

static int get_auth_type_for_control(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void set_idp_dlg_items(rs_dialog_controls idp_controls[], HWND hwndDlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void set_dlg_items_based_on_auth_type(int curAuthType, HWND hwndDlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);

static void rs_dsn_read_idp_items(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);
static void rs_dsn_read_auth_type_items(rs_dialog_controls idp_controls[], HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt);

static void
rs_dsn_read_text_entry(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, int item, const char *attr);

static void debugMsg(char *msg1, int msg2);

char *base64Password(const unsigned char *input, int length);
unsigned char *decode64Password(const char *input, int length);

static const rs_dsn_attr_t rs_dsn_attrs[] =
{
{ "AlternateServers", DFLT_ASVR },
// { "ApplicationUsingThreads", DFLT_AUT },
{ "ConnectionReset", DFLT_CR },
{ "ConnectionRetryCount", DFLT_CRC },
{ "ConnectionRetryDelay", DFLT_CRD },
{ "Database", DFLT_DB },
{ "DataSourceName", DFLT_DSN },
{ "Description", "" },
//{ "EnableDescribeParam", DFLT_EDP },
{ "EncryptionMethod", DFLT_EM },
//{ "ExtendedColumnMetaData", DFLT_ECMD },
{ "FailoverGranularity", DFLT_FG },
{ "FailoverMode", DFLT_FM },
{ "FailoverPreconnect", DFLT_FP },
{ "FetchRefCursor", DFLT_FRC },
{ "FetchTSWTZasTimestamp", DFLT_FTSWTZAT },
{ "FetchTWFSasTime", DFLT_FTWFSAT },
{ "HostName", DFLT_HOST },
//{ "HostNameInCertificate", DFLT_HNIC },
{ "InitializationString", DFLT_IS },
{ "KeyPassword", DFLT_KP },
{ "KeyStore", DFLT_KS },
{ "KeyStorePassword", DFLT_KSP },
{ "LoadBalanceTimeout", DFLT_LBT },
{ "LoadBalancing", DFLT_LB },
{ "LoginTimeout", DFLT_LT },
{ "LogonID", DFLT_UID },
{ "Password", DFLT_PWD },
{ "MaxPoolSize", DFLT_MXPS },
{ "MinPoolSize", DFLT_MNPS },
{ "Pooling", DFLT_POOL },
{ "PortNumber", DFLT_PORT },
{ "QueryTimeout", DFLT_QT },
{ "ReportCodepageConversionErrors", DFLT_RCCE },
{ "TransactionErrorBehavior", DFLT_TEB },
{ "TrustStore", DFLT_TS },
{ "TrustStorePassword", DFLT_TSP },
//{ "ValidateServerCertificate", DFLT_VSC },
{ "XMLDescribeType", DFLT_XDT },
{ "CscEnable", DFLT_CSC_ENABLE },
{ "CscThreshold", DFLT_CSC_THRESHOLD },
{ "CscPath", DFLT_CSC_PATH },
{ "CscMaxFileSize", DFLT_CSC_MAX_FILE_SIZE },
{ "MultiInsertCmdConvertEnable", DFLT_MICC_ENABLE },
// { "KerberosServiceName", DFLT_KSN },
// { "KerberosAPI", DFLT_KSA },
{ "StreamingCursorRows", DFLT_SC_ROWS },
{ RS_SSL_MODE, DFLT_SSL_MODE },
{ RS_IAM, DFLT_IAM},
{ RS_AUTH_TYPE, DFLT_AUTH_TYPE},
{ RS_CLUSTER_ID, DFLT_CLUSTER_ID},
{ RS_REGION , DFLT_REGION},
{ RS_DB_USER , DFLT_DB_USER},
{ RS_DB_GROUPS , DFLT_DB_GROUPS},
{ RS_DB_GROUPS_FILTER , DFLT_DB_GROUPS_FILTER},
{ RS_AUTO_CREATE , DFLT_AUTO_CREATE},
{ RS_FORCE_LOWER_CASE , DFLT_FORCE_LOWER_CASE},
{ RS_IDP_RESPONSE_TIMEOUT , DFLT_IDP_RESPONSE_TIMEOUT},
{ RS_IAM_DURATION , DFLT_IAM_DURATION},
{ RS_ACCESS_KEY_ID , DFLT_ACCESS_KEY_ID},
{ RS_SECRET_ACCESS_KEY , DFLT_SECRET_ACCESS_KEY},
{ RS_SESSION_TOKEN , DFLT_SESSION_TOKEN},
{ RS_PROFILE , DFLT_PROFILE},
{ RS_INSTANCE_PROFILE , DFLT_INSTANCE_PROFILE},
{ RS_PLUGIN_NAME, DFLT_PLUGIN_NAME},
{ RS_PREFERRED_ROLE, DFLT_PREFERRED_ROLE},
{ RS_IDP_HOST, DFLT_IDP_HOST},
{ RS_IDP_PORT, DFLT_IDP_PORT},
{ RS_SSL_INSECURE , DFLT_SSL_INSECURE},
{ RS_LOGIN_TO_RP , DFLT_LOGIN_TO_RP},
{ RS_IDP_TENANT , DFLT_IDP_TENANT},
{ RS_CLIENT_ID , DFLT_CLIENT_ID},
{ RS_CLIENT_SECRET , DFLT_CLIENT_SECRET},
{ RS_LOGIN_URL , DFLT_LOGIN_URL},
{ RS_LISTEN_PORT , DFLT_LISTEN_PORT},
{ RS_PARTNER_SPID , DFLT_PARTNER_SPID},
{ RS_APP_ID , DFLT_APP_ID},
{ RS_APP_NAME , DFLT_APP_NAME},
{ RS_WEB_IDENTITY_TOKEN , DFLT_WEB_IDENTITY_TOKEN},
{ RS_PROVIDER_NAME , DFLT_PROVIDER_NAME },
{ RS_ROLE_ARN , DFLT_ROLE_ARN},
{ RS_DURATION , DFLT_DURATION},
{ RS_ROLE_SESSION_NAME,DFLT_ROLE_SESSION_NAME},
{ RS_DATABASE_METADATA_CURRENT_DB_ONLY, DFLT_DATABASE_METADATA_CURRENT_DB_ONLY },
{ RS_READ_ONLY, DFLT_READ_ONLY },
{ RS_TCP_PROXY_HOST, DFLT_PROXY_HOST },
{ RS_TCP_PROXY_PORT, DFLT_PROXY_PORT },
{ RS_TCP_PROXY_USER_NAME, DFLT_PROXY_UID },
{ RS_TCP_PROXY_PASSWORD, DFLT_PROXY_PWD },
{ RS_HTTPS_PROXY_HOST, DFLT_HTTPS_PROXY_HOST },
{ RS_HTTPS_PROXY_PORT, DFLT_HTTPS_PROXY_PORT },
{ RS_HTTPS_PROXY_USER_NAME, DFLT_HTTPS_PROXY_UID },
{ RS_HTTPS_PROXY_PASSWORD, DFLT_HTTPS_PROXY_PWD },
{ RS_IDP_USE_HTTPS_PROXY, DFLT_IDP_USE_HTTPS_PROXY },
{ RS_END_POINT_URL , DFLT_ENDPOINT_URL },
{ RS_IAM_STS_ENDPOINT_URL , DFLT_STS_ENDPOINT_URL },
{ RS_IAM_AUTH_PROFILE , DFLT_AUTH_PROFILE },
{ RS_SCOPE , DFLT_SCOPE },
{ RS_TOKEN , DFLT_TOKEN },
{ RS_TOKEN_TYPE , DFLT_TOKEN_TYPE },
{ RS_IDENTITY_NAMESPACE , DFLT_IDENTITY_NAMESPACE },
{ RS_SERVERLESS, DFLT_SERVERLESS},
{ RS_WORKGROUP, DFLT_WORKGROUP},
{ "", "" }
};

static char*g_setOfKSA[] = {"SSPI","GSS"};

static const rs_odbc_glb_attr_t rs_odbc_glb_attrs[ODBC_GLB_ATTR_COUNT] =
{
	{ RS_LOG_LEVEL_OPTION_NAME , DFLT_LOG_LEVEL },
	{ RS_LOG_PATH_OPTION_NAME , DFLT_LOG_PATH },
	{ "", "" }
};


/*
 *	quick lookup map from code to Registry name
 */
static const rs_dsn_attr_t rs_dsn_code2name[] =
{
{ "ASVR", "AlternateServers" },
// { "AUT", "ApplicationUsingThreads" },
{ "CR", "ConnectionReset" },
{ "CRC", "ConnectionRetryCount" },
{ "CRD", "ConnectionRetryDelay" },
{ "DB", "Database" },
{ "DSN", "DataSourceName" },
{ "DESC", "Description" },
//{ "EDP", "EnableDescribeParam" },
{ "EM", "EncryptionMethod" },
//{ "ECMD", "ExtendedColumnMetaData" },
{ "FG", "FailoverGranularity" },
{ "FM", "FailoverMode" },
{ "FP", "FailoverPreconnect" },
{ "FRC", "FetchRefCursor" },
{ "FTSWTZAT", "FetchTSWTZasTimestamp" },
{ "FTWFSAT", "FetchTWFSasTime" },
{ "HOST", "HostName" },
// { "HNIC", "HostNameInCertificate" },
{ "IS", "InitializationString" },
{ "KP", "KeyPassword" },
{ "KS", "KeyStore" },
{ "KSP", "KeyStorePassword" },
{ "LBT", "LoadBalanceTimeout" },
{ "LB", "LoadBalancing" },
{ "LT", "LoginTimeout" },
{ "UID", "LogonID" },
{ "PWD", "Password" },
{ "MXPS", "MaxPoolSize" },
{ "MNPS", "MinPoolSize" },
{ "POOL", "Pooling" },
{ "PORT", "PortNumber" },
{ "QT", "QueryTimeout" },
{ "RCCE", "ReportCodepageConversionErrors" },
{ "TEB", "TransactionErrorBehavior" },
{ "TS", "TrustStore" },
{ "TSP", "TrustStorePassword" },
// { "VSC", "ValidateServerCertificate" },
{ "XDT", "XMLDescribeType" },
{ "CscEnable", "CscEnable" },
{ "CscThreshold", "CscThreshold" },
{ "CscPath", "CscPath" },
{ "CscMaxFileSize", "CscMaxFileSize" },
{ "MultiInsertCmdConvertEnable", "MultiInsertCmdConvertEnable" },
// { "KSN", "KerberosServiceName" },
// { "KSA", "KerberosAPI" },
{ "SCR", "StreamingCursorRows" },
{ RS_SSL_MODE, RS_SSL_MODE },
{ RS_IAM, RS_IAM },
{ RS_AUTH_TYPE, RS_AUTH_TYPE },
{ RS_CLUSTER_ID, RS_CLUSTER_ID },
{ RS_REGION , RS_REGION },
{ RS_DB_USER , RS_DB_USER },
{ RS_DB_GROUPS , RS_DB_GROUPS },
{ RS_DB_GROUPS_FILTER , RS_DB_GROUPS_FILTER },
{ RS_AUTO_CREATE , RS_AUTO_CREATE },
{ RS_FORCE_LOWER_CASE , RS_FORCE_LOWER_CASE },
{ RS_IDP_RESPONSE_TIMEOUT , RS_IDP_RESPONSE_TIMEOUT },
{ RS_IAM_DURATION , RS_IAM_DURATION },
{ RS_ACCESS_KEY_ID , RS_ACCESS_KEY_ID },
{ RS_SECRET_ACCESS_KEY , RS_SECRET_ACCESS_KEY },
{ RS_SESSION_TOKEN , RS_SESSION_TOKEN },
{ RS_PROFILE , RS_PROFILE },
{ RS_INSTANCE_PROFILE , RS_INSTANCE_PROFILE },
{ RS_PLUGIN_NAME, RS_PLUGIN_NAME },
{ RS_PREFERRED_ROLE, RS_PREFERRED_ROLE },
{ RS_IDP_HOST, RS_IDP_HOST },
{ RS_IDP_PORT, RS_IDP_PORT },
{ RS_SSL_INSECURE , RS_SSL_INSECURE },
{ RS_LOGIN_TO_RP , RS_LOGIN_TO_RP },
{ RS_IDP_TENANT , RS_IDP_TENANT },
{ RS_CLIENT_ID , RS_CLIENT_ID },
{ RS_CLIENT_SECRET , RS_CLIENT_SECRET },
{ RS_LOGIN_URL , RS_LOGIN_URL },
{ RS_LISTEN_PORT , RS_LISTEN_PORT },
{ RS_PARTNER_SPID , RS_PARTNER_SPID },
{ RS_APP_ID , RS_APP_ID },
{ RS_APP_NAME , RS_APP_NAME },
{ RS_WEB_IDENTITY_TOKEN , RS_WEB_IDENTITY_TOKEN },
{ RS_PROVIDER_NAME , RS_PROVIDER_NAME },
{ RS_ROLE_ARN , RS_ROLE_ARN },
{ RS_DURATION , RS_DURATION },
{ RS_ROLE_SESSION_NAME,RS_ROLE_SESSION_NAME },
{ RS_DATABASE_METADATA_CURRENT_DB_ONLY, RS_DATABASE_METADATA_CURRENT_DB_ONLY},
{ RS_READ_ONLY, RS_READ_ONLY },
{ RS_TCP_PROXY_HOST, RS_TCP_PROXY_HOST },
{ RS_TCP_PROXY_PORT, RS_TCP_PROXY_PORT },
{ RS_TCP_PROXY_USER_NAME, RS_TCP_PROXY_USER_NAME },
{ RS_TCP_PROXY_PASSWORD, RS_TCP_PROXY_PASSWORD },
{ RS_HTTPS_PROXY_HOST, RS_HTTPS_PROXY_HOST },
{ RS_HTTPS_PROXY_PORT, RS_HTTPS_PROXY_PORT },
{ RS_HTTPS_PROXY_USER_NAME, RS_HTTPS_PROXY_USER_NAME },
{ RS_HTTPS_PROXY_PASSWORD, RS_HTTPS_PROXY_PASSWORD },
{ RS_IDP_USE_HTTPS_PROXY, RS_IDP_USE_HTTPS_PROXY },
{ RS_END_POINT_URL , RS_END_POINT_URL },
{ RS_IAM_STS_ENDPOINT_URL , RS_IAM_STS_ENDPOINT_URL },
{ RS_IAM_AUTH_PROFILE , RS_IAM_AUTH_PROFILE },
{ RS_SCOPE , RS_SCOPE },
{ RS_TOKEN , RS_TOKEN },
{ RS_TOKEN_TYPE , RS_TOKEN_TYPE },
{ RS_IDENTITY_NAMESPACE , RS_IDENTITY_NAMESPACE },
{ RS_SERVERLESS, RS_SERVERLESS},
{ RS_WORKGROUP, RS_WORKGROUP},
{ "", "" }
};

static char *failover_modes[3] = {
	"Connection",
	"Extended Connection",
	"Select"
};

static int failover_mode_ids[3] = {
	IDC_FM_CONN,
	IDC_FM_EXT_CONN,
	IDC_FM_SELECT
};

static char *failover_granularity[4] = {
	"Non-Atomic",
	"Atomic",
	"Atomic Repositioned",
	"No Integrity Check"
};

static int failover_gran_ids[4] = {
	IDC_FG_NON_ATOMIC,
	IDC_FG_ATOMIC,
	IDC_FG_REPOS,
	IDC_FG_NO_INT_CHECK
};

static int rs_pooling_items[5] = {
	IDC_POOL_RESET,
	IDC_POOL_MAX_EDIT,
	IDC_POOL_MIN_EDIT,
	IDC_POOL_TO_EDIT,
	0
};

#define AUTH_STANDARD	0
#define AUTH_IAM		1
#define AUTH_PROFILE	2
#define AUTH_ADFS		3
#define AUTH_AZURE		4
#define AUTH_BROWSER_AZURE	5
#define AUTH_BROWSER_SAML	6
#define AUTH_JWT			7
#define AUTH_OKTA			8
#define AUTH_PING_FEDERATE	9
#define AUTH_BROWSER_AZURE_OAUTH2	10
#define AUTH_JWT_IAM		11
#define AUTH_IDP_TOKEN      12

static char *szAuthTypes[] = {
	"Standard",
	"AWS IAM Credentials",
	"AWS Profile",
	"Identity Provider: AD FS",
	"Identity Provider: Azure AD",
	"Identity Provider: Browser Azure AD",
	"Identity Provider: Browser SAML",
	"Identity Provider: JWT",
	"Identity Provider: Okta",
	"Identity Provider: PingFederate",
	"Identity Provider: Browser Azure AD OAUTH2",
	"Identity Provider: JWT IAM Auth Plugin",
	"IdP Token Auth Plugin",
	""
};

static char *szLogLevels[] = {
	"OFF",
	"FATAL",
	"ERROR",
	"WARN",
	"INFO",
	"DEBUG",
	"TRACE",
	""
};

#define SSL_MODE_DISABLE "disable"
#define SSL_MODE_REQUIRE "require"
#define SSL_MODE_VERIFY_CA "verify-ca"
#define SSL_MODE_VERIFY_FULL "verify-full"

#define MAX_IDP_CONTROLS 58

// Total controls of all IDP/IAM
static int rs_idp_controls[] =
{
	IDC_CID_STATIC, // Cluster ID
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_SERVERLESS_IDP_TOKEN,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC, // Force Lower Case
	IDC_AKI_STATIC, // Access Key ID
	IDC_AKI,
	IDC_SAK_STATIC, // Secret Access Key
	IDC_SAK,
	IDC_ST_STATIC, // Session Token
	IDC_ST,
	IDC_PN_STATIC, // Profile Name
	IDC_PN,
	IDC_UIP,	  // Use Instance Profile
	IDC_PR_STATIC, // Preferred Role
	IDC_PR,
	IDC_IDP_HOST_STATIC,
	IDC_IDP_HOST,
	IDC_IDP_PORT_STATIC,
	IDC_IDP_PORT,
	IDC_LTRP_STATIC, // Login to Rp
	IDC_LTRP,
	IDC_SSL_INSECURE,
	IDC_DGF_STATIC, // DB Groups Filter
	IDC_DGF,
	IDC_IDP_TENANT_STATIC,
	IDC_IDP_TENANT,
	IDC_AZURE_CI_STATIC, // Azure Client ID
	IDC_AZURE_CI,
	IDC_AZURE_CS_STATIC, // Azure Client Secret
	IDC_AZURE_CS,
	IDC_TO_STATIC, // Timeout
	IDC_TO,
	IDC_LOGIN_URL_STATIC,
	IDC_LOGIN_URL,
	IDC_LP_STATIC, // Listen Port
	IDC_LP,
	IDC_WIT_STATIC, // Web Identity Token
	IDC_WIT,
	IDC_PROVIDER_NAME_STATIC,
	IDC_PROVIDER_NAME,
	IDC_ROLE_STATIC,
	IDC_ROLE,
	IDC_DURATION_STATIC,
	IDC_DURATION,
	IDC_RSN_STATIC, // Role Session Name
	IDC_RSN,
	IDC_APP_ID_STATIC,
	IDC_APP_ID,
	IDC_APP_NAME_STATIC,
	IDC_APP_NAME,
	IDC_SPID_STATIC, // Partner SPID
	IDC_SPID,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	IDC_SCOPE_STATIC,
	IDC_SCOPE,
	IDC_TOKEN_STATIC,
	IDC_TOKEN,
	IDC_TOKEN_TYPE_STATIC,
	IDC_TOKEN_TYPE,
	IDC_IDENTITY_NAMESPACE_STATIC,
	IDC_IDENTITY_NAMESPACE,
	0
};

// Type of controls
#define RS_NONE_CONTROL		 0
#define RS_TEXT_CONTROL		 1
#define RS_DROP_DOWN_CONTROL 2
#define RS_CHECKBOX_CONTROL  3
#define RS_RADIO_CONTROL	 4

static int rs_iam_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_AKI_STATIC,
	IDC_AKI,
	IDC_SAK_STATIC,
	IDC_SAK,
	IDC_ST_STATIC,
	IDC_ST,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

// Controls which has values. i.e. exclude static text
static rs_dialog_controls rs_iam_val_controls[] =
{
	{IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS},
	{IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{IDC_AKI, RS_TEXT_CONTROL, RS_ACCESS_KEY_ID },
	{IDC_SAK, RS_TEXT_CONTROL, RS_SECRET_ACCESS_KEY },
	{IDC_ST,  RS_TEXT_CONTROL, RS_SESSION_TOKEN },
	{IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{0,  RS_NONE_CONTROL,"" }
};


static int rs_profile_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PN_STATIC,
	IDC_PN,
	IDC_UIP,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_profile_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PN, RS_TEXT_CONTROL, RS_PROFILE},
	{ IDC_UIP, RS_CHECKBOX_CONTROL, RS_INSTANCE_PROFILE},
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_uid_pwd_controls[] =
{
	IDC_USER_STATIC,
	IDC_USER,
	IDC_PWD_STATIC,
	IDC_PWD,
	0
};

static int rs_adfs_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PR_STATIC,
	IDC_PR,
	IDC_IDP_HOST_STATIC,
	IDC_IDP_HOST,
	IDC_IDP_PORT_STATIC,
	IDC_IDP_PORT,
	IDC_LTRP_STATIC,
	IDC_LTRP,
	IDC_SSL_INSECURE,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_adfs_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PR, RS_TEXT_CONTROL, RS_PREFERRED_ROLE },
	{ IDC_IDP_HOST, RS_TEXT_CONTROL, RS_IDP_HOST },
	{ IDC_IDP_PORT, RS_TEXT_CONTROL, RS_IDP_PORT },
	{ IDC_LTRP, RS_TEXT_CONTROL, RS_LOGIN_TO_RP},
	{ IDC_SSL_INSECURE, RS_CHECKBOX_CONTROL, RS_SSL_INSECURE},
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_STS_EPU,  RS_TEXT_CONTROL, RS_IAM_STS_ENDPOINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_azure_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PR_STATIC,
	IDC_PR,
	IDC_DGF_STATIC,
	IDC_DGF,
	IDC_IDP_TENANT_STATIC,
	IDC_IDP_TENANT,
	IDC_AZURE_CI_STATIC,
	IDC_AZURE_CI,
	IDC_AZURE_CS_STATIC,
	IDC_AZURE_CS,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_azure_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PR, RS_TEXT_CONTROL, RS_PREFERRED_ROLE },
	{ IDC_DGF, RS_TEXT_CONTROL, RS_DB_GROUPS_FILTER},
	{ IDC_IDP_TENANT, RS_TEXT_CONTROL, RS_IDP_TENANT},
	{ IDC_AZURE_CI, RS_TEXT_CONTROL, RS_CLIENT_ID},
	{ IDC_AZURE_CS, RS_TEXT_CONTROL, RS_CLIENT_SECRET},
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_STS_EPU,  RS_TEXT_CONTROL, RS_IAM_STS_ENDPOINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_azure_browser_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PR_STATIC,
	IDC_PR,
	IDC_DGF_STATIC,
	IDC_DGF,
	IDC_IDP_TENANT_STATIC,
	IDC_IDP_TENANT,
	IDC_AZURE_CI_STATIC,
	IDC_AZURE_CI,
	IDC_TO_STATIC,
	IDC_TO,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};


static rs_dialog_controls rs_azure_browser_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PR, RS_TEXT_CONTROL, RS_PREFERRED_ROLE },
	{ IDC_DGF, RS_TEXT_CONTROL, RS_DB_GROUPS_FILTER },
	{ IDC_IDP_TENANT, RS_TEXT_CONTROL, RS_IDP_TENANT },
	{ IDC_AZURE_CI, RS_TEXT_CONTROL, RS_CLIENT_ID },
	{ IDC_TO, RS_TEXT_CONTROL, RS_IDP_RESPONSE_TIMEOUT},
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_STS_EPU,  RS_TEXT_CONTROL, RS_IAM_STS_ENDPOINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_azure_browser_oauth2_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_IDP_TENANT_STATIC,
	IDC_IDP_TENANT,
	IDC_AZURE_CI_STATIC,
	IDC_AZURE_CI,
	IDC_TO_STATIC,
	IDC_TO,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_PROVIDER_NAME_STATIC,
	IDC_PROVIDER_NAME,
	IDC_SCOPE_STATIC,
	IDC_SCOPE,
	IDC_SERVERLESS,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_azure_browser_oauth2_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_IDP_TENANT, RS_TEXT_CONTROL, RS_IDP_TENANT },
	{ IDC_AZURE_CI, RS_TEXT_CONTROL, RS_CLIENT_ID },
	{ IDC_TO, RS_TEXT_CONTROL, RS_IDP_RESPONSE_TIMEOUT },
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_PROVIDER_NAME, RS_TEXT_CONTROL, RS_PROVIDER_NAME },
	{ IDC_SCOPE, RS_TEXT_CONTROL, RS_SCOPE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_saml_browser_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PR_STATIC,
	IDC_PR,
	IDC_DGF_STATIC,
	IDC_DGF,
	IDC_LOGIN_URL_STATIC,
	IDC_LOGIN_URL,
	IDC_LP_STATIC,
	IDC_LP,
	IDC_TO_STATIC,
	IDC_TO,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_saml_browser_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PR, RS_TEXT_CONTROL, RS_PREFERRED_ROLE },
	{ IDC_DGF, RS_TEXT_CONTROL, RS_DB_GROUPS_FILTER },
	{ IDC_LOGIN_URL, RS_TEXT_CONTROL, RS_LOGIN_URL },
	{ IDC_LP, RS_TEXT_CONTROL, RS_LISTEN_PORT },
	{ IDC_TO, RS_TEXT_CONTROL, RS_IDP_RESPONSE_TIMEOUT },
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_STS_EPU,  RS_TEXT_CONTROL, RS_IAM_STS_ENDPOINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_jwt_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_WIT_STATIC,
	IDC_WIT,
	IDC_PROVIDER_NAME_STATIC,
	IDC_PROVIDER_NAME,
	IDC_SERVERLESS,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
//	IDC_ROLE_STATIC,
//	IDC_ROLE,
//	IDC_DURATION_STATIC,
//	IDC_DURATION,
//	IDC_RSN_STATIC,
//	IDC_RSN,
	0
};

static rs_dialog_controls rs_jwt_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_WIT, RS_TEXT_CONTROL, RS_WEB_IDENTITY_TOKEN },
	{ IDC_PROVIDER_NAME, RS_TEXT_CONTROL, RS_PROVIDER_NAME },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
//	{ IDC_ROLE, RS_TEXT_CONTROL, RS_ROLE_ARN },
//	{ IDC_DURATION, RS_TEXT_CONTROL,  RS_DURATION },
//	{ IDC_RSN, RS_TEXT_CONTROL, RS_ROLE_SESSION_NAME },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_okta_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PR_STATIC,
	IDC_PR,
	IDC_IDP_HOST_STATIC,
	IDC_IDP_HOST,
	IDC_APP_ID_STATIC,
	IDC_APP_ID,
	IDC_APP_NAME_STATIC,
	IDC_APP_NAME,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_okta_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PR, RS_TEXT_CONTROL, RS_PREFERRED_ROLE },
	{ IDC_IDP_HOST, RS_TEXT_CONTROL, RS_IDP_HOST},
	{ IDC_APP_ID, RS_TEXT_CONTROL, RS_APP_ID },
	{ IDC_APP_NAME, RS_TEXT_CONTROL, RS_APP_NAME },
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_STS_EPU,  RS_TEXT_CONTROL, RS_IAM_STS_ENDPOINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};


static int rs_ping_federated_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBUSER_STATIC,
	IDC_DBUSER,
	IDC_AUTO_CREATE,
	IDC_SERVERLESS,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_FLC,
	IDC_PR_STATIC,
	IDC_PR,
	IDC_IDP_HOST_STATIC,
	IDC_IDP_HOST,
	IDC_IDP_PORT_STATIC,
	IDC_IDP_PORT,
	IDC_SSL_INSECURE,
	IDC_SPID_STATIC,
	IDC_SPID,
	IDC_STS_EPU_STATIC,
	IDC_STS_EPU,
	IDC_EPU_STATIC,
	IDC_EPU,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_ping_federated_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBUSER, RS_TEXT_CONTROL, RS_DB_USER },
	{ IDC_AUTO_CREATE, RS_CHECKBOX_CONTROL, RS_AUTO_CREATE },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_FLC, RS_CHECKBOX_CONTROL, RS_FORCE_LOWER_CASE },
	{ IDC_PR, RS_TEXT_CONTROL, RS_PREFERRED_ROLE },
	{ IDC_IDP_HOST, RS_TEXT_CONTROL, RS_IDP_HOST },
	{ IDC_IDP_PORT, RS_TEXT_CONTROL, RS_IDP_PORT },
	{ IDC_SPID, RS_TEXT_CONTROL, RS_PARTNER_SPID },
	{ IDC_SSL_INSECURE, RS_CHECKBOX_CONTROL, RS_SSL_INSECURE },
	{ IDC_EPU,  RS_TEXT_CONTROL, RS_END_POINT_URL },
	{ IDC_STS_EPU,  RS_TEXT_CONTROL, RS_IAM_STS_ENDPOINT_URL },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_jwt_iam_auth_controls[] =
{
	IDC_CID_STATIC,
	IDC_CID,
	IDC_REGION_STATIC,
	IDC_REGION,
	IDC_DBGROUPS_STATIC,
	IDC_DBGROUPS,
	IDC_WIT_STATIC,
	IDC_WIT,
	IDC_ROLE_STATIC,
	IDC_ROLE,
	IDC_DURATION_STATIC,
	IDC_DURATION,
	IDC_RSN_STATIC,
	IDC_RSN,
	IDC_SERVERLESS,
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_jwt_iam_auth_val_controls[] =
{
	{ IDC_CID, RS_TEXT_CONTROL,RS_CLUSTER_ID },
	{ IDC_REGION, RS_TEXT_CONTROL, RS_REGION },
	{ IDC_DBGROUPS, RS_TEXT_CONTROL, RS_DB_GROUPS },
	{ IDC_WIT, RS_TEXT_CONTROL, RS_WEB_IDENTITY_TOKEN },
	{ IDC_ROLE, RS_TEXT_CONTROL, RS_ROLE_ARN },
	{ IDC_DURATION, RS_TEXT_CONTROL,  RS_DURATION },
	{ IDC_RSN, RS_TEXT_CONTROL, RS_ROLE_SESSION_NAME },
	{IDC_SERVERLESS, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_idp_token_auth_controls[] =
{
	IDC_TOKEN_STATIC,
	IDC_TOKEN,
	IDC_TOKEN_TYPE_STATIC,
	IDC_TOKEN_TYPE,
	IDC_IDENTITY_NAMESPACE_STATIC,
	IDC_IDENTITY_NAMESPACE,
	IDC_SERVERLESS_IDP_TOKEN, 
	IDC_WORKGROUP_STATIC,
	IDC_WORKGROUP,
	0
};

static rs_dialog_controls rs_idp_token_auth_val_controls[] =
{
	{ IDC_TOKEN, RS_TEXT_CONTROL, RS_TOKEN },
	{ IDC_TOKEN_TYPE, RS_TEXT_CONTROL, RS_TOKEN_TYPE },
	{ IDC_IDENTITY_NAMESPACE, RS_TEXT_CONTROL, RS_IDENTITY_NAMESPACE },
	{IDC_SERVERLESS_IDP_TOKEN, RS_CHECKBOX_CONTROL, RS_SERVERLESS },
	{IDC_WORKGROUP,  RS_TEXT_CONTROL, RS_WORKGROUP },
	{ 0,  RS_NONE_CONTROL,"" }
};

static int rs_idc_browser_auth_controls[] =
{
	IDC_IDENTITY_NAMESPACE_STATIC,
	IDC_IDENTITY_NAMESPACE,
	0
};

static rs_dialog_controls rs_idc_browser_auth_val_controls[] =
{
	{ IDC_IDENTITY_NAMESPACE, RS_TEXT_CONTROL, RS_IDENTITY_NAMESPACE },
	{ 0,  RS_NONE_CONTROL,"" }
};


HINSTANCE hModule;

static HWND hTabMainControl = NULL;		/* holds the top level property sheet handle */
static char helpfile_url[1024] = "";
static BOOL process_ds_name = FALSE;
/*
 *	property sheet context
 */
static PROPSHEETPAGE psp[6];
static PROPSHEETHEADER psh;

// #define DSN_DEBUG

static void
rs_dsn_log(int lineno, char *fmt, ...)
{
#ifdef DSN_DEBUG
	FILE *logfile = fopen("C:\\rsodbc_dsn_dlg.log", "a");
	va_list		args;
	va_start(args, fmt);
	if (NULL == logfile)
		return;
	fprintf(logfile, "Line %d: ", lineno);
	vfprintf(logfile, fmt, args);
	va_end(args);
	fprintf(logfile, "\n");
	fflush(logfile);
	fclose(logfile);	// always close, we don't know the context/process we're called from
#endif
}

BOOL		WINAPI
DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	if (DLL_PROCESS_ATTACH == ul_reason_for_call)
	{
		hModule = hInst;	/* Save for dialog boxes */
	}

	return TRUE;

	UNREFERENCED_PARAMETER(lpReserved);
}

static BOOL
rs_dsn_set_attr(rs_dsn_setup_ptr_t rs_setup_ctxt, const char *attr, char *val)
{
	int i;
	int maxVal = (strcmp(attr, RS_WEB_IDENTITY_TOKEN) == 0 || strcmp(attr, RS_TOKEN) == 0)
					? MAX_JWT
					: MAXVALLEN;

	rs_dsn_log(__LINE__, "set_attr: attr %s value %s", attr, val);

	if (strlen(val) >= maxVal)	/* suppress too large values */
		return FALSE;

	for (i = 0; rs_dsn_code2name[i].attr_code[0] &&
		strcmp(attr, rs_dsn_code2name[i].attr_code) &&
		strcmp(attr, rs_dsn_code2name[i].attr_val); i++);
	if (0 == rs_dsn_code2name[i].attr_code[0]) {
		// rs_dsn_log(__LINE__, "missing rs_dsn_code2name NOT set_attr: attr %s value %s", attr, val);
		return FALSE;
	}

	set_attr_val(&(rs_setup_ctxt->attrs[i]), val);


	return TRUE;
}

static BOOL
rs_odbc_glb_set_attr(rs_dsn_setup_ptr_t rs_setup_ctxt, const char *attr, char *val)
{
	int i;
	int maxVal =  MAXVALLEN;

	rs_dsn_log(__LINE__, "set_odbc_glb_attr: attr %s value %s", attr, val);

	if (strlen(val) >= maxVal)	/* suppress too large values */
		return FALSE;

	for (i = 0; rs_odbc_glb_attrs[i].attr_code[0] &&
		strcmp(attr, rs_odbc_glb_attrs[i].attr_code); i++);
	if (0 == rs_odbc_glb_attrs[i].attr_code[0])
		return FALSE;

	strncpy(rs_setup_ctxt->odbc_glb_attrs[i].attr_val, val,sizeof(rs_setup_ctxt->odbc_glb_attrs[i].attr_val));


	return TRUE;
}

static char *
rs_dsn_get_attr(rs_dsn_setup_ptr_t rs_setup_ctxt, const char *attr)
{
	int i;
	// rs_dsn_log(__LINE__, "get_attr: attr %s", attr);
	for (i = 0; rs_dsn_code2name[i].attr_code[0] &&
		strcmp(attr, rs_dsn_code2name[i].attr_code) &&
		strcmp(attr, rs_dsn_code2name[i].attr_val); i++);
	// rs_dsn_log(__LINE__, "get_attr returns %s", rs_setup_ctxt->attrs[i].attr_val ? rs_setup_ctxt->attrs[i].attr_val : "NULL" );
	return (strcmp(attr, RS_WEB_IDENTITY_TOKEN) == 0 || strcmp(attr, RS_TOKEN) == 0)
			? rs_setup_ctxt->attrs[i].large_attr_val
			: rs_setup_ctxt->attrs[i].attr_val;
}

static char *
rs_odbc_glb_get_attr(rs_dsn_setup_ptr_t rs_setup_ctxt, const char *attr)
{
	int i;
	// rs_dsn_log(__LINE__, "get_odbc_glb_attr: attr %s", attr);
	for (i = 0; rs_odbc_glb_attrs[i].attr_code[0] &&
		strcmp(attr, rs_odbc_glb_attrs[i].attr_code); i++);
	// rs_dsn_log(__LINE__, "get_attr returns %s", rs_setup_ctxt->rs_odbc_glb_attrs[i].attr_val ? rs_setup_ctxt->rs_odbc_glb_attrs[i].attr_val : "NULL" );
	return  rs_setup_ctxt->odbc_glb_attrs[i].attr_val;
}

static const char *
rs_dsn_attr_code2name(const char *code)
{
	rs_dsn_log(__LINE__, "rs_dsn_attr_code2name %s",code);
	int i;
	for (i = 0; rs_dsn_code2name[i].attr_code[0] &&
		strcmp(code, rs_dsn_code2name[i].attr_code); i++);
	return rs_dsn_code2name[i].attr_val;
}

static const char *
rs_dsn_attr_name2code(const char *name) \
{
	int i;
	for (i = 0; rs_dsn_code2name[i].attr_val[0] &&
		strcmp(name, rs_dsn_code2name[i].attr_val); i++);
	return rs_dsn_code2name[i].attr_code;
}

/*-------
 *	Parse NUL separated attribute/value pairs and populate the
 *	attribute map
 */
static BOOL
rs_dsn_parse_attributes(
	LPCSTR connect_str, 		// connect string
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)	// our context
{
	const char *attrptr;
	char *valptr;

	rs_dsn_log(__LINE__, "rs_dsn_parse_attributes: connectstr %s", connect_str);
	attrptr = connect_str;
	for (attrptr = connect_str; *attrptr; attrptr = valptr + strlen(valptr) + 1)
	{
/*
 * split item on '='; attribute name on left, value on right
 */
		valptr = strchr(attrptr, '=');
		if (NULL == valptr) {
			// its an error...there probably needs to be an erro dialog, but we're buried
			//	beneath the Driver mgr, so just exit
			return FALSE;
		}
		*valptr++ = 0;	/* temp terminator, we'll restore later */
		// rs_dsn_log(__LINE__, "rs_dsn_parse_attributes: attr %s val %s", attrptr, valptr);
		if (strlen(valptr) >= MAXVALLEN)
			return FALSE;	/* too long ! */
		if (! rs_dsn_set_attr(rs_dsn_setup_ctxt, attrptr, valptr)) {
			// check for Description, PWD
		}
		valptr--;
		*valptr = '=';
	}
	return TRUE;
}

/*
 *	ODBC Setup public function. Called by the ODBC Data Source Administrator
 */
BOOL		CALLBACK
ConfigDSN(
	HWND hwnd,				// Parent window handle
	WORD fRequest,			// Request type (i.e., add, config, or remove)
	LPCSTR driver_desc,		// Driver name
	LPCSTR connect_str)		// data source attribute string
{
	BOOL		result = FALSE;
	GLOBALHANDLE rs_dsn_setup_handle;
	rs_dsn_setup_ptr_t	rs_dsn_setup_ctxt;
	const char *dsn;
	int len;
	HKEY helpKey;
	char original_dsn_name[MAXDSNAME];	/* Original data source name */


//	MessageBox(hwnd, "Debug", NULL, MB_ICONEXCLAMATION | MB_OK);

	rs_dsn_log(__LINE__, "ConfigDSN: request %d driver name %s connstr %s",
		fRequest,
		driver_desc ? driver_desc : "NULL",
		connect_str ? connect_str : "NULL");
/*
 *	make sure its ours
 */
	if (!driver_desc ||
		(_stricmp(driver_desc, "Amazon Redshift ODBC Driver (x64)"))
	   )
		return FALSE;
/*
 *	lookup helpfiles location from registry
 */
	helpfile_url[0] = 0;
	if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\ODBC\\ODBCINST.INI\\Amazon Redshift ODBC Driver (x64)", 0, KEY_ALL_ACCESS, &helpKey )) {
		char helpdir[1024] = "";
		len = 1023;
		if (ERROR_SUCCESS == RegQueryValueEx( helpKey, "HelpRootDirectory", 0, NULL, (LPBYTE)helpdir, &len )) {
			helpdir[len] = 0;
			for (len--; len >= 0; len--)
				if (helpdir[len] == '\\')
					helpdir[len] = '/';
			_snprintf(helpfile_url, sizeof(helpfile_url), "file:///%shelp.htm", helpdir);
			rs_dsn_log(__LINE__, "ConfigDSN: helpfile_url is %s", helpfile_url);
		}
		RegCloseKey(helpKey);
	}
/*
 *	Allocate dns setup context
 *	Note that it must be global, since we don't know how driver manager will trigger
 *	dialog events
 */
	rs_dsn_setup_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(rs_dsn_setup_t));
	if (!rs_dsn_setup_handle)
		return FALSE;
	rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GlobalLock(rs_dsn_setup_handle);
/*
 *	load defaults 1st, then apply any input string
 */
	memcpy(&rs_dsn_setup_ctxt->attrs, rs_dsn_attrs, sizeof(rs_dsn_attrs));
	if (connect_str && !rs_dsn_parse_attributes(connect_str, rs_dsn_setup_ctxt))
	{
		GlobalUnlock(rs_dsn_setup_handle);
		GlobalFree(rs_dsn_setup_handle);
		return FALSE;
	}
    rs_dsn_log(__LINE__, "odbc_glb_attrs <- rs_odbc_glb (defaults) ", helpfile_url);
	memcpy(&rs_dsn_setup_ctxt->odbc_glb_attrs, rs_odbc_glb_attrs, sizeof(rs_odbc_glb_attrs));

	/* Save original data source name */
	dsn = rs_dsn_get_attr(rs_dsn_setup_ctxt, "DSN");
	if (dsn[0] && !SQLValidDSN(dsn))
	{
		GlobalUnlock(rs_dsn_setup_handle);
		GlobalFree(rs_dsn_setup_handle);
		return FALSE;
	}
	strncpy(rs_dsn_setup_ctxt->dsn_name, dsn, MAXDSNAME-1);

	rs_dsn_setup_ctxt->hwndParent = hwnd;
	strncpy(rs_dsn_setup_ctxt->driver_desc, driver_desc, MAXVALLEN-1);
	rs_dsn_setup_ctxt->is_default_dsn = !_stricmp(dsn, "Amazon Redshift ODBC Driver (x64)");

	switch(fRequest) {
		case ODBC_REMOVE_DSN:
		case ODBC_REMOVE_SYS_DSN:
/************
**** Deleting a Data Source

To delete a data source, a data source name must be passed to ConfigDSN in lpszAttributes. ConfigDSN
checks that the data source name is in the Odbc.ini file (or registry). It then calls SQLRemoveDSNFromIni
in the installer DLL to remove the data source.
*************/
			result = dsn[0] ? SQLRemoveDSNFromIni(dsn) : FALSE;
			break;

		case ODBC_ADD_DSN:
		case ODBC_ADD_SYS_DSN:
/**********
*** Adding a Data Source

If a data source name is passed to ConfigDSN in lpszAttributes then
	ConfigDSN checks that the name is valid.
endif
If the data source name matches an existing data source name then
	if hwndParent is null then
		ConfigDSN overwrites the existing name
	else
		ConfigDSN prompts the user to overwrite the existing name.
	endif
endif

If lpszAttributes contains enough information to connect to a data source then
	ConfigDSN can add the data source or display a dialog box with which the user can
	change the connection information.
else
	ConfigDSN must determine the necessary information
	if hwndParent is not null then
		it displays a dialog box to retrieve the information from the user.
	endif
endif

If ConfigDSN displays a dialog box then
	it must display any connection information passed to it in lpszAttributes.
	In particular, if a data source name was passed to it, ConfigDSN displays that name
	but does not allow the user to change it.
	ConfigDSN can supply default values for connection information not passed to it in lpszAttributes.

If ConfigDSN cannot get complete connection information for a data source
	return FALSE.

If ConfigDSN can get complete connection information for a data source then
	it calls SQLWriteDSNToIni in the installer DLL to add the new data source specification
	to the Odbc.ini file (or registry). SQLWriteDSNToIni adds the data source name to the [ODBC Data Sources]
	section, creates the data source specification section, and adds the DRIVER keyword with the
	driver description as its value. ConfigDSN calls SQLWritePrivateProfileString in the installer DLL
	to add any additional keywords and values used by the driver.
**********/
/*
 *	we need to check if dsn name exists; if so, and we've got hwnd, prompt user
 */
			rs_dsn_setup_ctxt->is_new_dsn_name = TRUE;
/*
 * Open dialog if parent window handle
 */
			result =
				hwnd ? rs_dsn_setup_prop_sheet(hwnd, rs_dsn_setup_ctxt) :
				dsn[0] ? rs_dsn_write_attrs(hwnd, rs_dsn_setup_ctxt) :
				FALSE;
			break;

		case ODBC_CONFIG_DSN:
		case ODBC_CONFIG_SYS_DSN:
/******************
*** Modifying a Data Source

To modify a data source, a data source name must be passed to ConfigDSN in lpszAttributes. ConfigDSN
checks that the data source name is in the Odbc.ini file (or registry).

If hwndParent is null then
	ConfigDSN uses the information in lpszAttributes to modify the information in the Odbc.ini file
	(or registry)
else
	ConfigDSN displays a dialog box using the information in lpszAttributes
	for information not in lpszAttributes, it uses information from the system information
	The user can modify the information before ConfigDSN stores it in the system information.
endif

If the data source name was changed
	ConfigDSN first calls SQLRemoveDSNFromIni in the installer DLL to remove the existing
	data source specification from the Odbc.ini file (or registry).
	It then follows the steps in Adding A Data Source section to add the new data source specification.
	If the data source name was not changed
		ConfigDSN calls SQLWritePrivateProfileString in the installer DLL to make any other changes
		ConfigDSN may not delete or change the value of the Driver keyword.
	endif
endif
***************/
			strncpy(original_dsn_name, dsn, sizeof(original_dsn_name));

		/* read the existing values from the registry */
			if (!rs_dsn_read_attrs(rs_dsn_setup_ctxt))
				result = FALSE;
			else
			{
		/* Save passed variables for access from dialog handler */
				rs_dsn_setup_ctxt->is_new_dsn_name = FALSE;
/*
 * Open dialog if parent window handle
 */
				result =
					hwnd ? rs_dsn_setup_prop_sheet(hwnd, rs_dsn_setup_ctxt) :
					dsn[0] ? rs_dsn_write_attrs(hwnd, rs_dsn_setup_ctxt) :
					FALSE;

				if (result)
				{
					if (stricmp(original_dsn_name, rs_dsn_setup_ctxt->dsn_name))
					{
						// If user rename the DSN, DELETE the original one.
						result = original_dsn_name[0] ? SQLRemoveDSNFromIni(original_dsn_name) : FALSE;
					}
				}
			}
	}

	GlobalUnlock(rs_dsn_setup_handle);
	GlobalFree(rs_dsn_setup_handle);
	return result;
}

/*
 *	BEGIN DIALOG HANDLERS
 */
/*-------
 *	Center the dialog
 */
static void
rs_dsn_center_dialog(HWND hdlg)
{
	HWND		hwndFrame;
	RECT		dlg_rect, screen_rect, frame_rect;
	int			cx, cy;

	hwndFrame = GetParent(hdlg);

	GetWindowRect(hdlg, &dlg_rect);
	cx = dlg_rect.right - dlg_rect.left;
	cy = dlg_rect.bottom - dlg_rect.top;

	GetClientRect(hwndFrame, &frame_rect);
	ClientToScreen(hwndFrame, (LPPOINT) (&frame_rect.left));
	ClientToScreen(hwndFrame, (LPPOINT) (&frame_rect.right));
	dlg_rect.top = frame_rect.top + (((frame_rect.bottom - frame_rect.top) - cy) >> 1);
	dlg_rect.left = frame_rect.left + (((frame_rect.right - frame_rect.left) - cx) >> 1);
	dlg_rect.bottom = dlg_rect.top + cy;
	dlg_rect.right = dlg_rect.left + cx;

	GetWindowRect(GetDesktopWindow(), &screen_rect);
	if (dlg_rect.bottom > screen_rect.bottom)
	{
		dlg_rect.bottom = screen_rect.bottom;
		dlg_rect.top = dlg_rect.bottom - cy;
	}
	if (dlg_rect.right > screen_rect.right)
	{
		dlg_rect.right = screen_rect.right;
		dlg_rect.left = dlg_rect.right - cx;
	}

	if (dlg_rect.left < 0)
		dlg_rect.left = 0;
	if (dlg_rect.top < 0)
		dlg_rect.top = 0;

	MoveWindow(hdlg, dlg_rect.left, dlg_rect.top, cx, cy, TRUE);
	return;
}

static BOOL
rs_dsn_bool_attr(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, const char *attr)
{
	char *val = rs_dsn_get_attr(rs_dsn_setup_ctxt, attr);
	return val ? strcmp(val, "0") : FALSE;
}

static BOOL
rs_dsn_bool_attr_with_default(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, const char *attr, BOOL dflt_val)
{
	char *val = rs_dsn_get_attr(rs_dsn_setup_ctxt, attr);
	return val ? strcmp(val, "0") : dflt_val;
}

static int
rs_dsn_int_attr(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, const char *attr)
{
	char *val = rs_dsn_get_attr(rs_dsn_setup_ctxt, attr);
	return val ? atoi(val) : 0;
}

static int
rs_odbc_glb_int_attr(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, const char *attr)
{
	char *val = rs_odbc_glb_get_attr(rs_dsn_setup_ctxt, attr);
	return val ? atoi(val) : 0;
}

static void
rs_open_help_doc(char *anchor)
{
	char url[1024];
	char associated_exe[1024];

	//
	//	yes, we really have to enclose it in dbl quotes, else the anchor gets dropped/ignored
	//
	_snprintf(url, 1024, "\"%s%c%s\"", helpfile_url, '#', anchor);

	// Get the full path of help file without file:/// and then find the associated executable
	associated_exe[0] = '\0';
	FindExecutable(&helpfile_url[strlen("file:///")],NULL,associated_exe);

	if(associated_exe[0] != '\0')
	{
		// If we find associated exe then give url as an argument to that exe
		ShellExecute(NULL, "open", associated_exe, url, NULL, SW_SHOWNORMAL);
	}
	else
	{
		// Remove the quotes which could cause hang on IE7 or IE8.
		// Note that this couldn't navigate to the anchor but it will open the help file.
		_snprintf(url, 1024, "%s%c%s", helpfile_url, '#', anchor);
		ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
	}
}

/*-------
 *	Main property sheet handler
 */
static LRESULT CALLBACK rs_dsn_main_sheet(HWND hDlg, UINT message, LPARAM lParam)
{
	HWND helpbtn;

	switch( message )
	{
		case PSCB_INITIALIZED:
		/*
		 *	convert Help button to Test button
		 */
			helpbtn = GetDlgItem(hDlg, IDHELP);
			SetWindowText(helpbtn, "Test");
			hTabMainControl = hDlg;	// save for later use

			rs_dsn_center_dialog(hDlg);
			rs_dsn_log(__LINE__, "** PSCB_INITIALIZED main sheet ctxt %p**", lParam);
			return TRUE;

	}
	return FALSE;
}

/*-------
 *	Connection sheet handler
 */
static LRESULT CALLBACK rs_dsn_connect_sheet(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt;
	LPPROPSHEETPAGE sheet = (LPPROPSHEETPAGE)lParam;
	char dsn_name[MAXDSNAME];
	HWND okbtn;
	UINT dsnlen;
	LPPSHNOTIFY psnotify = (LPPSHNOTIFY)lParam;
	LRESULT lr;
	int i;
	int curAuthType;

	switch (message)
	{
		case WM_INITDIALOG:
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) sheet->lParam;
			rs_dsn_log(__LINE__, "rs_connect_tab(INIT) ctxt %p", rs_dsn_setup_ctxt);
			process_ds_name = FALSE;
			// need to disable WS_COMMAND while we do this, cuz we get an undefined ctxt ptr
			SetDlgItemText(hwndDlg, IDC_DSNAME, rs_dsn_get_attr(rs_dsn_setup_ctxt, "DSN"));
			SetDlgItemText(hwndDlg, IDC_DESC, rs_dsn_get_attr(rs_dsn_setup_ctxt, "Description"));
			SetDlgItemText(hwndDlg, IDC_SERVER, rs_dsn_get_attr(rs_dsn_setup_ctxt, "HostName"));
			SetDlgItemText(hwndDlg, IDC_USER, rs_dsn_get_attr(rs_dsn_setup_ctxt, "LogonID"));
			SetDlgItemText(hwndDlg, IDC_PWD, rs_dsn_get_attr(rs_dsn_setup_ctxt, "Password"));
			SetDlgItemText(hwndDlg, IDC_DATABASE, rs_dsn_get_attr(rs_dsn_setup_ctxt, "Database"));
			SetDlgItemText(hwndDlg, IDC_PORT_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "PortNumber"));
			SetDlgItemText(hwndDlg, IDC_AUTH_PROFILE, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_IAM_AUTH_PROFILE));
			if (rs_dsn_setup_ctxt->is_default_dsn)
			{
			/* disable DSN name/description if its for a DriverConnect/BrowseConnect */
				EnableWindow(GetDlgItem(hwndDlg, IDC_DSNAME), FALSE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_DSNAMETEXT), FALSE);
			}
			else
				SendDlgItemMessage(hwndDlg, IDC_DSNAME, EM_LIMITTEXT, (WPARAM) (MAXDSNAME - 1), 0L);
			SendDlgItemMessage(hwndDlg, IDC_DESC, EM_LIMITTEXT, (WPARAM) (MAXVALLEN - 1), 0L);

			for(i =0;;i++)
			{ 
				if (szAuthTypes[i][0] == '\0')
					break;
				lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_AUTH_TYPE), szAuthTypes[i]);
			}

			curAuthType = get_auth_type_for_control(rs_dsn_setup_ctxt);
			ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_COMBO_AUTH_TYPE), curAuthType);
			set_dlg_items_based_on_auth_type(curAuthType, hwndDlg, rs_dsn_setup_ctxt);

			rs_hide_show_authtype_controls(hwndDlg, szAuthTypes[curAuthType]);


			okbtn = GetDlgItem(hTabMainControl, IDOK);
			dsnlen = GetDlgItemText(hwndDlg, IDC_DSNAME, dsn_name, sizeof(dsn_name));
			EnableWindow(okbtn, dsnlen);
			process_ds_name = TRUE;
			SetWindowLongPtr(hwndDlg, DWLP_USER, sheet->lParam);

			// Set version
//			SetDlgItemText(hwndDlg, IDC_VERSION_STATIC, "v"REDSHIFT_DRIVER_VERSION"(64 bit)");

			rs_dsn_setup_ctxt->connect_inited = TRUE;
	        break;

		case WM_NOTIFY:
			switch (psnotify->hdr.code) {
				case PSN_KILLACTIVE:
					rs_dsn_log(__LINE__, "rs_connect_tab(PSN_KILLACTIVE)");
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, FALSE);
					break;

				case PSN_RESET:
					rs_dsn_log(__LINE__, "rs_connect_tab(PSN_RESET)");
					break;

				case PSN_APPLY:
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_log(__LINE__, "rs_connect_tab(PSN_APPLY)");
					if (!rs_dsn_setup_ctxt->is_default_dsn) {
/*
 *	need to SQLValidDSN on it before accepting it
 */
						GetDlgItemText(hwndDlg, IDC_DSNAME, dsn_name, sizeof(dsn_name));
						if (!SQLValidDSN(dsn_name)) {
//							debugMsg(dsn_name, 0);
							rs_dsn_log(__LINE__, "process_buttons: bad dsn name is %s", dsn_name);
							MessageBox(hwndDlg, "Invalid DSN name", NULL, MB_ICONEXCLAMATION | MB_OK);
							SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_INVALID);
							return TRUE;
						}
						strncpy(rs_dsn_setup_ctxt->dsn_name, dsn_name,sizeof(rs_dsn_setup_ctxt->dsn_name));
						rs_dsn_read_connect_tab(hwndDlg, rs_dsn_setup_ctxt);
						SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					}
					break;

				case PSN_HELP:
					rs_dsn_log(__LINE__, "rs_connect_tab(PSN_HELP)");
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt);
					break;

				case PSN_QUERYCANCEL:
					rs_dsn_log(__LINE__, "rs_connect_tab(PSN_QUERYCANCEL)");
					break;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
/*
 * Only enable OK button after data source name defined
 */
				case IDC_DSNAME:
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_log(__LINE__, "rs_connect_tab(COMMAND: DSNAME) hwnddlg %d ctxt %p",
							hwndDlg, rs_dsn_setup_ctxt);

					if (!process_ds_name || (EN_CHANGE != GET_WM_COMMAND_CMD(wParam, lParam)))
						return FALSE;

/* Enable/disable the OK button */
					okbtn = GetDlgItem(hTabMainControl, IDOK);
					dsnlen = GetDlgItemText(hwndDlg, IDC_DSNAME, dsn_name, sizeof(dsn_name));
					EnableWindow(okbtn, dsnlen);
					return FALSE;

				case IDC_COMBO_AUTH_TYPE:
				{
					switch (HIWORD(wParam))
					{
						case LBN_SELCHANGE:
						{
							HWND lb = GetDlgItem(hwndDlg, LOWORD(wParam));
							int n = ComboBox_GetCurSel(lb);
//							char szBuf[256];
							char *auth_type = szAuthTypes[n];

							rs_hide_show_authtype_controls(hwndDlg, auth_type);


//							snprintf(szBuf, 256, "%d", n);
//							MessageBox(NULL, auth_type, szBuf, MB_ICONEXCLAMATION | MB_OK);

							break;
						}
					}
					break;
				}

	            case IDC_CONN_HELP:
	            	if (helpfile_url[0])
	            		rs_open_help_doc("connect");
					break;
			}

			break;
	}
	return FALSE;
}

/*-------
 *	Advanced sheet handler
 */
static LRESULT CALLBACK rs_dsn_advanced_sheet(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt;
	LPPROPSHEETPAGE sheet = (LPPROPSHEETPAGE)lParam;
	LPPSHNOTIFY psnotify = (LPPSHNOTIFY)lParam;
	LRESULT lr;
	int i;

	switch (message)
	{
		case WM_INITDIALOG:
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) sheet->lParam;
			rs_dsn_log(__LINE__, "rs_advanced_tab(INIT) ctxt %p", rs_dsn_setup_ctxt);
//			CheckDlgButton(hwndDlg, IDC_DESC_PARAMS, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "EnableDescribeParam"));
//			CheckDlgButton(hwndDlg, IDC_COL_METADATA, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "ExtendedColumnMetaData"));
//			CheckDlgButton(hwndDlg, IDC_THREADS, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "ApplicationUsingThreads"));
			CheckDlgButton(hwndDlg, IDC_FETCH_REFCURSOR, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "FetchRefCursor"));
			CheckDlgButton(hwndDlg, IDC_ROLLBACK_ERR, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "TransactionErrorBehavior"));
			SetDlgItemText(hwndDlg, IDC_RETRY_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "ConnectionRetryCount"));
			SetDlgItemText(hwndDlg, IDC_DELAY_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "ConnectionRetryDelay"));
			SetDlgItemText(hwndDlg, IDC_LOGIN_TO_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "LoginTimeout"));
			SetDlgItemText(hwndDlg, IDC_QRY_TO_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "QueryTimeout"));
			SetDlgItemText(hwndDlg, IDC_INI_SQL_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "InitializationString"));
			CheckDlgButton(hwndDlg, IDC_CURRENT_DB_ONLY, rs_dsn_bool_attr_with_default(rs_dsn_setup_ctxt, RS_DATABASE_METADATA_CURRENT_DB_ONLY, TRUE));
			CheckDlgButton(hwndDlg, IDC_READ_ONLY, rs_dsn_bool_attr_with_default(rs_dsn_setup_ctxt, RS_READ_ONLY, FALSE));
			// Already read from reg into local cache(rs_dsn_setup_ctxt); and now update the page
			// Log Level
			for (i = 0;; i++)
			{
				if (szLogLevels[i][0] == '\0')
					break;
				lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_LOG_LEVEL_COMBO), szLogLevels[i]);
			}

			ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_LOG_LEVEL_COMBO), rs_odbc_glb_int_attr(rs_dsn_setup_ctxt, "LogLevel"));

			SetDlgItemText(hwndDlg, IDC_LOG_PATH_EDIT, rs_odbc_glb_get_attr(rs_dsn_setup_ctxt, RS_LOG_PATH_OPTION_NAME));


			SetWindowLongPtr(hwndDlg, DWLP_USER, sheet->lParam);
			rs_dsn_setup_ctxt->advanced_inited = TRUE;
			break;

		case WM_NOTIFY:
			switch (psnotify->hdr.code) {
				case PSN_KILLACTIVE:
					rs_dsn_log(__LINE__, "rs_advanced_tab(PSN_KILLACTIVE)");
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, FALSE);
					break;

				case PSN_RESET:
					rs_dsn_log(__LINE__, "rs_advanced_tab(PSN_RESET)");
					break;

				case PSN_APPLY:
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_log(__LINE__, "rs_advanced_tab(PSN_APPLY)");
					rs_dsn_read_advanced_tab(hwndDlg, rs_dsn_setup_ctxt);
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					break;

				case PSN_HELP:
					rs_dsn_log(__LINE__, "rs_advanced_tab(PSN_HELP)");
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt);
					break;

				case PSN_QUERYCANCEL:
					rs_dsn_log(__LINE__, "rs_advanced_tab(PSN_QUERYCANCEL)");
					break;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
            	case IDC_ADV_HELP:
            		if (helpfile_url[0])
           				rs_open_help_doc("advanced");
           			break;
			}
	}
	return FALSE;
}

/*-------
 *	Cursor sheet handler
 */
static LRESULT CALLBACK rs_dsn_csc_sheet(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt;
	LPPROPSHEETPAGE sheet = (LPPROPSHEETPAGE)lParam;
	LPPSHNOTIFY psnotify = (LPPSHNOTIFY)lParam;

	switch (message)
	{
		case WM_INITDIALOG:
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) sheet->lParam;
			rs_dsn_log(__LINE__, "rs_cursor_tab(INIT) ctxt %p", rs_dsn_setup_ctxt);
			CheckDlgButton(hwndDlg, IDC_CSC_ENABLE, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "CscEnable"));
			SetDlgItemText(hwndDlg, IDC_EDIT_CSC_THRESHOLD, rs_dsn_get_attr(rs_dsn_setup_ctxt, "CscThreshold"));
			SetDlgItemText(hwndDlg, IDC_EDIT_CSC_PATH, rs_dsn_get_attr(rs_dsn_setup_ctxt, "CscPath"));
			SetDlgItemText(hwndDlg, IDC_EDIT_CSC_MAX_FILE_SIZE, rs_dsn_get_attr(rs_dsn_setup_ctxt, "CscMaxFileSize"));
			SetDlgItemText(hwndDlg, IDC_EDIT_SC_ROWS, rs_dsn_get_attr(rs_dsn_setup_ctxt, "StreamingCursorRows"));

			SetWindowLongPtr(hwndDlg, DWLP_USER, sheet->lParam);
			rs_dsn_setup_ctxt->csc_inited = TRUE;
			break;

		case WM_NOTIFY:
			switch (psnotify->hdr.code) {
				case PSN_KILLACTIVE:
					rs_dsn_log(__LINE__, "rs_cursor_tab(PSN_KILLACTIVE)");
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, FALSE);
					break;

				case PSN_RESET:
					rs_dsn_log(__LINE__, "rs_cursor_tab(PSN_RESET)");
					break;

				case PSN_APPLY:
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_log(__LINE__, "rs_cursor_tab(PSN_APPLY)");
					rs_dsn_read_csc_tab(hwndDlg, rs_dsn_setup_ctxt);
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					break;

				case PSN_HELP:
					rs_dsn_log(__LINE__, "rs_cursor_tab(PSN_HELP)");
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt);
					break;

				case PSN_QUERYCANCEL:
					rs_dsn_log(__LINE__, "rs_cursor_tab(PSN_QUERYCANCEL)");
					break;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
            	case IDC_CSC_HELP:
            		if (helpfile_url[0])
           				rs_open_help_doc("cursor");
           			break;

            	case IDC_CSC_ENABLE:
					if(IsDlgButtonChecked(hwndDlg, IDC_CSC_ENABLE))
					{
						SetDlgItemText(hwndDlg, IDC_EDIT_SC_ROWS, "0");
						EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_SC_ROWS), FALSE);
					}
					else
						EnableWindow(GetDlgItem(hwndDlg, IDC_EDIT_SC_ROWS), TRUE);

           			break;

			}
	}
	return FALSE;
}

/*-------
 *	Cursor sheet handler
 */
static LRESULT CALLBACK rs_dsn_ssl_sheet(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt;
	LPPROPSHEETPAGE sheet = (LPPROPSHEETPAGE)lParam;
	LPPSHNOTIFY psnotify = (LPPSHNOTIFY)lParam;
    LRESULT lr;

	switch (message)
	{
		case WM_INITDIALOG:
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) sheet->lParam;
			rs_dsn_log(__LINE__, "rs_security_tab(INIT) ctxt %p", rs_dsn_setup_ctxt);
            lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_EM),"disable"); 
            lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_EM),"verify-ca"); 
			lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_EM), "verify-full");
			lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_EM), "require");
			ComboBox_SetCurSel(GetDlgItem(hwndDlg, IDC_COMBO_EM), rs_dsn_int_attr(rs_dsn_setup_ctxt, "EncryptionMethod"));
//			CheckDlgButton(hwndDlg, IDC_VSC, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "ValidateServerCertificate"));
//			SetDlgItemText(hwndDlg, IDC_EDIT_HNIC, rs_dsn_get_attr(rs_dsn_setup_ctxt, "HostNameInCertificate"));
			SetDlgItemText(hwndDlg, IDC_EDIT_TS, rs_dsn_get_attr(rs_dsn_setup_ctxt, "TrustStore"));
//			SetDlgItemText(hwndDlg, IDC_EDIT_KSN, rs_dsn_get_attr(rs_dsn_setup_ctxt, "KerberosServiceName"));
//            lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_KSA),g_setOfKSA[0]); 
//            lr = ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_KSA),g_setOfKSA[1]); 
//			ComboBox_SelectString(GetDlgItem(hwndDlg, IDC_COMBO_KSA), 0, rs_dsn_get_attr(rs_dsn_setup_ctxt, "KerberosAPI"));
			SetWindowLongPtr(hwndDlg, DWLP_USER, sheet->lParam);
			rs_dsn_setup_ctxt->ssl_inited = TRUE;
			break;

		case WM_NOTIFY:
			switch (psnotify->hdr.code) {
				case PSN_KILLACTIVE:
					rs_dsn_log(__LINE__, "rs_security_tab(PSN_KILLACTIVE)");
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, FALSE);
					break;

				case PSN_RESET:
					rs_dsn_log(__LINE__, "rs_security_tab(PSN_RESET)");
					break;

				case PSN_APPLY:
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_log(__LINE__, "rs_security_tab(PSN_APPLY)");
					rs_dsn_read_ssl_tab(hwndDlg, rs_dsn_setup_ctxt);
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					break;

				case PSN_HELP:
					rs_dsn_log(__LINE__, "rs_security_tab(PSN_HELP)");
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt);
					break;

				case PSN_QUERYCANCEL:
					rs_dsn_log(__LINE__, "rs_security_tab(PSN_QUERYCANCEL)");
					break;
			}
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
            	case IDC_SSL_HELP:
            		if (helpfile_url[0])
           				rs_open_help_doc("security");
           			break;
			}
	}
	return FALSE;
}

/*-------
*	Proxy sheet handler
*/
static LRESULT CALLBACK rs_dsn_proxy_sheet(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt;
	LPPROPSHEETPAGE sheet = (LPPROPSHEETPAGE)lParam;
	LPPSHNOTIFY psnotify = (LPPSHNOTIFY)lParam;
	LRESULT lr;

	switch (message)
	{
	case WM_INITDIALOG:
	{
		char *pProxyHost;
		char *pHttpsProxyHost;
		rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t)sheet->lParam;
		rs_dsn_log(__LINE__, "rs_proxy_tab(INIT) ctxt %p", rs_dsn_setup_ctxt);

		pProxyHost = rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_TCP_PROXY_HOST);
		if(pProxyHost && *pProxyHost != '\0')
			CheckDlgButton(hwndDlg, IDC_PROXY_REDSHIFT_CONNECTION, TRUE);
		else
			CheckDlgButton(hwndDlg, IDC_PROXY_REDSHIFT_CONNECTION, FALSE);
		SetDlgItemText(hwndDlg, IDC_PROXY_REDSHIFT_SERVER_EDIT, pProxyHost);
		SetDlgItemText(hwndDlg, IDC_PROXY_REDSHIFT_PORT_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_TCP_PROXY_PORT));
		SetDlgItemText(hwndDlg, IDC_PROXY_REDSHIFT_UID_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_TCP_PROXY_USER_NAME));
		SetDlgItemText(hwndDlg, IDC_PROXY_REDSHIFT_PWD_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_TCP_PROXY_PASSWORD));

		pHttpsProxyHost = rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_HTTPS_PROXY_HOST);
		if (pHttpsProxyHost && *pHttpsProxyHost != '\0')
		{
			CheckDlgButton(hwndDlg, IDC_PROXY_STS, TRUE);
			CheckDlgButton(hwndDlg, IDC_PROXY_IDP, TRUE);
		}
		else
		{
			CheckDlgButton(hwndDlg, IDC_PROXY_STS, FALSE);
			CheckDlgButton(hwndDlg, IDC_PROXY_IDP, FALSE);
		}
		SetDlgItemText(hwndDlg, IDC_PROXY_HTTPS_SERVER_EDIT, pHttpsProxyHost);
		SetDlgItemText(hwndDlg, IDC_PROXY_HTTPS_PORT_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_HTTPS_PROXY_PORT));
		SetDlgItemText(hwndDlg, IDC_PROXY_HTTPS_UID_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_HTTPS_PROXY_USER_NAME));
		SetDlgItemText(hwndDlg, IDC_PROXY_HTTPS_PWD_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_HTTPS_PROXY_PASSWORD));

		
		SetWindowLongPtr(hwndDlg, DWLP_USER, sheet->lParam);
		rs_dsn_setup_ctxt->proxy_inited = TRUE;
		break;
	}

	case WM_NOTIFY:
		switch (psnotify->hdr.code) {
		case PSN_KILLACTIVE:
			rs_dsn_log(__LINE__, "rs_proxy_tab(PSN_KILLACTIVE)");
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, FALSE);
			break;

		case PSN_RESET:
			rs_dsn_log(__LINE__, "rs_proxy_tab(PSN_RESET)");
			break;

		case PSN_APPLY:
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t)GetWindowLongPtr(hwndDlg, DWLP_USER);
			rs_dsn_log(__LINE__, "rs_proxy_tab(PSN_APPLY)");
			rs_dsn_read_proxy_tab(hwndDlg, rs_dsn_setup_ctxt);
			SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
			break;

		case PSN_HELP:
			rs_dsn_log(__LINE__, "rs_proxy_tab(PSN_HELP)");
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t)GetWindowLongPtr(hwndDlg, DWLP_USER);
			rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt);
			break;

		case PSN_QUERYCANCEL:
			rs_dsn_log(__LINE__, "rs_proxy_tab(PSN_QUERYCANCEL)");
			break;
		}
		break;

	case WM_COMMAND:
		break;
	}
	return FALSE;
}


/*-------
 *	Failover sheet handler
 */
static LRESULT CALLBACK rs_dsn_failover_sheet(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt;
	LPPROPSHEETPAGE sheet = (LPPROPSHEETPAGE)lParam;
	int mode, granularity;
	BOOL enabled = TRUE;
	LPPSHNOTIFY psnotify = (LPPSHNOTIFY)lParam;

	switch (message)
	{
		case WM_INITDIALOG:
			rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) sheet->lParam;
			rs_dsn_log(__LINE__, "rs_failover_tab(INIT) ctxt %p", rs_dsn_setup_ctxt);
			CheckDlgButton(hwndDlg, IDC_LOAD_BALANCE, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "LoadBalancing"));
			SetDlgItemText(hwndDlg, IDC_ALT_SRV_EDIT, rs_dsn_get_attr(rs_dsn_setup_ctxt, "AlternateServers"));

			mode = strtol(rs_dsn_get_attr(rs_dsn_setup_ctxt, "FailoverMode"), NULL, 0);
			CheckRadioButton(hwndDlg, failover_mode_ids[0], failover_mode_ids[2], failover_mode_ids[mode]);

			granularity = strtol(rs_dsn_get_attr(rs_dsn_setup_ctxt, "FailoverGranularity"), NULL, 0);
			CheckRadioButton(hwndDlg, failover_gran_ids[0], failover_gran_ids[3], failover_gran_ids[granularity]);

			// NOTE: this should only be enabled if certain mode/granularity is set
			CheckDlgButton(hwndDlg, IDC_FAIL_PRECONN, rs_dsn_bool_attr(rs_dsn_setup_ctxt, "FailoverPreconnect"));
			enabled = ((BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_FM_EXT_CONN)) ||
				(BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_FM_SELECT)));
			EnableWindow(GetDlgItem(hwndDlg, IDC_FAIL_PRECONN), enabled);
			SetWindowLongPtr(hwndDlg, DWLP_USER, sheet->lParam);
			rs_dsn_setup_ctxt->failover_inited = TRUE;
    	    break;

		case WM_NOTIFY:
			switch (psnotify->hdr.code) {
				case PSN_KILLACTIVE:
					rs_dsn_log(__LINE__, "rs_connect_tab(PSN_KILLACTIVE)");
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, FALSE);
					break;

				case PSN_RESET:
					rs_dsn_log(__LINE__, "rs_failover_tab(PSN_RESET)");
					break;

				case PSN_APPLY:
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_log(__LINE__, "rs_failover_tab(PSN_APPLY)");
					rs_dsn_read_failover_tab(hwndDlg, rs_dsn_setup_ctxt);
					SetWindowLong(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
					break;

				case PSN_HELP:
					rs_dsn_log(__LINE__, "rs_failover_tab(PSN_HELP)");
					rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hwndDlg, DWLP_USER);
					rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt);
					break;

				case PSN_QUERYCANCEL:
					rs_dsn_log(__LINE__, "rs_failover_tab(PSN_QUERYCANCEL)");
					break;
			}
			break;

	    case WM_COMMAND:
            switch (LOWORD(wParam))
            {
				// only enable preconn when certain modes are set (need to verify)
				case IDC_FM_CONN:
				case IDC_FM_EXT_CONN:
				case IDC_FM_SELECT:
					enabled = ((BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_FM_EXT_CONN)) ||
						(BST_CHECKED == IsDlgButtonChecked(hwndDlg, IDC_FM_SELECT)));
					EnableWindow(GetDlgItem(hwndDlg, IDC_FAIL_PRECONN), enabled);
					break;

	            case IDC_FAIL_HELP:
	            	if (helpfile_url[0])
	            		rs_open_help_doc("failover");
					break;
			}

	}
	return FALSE;
}


/*-------
 *	Property sheet initialization
 */
static BOOL rs_dsn_setup_prop_sheet(HWND hwndOwner, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	int propPageIndex;

	rs_dsn_log(__LINE__, "rs_dsn_setup_prop_sheets()");

	memset(psp, 0, sizeof(psp));
	memset(&psh, 0, sizeof(psh));

	propPageIndex = 0;
    psp[propPageIndex].dwSize = sizeof(PROPSHEETPAGE);
    psp[propPageIndex].dwFlags = PSP_DEFAULT|PSP_USETITLE|PSP_HASHELP;
    psp[propPageIndex].hInstance = hModule;
    psp[propPageIndex].pszTemplate = MAKEINTRESOURCE(DLG_CONNECTION);
    psp[propPageIndex].pfnDlgProc = (DLGPROC)rs_dsn_connect_sheet;
    psp[propPageIndex].pszTitle = "Connection";
    psp[propPageIndex].lParam = (LPARAM)rs_dsn_setup_ctxt;
    psp[propPageIndex].pfnCallback = NULL;

	propPageIndex++; // 1
	psp[propPageIndex].dwSize = sizeof(PROPSHEETPAGE);
	psp[propPageIndex].dwFlags = PSP_USETITLE | PSP_HASHELP;
	psp[propPageIndex].hInstance = hModule;
	psp[propPageIndex].pszTemplate = MAKEINTRESOURCE(DLG_SECURITY);
	psp[propPageIndex].pfnDlgProc = (DLGPROC)rs_dsn_ssl_sheet;
	psp[propPageIndex].pszTitle = "SSL";
	psp[propPageIndex].lParam = (LPARAM)rs_dsn_setup_ctxt;
	psp[propPageIndex].pfnCallback = NULL;

	propPageIndex++; // 2
	psp[propPageIndex].dwSize = sizeof(PROPSHEETPAGE);
	psp[propPageIndex].dwFlags = PSP_USETITLE | PSP_HASHELP;
	psp[propPageIndex].hInstance = hModule;
	psp[propPageIndex].pszTemplate = MAKEINTRESOURCE(DLG_PROXY);
	psp[propPageIndex].pfnDlgProc = (DLGPROC)rs_dsn_proxy_sheet;
	psp[propPageIndex].pszTitle = "Proxy";
	psp[propPageIndex].lParam = (LPARAM)rs_dsn_setup_ctxt;
	psp[propPageIndex].pfnCallback = NULL;

	propPageIndex++; // 3
	psp[propPageIndex].dwSize = sizeof(PROPSHEETPAGE);
	psp[propPageIndex].dwFlags = PSP_USETITLE | PSP_HASHELP;
	psp[propPageIndex].hInstance = hModule;
	psp[propPageIndex].pszTemplate = MAKEINTRESOURCE(DLG_CSC);
	psp[propPageIndex].pfnDlgProc = (DLGPROC)rs_dsn_csc_sheet;
	psp[propPageIndex].pszTitle = "Cursor";
	psp[propPageIndex].lParam = (LPARAM)rs_dsn_setup_ctxt;
	psp[propPageIndex].pfnCallback = NULL;

	propPageIndex++; // 4
	psp[propPageIndex].dwSize = sizeof(PROPSHEETPAGE);
    psp[propPageIndex].dwFlags = PSP_USETITLE|PSP_HASHELP;
    psp[propPageIndex].hInstance = hModule;
    psp[propPageIndex].pszTemplate = MAKEINTRESOURCE(DLG_ADVANCED);
    psp[propPageIndex].pfnDlgProc = (DLGPROC)rs_dsn_advanced_sheet;
    psp[propPageIndex].pszTitle = "Advanced";
    psp[propPageIndex].lParam = (LPARAM)rs_dsn_setup_ctxt;
    psp[propPageIndex].pfnCallback = NULL;

    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_PROPSHEETPAGE| PSH_USECALLBACK|PSH_HASHELP|PSH_NOCONTEXTHELP|PSH_NOAPPLYNOW;
    psh.hwndParent = hwndOwner;
    psh.hInstance = hModule;
    psh.pszCaption = (LPSTR) "Amazon Redshift ODBC Driver Setup""  v"REDSHIFT_DRIVER_VERSION"(64 bit)";
    psh.nPages = propPageIndex + 1; 
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;
    psh.pfnCallback = (PFNPROPSHEETCALLBACK) rs_dsn_main_sheet;
    if (0 <= PropertySheet(&psh)) {
		rs_dsn_log(__LINE__, "** PropertySHeet finished ok **");
		rs_dsn_write_attrs(hwndOwner, rs_dsn_setup_ctxt);
		return TRUE;
	}
	rs_dsn_log(__LINE__, "** PropertySHeet cancelled **");
	return FALSE;
}


static void rs_hide_show_authtype_controls(HWND hwndDlg, char *curAuthType)
{
	// Hide all IDP controls
	rs_hide_controls(hwndDlg, rs_idp_controls);
	rs_enable_controls(hwndDlg, rs_uid_pwd_controls, TRUE);

	if (strcmp(curAuthType,szAuthTypes[AUTH_STANDARD]) == 0) {
		// Standard. 
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_IAM]) == 0) {
		// IAM. 
//		rs_enable_controls(hwndDlg, rs_uid_pwd_controls, FALSE);
		rs_show_controls(hwndDlg, rs_iam_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_PROFILE]) == 0) {
		// Profile. 
		rs_show_controls(hwndDlg, rs_profile_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_ADFS]) == 0) {
		// ADFS. 
		rs_show_controls(hwndDlg, rs_adfs_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_AZURE]) == 0) {
		// Azure. 
		rs_show_controls(hwndDlg, rs_azure_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_BROWSER_AZURE]) == 0) {
		// Azure Browser. 
		rs_show_controls(hwndDlg, rs_azure_browser_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_BROWSER_SAML]) == 0) {
		// SAML Browser. 
		rs_show_controls(hwndDlg, rs_saml_browser_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_JWT]) == 0) {
		// JWT nativeIdP 
		rs_show_controls(hwndDlg, rs_jwt_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_OKTA]) == 0) {
		// Okta. 
		rs_show_controls(hwndDlg, rs_okta_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_PING_FEDERATE]) == 0) {
		// Ping Federated. 
		rs_show_controls(hwndDlg, rs_ping_federated_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_BROWSER_AZURE_OAUTH2]) == 0) {
		// Azure Browser. 
		rs_show_controls(hwndDlg, rs_azure_browser_oauth2_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_JWT_IAM]) == 0) {
		// JWT IAM federated auth 
		rs_show_controls(hwndDlg, rs_jwt_iam_auth_controls);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_IDP_TOKEN]) == 0) {
		// Basic Programmatic plugin 
		rs_show_controls(hwndDlg, rs_idp_token_auth_controls);
	}

}

static void rs_hide_controls(HWND hwndDlg, int controlId[])
{
	int id;
	for (id = 0; ;id++)
	{
		if (controlId[id] == 0)
			break;

		ShowWindow(GetDlgItem(hwndDlg, controlId[id]), SW_HIDE);
	}
}

static void rs_show_controls(HWND hwndDlg, int controlId[])
{
	int id;
	for (id = 0; ; id++)
	{
		if (controlId[id] == 0)
			break;

		ShowWindow(GetDlgItem(hwndDlg, controlId[id]), SW_SHOWNORMAL); // SW_SHOW
	}
}

static void rs_enable_controls(HWND hwndDlg, int controlId[], BOOL bEnable)
{
	int id;
	for (id = 0; ; id++)
	{
		if (controlId[id] == 0)
			break;
		EnableWindow(GetDlgItem(hwndDlg, controlId[id]), bEnable);
	}

}

static char*get_attr_val(rs_dsn_attr_t *attr, BOOL bEncrypt)
{
	if (strcmp(attr->attr_code, RS_WEB_IDENTITY_TOKEN) == 0 || strcmp(attr->attr_code, RS_TOKEN) == 0)
		return (attr->large_attr_val) ? attr->large_attr_val : "";
	else
	{
		if (bEncrypt && strcmp(attr->attr_code, "Password") == 0)
		{
			char *pwd = attr->attr_val;
			char *epwd = base64Password(pwd, strlen(pwd));
			if (epwd)
			{
				return epwd;
			}
			else
				return pwd;
		}
		else
			return attr->attr_val;
	}
}

static void set_attr_val(rs_dsn_attr_t *attr, char *val)
{
	if (strcmp(attr->attr_code, RS_WEB_IDENTITY_TOKEN) == 0 || strcmp(attr->attr_code, RS_TOKEN) == 0)
		attr->large_attr_val = _strdup(val);
	else
		strncpy(attr->attr_val, val,sizeof(attr->attr_val));
}

static void debugMsg(char *msg1, int msg2)
{
	char szBuf[256];

	snprintf(szBuf, sizeof(szBuf), "%d", msg2);
	MessageBox(NULL, msg1, szBuf, MB_ICONEXCLAMATION | MB_OK);

}

static int get_auth_type_for_control(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	int rc = AUTH_STANDARD;
	char *auth_type = rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE);
	if (auth_type != NULL && *auth_type != '\0')
	{
		if (strcmp(auth_type, RS_AUTH_TYPE_STATIC) == 0)
			rc = AUTH_IAM;
		else
		if (strcmp(auth_type, RS_AUTH_TYPE_PROFILE) == 0)
			rc = AUTH_PROFILE;
		else
		if (strcmp(auth_type, RS_AUTH_TYPE_PLUGIN) == 0)
		{
			char *plugin = rs_dsn_get_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME);
			if (plugin != NULL && *plugin != '\0')
			{
				if (strcmp(plugin, IAM_PLUGIN_ADFS) == 0)
					rc = AUTH_ADFS;
				else
				if (strcmp(plugin, IAM_PLUGIN_AZUREAD) == 0)
					rc = AUTH_AZURE;
				else
				if (strcmp(plugin, IAM_PLUGIN_BROWSER_AZURE) == 0)
					rc = AUTH_BROWSER_AZURE;
				else
				if (strcmp(plugin, IAM_PLUGIN_BROWSER_SAML) == 0)
					rc = AUTH_BROWSER_SAML;
				else
				if (strcmp(plugin, IAM_PLUGIN_JWT) == 0)
					rc = AUTH_JWT;
				else
				if (strcmp(plugin, IAM_PLUGIN_OKTA) == 0)
					rc = AUTH_OKTA;
				else
				if (strcmp(plugin, IAM_PLUGIN_PING) == 0)
					rc = AUTH_PING_FEDERATE;
				else
				if (strcmp(plugin, IAM_PLUGIN_BROWSER_AZURE_OAUTH2) == 0)
					rc = AUTH_BROWSER_AZURE_OAUTH2;
				else
				if (strcmp(plugin, PLUGIN_JWT_IAM_AUTH) == 0)
					rc = AUTH_JWT_IAM;
				else
				if (strcmp(plugin, PLUGIN_IDP_TOKEN_AUTH) == 0)
					rc = AUTH_IDP_TOKEN;
			}
		}
	}

	return rc;
}

static void set_idp_dlg_items(rs_dialog_controls idp_controls[], HWND hwndDlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	int id;
	for (id = 0; ; id++)
	{
		if (idp_controls[id].control == 0)
			break;

		if (idp_controls[id].type == RS_TEXT_CONTROL)
		{
			SetDlgItemText(hwndDlg, idp_controls[id].control, rs_dsn_get_attr(rs_dsn_setup_ctxt, idp_controls[id].dsn_entry));
		}
		else
		if (idp_controls[id].type == RS_CHECKBOX_CONTROL)
		{
			CheckDlgButton(hwndDlg, idp_controls[id].control, rs_dsn_bool_attr(rs_dsn_setup_ctxt, idp_controls[id].dsn_entry));
		}
	} // Loop
}

static void set_dlg_items_based_on_auth_type(int curAuthType, HWND hwndDlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	if (curAuthType == AUTH_STANDARD) return;

	if (curAuthType == AUTH_IAM)
	{
		set_idp_dlg_items(rs_iam_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_PROFILE)
	{
		set_idp_dlg_items(rs_profile_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_ADFS)
	{
		set_idp_dlg_items(rs_adfs_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_AZURE)
	{
		set_idp_dlg_items(rs_azure_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_BROWSER_AZURE)
	{
		set_idp_dlg_items(rs_azure_browser_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_BROWSER_SAML)
	{
		set_idp_dlg_items(rs_saml_browser_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_JWT)
	{
		set_idp_dlg_items(rs_jwt_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_OKTA)
	{
		set_idp_dlg_items(rs_okta_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_PING_FEDERATE)
	{
		set_idp_dlg_items(rs_ping_federated_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_BROWSER_AZURE_OAUTH2)
	{
		set_idp_dlg_items(rs_azure_browser_oauth2_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_JWT_IAM)
	{
		set_idp_dlg_items(rs_jwt_iam_auth_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
	else
	if (curAuthType == AUTH_IDP_TOKEN)
	{
		set_idp_dlg_items(rs_idp_token_auth_val_controls, hwndDlg, rs_dsn_setup_ctxt);
	}
}

static void rs_dsn_read_idp_items(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	HWND lb = GetDlgItem(hdlg, IDC_COMBO_AUTH_TYPE);
	int n = ComboBox_GetCurSel(lb);
	char *curAuthType = szAuthTypes[n];

	if (strcmp(curAuthType, szAuthTypes[AUTH_STANDARD]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, "");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "0");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, "");
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_IAM]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_STATIC);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, "");
		rs_dsn_read_auth_type_items(rs_iam_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_PROFILE]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PROFILE);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, "");
		rs_dsn_read_auth_type_items(rs_profile_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_ADFS]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_ADFS);
		rs_dsn_read_auth_type_items(rs_adfs_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_AZURE]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_AZUREAD);
		rs_dsn_read_auth_type_items(rs_azure_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_BROWSER_AZURE]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_BROWSER_AZURE);
		rs_dsn_read_auth_type_items(rs_azure_browser_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_BROWSER_SAML]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_BROWSER_SAML);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_read_auth_type_items(rs_saml_browser_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_JWT]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_JWT);
		rs_dsn_read_auth_type_items(rs_jwt_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_BROWSER_AZURE_OAUTH2]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_BROWSER_AZURE_OAUTH2);
		rs_dsn_read_auth_type_items(rs_azure_browser_oauth2_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_OKTA]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_OKTA);
		rs_dsn_read_auth_type_items(rs_okta_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_PING_FEDERATE]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, IAM_PLUGIN_PING);
		rs_dsn_read_auth_type_items(rs_ping_federated_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_JWT_IAM]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_IAM, "1");
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, PLUGIN_JWT_IAM_AUTH);
		rs_dsn_read_auth_type_items(rs_jwt_iam_auth_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
	else
	if (strcmp(curAuthType, szAuthTypes[AUTH_IDP_TOKEN]) == 0) {
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_AUTH_TYPE, RS_AUTH_TYPE_PLUGIN);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_PLUGIN_NAME, PLUGIN_IDP_TOKEN_AUTH);
		rs_dsn_read_auth_type_items(rs_idp_token_auth_val_controls, hdlg, rs_dsn_setup_ctxt);
	}
}

static void rs_dsn_read_auth_type_items(rs_dialog_controls idp_controls[], HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	int id;

	for (id = 0; ; id++)
	{
		if (idp_controls[id].control == 0)
			break;

		if (idp_controls[id].type == RS_TEXT_CONTROL)
		{
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, idp_controls[id].control, idp_controls[id].dsn_entry);
		}
		else
		if (idp_controls[id].type == RS_CHECKBOX_CONTROL)
		{
			rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, idp_controls[id].control, idp_controls[id].dsn_entry);
		}
	} // Loop
}


static BOOL
rs_dsn_init_password(
	HWND hdlg,
	LPARAM lParam)	// Message parameter
{
	SetWindowLongPtr(hdlg, DWLP_USER, lParam);
	rs_dsn_center_dialog(hdlg);

	SendDlgItemMessage(hdlg, IDC_PASSWORD_EDIT, EM_LIMITTEXT, (WPARAM) (MAXDSNAME - 1), 0L);
	return TRUE;
}

static BOOL
rs_dsn_process_password(
	HWND hdlg,
	WPARAM wParam,	// Message parameter
	LPARAM lParam)	// Message parameter
{
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt = (rs_dsn_setup_ptr_t) GetWindowLongPtr(hdlg, DWLP_USER);

	switch (GET_WM_COMMAND_ID(wParam, lParam))
	{
		case IDOK:
			GetDlgItemText(hdlg, IDC_PASSWORD_EDIT, rs_dsn_setup_ctxt->password,
				sizeof(rs_dsn_setup_ctxt->password));
			EndDialog(hdlg, wParam);
			rs_dsn_setup_ctxt->password_cancelled = FALSE;
			rs_dsn_log(__LINE__, "process_password: got the password string %s", rs_dsn_setup_ctxt->password);
			return TRUE;

		case IDCANCEL: /* close dialog box */
			EndDialog(hdlg, wParam);
			rs_dsn_setup_ctxt->password_cancelled = TRUE;
			return TRUE;

		default:
			return FALSE;
	}
}

/*-------
 *	Password dialog callback
 */
LRESULT	CALLBACK
rs_dsn_password_dialog(
	HWND hdlg,		// Dialog window handle
	UINT wMsg,		// Message
	WPARAM wParam,	// Message parameter
	LPARAM lParam)	// Message parameter
{
	return
		(WM_INITDIALOG == wMsg) ? rs_dsn_init_password(hdlg, lParam) :
		(WM_COMMAND    == wMsg) ? rs_dsn_process_password(hdlg, wParam, lParam)
								: FALSE;
}

static char *
rs_dsn_make_connect_str(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, char *connectstr,int buf_len)
{
	rs_dsn_attr_t *attrs = rs_dsn_setup_ctxt->attrs;
	char *ptr;
	int i;

	rs_dsn_log(__LINE__, "make_connect_str");
//	snprintf(connectstr, buf_len, "DRIVER=%s;PWD=%s", rs_dsn_setup_ctxt->driver_desc, rs_dsn_setup_ctxt->password);
	snprintf(connectstr,buf_len, "DRIVER=%s", rs_dsn_setup_ctxt->driver_desc);
	ptr = connectstr + strlen(connectstr);
	for (i = 0; attrs[i].attr_code[0]; i++) {
		rs_dsn_log(__LINE__, "%d: make_connect_str trying %s", i, attrs[i].attr_code);
/*
 *	need special handling for Description (possibly escaping)
 *	and a prompt dialog for password
 *	also need to verify we haven't overrun our output buffer
 */
 		if (!strcmp((const char *)attrs[i].attr_code, "DataSourceName") ||
 			!strcmp((const char *)attrs[i].attr_code, "Description") ||
/*
 *	seems that an issue w/ setting altserver in connection string
 *	for now we'll just skip over setting the alt server for Test Connection
 */
//#define BUGNNNN
#ifdef BUGNNNN
 			!strcmp((const char *)attrs[i].attr_code, "AlternateServers") ||
#endif
			((!attrs[i].attr_val[0])
				&& (attrs[i].large_attr_val == NULL)))
 			continue;
		*ptr++ = ';';
#ifndef BUGNNNN
		if (strcmp("ASVR", rs_dsn_attr_name2code(attrs[i].attr_code)))
			strncpy(ptr, rs_dsn_attr_name2code(attrs[i].attr_code), buf_len);
		else
			strncpy(ptr, "AlternateServers", buf_len);
#else
		strncpy(ptr, rs_dsn_attr_name2code(attrs[i].attr_code), buf_len);
#endif
		ptr += strlen(ptr);
		*ptr++ = '=';
		strncpy(ptr, get_attr_val(&attrs[i], FALSE), buf_len);
		buf_len -= strlen(ptr);
		ptr += strlen(ptr);
	}
	rs_dsn_log(__LINE__, "make_connect_str: returns %s", connectstr);
	return connectstr;
}

static BOOL
rs_dsn_test_connect(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
#define MAX_CONNECTION_STRING_SIZE (1024 * 8)
	HENV henv;
	HDBC hdbc;
	SQLRETURN rc;
	char resultmsg[256];
	char connectstr[MAX_CONNECTION_STRING_SIZE];
	char outconnstr[MAX_CONNECTION_STRING_SIZE];
	SQLSMALLINT outlen;
	int dlg_flag = MB_OK;

	if (NULL == rs_dsn_setup_ctxt)
		return FALSE;

	rs_dsn_log(__LINE__, "test_connect");

	rs_dsn_log(__LINE__, "test_connect: alloc'd handles");
	rs_dsn_read_connect_tab(PropSheet_IndexToHwnd(hTabMainControl, 0), rs_dsn_setup_ctxt);
	rs_dsn_read_ssl_tab(PropSheet_IndexToHwnd(hTabMainControl, 1), rs_dsn_setup_ctxt);
	rs_dsn_read_proxy_tab(PropSheet_IndexToHwnd(hTabMainControl, 2), rs_dsn_setup_ctxt);
	rs_dsn_read_csc_tab(PropSheet_IndexToHwnd(hTabMainControl, 3), rs_dsn_setup_ctxt);
	rs_dsn_read_advanced_tab(PropSheet_IndexToHwnd(hTabMainControl, 4), rs_dsn_setup_ctxt);
	rs_dsn_log(__LINE__, "test_connect: read dialog values");
/*
 *	prompt for password
 */
/*	rs_dsn_setup_ctxt->password_cancelled = TRUE;
	if ((IDOK != DialogBoxParam(hModule, MAKEINTRESOURCE(DLG_PASSWORD), hdlg,
		rs_dsn_password_dialog, (LPARAM) rs_dsn_setup_ctxt)) ||
		rs_dsn_setup_ctxt->password_cancelled)
	{
		rs_dsn_log(__LINE__, "test_connect: password failed");
//		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
//		SQLFreeHandle(SQL_HANDLE_ENV, henv);
		return TRUE;
	} */

	rc = SQLAllocHandle(SQL_HANDLE_ENV, 0, &henv);
	if (rc != SQL_SUCCESS) {
		rs_dsn_log(__LINE__, "test_connect: alloc env handle failed");
		return FALSE;
	}
	rc = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
	if (rc != SQL_SUCCESS) {
		rs_dsn_log(__LINE__, "test_connect: env version failed");
		return FALSE;
	}
	rc = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
	if (rc != SQL_SUCCESS) {
		rs_dsn_log(__LINE__, "test_connect: alloc dbc handle failed");
		return FALSE;
	}
	rc = SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
	if (rc != SQL_SUCCESS) {
		rs_dsn_log(__LINE__, "test_connect: set login timeout failed");
		return FALSE;
	}

	rs_dsn_log(__LINE__, "test_connect: connecting");
	rs_dsn_make_connect_str(rs_dsn_setup_ctxt, connectstr, sizeof(connectstr));
	rc = SQLDriverConnect(hdbc, NULL,
						connectstr, SQL_NTS,
						outconnstr, MAX_CONNECTION_STRING_SIZE, &outlen,
						SQL_DRIVER_NOPROMPT); 
//	rc = SQLConnect(hdbc,"test2012",SQL_NTS,"iggarish",SQL_NTS,"",SQL_NTS);
	if (rc != SQL_SUCCESS) {
		char sqlstate[6];
		char errmsg[513];
		SQLINTEGER errcode;
		SQLSMALLINT errmsglen;

		rs_dsn_log(__LINE__, "test_connect: connection failed rc %d, getting error", rc);
		SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, 1, sqlstate, &errcode, errmsg, 512, &errmsglen);
		_snprintf(resultmsg, sizeof(resultmsg), "Connection failed[%s]: %s", sqlstate, errmsg);
		rs_dsn_log(__LINE__, "test_connect: code %d error %s", errcode, resultmsg);
		dlg_flag |= MB_ICONEXCLAMATION;
	}
	else {
		SQLDisconnect(hdbc);
		strncpy(resultmsg, "Connection successful!",sizeof(resultmsg));
	}
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
	rs_dsn_log(__LINE__, "test_connect: posting msgbox");
	MessageBox(rs_dsn_setup_ctxt->hwndParent, resultmsg, "Connection Test", dlg_flag);
	return TRUE;
}

static void
rs_dsn_read_text_entry(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, int item, const char *attr)
{
	if (item == IDC_WIT || item == IDC_TOKEN)
	{
		char *valbuf = (char *)calloc(1, MAX_JWT);
		GetDlgItemText(hdlg, item, valbuf, MAX_JWT);
		rs_dsn_set_attr(rs_dsn_setup_ctxt, attr, valbuf);
	}
	else
	{
		char valbuf[MAXVALLEN];

		GetDlgItemText(hdlg, item, valbuf, sizeof(valbuf));
		rs_dsn_set_attr(rs_dsn_setup_ctxt, attr, valbuf);
	}
}

static void
rs_odbc_glb_read_text_entry(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, int item, const char *attr)
{
	char valbuf[MAXVALLEN];

	GetDlgItemText(hdlg, item, valbuf, sizeof(valbuf));
	rs_odbc_glb_set_attr(rs_dsn_setup_ctxt, attr, valbuf);
}

static void
rs_dsn_read_int_entry(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt, int item, const char *attr, char **setOfValues)
{
	char valbuf[MAXVALLEN];
    int iVal;

    iVal = ComboBox_GetCurSel(GetDlgItem(hdlg,item));
    if(iVal < 0)
        iVal = 0;
	if(setOfValues == NULL)
		snprintf(valbuf,sizeof(valbuf), "%d",iVal);
	else
		snprintf(valbuf, sizeof(valbuf), "%s",setOfValues[iVal]);
	
	rs_dsn_set_attr(rs_dsn_setup_ctxt, attr, valbuf);
}


static void
rs_dsn_read_connect_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_dsn_log(__LINE__, "rs_dsn_read_connect_tab");
	if (rs_dsn_setup_ctxt->connect_inited) {
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_DSNAME, "DSN");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_DESC, "Description");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_DATABASE, "Database");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_SERVER, "HostName");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_USER, "LogonID");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PWD, "Password");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PORT, "PortNumber");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_AUTH_PROFILE, RS_IAM_AUTH_PROFILE);

		// IDP
		rs_dsn_read_idp_items(hdlg, rs_dsn_setup_ctxt);

		// TODO: uid and pwd for Azure
	}
}

static void
rs_dsn_read_advanced_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	HWND lb;
	int n;
	char temp_buf[MAXVALLEN];

	rs_dsn_log(__LINE__, "rs_dsn_read_advanced_tab");

	if (rs_dsn_setup_ctxt->advanced_inited) {
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_INI_SQL_EDIT, "InitializationString");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_RETRY_EDIT, "ConnectionRetryCount");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_DELAY_EDIT, "ConnectionRetryDelay");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_LOGIN_TO_EDIT, "LoginTimeout");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_QRY_TO_EDIT, "QueryTimeout");

//		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_DESC_PARAMS, "EnableDescribeParam");
//		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_COL_METADATA, "ExtendedColumnMetaData");
//		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_THREADS, "ApplicationUsingThreads");
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_FETCH_REFCURSOR, "FetchRefCursor");
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_ROLLBACK_ERR, "TransactionErrorBehavior");
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_CURRENT_DB_ONLY, RS_DATABASE_METADATA_CURRENT_DB_ONLY);
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_READ_ONLY, RS_READ_ONLY);
		// read from page into local cache
		lb = GetDlgItem(hdlg, IDC_LOG_LEVEL_COMBO);
		n = ComboBox_GetCurSel(lb);
		snprintf(temp_buf, sizeof(temp_buf), "%d", n);

		rs_odbc_glb_set_attr(rs_dsn_setup_ctxt, RS_LOG_LEVEL_OPTION_NAME, temp_buf);
		rs_odbc_glb_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_LOG_PATH_EDIT, RS_LOG_PATH_OPTION_NAME);
	}
}

static void
rs_dsn_read_proxy_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_dsn_log(__LINE__, "rs_dsn_read_proxy_tab");

	if (rs_dsn_setup_ctxt->proxy_inited) {

		UINT isProxyRedshift = IsDlgButtonChecked(hdlg, IDC_PROXY_REDSHIFT_CONNECTION);
		UINT isHttpsProxy = IsDlgButtonChecked(hdlg, IDC_PROXY_STS);
		if (isProxyRedshift)
		{
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_REDSHIFT_SERVER_EDIT, RS_TCP_PROXY_HOST);
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_REDSHIFT_PORT_EDIT, RS_TCP_PROXY_PORT);
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_REDSHIFT_UID_EDIT, RS_TCP_PROXY_USER_NAME);
		}

		if (isHttpsProxy)
		{
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_HTTPS_SERVER_EDIT, RS_HTTPS_PROXY_HOST);
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_HTTPS_PORT_EDIT, RS_HTTPS_PROXY_PORT);
			rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_HTTPS_UID_EDIT, RS_HTTPS_PROXY_USER_NAME);
			rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_PROXY_IDP, RS_IDP_USE_HTTPS_PROXY);
		}

	}
}

static void
rs_dsn_read_csc_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_dsn_log(__LINE__, "rs_dsn_read_csc_tab");

	if (rs_dsn_setup_ctxt->csc_inited) {
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_CSC_THRESHOLD, "CscThreshold");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_CSC_PATH, "CscPath");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_CSC_MAX_FILE_SIZE, "CscMaxFileSize");
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_CSC_ENABLE, "CscEnable");

		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_SC_ROWS, "StreamingCursorRows");
	}
}

static void
rs_dsn_read_ssl_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_dsn_log(__LINE__, "rs_dsn_read_ssl_tab");

	if (rs_dsn_setup_ctxt->ssl_inited) 
    {
		rs_dsn_read_int_entry(hdlg, rs_dsn_setup_ctxt, IDC_COMBO_EM, "EncryptionMethod", NULL);
//		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_VSC, "ValidateServerCertificate");
//		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_HNIC, "HostNameInCertificate");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_TS, "TrustStore");
//		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_EDIT_KSN, "KerberosServiceName");
//		rs_dsn_read_int_entry(hdlg, rs_dsn_setup_ctxt, IDC_COMBO_KSA, "KerberosAPI", g_setOfKSA);

		// Derived SSLMode
		int em = rs_dsn_int_attr(rs_dsn_setup_ctxt, "EncryptionMethod");
//		BOOL vsc = rs_dsn_bool_attr(rs_dsn_setup_ctxt, "ValidateServerCertificate");
//		char *hn = rs_dsn_get_attr(rs_dsn_setup_ctxt, "HostNameInCertificate");
		char *sslMode = SSL_MODE_VERIFY_CA;
		if (em == 0)
			sslMode = SSL_MODE_DISABLE;
		else
		if (em == 1)
			sslMode = SSL_MODE_VERIFY_CA;
		else
		if (em == 2)
			sslMode = SSL_MODE_VERIFY_FULL;
		else
		if (em == 3)
			sslMode = SSL_MODE_REQUIRE;

		rs_dsn_set_attr(rs_dsn_setup_ctxt, RS_SSL_MODE, sslMode);
	}
}

static void
rs_dsn_read_failover_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	int mode, granularity;
	char t[50];

	rs_dsn_log(__LINE__, "rs_dsn_read_failover_tab");

	if (rs_dsn_setup_ctxt->failover_inited) {
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_LOAD_BALANCE, "LoadBalancing");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_ALT_SRV_EDIT, "AlternateServers");

		for (mode = 0; (mode < 3) &&
			(BST_CHECKED != IsDlgButtonChecked(hdlg, failover_mode_ids[mode])); mode++);
 		snprintf(t, sizeof(t), "%d", mode);
 		rs_dsn_set_attr(rs_dsn_setup_ctxt, "FailoverMode", t);

		for (granularity = 0; (granularity < 4) &&
			(BST_CHECKED != IsDlgButtonChecked(hdlg, failover_gran_ids[granularity])); granularity++);
 		snprintf(t, sizeof(t), "%d", granularity);
 		rs_dsn_set_attr(rs_dsn_setup_ctxt, "FailoverGranularity", t);

		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_FAIL_PRECONN, "FailoverPreconnect");
	}
}

static void
rs_dsn_read_pooling_tab(HWND hdlg, rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_dsn_log(__LINE__, "rs_dsn_read_pooling_tab");

	if (rs_dsn_setup_ctxt->failover_inited) {
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_POOL_ENABLE, "Pooling");
		rs_DSN_GET_CHECKBOX(hdlg, rs_dsn_setup_ctxt, IDC_POOL_RESET, "ConnectionReset");

		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_POOL_MAX_EDIT, "MaxPoolSize");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_POOL_MIN_EDIT, "MinPoolSize");
		rs_dsn_read_text_entry(hdlg, rs_dsn_setup_ctxt, IDC_POOL_TO_EDIT, "LoadBalanceTimeout");
	}
}


/*
 *	END DIALOG FUNCTIONS
 */

/*
 *	BEGIN REGISTRY FUNCTIONS
 */

static BOOL
rs_dsn_write_error(HWND hwndParent, char *dsn, char *reason, int lineno)
{
	RETCODE	ret = SQL_ERROR;
	DWORD	err = SQL_ERROR;
	char    errmsg[SQL_MAX_MESSAGE_LENGTH];

	ret = SQLInstallerError(1, &err, errmsg, sizeof(errmsg), NULL);
	rs_dsn_log(lineno, "%s failed: %s", reason, errmsg);
	if (hwndParent)
	{
		char		szBuf[256];

		if (SQL_SUCCESS != ret)
		{
			LoadString(hModule, IDS_BADDSN, szBuf, sizeof(szBuf));
			wsprintf(errmsg, szBuf, dsn);
		}
		LoadString(hModule, IDS_MSGTITLE, szBuf, sizeof(szBuf));
		MessageBox(hwndParent, errmsg, szBuf, MB_ICONEXCLAMATION | MB_OK);
	}
	return FALSE;
}

/*--------
 *	Write data source attributes to ODBC.INI
 */
static BOOL
rs_dsn_write_attrs(
	HWND hwndParent,
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	LPCSTR		dsn = rs_dsn_get_attr(rs_dsn_setup_ctxt, "DSN");
//	char *encoded_conn_settings;
	rs_dsn_attr_t *attrs = rs_dsn_setup_ctxt->attrs;
	int i;

	if (rs_dsn_setup_ctxt->is_new_dsn_name && !dsn[0])
		return FALSE;

	rs_dsn_log(__LINE__, "writing attributes for %s for driver %s", dsn, rs_dsn_setup_ctxt->driver_desc);
/* Write data source name */
	if (!SQLWriteDSNToIni(dsn, rs_dsn_setup_ctxt->driver_desc))
		return rs_dsn_write_error(hwndParent, (char *)dsn, "SQLWriteDSNToIni", __LINE__);
/*
 *	iterate over the attrs and write them out
 */
	for (i = 0; attrs[i].attr_code[0]; i++) {
		rs_dsn_log(__LINE__, "DSN %s: attr %s value %s", dsn, attrs[i].attr_code, attrs[i].attr_val);

		if (!SQLWritePrivateProfileString(dsn, attrs[i].attr_code, get_attr_val(&attrs[i], TRUE), ODBC_INI))
			return rs_dsn_write_error(hwndParent, (char *)dsn, "SQLWritePrivateProfileString", __LINE__);
	}
/*
 * data source name has changed, remove old name
 */
	if (_stricmp(rs_dsn_setup_ctxt->dsn_name, dsn)) {
		if (!SQLRemoveDSNFromIni(rs_dsn_setup_ctxt->dsn_name))
			return rs_dsn_write_error(hwndParent, rs_dsn_setup_ctxt->dsn_name, "SQLRemoveDSNFromIni", __LINE__);
	}

	// Write ODBC Global values such as LogLevel, LogPath.
	rs_odbc_glb_write_attrs(hwndParent, rs_dsn_setup_ctxt);

	return TRUE;
}

static BOOL
rs_odbc_glb_write_attrs(
	HWND hwndParent,
	rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_odbc_glb_attr_t *attrs = rs_dsn_setup_ctxt->odbc_glb_attrs;
	int i;
	int len;
	HKEY odbcKey;

	rs_dsn_log(__LINE__, "rs_odbc_glb_write_attrs()");
	//create an entry, if not exists
	if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\ODBC\\ODBC.INI\\ODBC",
	                                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
										 &odbcKey, NULL))
	{
		/*
		*	note that the value in the input context is our default
		*/
		for (i = 0; rs_odbc_glb_attrs[i].attr_code[0]; i++)
		{
			len = strlen(attrs[i].attr_val) + 1;
			RegSetValueExA(odbcKey, rs_odbc_glb_attrs[i].attr_code, 0, REG_SZ, (LPBYTE)attrs[i].attr_val, len);
			rs_dsn_log(__LINE__, "rs_odbc_glb_write_attrs() key:%s val:%s", rs_odbc_glb_attrs[i].attr_code, attrs[i].attr_val);
		}
		RegCloseKey(odbcKey);
	} else {
		rs_dsn_log(__LINE__, "rs_odbc_glb_write_attrs() NOT successful");
	}

	rs_dsn_log(__LINE__, "rs_odbc_glb_write_attrs() return");

	return TRUE;

}

/*--------
 *	Read data source attributes from ODBC.INI
 */
static BOOL
rs_dsn_read_attrs(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	LPCSTR		dsn = rs_dsn_setup_ctxt->dsn_name;
	rs_dsn_attr_t *attrs = rs_dsn_setup_ctxt->attrs;
	int i;

	if (rs_dsn_setup_ctxt->is_new_dsn_name && !dsn[0])
		return FALSE;
/*
 *	note that the value in the input context is our default
 */
	for (i = 0; rs_dsn_attrs[i].attr_code[0]; i++)
	{ 
		if (strcmp(rs_dsn_attrs[i].attr_code, RS_WEB_IDENTITY_TOKEN) == 0 || 
		    strcmp(rs_dsn_attrs[i].attr_code, RS_TOKEN) == 0)
		{
			attrs[i].large_attr_val = (char *)calloc(1, MAX_JWT);
			SQLGetPrivateProfileString(dsn, rs_dsn_attrs[i].attr_code, "",
				attrs[i].large_attr_val, MAX_JWT, ODBC_INI);
		}
		else
		{
			SQLGetPrivateProfileString(dsn, rs_dsn_attrs[i].attr_code, attrs[i].attr_val,
				attrs[i].attr_val, sizeof(attrs[i].attr_val), ODBC_INI);

			if (strcmp(rs_dsn_attrs[i].attr_code, "Password") == 0)
			{
				// Decode the e-password
				char *pwd = decode64Password(attrs[i].attr_val, strlen(attrs[i].attr_val));
				if (pwd)
				{
					strncpy(attrs[i].attr_val, pwd, sizeof(attrs[i].attr_val));
					free(pwd);
				}
			}
		}
	}

	// Read ODBC Global values such as LogLevel, LogPath.
	rs_odbc_glb_read_attrs(rs_dsn_setup_ctxt);

	return TRUE;
}

static BOOL
rs_odbc_glb_read_attrs(rs_dsn_setup_ptr_t rs_dsn_setup_ctxt)
{
	rs_odbc_glb_attr_t *attrs = rs_dsn_setup_ctxt->odbc_glb_attrs;
	int i;
	int len;
	HKEY odbcKey;

	rs_dsn_log(__LINE__, "rs_odbc_glb_read_attrs()");

	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\ODBC\\ODBC.INI\\ODBC", 0, KEY_ALL_ACCESS, &odbcKey))
	{
		/*
		*	note that the value in the input context is our default
		*/
		for (i = 0; rs_odbc_glb_attrs[i].attr_code[0]; i++)
		{
			len = sizeof(attrs[i].attr_val);
			RegQueryValueEx(odbcKey, rs_odbc_glb_attrs[i].attr_code, 0, NULL, (LPBYTE)attrs[i].attr_val, &len);
			rs_dsn_log(__LINE__, "rs_odbc_glb_read_attrs() key:%s val:%s", rs_odbc_glb_attrs[i].attr_code, attrs[i].attr_val);
		}
		RegCloseKey(odbcKey);
	} else {
		rs_dsn_log(__LINE__, "rs_odbc_glb_read_attrs() NOT successfull");
	}

	rs_dsn_log(__LINE__, "rs_odbc_glb_read_attrs() return");


	return TRUE;
}

/*
 *	END REGISTRY FUNCTIONS
 */

/*====================================================================================================================================================*/

char *base64Password(const unsigned char *input, int length) {
	if (length != 0)
	{
		const int pl = 4 * ((length + 2) / 3);
		char *output = (char *)calloc(pl + 1, 1); //+1 for the terminating null that EVP_EncodeBlock adds on
		const int ol = EVP_EncodeBlock((unsigned char *)output, input, length);
		if (ol != pl)
		{
			//		fprintf(stderr, "Encode predicted %d but we got %d\n", pl, ol); 
		}
		return output;
	}
	else
		return NULL;
}

/*====================================================================================================================================================*/

unsigned char *decode64Password(const char *input, int length) {
	if (length != 0)
	{
		const int pl = 3 * length / 4;
		unsigned char *output = (unsigned char *)calloc(pl + 1, 1);
		const int ol = EVP_DecodeBlock(output, (const unsigned char *)input, length);
		if (pl != ol)
		{
			//		fprintf(stderr, "Decode predicted %d but we got %d\n", pl, ol); 
		}
		return output;
	}
	else
		return NULL;
}
