#ifdef WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif

#include "rslock.h"
#include "RsIamClient.h"
#include "IAMUtils.h"
#include "RsSettings.h"
#include <rslog.h>

#include <ares.h>
#include <ares_dns.h>
#if defined LINUX
#include <arpa/nameser.h>
#include <netdb.h>    // Include netdb.h for the complete struct hostent definition
#endif

#include <aws/redshift/model/GetClusterCredentialsRequest.h>
#include <aws/redshift/model/GetClusterCredentialsWithIAMRequest.h>
#include <aws/redshift/model/DescribeClustersRequest.h>
#include <aws/redshift/model/Cluster.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/redshift-serverless/model/GetCredentialsRequest.h>
//#include <aws/RedshiftServerless/model/DescribeConfigurationRequest.h>
#include <aws/redshift/model/DescribeAuthenticationProfilesRequest.h>
#include <aws/redshift/model/AuthenticationProfile.h>
#include <aws/redshift/model/DescribeCustomDomainAssociationsRequest.h>
#include <aws/redshift/model/DescribeCustomDomainAssociationsResult.h>
#include <aws/redshift-serverless/model/GetCredentialsRequest.h>


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
    static const rs_string PGO_FEDERATED_NON_IAM_ERROR_SSL_DISABLED(
        "Authentication must use an SSL connection.");
}

// Static ==========================================================================================
int RsIamClient::s_iamClientCount = 0;
MUTEX_HANDLE RsIamClient::s_criticalSection = rsCreateMutex();

// Public ==========================================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////
RsIamClient::RsIamClient(const RsSettings& in_settings) :
m_settings(in_settings)
{
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::RsIamClient");

    /**
    Initialize some static factories for client, e.g., Crypto, HttpClient
    https://aws.amazon.com/blogs/developer/aws-sdk-for-c-simplified-configuration-and-initialization/
    */
    rsLockMutex(s_criticalSection);
    if (s_iamClientCount == 0)
    {
        Aws::SDKOptions options;
        options.loggingOptions.crt_logger_create_fn =
        [](){ return Aws::MakeShared<Aws::Utils::Logging::DefaultCRTLogSystem>("CRTLogSystem", Aws::Utils::Logging::LogLevel::Off); };

        Aws::InitAPI(options);
    }
    // Increase the static RsIamClient count, used to InitAPI and ShutdownAPI
    s_iamClientCount++;
    rsUnlockMutex(s_criticalSection);
}

bool static isIdcOrNativeIdpPlugin(const rs_string &pluginName) {
    return IAMUtils::isEqual(IAMUtils::convertStringToWstring(pluginName),
                             IAMUtils::convertCharStringToWstring(IAM_PLUGIN_BROWSER_AZURE_OAUTH2), false) ||
           IAMUtils::isEqual(IAMUtils::convertStringToWstring(pluginName),
                             IAMUtils::convertCharStringToWstring(PLUGIN_IDP_TOKEN_AUTH), false) ||
           IAMUtils::isEqual(IAMUtils::convertStringToWstring(pluginName),
                             IAMUtils::convertCharStringToWstring(PLUGIN_BROWSER_IDC_AUTH), false);
}

bool static isIdcPlugin(const rs_string &pluginName) {
    return IAMUtils::isEqual(IAMUtils::convertStringToWstring(pluginName),
                             IAMUtils::convertCharStringToWstring(PLUGIN_IDP_TOKEN_AUTH), false)||
           IAMUtils::isEqual(IAMUtils::convertStringToWstring(pluginName),
                             IAMUtils::convertCharStringToWstring(PLUGIN_BROWSER_IDC_AUTH), false);
}

