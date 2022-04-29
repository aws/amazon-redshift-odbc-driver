#ifdef WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

#include "rslock.h"
#include "RsIamClient.h"
#include "IAMUtils.h"

#include <aws/redshift/model/GetClusterCredentialsRequest.h>
#include <aws/redshift/model/DescribeClustersRequest.h>
#include <aws/redshift/model/Cluster.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/redshiftarcadiacoral/model/GetCredentialsRequest.h>
#include <aws/redshiftarcadiacoral/model/DescribeConfigurationRequest.h>
#include <aws/redshift/model/DescribeAuthenticationProfilesRequest.h>
#include <aws/redshift/model/AuthenticationProfile.h>


using namespace RedshiftODBC;
using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Redshift;
using namespace Aws::Client;
using namespace Aws::Utils;

namespace
{
    // Helpers =====================================================================================
    // Allocation Tag used for memory management in AWS SDK
    static const char* ALLOC_TAG = "RsIamClient::RedshiftIAMAuth";

    /* Types of credentials providers used by the client */

    /************************************************************************/
    /* List of Error Messages to return in PGOConnectionError               */
    /************************************************************************/
    static const rs_string PGO_IAM_ERROR_SSL_DISABLED(
        "IAM Authentication requires SSL connection.");
}

// Static ==========================================================================================
int RsIamClient::s_iamClientCount = 0;
MUTEX_HANDLE RsIamClient::s_criticalSection = rsCreateMutex();

