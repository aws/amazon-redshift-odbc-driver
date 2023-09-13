#include "RsIamHelper.h"
#include "RsIamClient.h"
#include "IAMUtils.h"
#include "rslock.h"
#include "RsLogger.h"
#include <regex>

#include <string>
#include <unordered_map>
#ifdef WIN32
#include <lmcons.h>
#endif


/* Json::JsonValue class contains a member function: GetObject. There is a predefined
MACRO GetObject in wingdi.h that will cause the conflict. We need to undef GetObject
in order to use the GetObject memeber function from Json::JsonValue */
#ifdef GetObject
#undef GetObject
#endif

#include "aws/core/utils/json/JsonSerializer.h"


using namespace Redshift::IamSupport;

// Static ==========================================================================================
// Map user credentials to unique keys to improve security
static std::unordered_map<rs_string, RsCredentials> s_iamCredentialsCache;

std::regex hostPattern("(.+)\\.(.+)\\.(.+).redshift(-dev)?\\.amazonaws\\.com(.)*");
std::regex serverlessWorkgroupHostPattern("(.+)\\.(.+)\\.(.+).redshift-serverless(-dev)?\\.amazonaws\\.com(.)*");


RsSettings RsIamHelper::s_rsSettings;
MUTEX_HANDLE RsIamHelper::s_iam_helper_criticalSection = rsCreateMutex();;


