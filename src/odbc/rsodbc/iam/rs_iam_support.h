/*-------------------------------------------------------------------------
*
* Copyright(c) 2021, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/


#ifndef _RS_IAM_SUPPORT_H_
#define _RS_IAM_SUPPORT_H_

#include <memory>

#include "rs_string.h"
#include "rs_wstring.h"

#include "RsHttpProxySettings.h"
#include "RsUidPwdSettings.h"

#include "RsErrorException.h"

namespace Redshift
{
namespace IamSupport
{
    /// AuthType specified in IAMConfiguration
    enum AuthType
    {
        DEFAULT = 0,
        STATIC,
        PROFILE,
        PLUGIN,
        INSTANCE_PROFILE
    };

    /// Component identifier for IamSupport errors.
    static const int IAM_ERROR = 860;

    /// SQLSTATE string for communication link failure error.
    #define SQLSTATE_COMM_LINK_FAILURE "08S01"

    // The error messages file to use.
    #define IAM_ERROR_MESSAGES_FILE "IamMessages"

    /* The default profile name to look up */
    #define IAM_DEFAULT_PROFILE "default"

    /* The default environment variable used to look up profile name */
    #define AWS_PROFILE_ENVIRONMENT_VARIABLE "AWS_DEFAULT_PROFILE"

    /* The default SSL root certificate name */
    #define IAM_SSLROOTCERT_NAME    L"root.crt"

    /* Keys used for reading attributes from profile files */
    #define IAM_KEY_PROFILE_NAME    "profile_name"
    #define IAM_KEY_ACCESS_ID       "aws_access_key_id"
    #define IAM_KEY_SECRET_KEY      "aws_secret_access_key"
    #define IAM_KEY_SESSION_TOKEN   "aws_session_token"
    #define IAM_KEY_PLUGIN_NAME     "plugin_name"
    #define IAM_KEY_DBUSER          "dbuser"
    #define IAM_KEY_AUTOCREATE      "autocreate"
    #define IAM_KEY_DBGROUPS        "dbgroups"
    #define IAM_KEY_FORCELOWERCASE  "forcelowercase"
    #define IAM_KEY_USER            "user"
    #define IAM_KEY_PASSWORD        "password"
    #define IAM_KEY_IDP_HOST        "idp_host"
    #define IAM_KEY_IDP_PORT        "idp_port"
    #define IAM_KEY_IDP_TENANT      "idp_tenant"
    #define IAM_KEY_IDP_PARTITION   "idp_partition"
    #define IAM_KEY_CLIENT_SECRET   "client_secret"
    #define IAM_KEY_CLIENT_SECRET_ENCRYPTED     "client_secret_encrypted"
    #define IAM_KEY_CLIENT_ID       "client_id"
    #define IAM_KEY_IDP_RESPONSE_TIMEOUT         "idp_response_timeout"
    #define IAM_KEY_LOGIN_URL       "login_url"
    #define IAM_KEY_LISTEN_PORT     "listen_port"
    #define IAM_KEY_DBGROUPS_FILTER "dbgroups_filter"
    #define IAM_KEY_IDP_USE_HTTPS_PROXY "idp_use_https_proxy"
    #define IAM_KEY_DURATION        "duration"
    #define IAM_KEY_PREFERRED_ROLE  "preferred_role"
    #define IAM_KEY_SSL_INSECURE    "ssl_insecure"
    #define IAM_KEY_APP_ID          "app_id"
    #define IAM_KEY_APP_NAME        "app_name"
    #define IAM_KEY_PARTNER_SPID    "partner_spid"
    #define IAM_KEY_LOGINTORP       "loginToRp"
    #define IAM_KEY_HTTPS_PROXY_HOST    "https_proxy_host"
    #define IAM_KEY_HTTPS_PROXY_PORT    "https_proxy_port"
    #define IAM_KEY_HTTPS_PROXY_UID     "https_proxy_username"
    #define IAM_KEY_HTTPS_PROXY_PWD     "https_proxy_password"
    #define IAM_KEY_HTTPS_PROXY_EPWD    "https_proxy_encrypted_password"
    #define IAM_KEY_ROLE_ARN            "role_arn"
    #define IAM_KEY_WEB_IDENTITY_TOKEN  "web_identity_token"
    #define IAM_KEY_ROLE_SESSION_NAME   "role_session_name"
    #define IAM_KEY_REGION				"region"
	#define IAM_KEY_STS_ENDPOINT_URL    "StsEndpointUrl"
	#define IAM_KEY_ENDPOINT_URL		"EndpointUrl"