rs_string RsIamClient::GetPluginName() {
    rs_wstring pluginName = m_settings.m_pluginName;
    IAMUtils::trim(pluginName);
    pluginName = IAMUtils::toLower(pluginName);
    return IAMUtils::convertToUTF8(pluginName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::Connect()
{
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::Connect");
	bool isNativeAuth = false;

    const rs_string authType = InferCredentialsProvider();
    ValidateConnectionAttributes(authType);

    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::Connect IAM AuthType: %s",
        ((authType.empty()) ? (char *)"Default" : (char *)authType.c_str()));

    std::shared_ptr<AWSCredentialsProvider> credentialsProvider;
    IAMConfiguration config = CreateIAMConfiguration(authType);

    if (authType == IAM_AUTH_TYPE_PLUGIN)
    {
        if (isIdcPlugin(config.GetPluginName()) && m_settings.m_iamAuth) {
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "You can not use this authentication plugin with IAM enabled.");
        }

        credentialsProvider = IAMFactory::CreatePluginCredentialsProvider(config);
        if (credentialsProvider)
        {
            if (isIdcOrNativeIdpPlugin(config.GetPluginName())) {
                isNativeAuth = true;
            }

            if (!isNativeAuth) {
                static_cast<IAMPluginCredentialsProvider &>(*credentialsProvider)
                    .GetConnectionSettings(m_credentials);
            }
        }
    }
    else if (authType == IAM_AUTH_TYPE_PROFILE)
    {
        credentialsProvider = IAMFactory::CreateProfileCredentialsProvider(config);
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

	if (!isNativeAuth)
	{
		// Support serverless Redshift end point
		if (m_settings.m_isServerless)
		{
			// serverless connection
			GetServerlessCredentials(credentialsProvider);
		}
		else if (m_settings.m_groupFederation)
		{
			GetClusterCredentialsWithIAM(credentialsProvider);
		}
		else
		{
			/* Get the cluster credentials from the Redshift server */
			GetClusterCredentials(credentialsProvider);
		}
	}
    else
    {
        if (isIdcPlugin(config.GetPluginName())) {
            rs_string auth_token =
                static_cast<NativePluginCredentialsProvider &>(*credentialsProvider).GetAuthToken();
            m_credentials.SetIdpToken(auth_token);
        } else { // native idp AzureAD plugin
            /* Validate that all required arguments for plugin are provided */
            static_cast<IAMJwtPluginCredentialsProvider &>(*credentialsProvider)
                .ValidateArgumentsMap();
            rs_string idp_token =
                static_cast<IAMJwtPluginCredentialsProvider &>(*credentialsProvider).GetJwtAssertion();
            m_credentials.SetIdpToken(idp_token);
        }
        m_credentials.SetFixExpirationTime();
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
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::~RsIamClient");

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
		RS_LOG_ERROR("IAMCLNT", "RsIamClient::DescribeCluster:Failed to describe cluster");

        IAMUtils::ThrowConnectionExceptionWithInfo("Failed to describe cluster.");
    }

    /* return the first endpoint in the clusters */
    const Model::Cluster cluster = clusters[0];
    const Model::Endpoint endpoint = cluster.GetEndpoint();

    if (endpoint.GetAddress().empty() || endpoint.GetPort() == 0)
    {
		RS_LOG_ERROR("IAMCLNT", "RsIamClient::DescribeCluster:Cluster is not fully created yet.");
		IAMUtils::ThrowConnectionExceptionWithInfo("Cluster is not fully created yet.");
    }

    return endpoint;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/* Aws::RedshiftArcadiaCoralService::Model::Endpoint RsIamClient::DescribeConfiguration(
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
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
Model::GetClusterCredentialsOutcome RsIamClient::SendClusterCredentialsRequest(
    const std::shared_ptr<AWSCredentialsProvider>& in_credentialsProvider)
{
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendClusterCredentialRequest");

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

    if(m_settings.m_isCname && inferredAwsRegion.empty() ){
    config.region = IAMUtils().GetAwsRegionFromCname(m_settings.m_host);
    }
    else
    {
    config.region = m_settings.m_awsRegion.empty() ? inferredAwsRegion : m_settings.m_awsRegion;
    config.endpointOverride = IAMUtils::convertToUTF8(m_settings.m_endpointUrl);
    }

	if (m_settings.m_stsConnectionTimeout > 0)
	{
		config.httpRequestTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
		config.connectTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
		config.requestTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
	}

	RS_LOG_DEBUG("IAMCLNT",
		"RsIamClient::SendClusterCredentialRequest",
		"httpRequestTimeoutMs: %ld, connectTimeoutMs: %ld, requestTimeoutMs: %ld",
		config.httpRequestTimeoutMs,
		config.connectTimeoutMs,
		config.requestTimeoutMs);


#ifndef _WIN32
    // Added CA file to the config for verifying STS server certificate
    if (!m_settings.m_caFile.empty())
    {
        config.caFile = m_settings.m_caFile;
    } else if (!m_settings.m_caPath.empty()) {
        config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile(m_settings.m_caPath));
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
  
    Aws::Redshift::RedshiftClient client(in_credentialsProvider, config);

    // Compose the ClusterCredentialRequest object
    Model::GetClusterCredentialsRequest request;
    if(m_settings.m_isCname) {
        request.SetCustomDomainName(m_settings.m_host.c_str());
    }
    else {
        request.SetClusterIdentifier(m_settings.m_clusterIdentifer.empty() ? 
        inferredClusterId.c_str() : m_settings.m_clusterIdentifer.c_str());
    }

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
    
    /*
     If we know this is a custom cluster name, we need to fetch the clusterId associated with it
    */
    Model::GetClusterCredentialsOutcome outcome;
    rs_string tempClusterIdentifier;

    try {
        if (m_settings.m_isCname) {
            Model::DescribeCustomDomainAssociationsRequest describeRequest;
            describeRequest.SetCustomDomainName(m_settings.m_host);

            const Model::DescribeCustomDomainAssociationsOutcome& describeResponseOutcome = client.DescribeCustomDomainAssociations(describeRequest);
            if (describeResponseOutcome.IsSuccess()) {
                const Model::DescribeCustomDomainAssociationsResult& describeResponse = describeResponseOutcome.GetResult();
                const auto& associations = describeResponse.GetAssociations();
                if (!associations.empty()) {
                    const auto& certificateAssociations = associations[0].GetCertificateAssociations();
                    if (!certificateAssociations.empty()) {
                        tempClusterIdentifier = certificateAssociations[0].GetClusterIdentifier();
                    }
                    else{
                        RS_LOG_ERROR("IAMCLNT", "RsIamClient::SendClusterCredentialRequest:CNAME Feature: No certificateAssociations list Found!");
                       IAMUtils::ThrowConnectionExceptionWithInfo("CNAME FEATURE::No certificateAssociations list Found!");
                    }
                }
                else{
                    RS_LOG_ERROR("IAMCLNT", "RsIamClient::SendClusterCredentialRequest:CNAME Feature: No Associations list Found!");
                    IAMUtils::ThrowConnectionExceptionWithInfo("CNAME FEATURE::No Associations list Found!");
                }

            } 
            else {
                RS_LOG_DEBUG("IAMCLNT", "Failed to describe custom domain associations. Assuming cluster incorrectly classified as cname, setting cluster identifier directly.");
                
                request.SetCustomDomainName(""); // Clear the custom domain name
                request.SetClusterIdentifier(m_settings.m_clusterIdentifer.empty() ? inferredClusterId.c_str() : m_settings.m_clusterIdentifer.c_str()); // Set the cluster identifier instead
            }
        }
    
        /* if host is empty describe cluster using cluster id */
        if (m_settings.m_host.empty() || m_settings.m_port == 0) {
            if (m_settings.m_clusterIdentifer.empty() && tempClusterIdentifier.empty()) {
                RS_LOG_ERROR("IAMCLNT", "RsIamClient::SendClusterCredentialRequest: Can not describe cluster: missing clusterId or region.");
                IAMUtils::ThrowConnectionExceptionWithInfo("Can not describe cluster: missing clusterId or region.");
            }

            Model::Endpoint endpoint = DescribeCluster(client, tempClusterIdentifier.empty() ? m_settings.m_clusterIdentifer : tempClusterIdentifier);
            m_credentials.SetHost(endpoint.GetAddress());
            m_credentials.SetPort(static_cast<short>(endpoint.GetPort()));
        }

        RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendClusterCredentialRequest: Before GetClusterCredentials()");

        // Send the request using RedshiftClient
        outcome = client.GetClusterCredentials(request);

        RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendClusterCredentialRequest: After GetClusterCredentials()");
    } 
    catch (const Aws::Client::AWSError<Aws::Redshift::RedshiftErrors>& ex) {
        RS_LOG_DEBUG("IAMCLNT", "Failed to call GetClusterCredentials. Exception: %s", ex.GetMessage().c_str());
        throw ex;
    }

    return outcome;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Model::GetClusterCredentialsWithIAMOutcome RsIamClient::SendClusterCredentialsWithIAMRequest(
	const std::shared_ptr<AWSCredentialsProvider>& in_credentialsProvider)
{
	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendClusterCredentialsWithIAMRequest");

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

    if(m_settings.m_isCname && inferredAwsRegion.empty()){
    config.region = IAMUtils().GetAwsRegionFromCname(m_settings.m_host);
    }
    else
    {
	config.region = m_settings.m_awsRegion.empty() ? inferredAwsRegion : m_settings.m_awsRegion;
	config.endpointOverride = IAMUtils::convertToUTF8(m_settings.m_endpointUrl);
    }

	if (m_settings.m_stsConnectionTimeout > 0)
	{
		config.httpRequestTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
		config.connectTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
		config.requestTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
	}

	RS_LOG_DEBUG("IAMCLNT",
		"RsIamClient::SendClusterCredentialsWithIAMRequest",
		"httpRequestTimeoutMs: %ld, connectTimeoutMs: %ld, requestTimeoutMs: %ld",
		config.httpRequestTimeoutMs,
		config.connectTimeoutMs,
		config.requestTimeoutMs);


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
	Model::GetClusterCredentialsWithIAMRequest request;
    if(m_settings.m_isCname) {
        //suppose to call the request.SetCustomDomainName() API but its not available.
        //request.SetCustomDomainName();
        request.SetCustomDomainName(m_settings.m_host.c_str());
    }
    else {
        request.SetClusterIdentifier(m_settings.m_clusterIdentifer.empty() ?
		inferredClusterId.c_str() : m_settings.m_clusterIdentifer.c_str());
    }
	

	request.SetDbName(m_settings.m_database);
	// m_accessDuration setting corresponds to IAMDuration option
	request.SetDurationSeconds(m_settings.m_accessDuration);

     /*
     If we know this is a custom cluster name, we need to fetch the clusterId associated with it
    */
    Model::GetClusterCredentialsWithIAMOutcome outcome;
    rs_string tempClusterIdentifierIAM;

    try {
        if (m_settings.m_isCname) {
            Model::DescribeCustomDomainAssociationsRequest describeRequest;
            describeRequest.SetCustomDomainName(m_settings.m_host);

            const Model::DescribeCustomDomainAssociationsOutcome& describeResponseOutcome = client.DescribeCustomDomainAssociations(describeRequest);
            if (describeResponseOutcome.IsSuccess()) {
                const Model::DescribeCustomDomainAssociationsResult& describeResponse = describeResponseOutcome.GetResult();
                const auto& associations = describeResponse.GetAssociations();
                if (!associations.empty()) {
                    const auto& certificateAssociations = associations[0].GetCertificateAssociations();
                    if (!certificateAssociations.empty()) {
                        tempClusterIdentifierIAM = certificateAssociations[0].GetClusterIdentifier();
                    }
                    else{
                        RS_LOG_ERROR("IAMCLNT", "RsIamClient::SendClusterCredentialWithIAMRequest:CNAME Feature: No certificateAssociations list Found!");
                       IAMUtils::ThrowConnectionExceptionWithInfo("CNAME FEATURE::No certificateAssociations list Found!");
                    }
                }
                else{
                        RS_LOG_ERROR("IAMCLNT", "RsIamClient::SendClusterCredentialWithIAMRequest:CNAME Feature: No Associations list Found!");
                       IAMUtils::ThrowConnectionExceptionWithInfo("CNAME FEATURE::No Associations list Found!");
                }
            } 
            else {
                RS_LOG_DEBUG("IAMCLNT", "Failed to describe custom domain associations. Assuming cluster incorrectly classified as cname, setting cluster identifier directly.");

                request.SetCustomDomainName(""); // Clear the custom domain name
                request.SetClusterIdentifier(m_settings.m_clusterIdentifer.empty() ? inferredClusterId.c_str() : m_settings.m_clusterIdentifer.c_str()); // Set the cluster identifier instead
            }
        }
        /* if host is empty describe cluster using cluster id */
        if (m_settings.m_host.empty() || m_settings.m_port==0) {
            if (m_settings.m_clusterIdentifer.empty() && tempClusterIdentifierIAM.empty()) {
                RS_LOG_ERROR("IAMCLNT", "RsIamClient::SendClusterCredentialsithIAMRequest: Can not describe cluster: missing clusterId or region.");
                IAMUtils::ThrowConnectionExceptionWithInfo("Can not describe cluster: missing clusterId or region.");
            }

            Model::Endpoint endpoint = DescribeCluster(client, tempClusterIdentifierIAM.empty() ? m_settings.m_clusterIdentifer : tempClusterIdentifierIAM);
            m_credentials.SetHost(endpoint.GetAddress());
            m_credentials.SetPort(static_cast<short>(endpoint.GetPort()));
        }

        RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendClusterCredentialswithIAMRequest: Before GetClusterCredentialswithIAM()");

        // Send the request using RedshiftClient
        outcome = client.GetClusterCredentialsWithIAM(request);

        RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendClusterCredentialswithIAMRequest: After GetClusterCredentialswithIAM()");
    } 
    catch (const Aws::Client::AWSError<Aws::Redshift::RedshiftErrors>& ex) {
        RS_LOG_DEBUG("IAMCLNT", "Failed to call GetClusterCredentialsWithIAM. Exception:%s", ex.GetMessage().c_str());
        throw ex;
    }

    return outcome;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Aws::RedshiftServerless::Model::GetCredentialsOutcome RsIamClient::SendCredentialsRequest(
	const std::shared_ptr<AWSCredentialsProvider>& in_credentialsProvider)
{
	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::SendCredentialsRequest");

	ClientConfiguration config;

	/* infer awsRegion from host name if not provided in the connection string
	e.g., default.518627716765.us-east-1.redshift-serverless.amazonaws.com,
	should be tokenized to at least 6 elements
	*/
	std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(m_settings.m_host, ".");
	rs_string inferredAwsRegion;
	rs_string inferredWorkgroup;

	if (hostnameTokens.size() >= 6)
	{
		// e.g., default.518627716765.us-east-1.redshift-serverless.amazonaws.com
		inferredWorkgroup = hostnameTokens[0];
		inferredAwsRegion = hostnameTokens[2];
	}

	config.region = m_settings.m_awsRegion.empty() ? inferredAwsRegion : m_settings.m_awsRegion;
    RS_LOG_DEBUG("IAMCLNT",
    "configured region=%s inferredAwsRegion=%s => config.region=%s", 
    m_settings.m_awsRegion.c_str(), inferredAwsRegion.c_str(), config.region.c_str());
    
    if(m_settings.m_isCname && config.region.empty()){
        config.region = IAMUtils().GetAwsRegionFromCname(m_settings.m_host);
        RS_LOG_DEBUG("IAMCLNT", "GetAwsRegionFromCname->%s", config.region.c_str());
    }
	config.endpointOverride = IAMUtils::convertToUTF8(m_settings.m_endpointUrl);
    
	if (m_settings.m_stsConnectionTimeout > 0)
	{
		config.httpRequestTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
		config.connectTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
		config.requestTimeoutMs = m_settings.m_stsConnectionTimeout * 1000;
	}

	RS_LOG_DEBUG("IAMCLNT",
		"RsIamClient::SendCredentialRequest",
		"httpRequestTimeoutMs: %ld, connectTimeoutMs: %ld, requestTimeoutMs: %ld",
		config.httpRequestTimeoutMs,
		config.connectTimeoutMs,
		config.requestTimeoutMs);


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

	Aws::RedshiftServerless::RedshiftServerlessClient client(in_credentialsProvider, config);

	// Compose the ClusterCredentialRequest object
	Aws::RedshiftServerless::Model::GetCredentialsRequest request;
    
    if(m_settings.m_isCname){
        request.SetCustomDomainName(m_settings.m_host.c_str());
        const Aws::String& customDomainName = request.GetCustomDomainName();
        RS_LOG_DEBUG("IAMCLNT", "Custom Domain Name: %s", customDomainName.c_str());
    }         

	request.SetDbName(m_settings.m_database);
    if(!m_settings.m_workGroup.empty()){
        request.SetWorkgroupName(m_settings.m_workGroup);
    }
    else
    {
        request.SetWorkgroupName(inferredWorkgroup);
    }
	/* if port is 0 describe configuration to get host and port*/
	if (m_settings.m_port == 0)
	{
		if (m_settings.m_awsRegion.empty())
		{
			IAMUtils::ThrowConnectionExceptionWithInfo(
				"Can not describe cluster: missing region.");
		}

/*		Aws::RedshiftArcadiaCoralService::Model::Endpoint endpoint = DescribeConfiguration(client);
		m_credentials.SetHost(endpoint.GetAddress());
		m_credentials.SetPort(static_cast<short>(endpoint.GetPort()));
*/
	}

	// Send the request using RedshiftClient
	Aws::RedshiftServerless::Model::GetCredentialsOutcome outcome =
		client.GetCredentials(request);

	return outcome;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ProcessClusterCredentialsOutcome(
    const Model::GetClusterCredentialsOutcome& in_outcome)
{
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ProcessClusterCredentialsOutcome");

    if (in_outcome.IsSuccess())
    {
        /* save the database credentials to the credentials holder */
        const Model::GetClusterCredentialsResult& result = in_outcome.GetResult();
        m_credentials.SetDbUser(result.GetDbUser());
        m_credentials.SetDbPassword(result.GetDbPassword());
        m_credentials.SetExpirationTime(result.GetExpiration().Millis());

        long currentTime = Aws::Utils::DateTime::Now().Millis();

        RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ProcessClusterCredentialsOutcome Expiration time (in milli):%d", (result.GetExpiration().Millis() - currentTime));
    }
    else
    {
        ThrowExceptionWithError(in_outcome.GetError());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ProcessClusterCredentialsWithIAMOutcome(
	const Model::GetClusterCredentialsWithIAMOutcome& in_outcome)
{
	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ProcessClusterCredentialsWithIAMOutcome");

	if (in_outcome.IsSuccess())
	{
		/* save the database credentials to the credentials holder */
		const Model::GetClusterCredentialsWithIAMResult& result = in_outcome.GetResult();
		m_credentials.SetDbUser(result.GetDbUser());
		m_credentials.SetDbPassword(result.GetDbPassword());
		m_credentials.SetExpirationTime(result.GetExpiration().Millis());

		long currentTime = Aws::Utils::DateTime::Now().Millis();

		RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ProcessClusterCredentialsWithIAMOutcome Expiration time (in milli):%d", (result.GetExpiration().Millis() - currentTime));
	}
	else
	{
		ThrowExceptionWithError(in_outcome.GetError());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ProcessServerlessCredentialsOutcome(
	const Aws::RedshiftServerless::Model::GetCredentialsOutcome& in_outcome)
{
	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ProcessServerlessCredentialsOutcome");

	if (in_outcome.IsSuccess())
	{
		/* save the database credentials to the credentials holder */
		const Aws::RedshiftServerless::Model::GetCredentialsResult& result =
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
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::GetClusterCredentials");
    if (!in_credentialsProvider)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to get cluster credentials due to AWSCredentialsProvider being NULL.");
    }
    ProcessClusterCredentialsOutcome(SendClusterCredentialsRequest(in_credentialsProvider));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::GetClusterCredentialsWithIAM(
	const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider)
{
	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::GetClusterCredentialsWithIAM");
	if (!in_credentialsProvider)
	{
		IAMUtils::ThrowConnectionExceptionWithInfo(
			"Failed to get cluster credentials due to AWSCredentialsProvider being NULL.");
	}
	ProcessClusterCredentialsWithIAMOutcome(SendClusterCredentialsWithIAMRequest(in_credentialsProvider));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::GetServerlessCredentials(
	const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider)
{
	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::GetServerlessCredentials");
	if (!in_credentialsProvider)
	{
		IAMUtils::ThrowConnectionExceptionWithInfo(
			"Failed to get serverless credentials due to AWSCredentialsProvider being NULL.");
	}
	ProcessServerlessCredentialsOutcome(SendCredentialsRequest(in_credentialsProvider));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ValidateConnectionAttributes(const rs_string &in_authType)
{
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ValidateConnectionAttributes");

    const rs_string& sslMode = m_settings.m_sslMode;

    // For IAM Authentication SSLMode must not be DISABLED.
    // IAM requires an SSL connection to work. Check that 
    // is set to SSL level ALLOW/PREFER or higher.
    bool isSSLModeEnabled = (sslMode == SSL_AUTH_REQUIRE
        || sslMode == SSL_AUTH_VERIFY_CA
        || sslMode == SSL_AUTH_VERIFY_FULL);

    if (!isSSLModeEnabled)
    {
        if(in_authType == IAM_AUTH_TYPE_PLUGIN && isIdcOrNativeIdpPlugin(GetPluginName())) {
            IAMUtils::ThrowConnectionExceptionWithInfo(PGO_FEDERATED_NON_IAM_ERROR_SSL_DISABLED);
        }
        IAMUtils::ThrowConnectionExceptionWithInfo(PGO_IAM_ERROR_SSL_DISABLED);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RsIamClient::ThrowExceptionWithError(const Aws::Client::AWSError<RedshiftErrors>& in_error)
{
    RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ThrowExceptionWithError");

    const rs_string& exceptionName = in_error.GetExceptionName();
    const rs_string& errorMessage = in_error.GetMessage();

    rs_string fullErrorMsg = exceptionName + ": " + errorMessage;

	RS_LOG_DEBUG("IAMCLNT", "RsIamClient::ThrowExceptionWithError:Error %s", (char *)fullErrorMsg.c_str());

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
	config.SetStsConnectionTimeout(m_settings.m_stsConnectionTimeout * 1000);

    /* Username and password for Profile and Plugin */
    config.SetUser(m_settings.m_username);
    config.SetPassword(m_settings.m_password);
    
    /* Set Customized CA file for SSL connection */
    config.SetCaFile(m_settings.m_caFile);

    if (in_authType == IAM_AUTH_TYPE_PLUGIN)
    {
        /* Plugin AuthType specific connection attributes */
        config.SetPluginName(GetPluginName());

        config.SetIdpHost(IAMUtils::convertToUTF8(m_settings.m_idpHost));
        config.SetIdpPort(m_settings.m_idpPort);
        config.SetIdpTenant(m_settings.m_idpTenant);
        config.SetClientSecret(m_settings.m_clientSecret);
        config.SetClientId(m_settings.m_clientId);
		config.SetScope(m_settings.m_scope);
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
        config.SetIdpAuthToken(m_settings.m_idpAuthToken);
        config.SetIdpAuthTokenType(m_settings.m_idpAuthTokenType);
        config.SetRegion(m_settings.m_awsRegion);
        config.SetIssuerUrl(m_settings.m_issuerUrl);
        config.SetIdcRegion(m_settings.m_idcRegion);
        config.SetIdcClientDisplayName(m_settings.m_idcClientDisplayName);
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