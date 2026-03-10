#include "IdpTokenAuthPlugin.h"

// AWS SDK includes for GetIdentityCenterAuthToken API
#include <aws/redshift/RedshiftClient.h>
#include <aws/redshift/model/GetIdentityCenterAuthTokenRequest.h>
#include <aws/redshift-serverless/RedshiftServerlessClient.h>
#include <aws/redshift-serverless/model/GetIdentityCenterAuthTokenRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

using namespace Redshift::IamSupport;

IdpTokenAuthPlugin::IdpTokenAuthPlugin(
    const IAMConfiguration &in_config,
    const std::map<rs_string, rs_string> &in_argsMap)
    : NativePluginCredentialsProvider(in_config, in_argsMap) {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::IdpTokenAuthPlugin");
    InitArgumentsMap();
}

IdpTokenAuthPlugin::~IdpTokenAuthPlugin() {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::~IdpTokenAuthPlugin");
}

rs_string IdpTokenAuthPlugin::GetAuthToken() {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::GetAuthToken");
    
    // Step 1: Validate the arguments map to ensure proper parameters are provided
    ValidateArgumentsMap();
    
    // Step 2: Determine which authentication flow to use
    if (IsUsingIdentityEnhancedCredentials()) {
        // Identity-enhanced credentials flow: exchange AWS credentials for subject token
        RS_LOG_DEBUG("IAMIDP", "Using identity-enhanced credentials flow");
        return GetSubjectToken();
    }
    
    // Direct token flow: return the pre-obtained token provided by the user
    RS_LOG_DEBUG("IAMIDP", "Using direct token flow");
    return m_argsMap[KEY_IDP_AUTH_TOKEN];
}

void IdpTokenAuthPlugin::InitArgumentsMap() {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::InitArgumentsMap");

    // Read direct token flow parameters
    const rs_string authToken = m_config.GetIdpAuthToken();
    const rs_string tokenType = m_config.GetIdpAuthTokenType();
    if (!IAMUtils::rs_trim(authToken).empty()) {
        m_argsMap[KEY_IDP_AUTH_TOKEN] = authToken;
    }
    if (!IAMUtils::rs_trim(tokenType).empty()) {
        m_argsMap[KEY_IDP_AUTH_TOKEN_TYPE] = tokenType;
        RS_LOG_DEBUG("IAMIDP", "Setting token_type=%s", tokenType.c_str());
    }

    // Read identity-enhanced credentials flow parameters
    // These are used to exchange AWS credentials for a subject token via GetIdentityCenterAuthToken API
    m_accessKeyId = IAMUtils::rs_trim(m_config.GetAccessId());
    m_secretAccessKey = IAMUtils::rs_trim(m_config.GetSecretKey());
    m_sessionToken = IAMUtils::rs_trim(m_config.GetSessionToken());
    m_endpointUrl = IAMUtils::rs_trim(m_config.GetEndpointUrl());

    // Log credential initialization status
    RS_LOG_DEBUG("IAMIDP", "Identity-enhanced credentials initialized: AccessKeyID=%s, SecretAccessKey=%s, SessionToken=%s, EndpointUrl=%s",
        m_accessKeyId.empty() ? "not provided" : "provided",
        m_secretAccessKey.empty() ? "not provided" : "provided",
        m_sessionToken.empty() ? "not provided" : "provided",
        m_endpointUrl.empty() ? "not provided" : "provided");
}

bool IdpTokenAuthPlugin::IsUsingIdentityEnhancedCredentials() const {
    // Return true if AccessKeyID, SecretAccessKey, and SessionToken are provided
    return !m_accessKeyId.empty() && !m_secretAccessKey.empty() && !m_sessionToken.empty();
}