// Public ==========================================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////
RsIamClient::RsIamClient(const RsSettings& in_settings, RsLogger* in_logger) :
m_settings(in_settings),
m_log(in_logger)
{
    RS_LOG(m_log)("RsIamClient::RsIamClient");

    /**
    Initialize some static factories for client, e.g., Crypto, HttpClient
    https://aws.amazon.com/blogs/developer/aws-sdk-for-c-simplified-configuration-and-initialization/
    */
    rsLockMutex(s_criticalSection);
    if (s_iamClientCount == 0)
    {
        Aws::SDKOptions options;
        if(in_logger->traceEnable)
          options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
        Aws::InitAPI(options);
    }
    // Increase the static RsIamClient count, used to InitAPI and ShutdownAPI
    s_iamClientCount++;
    rsUnlockMutex(s_criticalSection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::Connect()
{
    RS_LOG(m_log)("RsIamClient::Connect");

    ValidateConnectionAttributes();
    const rs_string authType = InferCredentialsProvider();

    RS_LOG(m_log)("RsIamClient::Connect IAM AuthType: %s",
        ((authType.empty()) ? (char *)"Default" : (char *)authType.c_str()));

    std::shared_ptr<AWSCredentialsProvider> credentialsProvider;
    IAMConfiguration config = CreateIAMConfiguration(authType);

    if (authType == IAM_AUTH_TYPE_PLUGIN)
    {
        credentialsProvider = IAMFactory::CreatePluginCredentialsProvider(m_log, config);
        if (credentialsProvider)
        {
            static_cast<IAMPluginCredentialsProvider&>(*credentialsProvider)
                .GetConnectionSettings(m_credentials);
        }
    }
    else if (authType == IAM_AUTH_TYPE_PROFILE)
    {
        credentialsProvider = IAMFactory::CreateProfileCredentialsProvider(m_log, config);
        if (credentialsProvider)
        {
            static_cast<IAMProfileCredentialsProvider&>(*credentialsProvider)
                .GetConnectionSettings(m_credentials);
        }
    }
    else if (authType == IAM_AUTH_TYPE_INSTANCE_PROFILE)
    {
        credentialsProvider = IAMFactory::CreateInstanceProfileCredentialsProvider();
    }
    else if (authType == IAM_AUTH_TYPE_STATIC)
    {
        credentialsProvider = IAMFactory::CreateStaticCredentialsProvider(config);
    }
    else
    {
        /* Use the DefaultAwsCredentialsProviderChain */
        credentialsProvider = IAMFactory::CreateDefaultCredentialsProvider();
    }

	// Support serverless Redshift end point
	std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(m_settings.m_host, ".");
	if ((hostnameTokens.size() >= 5) && (hostnameTokens[2].find("serverless") != rs_string::npos))
	{
		// serverless connection
		GetServerlessCredentials(credentialsProvider);
	}
	else
	{
		/* Get the cluster credentials from the Redshift server */
		GetClusterCredentials(credentialsProvider);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
RsCredentials RsIamClient::GetCredentials() const
{
    return m_credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////     
void RsIamClient::SetCredentials(const RsCredentials& in_credentials)
{
    m_credentials = in_credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
RsIamClient::~RsIamClient()
{
    RS_LOG(m_log)("RsIamClient::~RsIamClient");

    rsLockMutex(s_criticalSection);
    s_iamClientCount--;

    // Shutdown the API client used if no more active RsIamClient is available
    if (s_iamClientCount == 0)
    {
        Aws::SDKOptions options;
        Aws::ShutdownAPI(options);
    }

    rsUnlockMutex(s_criticalSection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Model::Endpoint RsIamClient::DescribeCluster(
    const RedshiftClient& in_client, 
    const rs_string& in_clusterId)
{
    Model::DescribeClustersRequest request;
    request.SetClusterIdentifier(in_clusterId);

    Model::DescribeClustersOutcome outcome = in_client.DescribeClusters(request);

    if (!outcome.IsSuccess())
    {
        ThrowExceptionWithError(outcome.GetError());
    }

   const Aws::Vector<Model::Cluster>& clusters = outcome.GetResult().GetClusters();

    if (clusters.empty())
    {
		RS_LOG(m_log)("RsIamClient::DescribeCluster:Failed to describe cluster");

        IAMUtils::ThrowConnectionExceptionWithInfo("Failed to describe cluster.");
    }

    /* return the first endpoint in the clusters */
    const Model::Cluster cluster = clusters[0];
    const Model::Endpoint endpoint = cluster.GetEndpoint();

    if (endpoint.GetAddress().empty() || endpoint.GetPort() == 0)
    {
		RS_LOG(m_log)("RsIamClient::DescribeCluster:Cluster is not fully created yet.");
		IAMUtils::ThrowConnectionExceptionWithInfo("Cluster is not fully created yet.");
    }

    return endpoint;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Aws::RedshiftArcadiaCoralService::Model::Endpoint RsIamClient::DescribeConfiguration(
	const Aws::RedshiftArcadiaCoralService::RedshiftArcadiaCoralServiceClient& in_client)
{
	Aws::RedshiftArcadiaCoralService::Model::DescribeConfigurationRequest request;

	Aws::RedshiftArcadiaCoralService::Model::DescribeConfigurationOutcome outcome =
		in_client.DescribeConfiguration(request);

	if (!outcome.IsSuccess())
	{
		ThrowExceptionWithError(outcome.GetError());
	}

	const Aws::RedshiftArcadiaCoralService::Model::DescribeConfigurationResult result =
		outcome.GetResult();

	const Aws::RedshiftArcadiaCoralService::Model::Endpoint endpoint = result.GetEndpoint();

	if (endpoint.GetAddress().empty() || endpoint.GetPort() == 0)
	{
		IAMUtils::ThrowConnectionExceptionWithInfo("Configuration is not fully created yet.");
	}

	return endpoint;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Model::GetClusterCredentialsOutcome RsIamClient::SendClusterCredentialsRequest(
    const std::shared_ptr<AWSCredentialsProvider>& in_credentialsProvider)
{
    RS_LOG(m_log)("RsIamClient::SendClusterCredentialRequest");

    ClientConfiguration config;
    

    /* infer awsRegion and ClusterId from host name if not provided in the connection string 
       e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com, 
       should be tokenized to at least 6 elements
    */
    std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(m_settings.m_host, ".");
    rs_string inferredClusterId, inferredAwsRegion;
    if (hostnameTokens.size() >= 6)
    {
        /* e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com 
           e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com.cn
        */
        inferredClusterId = hostnameTokens[0];
        inferredAwsRegion = hostnameTokens[2];
    }

    config.region = m_settings.m_awsRegion.empty() ? inferredAwsRegion : m_settings.m_awsRegion;
    config.endpointOverride = IAMUtils::convertToUTF8(m_settings.m_endpointUrl);

#ifndef _WIN32
    // Added CA file to the config for verifying STS server certificate
    if (!m_settings.m_caFile.empty())
    {
        config.caFile = m_settings.m_caFile;
    }
    else
    {
        config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile()); // .GetAsPlatformString()
    }
#endif

    if (!m_settings.m_httpsProxyHost.empty())
    {
        config.proxyHost = m_settings.m_httpsProxyHost;
        config.proxyPort = m_settings.m_httpsProxyPort;
        config.proxyUserName = m_settings.m_httpsProxyUsername;
        config.proxyPassword = m_settings.m_httpsProxyPassword;
    }
  
    RedshiftClient client(in_credentialsProvider, config);

    // Compose the ClusterCredentialRequest object
    Model::GetClusterCredentialsRequest request;

    request.SetClusterIdentifier(m_settings.m_clusterIdentifer.empty() ? 
        inferredClusterId.c_str() : m_settings.m_clusterIdentifer.c_str());

    request.SetDbName(m_settings.m_database);
    request.SetDbUser(m_settings.m_dbUser);
    request.SetAutoCreate(m_settings.m_userAutoCreate);
    if (!IAMUtils::isEmpty(m_settings.m_dbGroups))
    {
        rs_wstring dbGroupsCaseSensitive(m_settings.m_dbGroups);
        if (m_settings.m_forceLowercase) 
        { 
          dbGroupsCaseSensitive = IAMUtils::toLower(dbGroupsCaseSensitive);
        }
        request.SetDbGroups(IAMUtils::TokenizeSetting(dbGroupsCaseSensitive, L","));
    }

    /* If m_credentials contains dbUser, dbGroups, forceLowercase, and autoCreate,
       and if these values are empty in m_settings, use these settings */
    const rs_string& dbUser    = m_credentials.GetDbUser();
	
	const rs_wstring& dbGroups = IAMUtils::convertStringToWstring(m_credentials.GetDbGroups());
    bool forceLowercase = m_credentials.GetForceLowercase();
    bool userAutoCreate           = m_credentials.GetAutoCreate();

    if (m_settings.m_dbUser.empty() && !dbUser.empty())
    {
        request.SetDbUser(dbUser);
    }

    if (IAMUtils::isEmpty(m_settings.m_dbGroups) && !IAMUtils::isEmpty(dbGroups))
    {
        rs_wstring dbGroupsCaseSensitive(dbGroups);
        if (m_settings.m_forceLowercase || forceLowercase) 
        { 
          dbGroupsCaseSensitive = IAMUtils::toLower(dbGroupsCaseSensitive);
        }
        request.SetDbGroups(IAMUtils::TokenizeSetting(dbGroupsCaseSensitive, L","));
    }

    if (!m_settings.m_userAutoCreate)
    {
        request.SetAutoCreate(userAutoCreate);
    }
    

    /* if host is empty describe cluster using cluster id */
    if (m_settings.m_host.empty())
    {
        if (m_settings.m_clusterIdentifer.empty() || m_settings.m_awsRegion.empty())
        {
			RS_LOG(m_log)("RsIamClient::SendClusterCredentialRequest: Can not describe cluster: missing clusterId or region.");

            IAMUtils::ThrowConnectionExceptionWithInfo(
                "Can not describe cluster: missing clusterId or region.");
        }

        Model::Endpoint endpoint = DescribeCluster(client, m_settings.m_clusterIdentifer);
        m_credentials.SetHost(endpoint.GetAddress());
        m_credentials.SetPort(static_cast<short>(endpoint.GetPort()));
    }

	RS_LOG(m_log)("RsIamClient::SendClusterCredentialRequest: Before GetClusterCredentials()");

    // Send the request using RedshiftClient
    Model::GetClusterCredentialsOutcome outcome = client.GetClusterCredentials(request);

	RS_LOG(m_log)("RsIamClient::SendClusterCredentialRequest: After GetClusterCredentials()");

    return outcome;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Aws::RedshiftArcadiaCoralService::Model::GetCredentialsOutcome RsIamClient::SendCredentialsRequest(
	const std::shared_ptr<AWSCredentialsProvider>& in_credentialsProvider)
{
	RS_LOG(m_log)("RsIamClient::SendCredentialsRequest");

	ClientConfiguration config;

	/* infer awsRegion from host name if not provided in the connection string
	e.g., 412074911972.us-east-1-dev.redshift-serverless-dev.amazonaws.com,
	should be tokenized to at least 5 elements
	*/
	std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(m_settings.m_host, ".");
	rs_string inferredAwsRegion;
	if (hostnameTokens.size() >= 5)
	{
		// e.g., 412074911972.us-east-1-dev.redshift-serverless-dev.amazonaws.com
		inferredAwsRegion = hostnameTokens[1];
	}

	config.region = m_settings.m_awsRegion.empty() ? inferredAwsRegion : m_settings.m_awsRegion;
	config.endpointOverride = IAMUtils::convertToUTF8(m_settings.m_endpointUrl);

#ifndef _WIN32
	// Added CA file to the config for verifying STS server certificate
	if (!m_settings.m_caFile.empty())
	{
		config.caFile = m_settings.m_caFile;
	}
	else
	{
		config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile()); // .GetAsPlatformString()
	}
#endif

	if (!m_settings.m_httpsProxyHost.empty())
	{
		config.proxyHost = m_settings.m_httpsProxyHost;
		config.proxyPort = m_settings.m_httpsProxyPort;
		config.proxyUserName = m_settings.m_httpsProxyUsername;
		config.proxyPassword = m_settings.m_httpsProxyPassword;
	}

	Aws::RedshiftArcadiaCoralService::RedshiftArcadiaCoralServiceClient client(in_credentialsProvider, config);

	// Compose the ClusterCredentialRequest object
	Aws::RedshiftArcadiaCoralService::Model::GetCredentialsRequest request;

	request.SetDbName(m_settings.m_database);

	/* if port is 0 describe configuration to get host and port*/
	if (m_settings.m_port == 0)
	{
		if (m_settings.m_awsRegion.empty())
		{
			IAMUtils::ThrowConnectionExceptionWithInfo(
				"Can not describe cluster: missing region.");
		}

		Aws::RedshiftArcadiaCoralService::Model::Endpoint endpoint = DescribeConfiguration(client);
		m_credentials.SetHost(endpoint.GetAddress());
		m_credentials.SetPort(static_cast<short>(endpoint.GetPort()));
	}

	// Send the request using RedshiftClient
	Aws::RedshiftArcadiaCoralService::Model::GetCredentialsOutcome outcome =
		client.GetCredentials(request);

	return outcome;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ProcessClusterCredentialsOutcome(
    const Model::GetClusterCredentialsOutcome& in_outcome)
{
    RS_LOG(m_log)("RsIamClient::ProcessClusterCredentialsOutcome");

    if (in_outcome.IsSuccess())
    {
        /* save the database credentials to the credentials holder */
        const Model::GetClusterCredentialsResult& result = in_outcome.GetResult();
        m_credentials.SetDbUser(result.GetDbUser());
        m_credentials.SetDbPassword(result.GetDbPassword());
        m_credentials.SetExpirationTime(result.GetExpiration().Millis());

        long currentTime = Aws::Utils::DateTime::Now().Millis();

        RS_LOG(m_log)("RsIamClient::ProcessClusterCredentialsOutcome Expiration time (in milli):%d", (result.GetExpiration().Millis() - currentTime));
    }
    else
    {
        ThrowExceptionWithError(in_outcome.GetError());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ProcessServerlessCredentialsOutcome(
	const Aws::RedshiftArcadiaCoralService::Model::GetCredentialsOutcome& in_outcome)
{
	RS_LOG(m_log)("RsIamClient::ProcessServerlessCredentialsOutcome");

	if (in_outcome.IsSuccess())
	{
		/* save the database credentials to the credentials holder */
		const Aws::RedshiftArcadiaCoralService::Model::GetCredentialsResult& result =
			in_outcome.GetResult();
		m_credentials.SetDbUser(result.GetDbUser());
		m_credentials.SetDbPassword(result.GetDbPassword());
		m_credentials.SetExpirationTime(result.GetExpiration().Millis());
	}
	else
	{
		ThrowExceptionWithError(in_outcome.GetError());
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::GetClusterCredentials(
    const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider)
{
    RS_LOG(m_log)("RsIamClient::GetClusterCredentials");
    if (!in_credentialsProvider)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to get cluster credentials due to AWSCredentialsProvider being NULL.");
    }
    ProcessClusterCredentialsOutcome(SendClusterCredentialsRequest(in_credentialsProvider));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::GetServerlessCredentials(
	const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider)
{
	RS_LOG(m_log)("RsIamClient::GetServerlessCredentials");
	if (!in_credentialsProvider)
	{
		IAMUtils::ThrowConnectionExceptionWithInfo(
			"Failed to get serverless credentials due to AWSCredentialsProvider being NULL.");
	}
	ProcessServerlessCredentialsOutcome(SendCredentialsRequest(in_credentialsProvider));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ValidateConnectionAttributes()
{
    RS_LOG(m_log)("RsIamClient::ValidateConnectionAttributes");

    const rs_string& sslMode = m_settings.m_sslMode;

    // For IAM Authentication SSLMode must not be DISABLED.
    // IAM requires an SSL connection to work. Check that 
    // is set to SSL level ALLOW/PREFER or higher.
    bool isSSLModeEnabled = (sslMode == SSL_AUTH_REQUIRE
        || sslMode == SSL_AUTH_VERIFY_CA
        || sslMode == SSL_AUTH_VERIFY_FULL);

    if (!isSSLModeEnabled)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(PGO_IAM_ERROR_SSL_DISABLED);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ThrowExceptionWithError(const Aws::Client::AWSError<RedshiftErrors>& in_error)
{
    RS_LOG(m_log)("RsIamClient::ThrowExceptionWithError");

    const rs_string& exceptionName = in_error.GetExceptionName();
    const rs_string& errorMessage = in_error.GetMessage();

    rs_string fullErrorMsg = exceptionName + ": " + errorMessage;

	RS_LOG(m_log)("RsIamClient::ThrowExceptionWithError:Error %s", (char *)fullErrorMsg.c_str());

    IAMUtils::ThrowConnectionExceptionWithInfo(fullErrorMsg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string RsIamClient::InferCredentialsProvider()
{
    const rs_string& authType = m_settings.m_authType;
    const rs_string& accessKeyId = m_settings.m_accessKeyID;
    const rs_string& secretAccessKey = m_settings.m_secretAccessKey;
    const rs_string& awsProfile = m_settings.m_awsProfile;
    const rs_wstring& pluginName = m_settings.m_pluginName;

    if (!authType.empty())
    {   
        if (authType == IAM_AUTH_TYPE_PROFILE)
        {
            /* If Instance profile is enabled, use instance profile */
            if (m_settings.m_useInstanceProfile)
            {
                return IAM_AUTH_TYPE_INSTANCE_PROFILE;
            }

            /* If AuthType is Profile and connection attribute profile is empty,
            use default credentials provider */
            if (awsProfile.empty())
            {
                return rs_string();
            }
        }
        return authType;
    }

    if (!IAMUtils::isEmpty(pluginName))
    {
        return IAM_AUTH_TYPE_PLUGIN;
    }

    if (m_settings.m_useInstanceProfile)
    {
        return IAM_AUTH_TYPE_INSTANCE_PROFILE;
    }

    if (!awsProfile.empty())
    {
        return IAM_AUTH_TYPE_PROFILE;
    }

    if (!accessKeyId.empty() && !secretAccessKey.empty())
    {
        return IAM_AUTH_TYPE_STATIC;
    }

    return rs_string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMConfiguration RsIamClient::CreateIAMConfiguration(const rs_string& in_authType)
{
    IAMConfiguration config;

    /* Common connection attributes that will be used for all authentication types */
    config.SetDbUser(m_settings.m_dbUser);
    config.SetDbGroups(IAMUtils::convertToUTF8(m_settings.m_dbGroups));
    config.SetForceLowercase(m_settings.m_forceLowercase);
    config.SetAutoCreate(m_settings.m_userAutoCreate);
	config.SetStsEndpointUrl(IAMUtils::convertToUTF8(m_settings.m_stsEndpointUrl));
	config.SetEndpointUrl(IAMUtils::convertToUTF8(m_settings.m_endpointUrl));
	config.SetAuthProfile(m_settings.m_authProfile);


    /* Username and password for Profile and Plugin */
    config.SetUser(m_settings.m_username);
    config.SetPassword(m_settings.m_password);
    
    /* Set Customized CA file for SSL connection */
    config.SetCaFile(m_settings.m_caFile);

    if (in_authType == IAM_AUTH_TYPE_PLUGIN)
    {
        /* Plugin AuthType specific connection attributes */
        rs_wstring pluginName = m_settings.m_pluginName;
        IAMUtils::trim(pluginName);
        pluginName = IAMUtils::toLower(pluginName);
        config.SetPluginName(IAMUtils::convertToUTF8(pluginName));

        config.SetIdpHost(IAMUtils::convertToUTF8(m_settings.m_idpHost));
        config.SetIdpPort(m_settings.m_idpPort);
        config.SetIdpTenant(m_settings.m_idpTenant);
        config.SetClientSecret(m_settings.m_clientSecret);
        config.SetClientId(m_settings.m_clientId);
        config.SetIdpResponseTimeout(m_settings.m_idp_response_timeout);
        config.SetLoginURL(m_settings.m_login_url);
        config.SetListenPort(m_settings.m_listen_port);
        config.SetAppId(IAMUtils::convertToUTF8(m_settings.m_appId));
        config.SetPreferredRole(IAMUtils::convertToUTF8(m_settings.m_preferredRole));
        config.SetSslInsecure(m_settings.m_sslInsecure);
        config.SetPartnerSpId(m_settings.m_partnerSpid);
        config.SetLoginToRp(m_settings.m_loginToRp);
        config.SetAppName(m_settings.m_oktaAppName);
        config.SetDbGroupsFilter(m_settings.m_dbGroupsFilter);
        config.SetRoleARN(m_settings.m_role_arn);
        config.SetWebIdentityToken(m_settings.m_web_identity_token);
        config.SetDuration(m_settings.m_duration);
        config.SetRoleSessionName(m_settings.m_role_session_name);
        config.SetRegion(m_settings.m_awsRegion);
    }
    else if (in_authType == IAM_AUTH_TYPE_PROFILE)
    {
        /* Profile AuthType specific connection attributes */
        config.SetProfileName(m_settings.m_awsProfile);
    }
    else if (in_authType == IAM_AUTH_TYPE_STATIC)
    {
        /* Static AuthType specific connection attributes */
        config.SetAWSCredentials(
            AWSCredentials(
            m_settings.m_accessKeyID, 
            m_settings.m_secretAccessKey, 
            m_settings.m_sessionToken));
    }

    config.SetUseProxyIdpAuth(m_settings.m_useProxyForIdpAuth);

    if (!m_settings.m_httpsProxyHost.empty())
    {
        config.SetUsingHTTPSProxy(true);
        config.SetHTTPSProxyHost(m_settings.m_httpsProxyHost);
        config.SetHTTPSProxyPort(m_settings.m_httpsProxyPort);
        config.SetHTTPSProxyUser(m_settings.m_httpsProxyUsername);
        config.SetHTTPSProxyPassword(m_settings.m_httpsProxyPassword);
    }

    return config;
}

rs_string RsIamClient::ReadAuthProfile(rs_string auth_profile_name,
									rs_string accessKey,
									rs_string secretKey,
									rs_string host,
									rs_string region)
{
	ClientConfiguration config;


	/* infer awsRegion and ClusterId from host name if not provided in the connection string
	e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com,
	should be tokenized to at least 6 elements
	*/
	std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(host, ".");
	rs_string inferredAwsRegion;

	if ((hostnameTokens.size() >= 5) && (hostnameTokens[2].find("serverless") != rs_string::npos))
	{
		// e.g., 412074911972.us-east-1-dev.redshift-serverless-dev.amazonaws.com
		inferredAwsRegion = hostnameTokens[1];
	}
	else
	if (hostnameTokens.size() >= 6)
	{
		/* e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com
		e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com.cn
		*/
		inferredAwsRegion = hostnameTokens[2];
	}

	config.region = region.empty() ? inferredAwsRegion : region;

	if (config.region.empty())
	{
		rs_string errorMsg = "AuthProfile region is not provided or not derived from host";

		IAMUtils::ThrowConnectionExceptionWithInfo(errorMsg, "IAMProfileError");
	}

	// use option values in Auth Profile
	std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credentialsProvider =
		std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
			accessKey,
			secretKey);

	Aws::Redshift::RedshiftClient client(credentialsProvider, config);

	Aws::Redshift::Model::DescribeAuthenticationProfilesRequest request;
	request.SetAuthenticationProfileName(auth_profile_name.c_str());
	Aws::Redshift::Model::DescribeAuthenticationProfilesOutcome outcome =
		client.DescribeAuthenticationProfiles(request);
	Aws::Redshift::Model::DescribeAuthenticationProfilesResult result = outcome.GetResult();
	const Aws::Vector<Aws::Redshift::Model::AuthenticationProfile>& profilesList =
		result.GetAuthenticationProfiles();

	if (profilesList.empty())
	{

		rs_string errorMsg = "AuthProfile " + auth_profile_name + " not found.";

		IAMUtils::ThrowConnectionExceptionWithInfo(errorMsg, "IAMProfileError");
	}

	const Aws::Redshift::Model::AuthenticationProfile& authProfile = profilesList[0];

	return authProfile.GetAuthenticationProfileContent();
}