////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamHelper::IamAuthentication(
                  bool isIAMAuth,
                  RS_IAM_CONN_PROPS_INFO *pIamProps,
                  RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
                  RsSettings& settings,
                  RsLogger *logger)
{
    RsCredentials credentials;

    // Set connection props from RS_CONN_INFO to settings
    SetIamSettings(isIAMAuth, pIamProps, pHttpsProps, settings, logger);

    if (!GetIamCachedSettings(credentials, settings, logger, false))
    {
        RsIamClient iamClient(settings, logger);

        // Connect to retrieve dbUser and dbPassword using AWS credentials
        iamClient.Connect();
        credentials = iamClient.GetCredentials();
        SetIamCachedSettings(credentials, settings, logger);
    }

    /* update the corresponding connection attributes using
       the new setting retrieved from IAM authentication */
    UpdateConnectionSettingsWithCredentials(credentials, settings, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamHelper::NativePluginAuthentication(
    bool isIAMAuth,
    RS_IAM_CONN_PROPS_INFO *pIamProps,
    RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
    RsSettings& settings,
    RsLogger *logger)
{
    RsCredentials credentials;

    // Set connection props from RS_CONN_INFO to settings
    SetIamSettings(isIAMAuth, pIamProps, pHttpsProps, settings, logger);

    if (!GetIamCachedSettings(credentials, settings, logger, true))
    {
        RsIamClient iamClient(settings, logger);

        RS_LOG(logger)("RsIamHelper::NativePluginAuthentication not from cache");

        // Connect to retrieve idp token credentials
        iamClient.Connect();
        credentials = iamClient.GetCredentials();
        SetIamCachedSettings(credentials, settings, logger);
    }
    else
    {
        RS_LOG(logger)("RsIamHelper::NativePluginAuthentication from cache\n");
    }

    /* update the corresponding connection attributes using
    the new setting retrieved from IAM authentication */
    UpdateConnectionSettingsWithCredentials(credentials, settings, true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
bool RsIamHelper::GetIamCachedSettings(
    RsCredentials& out_iamCredentials,
    const RsSettings& in_settings,
    RsLogger *logger,
    bool isNativeAuth)
{
   bool rc;

   rsLockMutex(s_iam_helper_criticalSection);

    if (in_settings.m_disableCache || !IsValidIamCachedSettings(in_settings, logger, isNativeAuth))
    {
        rc = false;
    }
    else
    {
        rs_string cacheKey = GetCacheKey(in_settings, logger);
        out_iamCredentials = s_iamCredentialsCache[cacheKey];

        rc = true;
    }

    rsUnlockMutex(s_iam_helper_criticalSection);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool RsIamHelper::IsValidIamCachedSettings(const RsSettings& in_settings, RsLogger *logger, bool isNativeAuth)
{
    bool rc;

//    paLockMutex(s_iam_helper_criticalSection);

    long currentTime = Aws::Utils::DateTime::Now().Millis();

    /* if users use profile based authentication, disable cache */
    if (in_settings.m_authType == IAM_AUTH_TYPE_PROFILE ||
        !in_settings.m_awsProfile.empty() ||
        in_settings.m_useInstanceProfile)
    {
        rc = false;
    }
    else
    {
        rs_string cacheKey = GetCacheKey(in_settings, logger);
        std::unordered_map<rs_string, RsCredentials>::iterator credentialItr = s_iamCredentialsCache.find(cacheKey);
        if (credentialItr == s_iamCredentialsCache.end())
        {
            rc = false;
        }
        else
        {
            RsCredentials cachedCredentials = credentialItr->second;

            if (!isNativeAuth
                &&
                (cachedCredentials.GetDbUser().empty() ||
                cachedCredentials.GetDbPassword().empty() ||
                cachedCredentials.GetExpirationTime() == 0 ||
                currentTime > cachedCredentials.GetExpirationTime()))
            {
                // remove invalid cached credentials
                s_iamCredentialsCache.erase(credentialItr);

                rc =  false;
            }
            else
            if (isNativeAuth
                &&
                (cachedCredentials.GetIdpToken().empty() ||
                    cachedCredentials.GetExpirationTime() == 0 ||
                    currentTime > cachedCredentials.GetExpirationTime()))
            {
                RS_LOG(logger)("RsIamHelper::IsValidIamCachedSettings not from cache: currentTime:%ld GetExpirationTime():%ld no token:%d", 
                    currentTime, cachedCredentials.GetExpirationTime(), cachedCredentials.GetIdpToken().empty());

                // remove invalid cached credentials
                s_iamCredentialsCache.erase(credentialItr);

                rc = false;
            }
            else
            {
                /* Update this function every time when an IAM related connection attribute is added */
                rc =
                    s_rsSettings.m_host == in_settings.m_host     &&
                    s_rsSettings.m_username == in_settings.m_username &&
                    s_rsSettings.m_password == in_settings.m_password &&
                    s_rsSettings.m_database == in_settings.m_database &&
                    s_rsSettings.m_sslMode == in_settings.m_sslMode  &&
                    s_rsSettings.m_disableCache == in_settings.m_disableCache &&

                    s_rsSettings.m_authType == in_settings.m_authType &&
                    s_rsSettings.m_dbUser == in_settings.m_dbUser &&
                    s_rsSettings.m_accessKeyID == in_settings.m_accessKeyID &&
                    s_rsSettings.m_secretAccessKey == in_settings.m_secretAccessKey &&
                    s_rsSettings.m_sessionToken == in_settings.m_sessionToken &&
                    s_rsSettings.m_awsRegion == in_settings.m_awsRegion &&
                    s_rsSettings.m_clusterIdentifer == in_settings.m_clusterIdentifer &&
                    s_rsSettings.m_dbGroups == in_settings.m_dbGroups &&
                    s_rsSettings.m_forceLowercase == in_settings.m_forceLowercase &&
                    s_rsSettings.m_userAutoCreate == in_settings.m_userAutoCreate &&
                    s_rsSettings.m_endpointUrl == in_settings.m_endpointUrl &&
                    s_rsSettings.m_stsEndpointUrl == in_settings.m_stsEndpointUrl &&
                    s_rsSettings.m_authProfile == in_settings.m_authProfile &&
                    s_rsSettings.m_stsConnectionTimeout == in_settings.m_stsConnectionTimeout &&

                    s_rsSettings.m_accessDuration == in_settings.m_accessDuration &&
                    s_rsSettings.m_pluginName == in_settings.m_pluginName &&
                    s_rsSettings.m_idpHost == in_settings.m_idpHost &&
                    s_rsSettings.m_idpPort == in_settings.m_idpPort &&
                    s_rsSettings.m_idpTenant == in_settings.m_idpTenant &&
                    s_rsSettings.m_clientSecret == in_settings.m_clientSecret &&
                    s_rsSettings.m_clientId == in_settings.m_clientId &&
                    s_rsSettings.m_scope == in_settings.m_scope &&
                    s_rsSettings.m_idp_response_timeout == in_settings.m_idp_response_timeout &&
                    s_rsSettings.m_login_url == in_settings.m_login_url &&
                    s_rsSettings.m_role_arn == in_settings.m_role_arn &&
                    s_rsSettings.m_web_identity_token == in_settings.m_web_identity_token &&
                    s_rsSettings.m_duration == in_settings.m_duration &&
                    s_rsSettings.m_role_session_name == in_settings.m_role_session_name &&
                    s_rsSettings.m_dbGroupsFilter == in_settings.m_dbGroupsFilter &&
                    s_rsSettings.m_listen_port == in_settings.m_listen_port &&
                    s_rsSettings.m_appId == in_settings.m_appId &&
                    s_rsSettings.m_oktaAppName == in_settings.m_oktaAppName &&
                    s_rsSettings.m_partnerSpid == in_settings.m_partnerSpid &&
                    s_rsSettings.m_loginToRp == in_settings.m_loginToRp &&
                    s_rsSettings.m_preferredRole == in_settings.m_preferredRole &&
                    s_rsSettings.m_sslInsecure == in_settings.m_sslInsecure &&
                    s_rsSettings.m_groupFederation == in_settings.m_groupFederation;
            }
        }
    }

//    paUnlockMutex(s_iam_helper_criticalSection);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamHelper::SetIamCachedSettings(
    const RsCredentials& in_iamCredentials,
    const RsSettings& in_settings,
    RsLogger *logger)
{
    if(!in_settings.m_disableCache) {
        rsLockMutex(s_iam_helper_criticalSection);

        rs_string cacheKey = GetCacheKey(in_settings, logger);
        s_iamCredentialsCache[cacheKey] = in_iamCredentials;

        s_rsSettings = in_settings;

        rsUnlockMutex(s_iam_helper_criticalSection);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamHelper::UpdateConnectionSettingsWithCredentials(
    const RsCredentials& in_credentials,
    RsSettings& settings,
    bool isNativeAuth)
{
    if (!isNativeAuth)
    {
        settings.m_username = in_credentials.GetDbUser();
        settings.m_password = in_credentials.GetDbPassword();

        const rs_string& host = in_credentials.GetHost();
        short port = in_credentials.GetPort();

        if (!host.empty())
        {
            settings.m_host = host;
        }

        if (port != 0)
        {
            settings.m_port = port;
        }
    }
    else
    {
        settings.m_web_identity_token = in_credentials.GetIdpToken();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamHelper::SetIamSettings(
    bool isIAMAuth,
    RS_IAM_CONN_PROPS_INFO *pIamProps,
    RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
    RsSettings& settings,
    RsLogger *logger)
{
    RS_LOG(logger)("RsIamHelper::SetIamSettings");
    rs_string temp;

    settings.m_iamAuth = isIAMAuth;

    if(settings.m_iamAuth == true) {
        SetCommonFederatedAuthSettings(pIamProps, settings);
        settings.m_username = pIamProps->szUser;
        settings.m_password = pIamProps->szPassword;

        std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(settings.m_host, ".");
        rs_string workGroup;
        rs_string acctId;
        rs_string requiredClusterId;
        rs_string requiredAwsRegion;
        if (hostnameTokens.size() >= 6 && (hostnameTokens[3].find("serverless") != rs_string::npos))
            {
                /*Serverless_cluster's host examples:
                e.g., default.518627716765.us-east-1.redshift-serverless.amazonaws.com
                */ 
                workGroup = hostnameTokens[0];
                acctId = hostnameTokens[1];
                settings.m_isServerless = true; 
            }
            else if (hostnameTokens.size() >= 6)
            {
                /*provisioned_cluster_host examples: 
                e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com
                e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com.cn
                */
                requiredClusterId = hostnameTokens[0];
                requiredAwsRegion = hostnameTokens[2];
            }

        std::smatch mProvisioned;
        std::smatch mServerless;

        bool provisionedMatches = std::regex_match(settings.m_host, mProvisioned, hostPattern);
        bool serverlessMatches = std::regex_match(settings.m_host, mServerless, serverlessWorkgroupHostPattern);
        rs_string clusterId;
        rs_string userInputClusterId = pIamProps->szClusterId;

        if(provisionedMatches){
            RS_LOG(logger)("Code flow for regular provisioned cluster");
            clusterId = requiredClusterId;
        }
        else if(serverlessMatches){
            // serverless vanilla
            // do nothing, regular serverless logic flow
            RS_LOG(logger)("Code flow for regular serverless cluster");
        }
        else if(settings.m_isServerless){
            // hostname doesn't match serverless regex but serverless set to true explicitly by user
            // when ready for implementation, remove setting of the isServerless property automatically in parseUrl(),
            // set it here instead
            // currently do nothing as server does not support cname for serverless
            if(!(workGroup.empty())){
                // workgroup specified by user - serverless nlb call
                // check for serverlessAcctId to enter serverless NLB logic flow, for when we implement this for serverless after server side is ready
                // currently do nothing as regular code flow is sufficient
                RS_LOG(logger)("Code flow for nlb serverless cluster");
            }
            else{
                // attempt serverless cname call - currently not supported by server
                // currently sets isCname to true which will be asserted on later, as cname for serverless is not supported yet
                RS_LOG(logger)("Code flow for cname serverless cluster");
                settings.m_isCname = true;
            }
        }
        else {
            // clusterID specified by user - provisioned nlb call
            // regular logic flow, no change needed
            RS_LOG(logger)("Code flow for nlb/cname provisioned cluster");
            clusterId = pIamProps->szClusterId;
            // attempt provisioned cname call
            // cluster id will be fetched upon describing custom domain name
            settings.m_isCname = true;
        }
        
        settings.m_accessKeyID = pIamProps->szAccessKeyID;
        settings.m_secretAccessKey = pIamProps->szSecretAccessKey;
        settings.m_sessionToken = pIamProps->szSessionToken;
        settings.m_accessDuration = pIamProps->lIAMDuration;
        if (settings.m_accessDuration > 3600)
            settings.m_accessDuration = 3600;
        else
        if (settings.m_accessDuration < 900)
            settings.m_accessDuration = 900;

        settings.m_clusterIdentifer = clusterId;
        RS_LOG(logger)("Cluster Identifier:%s", settings.m_clusterIdentifer );
        //settings.m_clusterIdentifer = pIamProps->szClusterId;


        if(!settings.m_isServerless && !settings.m_isCname && (settings.m_clusterIdentifer.empty())){
            RS_LOG(logger)( "Invalid connection property setting. cluster_identifier must be provided when IAM is enabled");
        }

        settings.m_dbUser = pIamProps->szDbUser;
        temp = pIamProps->szDbGroups;
        settings.m_dbGroups = IAMUtils::convertStringToWstring(temp);
        settings.m_awsProfile = pIamProps->szProfile;
        temp = pIamProps->szStsEndpointUrl;
        settings.m_stsEndpointUrl = IAMUtils::convertStringToWstring(temp);

        settings.m_oktaAppName = pIamProps->szAppName;
        settings.m_partnerSpid = pIamProps->szPartnerSpid;
        settings.m_loginToRp = pIamProps->szLoginToRp;
        temp = pIamProps->szIdpHost;
        settings.m_idpHost = IAMUtils::convertStringToWstring(temp);
        settings.m_idpPort = pIamProps->iIdpPort;

        settings.m_idpTenant = pIamProps->szIdpTenant;
        settings.m_clientSecret = pIamProps->szClientSecret;
        settings.m_clientId = pIamProps->szClientId;
        settings.m_scope = pIamProps->szScope;
        settings.m_idp_response_timeout = pIamProps->lIdpResponseTimeout;
        settings.m_login_url = pIamProps->szLoginUrl;
        settings.m_role_arn = pIamProps->szRoleArn;
        //Added the autoCreate keyword from the user input connection string
        settings.m_userAutoCreate = pIamProps->isAutoCreate;

        settings.m_duration = pIamProps->lDuration;
        settings.m_role_session_name = pIamProps->szRoleSessionName;
        settings.m_dbGroupsFilter = pIamProps->szDbGroupsFilter;
        settings.m_listen_port = pIamProps->lListenPort;
        temp = pIamProps->szAppId;
        settings.m_appId = IAMUtils::convertStringToWstring(temp);
        temp = pIamProps->szPreferredRole;
        settings.m_preferredRole = IAMUtils::convertStringToWstring(temp);

        settings.m_useInstanceProfile = pIamProps->isInstanceProfile;
        settings.m_authProfile = pIamProps->szAuthProfile;
        settings.m_disableCache = pIamProps->isDisableCache;
        settings.m_stsConnectionTimeout = pIamProps->iStsConnectionTimeout;

        if(pHttpsProps) {
            settings.m_httpsProxyHost = pHttpsProps->szHttpsHost;
            settings.m_httpsProxyPort = pHttpsProps->iHttpsPort;
            settings.m_httpsProxyUsername = pHttpsProps->szHttpsUser;
            settings.m_httpsProxyPassword = pHttpsProps->szHttpsPassword;
            settings.m_useProxyForIdpAuth = pHttpsProps->isUseProxyForIdp;
        }

        settings.m_groupFederation = pIamProps->isGroupFederation;
    } else { // non-IAM
        settings.m_clusterIdentifer = pIamProps->szClusterId;
        if (pIamProps->szPluginName[0] != '\0' && (_stricmp(pIamProps->szPluginName, PLUGIN_IDP_TOKEN_AUTH) == 0 ||
            _stricmp(pIamProps->szPluginName, PLUGIN_BROWSER_IDC_AUTH) == 0)) {
            SetCommonFederatedAuthSettings(pIamProps, settings);
            settings.m_idpAuthToken = pIamProps->szBasicAuthToken;
            settings.m_idpAuthTokenType = pIamProps->szTokenType;
            settings.m_startUrl = pIamProps->szStartUrl;
            settings.m_idcRegion = pIamProps->szIdcRegion;
            settings.m_disableCache = true; // explicitly disable caching for idc plugins
            settings.m_idcResponseTimeout = pIamProps->lIdcResponseTimeout;
            settings.m_idcClientDisplayName = pIamProps->szIdcClientDisplayName;
        } 
    }
}

void RsIamHelper::SetCommonFederatedAuthSettings(RS_IAM_CONN_PROPS_INFO *pIamProps, RsSettings &settings) {
    settings.m_sslMode = pIamProps->szSslMode;
    settings.m_host = pIamProps->szHost;
    settings.m_port = (pIamProps->szPort[0] != '\0') ? std::stoi(pIamProps->szPort) : 0;
    settings.m_database = pIamProps->szDatabase;
    settings.m_awsRegion = pIamProps->szRegion;
    settings.m_endpointUrl = IAMUtils::convertStringToWstring(pIamProps->szEndpointUrl);
    settings.m_pluginName = IAMUtils::convertStringToWstring(pIamProps->szPluginName);

    if (pIamProps->pszJwt) {
        settings.m_web_identity_token = pIamProps->pszJwt;
    }

    settings.m_sslInsecure = pIamProps->isSslInsecure;
    settings.m_authType = pIamProps->szAuthType;

    settings.m_caPath = pIamProps->szCaPath;
    settings.m_caFile = pIamProps->szCaFile;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string RsIamHelper::GetCacheKey(const RsSettings& in_settings, RsLogger *logger)
{
    rs_string cacheKey("");

#if defined(WIN32) || defined(_WIN64)
    char uname[UNLEN + 1];
    DWORD unameLength = UNLEN + 1;
    if (GetUserNameA(uname, &unameLength) == 0)
    {
        rs_string errMsg = "Failed to retrieve Windows username, error: " + std::to_string(GetLastError());
        RS_LOG(logger)(	"RsIamHelper::GetCacheKey %s",	errMsg.c_str()); 
    }
    else
    {
        cacheKey = rs_string(uname);
    }
#endif

    cacheKey += in_settings.m_username + in_settings.m_password + IAMUtils::convertToUTF8(in_settings.m_pluginName)
        + IAMUtils::convertToUTF8(in_settings.m_idpHost) + std::to_string(in_settings.m_idpPort)
        + IAMUtils::convertToUTF8(in_settings.m_preferredRole);

    return cacheKey;
}

/*======================================================================================*/

char * RsIamHelper::ReadAuthProfile(
    bool isIAMAuth,
    RS_IAM_CONN_PROPS_INFO *pIamProps,
    RS_PROXY_CONN_PROPS_INFO *pHttpsProps,
    RsLogger *logger)
{
    // The contents of the Auth Profile as JSON object
    Aws::Utils::Json::JsonValue profileValue;
    rs_string auth_profile_name = pIamProps->szAuthProfile;
    rs_string accessKey = pIamProps->szAccessKeyID;
    rs_string secretKey = pIamProps->szSecretAccessKey;
    rs_string host = pIamProps->szHost;
    rs_string region = pIamProps->szRegion;

    if(pIamProps->szAccessKeyID[0] == '\0'
         || pIamProps->szSecretAccessKey[0] == '\0')
    {
        rs_string errorMsg = "Missing AccessKeyID or SecretAccessKey for AuthProfile.";
        IAMUtils::ThrowConnectionExceptionWithInfo(errorMsg);
    }

    RsSettings settings;
    RsIamClient iamClient(settings, logger);

    profileValue = Aws::Utils::Json::JsonValue(iamClient.ReadAuthProfile(auth_profile_name, accessKey, secretKey, host, region));

    Aws::Utils::Json::JsonView profileView = profileValue.View();
    if (!profileView.IsNull())
    {
        // Read values
        rs_string connect_string = "";

        Aws::Map<Aws::String, Aws::Utils::Json::JsonView> attributesJsonMap = profileView.GetAllObjects();
        for (auto& attributesItem : attributesJsonMap)
        {
            rs_string key = attributesItem.first;
            rs_string val = attributesItem.second.AsString();

            connect_string += key + "=" + val + ";";
        }

        return strdup(connect_string.c_str());
    }

    return NULL;
}