void IdpTokenAuthPlugin::ValidateArgumentsMap() {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::ValidateArgumentsMap");

    // Check if direct token flow parameters are provided
    bool hasDirectToken = m_argsMap.count(KEY_IDP_AUTH_TOKEN) > 0 ||
                          m_argsMap.count(KEY_IDP_AUTH_TOKEN_TYPE) > 0;
    
    // Check if identity-enhanced credentials flow parameters are provided
    bool hasIdentityEnhanced = IsUsingIdentityEnhancedCredentials();

    // Check for conflicting parameters - both flows cannot be used simultaneously
    if (hasDirectToken && hasIdentityEnhanced) {
        RS_LOG_ERROR("IAMIDP", 
            "IdC authentication failed: conflicting parameters - both direct token and identity-enhanced credentials provided");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: Cannot provide both direct token parameters "
            "(token, token_type) and identity-enhanced credentials "
            "(AccessKeyID, SecretAccessKey, SessionToken).");
    }

    if (!hasDirectToken && !hasIdentityEnhanced){
        RS_LOG_ERROR("IAMIDP", 
            "IdC authentication failed: neither direct token or identity-enhanced credentials provided");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: Must provide either direct token parameters "
            "(token, token_type) or identity-enhanced credentials "
            "(AccessKeyID, SecretAccessKey, SessionToken).");
    }

    // Validate direct token flow
    if (hasDirectToken) {
        if (!m_argsMap.count(KEY_IDP_AUTH_TOKEN)) {
            RS_LOG_ERROR("IAMIDP",
                "IdC authentication failed: token needs to be provided in connection params");
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "IdC authentication failed: The token must be included in the "
                "connection parameters.");
        }
        if (!m_argsMap.count(KEY_IDP_AUTH_TOKEN_TYPE)) {
            RS_LOG_ERROR("IAMIDP",
                "IdC authentication failed: token type needs to be provided in connection params");
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "IdC authentication failed: The token type must be included in the "
                "connection parameters.");
        }
    }

}

// ============================================================
// Hostname Parsing Utilities
// ============================================================

bool IdpTokenAuthPlugin::IsServerlessHost(const rs_string& host) const {
    // Check if hostname matches serverless pattern: *.redshift-serverless.amazonaws.com
    return host.find("redshift-serverless.amazonaws.com") != rs_string::npos;
}

rs_string IdpTokenAuthPlugin::ExtractClusterIdFromHost(const rs_string& host) const {
    // Parse provisioned hostname pattern: cluster.account.region.redshift.amazonaws.com
    // Extract cluster ID from first token
    // Return empty string if pattern doesn't match    
    std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(host, ".");
    
    // Hostname should have at least 6 tokens for provisioned pattern
    // e.g., redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com
    // tokens[0] = cluster-id, tokens[1] = account, tokens[2] = region, 
    // tokens[3] = "redshift", tokens[4] = "amazonaws", tokens[5] = "com"
    if (hostnameTokens.size() >= 6 && hostnameTokens[3] == "redshift") {
        return hostnameTokens[0];
    }
    
    return "";
}

rs_string IdpTokenAuthPlugin::ExtractWorkgroupFromHost(const rs_string& host) const {
    // Parse serverless hostname pattern: workgroup.account.region.redshift-serverless.amazonaws.com
    // Extract workgroup name from first token
    // Return empty string if pattern doesn't match
    // This follows the same pattern used in RsIamClient.cpp SendCredentialsRequest
    
    std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(host, ".");
    
    // Hostname should have at least 6 tokens for serverless pattern
    // e.g., default.518627716765.us-east-1.redshift-serverless.amazonaws.com
    // tokens[0] = workgroup, tokens[1] = account, tokens[2] = region,
    // tokens[3] = "redshift-serverless", tokens[4] = "amazonaws", tokens[5] = "com"
    if (hostnameTokens.size() >= 6 && hostnameTokens[3] == "redshift-serverless") {
        return hostnameTokens[0];
    }
    
    return "";
}

rs_string IdpTokenAuthPlugin::ExtractRegionFromHost(const rs_string& host) const {
    // Extract region from third token of hostname
    // Works for both provisioned and serverless patterns
    // Return empty string if pattern doesn't match
    // This follows the same pattern used in RsIamClient.cpp
    
    std::vector<rs_string> hostnameTokens = IAMUtils::TokenizeSetting(host, ".");
    
    // Both provisioned and serverless hostnames have region at tokens[2]
    // Provisioned: cluster.account.region.redshift.amazonaws.com
    // Serverless: workgroup.account.region.redshift-serverless.amazonaws.com
    // Hostname should have at least 6 tokens
    if (hostnameTokens.size() >= 6) {
        // Verify it's a valid Redshift hostname pattern
        // tokens[3] should be "redshift" or "redshift-serverless"
        if (hostnameTokens[3] == "redshift" || hostnameTokens[3] == "redshift-serverless") {
            return hostnameTokens[2];
        }
    }
    
    return "";
}

// ============================================================
// Cluster Information Resolution
// ============================================================