//	#define IAM_KEY_PROVIDER_NAME       "provider_name"
	#define IAM_KEY_AUTH_PROFILE        "AuthProfile"
	#define IAM_KEY_STS_CONNECTION_TIMEOUT  "StsConnectionTimeout"
	#define IAM_KEY_SCOPE					"scope" // "api://" + client_id + "/User.Read"
	#define IAM_KEY_PROXY_BYPASS_LIST		"proxy_bypass_list" // Browser proxy bypass list for OAuth redirect

    #define KEY_IDP_AUTH_TOKEN              "token"
    #define KEY_IDP_AUTH_TOKEN_TYPE         "token_type"
    #define KEY_IDC_ISSUER_URL              "issuer_url"
    #define KEY_IDC_REGION                  "idc_region"
    #define KEY_IDC_CLIENT_DISPLAY_NAME     "idc_client_display_name"


    /* SAML pattern used to extract SAML assertion from the HTML response page */
    #define IAM_PLUGIN_SAML_PATTERN "SAMLResponse.+?value=\"([^\"]+)\""

    /* Predefined external plug-in */
    #define IAM_PLUGIN_ADFS             "ADFS"
    #define IAM_PLUGIN_AZUREAD          "AzureAD"
    #define IAM_PLUGIN_BROWSER_AZURE    "BrowserAzureAD"
    #define IAM_PLUGIN_BROWSER_SAML	    "BrowserSAML"
    #define IAM_PLUGIN_PING             "Ping"
    #define IAM_PLUGIN_OKTA             "Okta"
    #define IAM_PLUGIN_EXTERNAL         "External"
    #define IAM_PLUGIN_JWT              "JWT"    // used for federated native IdP auth
    #define IAM_PLUGIN_BROWSER_AZURE_OAUTH2    "BrowserAzureADOAuth2"  // used for federated native IdP auth
    #define JWT_IAM_AUTH_PLUGIN         "JwtIamAuthPlugin"    // used for federated Jwt IAM auth
    #define PLUGIN_IDP_TOKEN_AUTH               "IdpTokenAuthPlugin"
    #define PLUGIN_BROWSER_IDC_AUTH             "BrowserIdcAuthPlugin"

    /**
    * The CA path used to look up CA files
    */
    #define IAM_CA_PATH "CaPath"

    /**
    * The CA file used to verify SSL certificate
    */
    #define IAM_CA_FILE "CaFile"

    // The default IdP port.
    #define IAM_DEFAULT_IDP_PORT 443

    // The default IdP port string.
    #define IAM_DEFAULT_IDP_PORT_STR L"443"

    // The default timeout in seconds for browser plugin.
    #define IAM_DEFAULT_BROWSER_PLUGIN_TIMEOUT 120

    // The default listen port.
    #define IAM_DEFAULT_LISTEN_PORT 7890

    // The default duration
    #define IAM_DEFAULT_DURATION 0

    #define IAM_URL_PATTERN_SCHEME "https"
 	#define IAM_URL_PATTERN_DOMAIN "[a-zA-Z0-9_~.-]+"
    #define IAM_URL_PATTERN_PATH "/[A-Za-z0-9_.~%+/-]*"
    #define IAM_URL_PATTERN_PORT "[0-9]+"
    #define IAM_URL_PATTERN_QUERY "[A-Za-z0-9_.~%+&=-]+"
    #define IAM_URL_PATTERN_FRAGMENT "[a-zA-Z0-9+&@/%=~_!:,.-]+"

    // This is used for validation on Windows, since the windows apis combine fragments
    // and queries together when cracking them

    #define IAM_URL_PATTERN_QUERY_AND_FRAGMENT L"[a-zA-Z0-9+&@/%?#=~_!:,.-]+"

    // The default timeout in milliseconds.
    static const long DEFAULT_TIMEOUT = 60000;

    /// @brief The IAM related settings.
    struct IamSettings
    {
        /// @brief Constructor.
        ///
        /// @param in_defaultCaFile         The default CA file path. Default empty string.
        IamSettings(const rs_string& in_defaultCaFile = ""):
            m_idpPort(IAM_DEFAULT_IDP_PORT),
            m_idp_response_timeout(IAM_DEFAULT_BROWSER_PLUGIN_TIMEOUT),
            m_listen_port(IAM_DEFAULT_LISTEN_PORT),
            m_duration(IAM_DEFAULT_DURATION),
            m_caFile(in_defaultCaFile),
            m_authType(DEFAULT),
            m_sslInsecure(false),
            m_enableDbUserDbGroups(false),
            m_forceLowercase(false),
            m_userAutoCreate(false),
            m_useProxyForIdpAuth(false)
        {
            ; // Do nothing.
        }

        /// The HTTP proxy related settings.
        RsHttpProxySettings m_proxySettings;

        /// The UID/PWD settings for profile or plug-in authentication types.
        RsUidPwdSettings m_uidPwdSettingsForProfileOrPlugin;

        /// The DB user.
        rs_string m_dbUser;

        /// The DB group.
        rs_string m_dbGroups;

        /// The plug-in name.
        rs_string m_pluginName;

        /// The IdP server host.
        rs_string m_idpHost;

        /// The IdP server port.
        short m_idpPort;

        /// The Idp Server tenant.
        rs_string m_idpTenant;

        /// The Idp Server partition.
        rs_string m_idpPartition;

        /// The IDP Response Timeout for Browser based authentication.
        short m_idp_response_timeout;

        /// The Listen port for Browser based authentication.
        short m_listen_port;

        /// The Browser Login URL.
        rs_string m_login_url;

        /// The dbGroups filter.
        rs_string m_dbGroupsFilter;

        /// The Client Secret.
        rs_string m_clientSecret;

        /// The Client ID.
        rs_string m_clientId;

		/// The Scope.
		rs_string m_scope;

        /// The application ID.
        rs_string m_appId;

        /// The preferred role.
        rs_string m_preferredRole;

        /// The PartnerSpId.
        rs_string m_partnerSpId;

        /// The Okta app name.
        rs_string m_oktaAppName;

        /// The AWS profile path.
        rs_string m_awsProfile;

        /// The access Key ID.
        rs_string m_accessKeyId;

        /// The secret access key.
        rs_string m_secretAccessKey;

        /// The session token.
        rs_string m_sessionToken;

        /// The CA file path.
        rs_string m_caFile;

        /// The Amazon Resource Name (ARN) of the role that the caller is assuming.
        rs_string m_role_arn;

        /// The OAuth 2.0 access token or OpenID Connect ID token.
        rs_string m_web_identity_token;

        /// An identifier for the assumed role session.
        rs_string m_role_session_name;

        /// The duration, in seconds, of the role session.
        short m_duration;

        /// The authentication type.
        AuthType m_authType;

        /// Indicate whether to use insecure SSL connection with the IdP server.
        bool m_sslInsecure;

        /// Indicate whether to enable DB user and group feature.
        bool m_enableDbUserDbGroups;

        /// Indicate whether to force lowercase the DB groups.
        bool m_forceLowercase;

        /// Indicate whether to automatically create the DB user.
        bool m_userAutoCreate;

        /// Indicate whether to use HTTP proxy for the connection with the IdP server.
        bool m_useProxyForIdpAuth;
    };
}
}

// Throw an ErrorException with the given state key, component id IAM_ERROR, the given message
// id and the given message parameter.
#define IAMTHROW(key, id, param)                                                                  \
{                                                                                                  \
    std::vector<rs_string> msgParams;                                                          \
    msgParams.push_back(param);                                                                    \
    throw RsErrorException(key, IAM_ERROR, id, msgParams);                           \
}

#endif
