#ifndef _RS_IAM_H_
#define _RS_IAM_H_

#define MAX_IAM_BUF_VAL     256
#define MAX_IAM_SESSION_TOKEN_LEN 2048
#define MAX_IAM_DBGROUPS_LEN 1024
#define MAX_IAM_JWT_LEN (16 * 1024)
#define MAX_IDEN_LEN        (1024+ 1)
#define MAX_BASIC_AUTH_TOKEN_LEN (16 * 1024)

struct RS_IAM_CONN_PROPS_INFO {
  // Standard = "", IAM = "Static", Profile = "Profile", any plugin = "Plugin"
  char szAuthType[MAX_IDEN_LEN]; // The connection key to determine which IAM Auth Type to use.
  char szAccessKeyID[MAX_IAM_BUF_VAL];
  char szSecretAccessKey[MAX_IAM_BUF_VAL];
  char szSessionToken[MAX_IAM_SESSION_TOKEN_LEN];
  char szClusterId[MAX_IAM_BUF_VAL];
  char szRegion[MAX_IDEN_LEN];
  long  lIAMDuration; // The connection key specifying the time in seconds until the IAM credentials expire.
  bool isAutoCreate;
  char szDbUser[MAX_IDEN_LEN];
  char szDbGroups[MAX_IAM_DBGROUPS_LEN];
  bool isForceLowercase;
  char szProfile[MAX_IAM_BUF_VAL]; // The AWS profile name for credentials.
  bool isInstanceProfile; // Use AWS instance profile for IAM authentication.
  char szEndpointUrl[MAX_IAM_BUF_VAL];
  char szStsEndpointUrl[MAX_IAM_BUF_VAL];
  char szPluginName[MAX_IDEN_LEN]; // plugin_name
  char szIdpHost[MAX_IAM_BUF_VAL]; // idp_host
  int iIdpPort; // idp_port
  char szIdpTenant[MAX_IAM_BUF_VAL]; // idp_tenant
  long lIdpResponseTimeout; // idp_response_timeout
  char szLoginUrl[MAX_IAM_BUF_VAL]; // login_url
  long lListenPort; // listen_port
  char szDbGroupsFilter[MAX_IAM_DBGROUPS_LEN]; // dbgroups_filter
  char szClientSecret[MAX_IAM_BUF_VAL]; // client_secret
  char szClientId[MAX_IAM_BUF_VAL]; // client_id
  char szScope[MAX_IAM_BUF_VAL]; // scope
  long lDuration; // duration
  char szPreferredRole[MAX_IAM_BUF_VAL]; // preferred_role
  bool isSslInsecure; // ssl_insecure
  char szAppId[MAX_IAM_BUF_VAL]; // app_id
  char szAppName[MAX_IAM_BUF_VAL]; // app_name
  char szPartnerSpid[MAX_IAM_BUF_VAL];// partner_spid
  char szLoginToRp[MAX_IAM_BUF_VAL]; // loginToRp
  char szRoleArn[MAX_IAM_BUF_VAL]; // role_arn
  char *pszJwt; // [MAX_IAM_JWT_LEN]; // web_identity_token
  char szRoleSessionName[MAX_IAM_BUF_VAL]; // role_session_name
  char szUser[MAX_IDEN_LEN];
  char szPassword[MAX_IDEN_LEN];
  char szSslMode[MAX_IDEN_LEN];
  char szCaPath[MAX_IAM_BUF_VAL];
  char szCaFile[MAX_IAM_BUF_VAL];
  char szHost[MAX_IDEN_LEN];
  char szPort[MAX_IDEN_LEN];
  char szDatabase[MAX_IDEN_LEN];
  char szAuthProfile[MAX_IAM_BUF_VAL];
  char szBasicAuthToken[MAX_BASIC_AUTH_TOKEN_LEN];
  char szIdentityNamespace[MAX_IAM_BUF_VAL];
  char szTokenType[MAX_IDEN_LEN];
  char szIssuerUrl[MAX_IAM_BUF_VAL];
  char szIdcRegion[MAX_IDEN_LEN];
  bool isDisableCache; 
  int iStsConnectionTimeout; 
  bool isGroupFederation;
  long lIdcResponseTimeout; // idc_response_timeout
  char szIdcClientDisplayName[MAX_IAM_BUF_VAL]; // idc_client_display_name
  bool isServerless;
  char szWorkGroup[MAX_IDEN_LEN];
  char szManagedVpcUrl[MAX_IAM_BUF_VAL];
};

struct RS_PROXY_CONN_PROPS_INFO {
  char szHttpsHost[MAX_IAM_BUF_VAL]; // https proxy host
  int iHttpsPort; // https proxy port
  char szHttpsUser[MAX_IDEN_LEN];
  char szHttpsPassword[MAX_IDEN_LEN];
  bool isUseProxyForIdp;
};

#endif