void IdpTokenAuthPlugin::ResolveClusterInfo() {    
    // Read Host, ClusterId, Workgroup, Region from IAMConfiguration
    const rs_string host = m_config.GetHost();
    const rs_string explicitClusterId = m_config.GetClusterId();
    const rs_string explicitWorkgroup = m_config.GetWorkgroup();
    const rs_string explicitRegion = m_config.GetRegion();
    bool explicitServerless = m_config.GetIsServerless();
    
    // Determine if serverless mode from explicit flag or hostname pattern
    // Explicit serverless flag takes precedence over hostname-based detection
    m_isServerless = explicitServerless || IsServerlessHost(host);
    
    RS_LOG_DEBUG("IAMIDP", "ResolveClusterInfo: host=%s, explicitClusterId=%s, explicitWorkgroup=%s, explicitRegion=%s, explicitServerless=%s",
        host.c_str(),
        explicitClusterId.empty() ? "(empty)" : explicitClusterId.c_str(),
        explicitWorkgroup.empty() ? "(empty)" : explicitWorkgroup.c_str(),
        explicitRegion.empty() ? "(empty)" : explicitRegion.c_str(),
        explicitServerless ? "true" : "false");
    
    if (m_isServerless) {
        // Resolve workgroup: explicit Workgroup takes precedence over hostname extraction
        if (!explicitWorkgroup.empty()) {
            m_resolvedWorkgroup = explicitWorkgroup;
            RS_LOG_DEBUG("IAMIDP", "Using explicit workgroup: %s", m_resolvedWorkgroup.c_str());
        } else {
            m_resolvedWorkgroup = ExtractWorkgroupFromHost(host);
            RS_LOG_DEBUG("IAMIDP", "Extracted workgroup from hostname: %s", 
                m_resolvedWorkgroup.empty() ? "(empty)" : m_resolvedWorkgroup.c_str());
        }
        
        // Throw descriptive error if workgroup cannot be resolved
        if (m_resolvedWorkgroup.empty()) {
            RS_LOG_ERROR("IAMIDP", "IdC authentication failed: cannot resolve serverless workgroup");
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "IdC authentication failed: Serverless workgroup must be provided via "
                "Workgroup connection parameter or resolvable from hostname. "
                "Expected hostname format: workgroup.account.region.redshift-serverless.amazonaws.com");
        }
    } else {
        // Resolve cluster ID: explicit ClusterId takes precedence over hostname extraction
        if (!explicitClusterId.empty()) {
            m_resolvedClusterId = explicitClusterId;
            RS_LOG_DEBUG("IAMIDP", "Using explicit cluster ID: %s", m_resolvedClusterId.c_str());
        } else {
            m_resolvedClusterId = ExtractClusterIdFromHost(host);
            RS_LOG_DEBUG("IAMIDP", "Extracted cluster ID from hostname: %s", 
                m_resolvedClusterId.empty() ? "(empty)" : m_resolvedClusterId.c_str());
        }
        
        // Throw descriptive error if cluster ID cannot be resolved
        if (m_resolvedClusterId.empty()) {
            RS_LOG_ERROR("IAMIDP", "IdC authentication failed: cannot resolve cluster identifier");
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "IdC authentication failed: Cluster identifier must be provided via "
                "ClusterId connection parameter or resolvable from hostname. "
                "Expected hostname format: cluster.account.region.redshift.amazonaws.com");
        }
    }
    
    // Resolve region: explicit Region takes precedence over hostname extraction
    if (!explicitRegion.empty()) {
        m_resolvedRegion = explicitRegion;
        RS_LOG_DEBUG("IAMIDP", "Using explicit region: %s", m_resolvedRegion.c_str());
    } else {
        m_resolvedRegion = ExtractRegionFromHost(host);
        RS_LOG_DEBUG("IAMIDP", "Extracted region from hostname: %s", 
            m_resolvedRegion.empty() ? "(empty)" : m_resolvedRegion.c_str());
    }
    
    // Throw descriptive error if region cannot be resolved
    if (m_resolvedRegion.empty()) {
        RS_LOG_ERROR("IAMIDP", "IdC authentication failed: cannot resolve region");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: Region must be provided via "
            "Region connection parameter or resolvable from hostname.");
    }
    
    // Log resolved values for debugging
    RS_LOG_DEBUG("IAMIDP", "Resolved cluster info: isServerless=%s, clusterId=%s, workgroup=%s, region=%s",
        m_isServerless ? "true" : "false",
        m_resolvedClusterId.empty() ? "(empty)" : m_resolvedClusterId.c_str(),
        m_resolvedWorkgroup.empty() ? "(empty)" : m_resolvedWorkgroup.c_str(),
        m_resolvedRegion.c_str());
}

