#ifndef _RS_SETTINGS_H_
#define _RS_SETTINGS_H_

#include "rs_string.h"
#include "rs_wstring.h"

struct RsSettings
{
    rs_string  m_host;
	int   m_port;
	rs_string  m_username;
    rs_string  m_password;
    rs_string  m_database;
    rs_string  m_sslMode;
	bool       m_disableCache;

    rs_string  m_proxyHost;
	int		   m_proxyPort;
    rs_string  m_proxyCredentials;
    rs_string  m_httpsProxyHost;
	int		   m_httpsProxyPort;
	rs_string  m_httpsProxyUsername;
    rs_string  m_httpsProxyPassword;
	bool       m_useProxyForIdpAuth;

/*
    rs_string  m_krbsrvname;
    rs_wstring m_appName;
    rs_wstring m_sslCertPath;

    int   m_keepAlives;
    int   m_keepAlivesIdle;
    int   m_keepAlivesInterval;
    int   m_keepAlivesCount;
    int   m_cacheSize;
    int   m_maxVarchar;
    int   m_maxLongVarchar;
    int   m_maxBytea;

    // The Minimum TLS version number
    // Note:
    // When the user selects a particular version, the driver will disable all version of TLS
    // that is lower than the selected the version.
    rs_string m_TLSVersion;

    bool          m_isODBC3;
    bool          m_useUnicode;
    bool          m_exposeTextAsLongVarchar;
    bool          m_exposeByteaAsLongVarBinary;
    bool          m_exposeMoneyAsDecimal;
    bool          m_useSingleRowMode;
    bool          m_exposeBoolAsChar;
    bool          m_useDeclareFetch;
    bool          m_useMultipleStatements;
    bool          m_useSystemTrustStore;
    bool          m_allowSelfSigned;
    bool          m_checkCertRevocation;
    bool          m_enforceSingleStatement;
    bool          m_useKerberos;

    //The flag of if the datasource has the schema view for catalog function support of Spectrum
    bool          m_hasNewSchemaView;
    bool          m_enableTableTypes;

    //The flag if the server is read-only
    bool          m_readOnly;

    //The flag if auto-commit is off
    bool          m_isAutoCommitOff;

    //
    // [PGODBC-1307] Datashare Support for Redshift                         
    //

    // Flag to indicate if application is ready to support multidatabase datashare catalog
    // (false if it supports, true otherwise)
    bool          m_databaseMetadataCurrentDbOnly;
    // Flag to indicate if server supports datashare
    bool          m_datashareEnabled;
*/

    /************************************************************************/
    /* IAM Authentication Settings                                          */
    /************************************************************************/
    rs_string  m_accessKeyID;
    rs_string  m_secretAccessKey;
    rs_string  m_sessionToken;
    rs_string  m_awsRegion;
    rs_string  m_clusterIdentifer;
    rs_string  m_awsProfile;
    rs_string  m_dbUser;
    rs_string  m_authType;
    rs_string  m_caPath;
    rs_string  m_caFile;
    rs_string  m_partnerSpid;
    rs_string  m_loginToRp;
    rs_string  m_oktaAppName;
    rs_string  m_acctId;
    rs_string  m_workGroup;


    int   m_accessDuration;
    short  m_idpPort;
    rs_wstring m_pluginName;
    rs_wstring m_dbGroups;
    rs_wstring m_endpointUrl;
	rs_wstring m_stsEndpointUrl;
    rs_wstring m_idpHost;
    rs_string m_idpTenant;
    short m_idp_response_timeout;
    rs_string m_login_url;
    rs_string m_dbGroupsFilter;
    short m_listen_port;
    rs_string m_clientSecret;
    rs_string m_clientId;
	rs_string m_scope;
	rs_wstring m_appId;
    rs_wstring m_preferredRole;
    rs_string m_role_arn;
    rs_string m_web_identity_token;
    rs_string m_role_session_name;
    short m_duration;
	rs_string m_authProfile;
	int   m_stsConnectionTimeout;

    bool          m_iamAuth;
    bool          m_forceLowercase;
    bool          m_userAutoCreate;
    bool          m_sslInsecure;
    bool          m_useInstanceProfile;
	bool		  m_groupFederation;
    bool          m_isCname;
    bool          m_isServerless;

    RsSettings() :
        m_port(0),
        m_idpPort(0),
        m_idp_response_timeout(0),
        m_listen_port(0),
        m_duration(0),
        m_accessDuration(0),
        m_iamAuth(false),
		m_disableCache(false),
        m_forceLowercase(false),
        m_userAutoCreate(false),
        m_sslInsecure(false),
        m_useInstanceProfile(false),
//        m_hasNewSchemaView(false),
//        m_enforceSingleStatement(false),
        m_sslMode(""),
        m_host(""),
		m_scope(""),
		m_stsConnectionTimeout(0),
		m_groupFederation(false),
        m_isCname(false),
        m_isServerless(false)
    {
        /* Do nothing */
    }

    ~RsSettings() {
      // Do nothing
    }
};

#endif