// ============================================================
// AWS Client Configuration
// ============================================================

Aws::Client::ClientConfiguration IdpTokenAuthPlugin::CreateClientConfiguration(const rs_string& region) const {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::CreateClientConfiguration for region: %s", region.c_str());
    
    Aws::Client::ClientConfiguration config;
    
    // Set region from resolved region
    config.region = region;
    
    // Set endpointOverride from EndpointUrl if provided
    if (!m_endpointUrl.empty()) {
        config.endpointOverride = m_endpointUrl;
        RS_LOG_DEBUG("IAMIDP", "Using custom endpoint URL: %s", m_endpointUrl.c_str());
    }
    
    // Configure CA file for SSL on non-Windows platforms
    // This follows the same pattern as RsIamClient::SendClusterCredentialsRequest()
#ifndef _WIN32
    rs_string caFile = m_config.GetCaFile();
    if (!caFile.empty()) {
        config.caFile = caFile;
        RS_LOG_DEBUG("IAMIDP", "Using CA file from config: %s", caFile.c_str());
    } else {
        config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile());
        RS_LOG_DEBUG("IAMIDP", "Using default CA file: %s", config.caFile.c_str());
    }
#endif

    // Configure HTTPS proxy if enabled
    if (m_config.GetUsingHTTPSProxy()) {
        config.proxyHost = m_config.GetHTTPSProxyHost();
        config.proxyPort = m_config.GetHTTPSProxyPort();
        config.proxyUserName = m_config.GetHTTPSProxyUser();
        config.proxyPassword = m_config.GetHTTPSProxyPassword();
        RS_LOG_DEBUG("IAMIDP", "Configured HTTPS proxy: host=%s, port=%d",
            config.proxyHost.c_str(), config.proxyPort);
    }
    
    return config;
}


// ============================================================
// GetIdentityCenterAuthToken API Calls
// ============================================================

rs_string IdpTokenAuthPlugin::GetProvisionedAuthToken(const rs_string& clusterId, const rs_string& region) {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::GetProvisionedAuthToken for cluster: %s, region: %s",
        clusterId.c_str(), region.c_str());
    
    // Create client configuration using the helper method
    Aws::Client::ClientConfiguration config = CreateClientConfiguration(region);
    
    // Create credentials provider with the user-provided AWS credentials
    // This follows the same pattern as RsIamClient::SendClusterCredentialsRequest
    auto credentialsProvider = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
        m_accessKeyId,
        m_secretAccessKey,
        m_sessionToken);
    
    // Create Redshift client with credentials provider and config
    Aws::Redshift::RedshiftClient client(credentialsProvider, config);
    
    // Create GetIdentityCenterAuthToken request
    // Note: The API uses SetClusterIds (plural) with a vector, not SetClusterIdentifier
    Aws::Redshift::Model::GetIdentityCenterAuthTokenRequest request;
    request.AddClusterIds(clusterId);
    
    RS_LOG_DEBUG("IAMIDP", "Calling GetIdentityCenterAuthToken API for provisioned cluster");
    
    // Make the API call
    Aws::Redshift::Model::GetIdentityCenterAuthTokenOutcome outcome = 
        client.GetIdentityCenterAuthToken(request);
    
    // Validate outcome object
    if (&outcome == nullptr) {
        RS_LOG_ERROR("IAMIDP", "GetIdentityCenterAuthToken returned null outcome");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: GetIdentityCenterAuthToken returned null response.");
    }
    
    // Handle the response
    if (!outcome.IsSuccess()) {
        const auto& error = outcome.GetError();
        rs_string errorMsg = error.GetExceptionName() + ": " + error.GetMessage();
        RS_LOG_ERROR("IAMIDP", "GetIdentityCenterAuthToken API call failed: %s", errorMsg.c_str());
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: " + errorMsg);
    }
    
    // Extract token from the successful response
    const auto& result = outcome.GetResult();
    rs_string token = result.GetToken();
    
    // Validate that we received a non-empty token
    if (token.empty()) {
        RS_LOG_ERROR("IAMIDP", "GetIdentityCenterAuthToken returned empty token");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: GetIdentityCenterAuthToken returned empty token.");
    }
    
    return token;
}


rs_string IdpTokenAuthPlugin::GetServerlessAuthToken(const rs_string& workgroup, const rs_string& region) {
    RS_LOG_DEBUG("IAMIDP", "IdpTokenAuthPlugin::GetServerlessAuthToken for workgroup: %s, region: %s",
        workgroup.c_str(), region.c_str());
    
    // Create client configuration using the helper method
    Aws::Client::ClientConfiguration config = CreateClientConfiguration(region);
    
    // Create credentials provider with the user-provided AWS credentials
    // This follows the same pattern as RsIamClient::SendCredentialsRequest
    auto credentialsProvider = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
        m_accessKeyId,
        m_secretAccessKey,
        m_sessionToken);
    
    // Create Redshift Serverless client with credentials provider and config
    Aws::RedshiftServerless::RedshiftServerlessClient client(credentialsProvider, config);
    
    // Create GetIdentityCenterAuthToken request for serverless
    // Note: The API uses AddWorkgroupNames (plural) with a vector, similar to provisioned AddClusterIds
    Aws::RedshiftServerless::Model::GetIdentityCenterAuthTokenRequest request;
    request.AddWorkgroupNames(workgroup);
    
    RS_LOG_DEBUG("IAMIDP", "Calling GetIdentityCenterAuthToken API for serverless workgroup");
    
    // Make the API call
    Aws::RedshiftServerless::Model::GetIdentityCenterAuthTokenOutcome outcome = 
        client.GetIdentityCenterAuthToken(request);
    
    // Validate outcome object
    if (&outcome == nullptr) {
        RS_LOG_ERROR("IAMIDP", "GetIdentityCenterAuthToken (serverless) returned null outcome");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: GetIdentityCenterAuthToken returned null response.");
    }
    
    // Handle the response
    if (!outcome.IsSuccess()) {
        const auto& error = outcome.GetError();
        rs_string errorMsg = error.GetExceptionName() + ": " + error.GetMessage();
        RS_LOG_ERROR("IAMIDP", "GetIdentityCenterAuthToken (serverless) API call failed: %s", errorMsg.c_str());
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: " + errorMsg);
    }
    
    // Extract token from the successful response
    const auto& result = outcome.GetResult();
    rs_string token = result.GetToken();
    
    // Validate that we received a non-empty token
    if (token.empty()) {
        RS_LOG_ERROR("IAMIDP", "GetIdentityCenterAuthToken (serverless) returned empty token");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: GetIdentityCenterAuthToken returned empty token.");
    }
    
    return token;
}

// ============================================================
// Main Authentication Flow - GetSubjectToken
// ============================================================

rs_string IdpTokenAuthPlugin::GetSubjectToken() {    
    // Resolve cluster/workgroup information from hostname or explicit parameters
    ResolveClusterInfo();
    
    rs_string subjectToken;
    
    // Call the appropriate API based on cluster type (serverless vs provisioned)
    if (m_isServerless) {
        RS_LOG_DEBUG("IAMIDP", "Calling serverless GetIdentityCenterAuthToken for workgroup: %s, region: %s",
            m_resolvedWorkgroup.c_str(), m_resolvedRegion.c_str());
        subjectToken = GetServerlessAuthToken(m_resolvedWorkgroup, m_resolvedRegion);
    } else {
        RS_LOG_DEBUG("IAMIDP", "Calling provisioned GetIdentityCenterAuthToken for cluster: %s, region: %s",
            m_resolvedClusterId.c_str(), m_resolvedRegion.c_str());
        subjectToken = GetProvisionedAuthToken(m_resolvedClusterId, m_resolvedRegion);
    }
    
    // Set token_type to "SUBJECT_TOKEN" in m_argsMap for identity-enhanced flow
    // This is required per Requirement 6.1: identity-enhanced credentials flow SHALL set token_type to "SUBJECT_TOKEN"
    m_argsMap[KEY_IDP_AUTH_TOKEN_TYPE] = "SUBJECT_TOKEN";
    RS_LOG_DEBUG("IAMIDP", "Set token_type to SUBJECT_TOKEN for identity-enhanced credentials flow");
    
    return subjectToken;
}
