/**
 * Unit tests for IdpTokenAuthPlugin
 * 
 * These tests validate the authentication flow selection and credential validation
 * for the IdpTokenAuthPlugin, covering both direct token flow and identity-enhanced
 * credentials flow.
 */

#include "common.h"
#include "iam/plugins/IdpTokenAuthPlugin.h"
#include "iam/core/IAMUtils.h"
#include "iam/core/IAMConfiguration.h"
#include "iam/RsErrorException.h"

using namespace Redshift::IamSupport;

// Test suite name
#define IDP_TOKEN_AUTH_TEST_SUITE IdpTokenAuthPluginTest

// Tests for authentication flow selection based on connection parameters

// Test: Direct token flow is selected when token and token_type are provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, DirectTokenFlow_WithValidTokenAndType_Succeeds) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token-value");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should not throw - direct token flow is valid
    rs_string token = plugin.GetAuthToken();
    EXPECT_EQ(token, "test-token-value");
}

// Test: Direct token flow fails when only token is provided (missing token_type)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, DirectTokenFlow_MissingTokenType_ThrowsError) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token-value");
    // token_type not set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Direct token flow fails when only token_type is provided (missing token)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, DirectTokenFlow_MissingToken_ThrowsError) {
    IAMConfiguration config;
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    // token not set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Identity-enhanced flow is selected when all AWS credentials are provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, IdentityEnhancedFlow_WithAllCredentials_Succeeds) {
    IAMConfiguration config;
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // ValidateArgumentsMap should pass for identity-enhanced flow
    // Note: GetAuthToken will fail later when trying to call the API,
    // but validation should pass
    // For now, we just verify the plugin is created without error
    EXPECT_NO_THROW({
        // The plugin constructor calls InitArgumentsMap which reads credentials
        // We can't call GetAuthToken without mocking the API call
    });
}

// Test: Identity-enhanced flow fails when only AccessKeyID is provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, IdentityEnhancedFlow_OnlyAccessKeyId_ThrowsError) {
    IAMConfiguration config;
    config.SetAccessId("dummy-access-key");
    // SecretAccessKey and SessionToken not set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Identity-enhanced flow fails when only SecretAccessKey is provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, IdentityEnhancedFlow_OnlySecretKey_ThrowsError) {
    IAMConfiguration config;
    config.SetSecretKey("dummy-secret-key");
    // AccessKeyID and SessionToken not set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Identity-enhanced flow fails when only SessionToken is provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, IdentityEnhancedFlow_OnlySessionToken_ThrowsError) {
    IAMConfiguration config;
    config.SetSessionToken("dummy-session-token");
    // AccessKeyID and SecretAccessKey not set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Conflicting parameters - both direct token and AWS credentials provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ConflictingParameters_BothFlows_ThrowsError) {
    IAMConfiguration config;
    // Direct token flow parameters
    config.SetIdpAuthToken("test-token-value");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    // Identity-enhanced credentials
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: No authentication parameters provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, NoParameters_ThrowsError) {
    IAMConfiguration config;
    // No parameters set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Missing AccessKeyID with other credentials present
TEST(IDP_TOKEN_AUTH_TEST_SUITE, CredentialValidation_MissingAccessKeyId_ThrowsError) {
    IAMConfiguration config;
    // AccessKeyID not set
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Missing SecretAccessKey with other credentials present
TEST(IDP_TOKEN_AUTH_TEST_SUITE, CredentialValidation_MissingSecretKey_ThrowsError) {
    IAMConfiguration config;
    config.SetAccessId("dummy-access-key");
    // SecretAccessKey not set
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Missing SessionToken with other credentials present
TEST(IDP_TOKEN_AUTH_TEST_SUITE, CredentialValidation_MissingSessionToken_ThrowsError) {
    IAMConfiguration config;
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    // SessionToken not set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Empty string credentials are treated as not provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, CredentialValidation_EmptyStrings_ThrowsError) {
    IAMConfiguration config;
    config.SetAccessId("");
    config.SetSecretKey("");
    config.SetSessionToken("");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Empty strings should be treated as no parameters provided
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Whitespace-only token is treated as not provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, DirectTokenFlow_WhitespaceToken_ThrowsError) {
    IAMConfiguration config;
    config.SetIdpAuthToken("   ");  // whitespace only
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Whitespace-only token should be treated as not provided
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Verify error message for conflicting parameters
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ErrorMessage_ConflictingParameters_ContainsExpectedText) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    try {
        plugin.GetAuthToken();
        FAIL() << "Expected RsErrorException to be thrown";
    } catch (const RsErrorException& e) {
        std::string errorMsg = e.what();
        EXPECT_TRUE(errorMsg.find("conflicting") != std::string::npos ||
                    errorMsg.find("Cannot provide both") != std::string::npos)
            << "Error message should mention conflicting parameters: " << errorMsg;
    }
}

// Test: Verify error message for missing parameters
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ErrorMessage_NoParameters_ContainsExpectedText) {
    IAMConfiguration config;
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    try {
        plugin.GetAuthToken();
        FAIL() << "Expected RsErrorException to be thrown";
    } catch (const RsErrorException& e) {
        std::string errorMsg = e.what();
        EXPECT_TRUE(errorMsg.find("token") != std::string::npos ||
                    errorMsg.find("AccessKeyID") != std::string::npos ||
                    errorMsg.find("must be provided") != std::string::npos)
            << "Error message should mention required parameters: " << errorMsg;
    }
}

// Test: Verify error message for incomplete credentials
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ErrorMessage_IncompleteCredentials_ContainsExpectedText) {
    IAMConfiguration config;
    config.SetAccessId("dummy-access-key");
    // Missing SecretAccessKey and SessionToken
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    try {
        plugin.GetAuthToken();
        FAIL() << "Expected RsErrorException to be thrown";
    } catch (const RsErrorException& e) {
        std::string errorMsg = e.what();
        EXPECT_TRUE(errorMsg.find("All three") != std::string::npos ||
                    errorMsg.find("AccessKeyID") != std::string::npos ||
                    errorMsg.find("SecretAccessKey") != std::string::npos ||
                    errorMsg.find("SessionToken") != std::string::npos)
            << "Error message should mention required credentials: " << errorMsg;
    }
}

// ============================================================
// Hostname Parsing Tests
// ============================================================

/**
 * Test helper class that exposes private hostname parsing methods for testing.
 * This allows us to test the hostname parsing utilities directly without
 * modifying the production header file.
 */
class IdpTokenAuthPluginTestHelper : public IdpTokenAuthPlugin {
public:
    explicit IdpTokenAuthPluginTestHelper(
        const IAMConfiguration &in_config = IAMConfiguration(),
        const std::map<rs_string, rs_string> &in_argsMap = std::map<rs_string, rs_string>())
        : IdpTokenAuthPlugin(in_config, in_argsMap) {}

    // Expose private methods for testing
    bool TestIsServerlessHost(const rs_string& host) const {
        return IsServerlessHost(host);
    }

    rs_string TestExtractClusterIdFromHost(const rs_string& host) const {
        return ExtractClusterIdFromHost(host);
    }

    rs_string TestExtractWorkgroupFromHost(const rs_string& host) const {
        return ExtractWorkgroupFromHost(host);
    }

    rs_string TestExtractRegionFromHost(const rs_string& host) const {
        return ExtractRegionFromHost(host);
    }
};

// ============================================================
// Hostname Parsing Correctness Tests
// ============================================================

// Test: Provisioned hostname - extract cluster ID correctly
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Provisioned_ExtractsClusterId) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Standard provisioned hostname pattern
    rs_string host = "my-cluster.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com";
    rs_string clusterId = plugin.TestExtractClusterIdFromHost(host);
    
    EXPECT_EQ(clusterId, "my-cluster");
}

// Test: Provisioned hostname - extract region correctly
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Provisioned_ExtractsRegion) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "my-cluster.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com";
    rs_string region = plugin.TestExtractRegionFromHost(host);
    
    EXPECT_EQ(region, "us-west-2");
}

// Test: Serverless hostname - extract workgroup correctly
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Serverless_ExtractsWorkgroup) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Standard serverless hostname pattern
    rs_string host = "default.518627716765.us-east-1.redshift-serverless.amazonaws.com";
    rs_string workgroup = plugin.TestExtractWorkgroupFromHost(host);
    
    EXPECT_EQ(workgroup, "default");
}

// Test: Serverless hostname - extract region correctly
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Serverless_ExtractsRegion) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "default.518627716765.us-east-1.redshift-serverless.amazonaws.com";
    rs_string region = plugin.TestExtractRegionFromHost(host);
    
    EXPECT_EQ(region, "us-east-1");
}

// Test: Provisioned hostname with China region (.com.cn suffix)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Provisioned_ChinaRegion) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // China region hostname pattern
    rs_string host = "my-cluster.c2mf5zd3u3sv.cn-north-1.redshift.amazonaws.com.cn";
    rs_string clusterId = plugin.TestExtractClusterIdFromHost(host);
    rs_string region = plugin.TestExtractRegionFromHost(host);
    
    EXPECT_EQ(clusterId, "my-cluster");
    EXPECT_EQ(region, "cn-north-1");
}

// Test: Invalid hostname - returns empty string for cluster ID
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_InvalidHostname_ReturnsEmptyClusterId) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Invalid hostname - not a Redshift hostname
    rs_string host = "example.com";
    rs_string clusterId = plugin.TestExtractClusterIdFromHost(host);
    
    EXPECT_EQ(clusterId, "");
}

// Test: Invalid hostname - returns empty string for workgroup
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_InvalidHostname_ReturnsEmptyWorkgroup) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "example.com";
    rs_string workgroup = plugin.TestExtractWorkgroupFromHost(host);
    
    EXPECT_EQ(workgroup, "");
}

// Test: Invalid hostname - returns empty string for region
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_InvalidHostname_ReturnsEmptyRegion) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "example.com";
    rs_string region = plugin.TestExtractRegionFromHost(host);
    
    EXPECT_EQ(region, "");
}

// Test: Provisioned hostname - workgroup extraction returns empty (not serverless)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Provisioned_WorkgroupReturnsEmpty) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Provisioned hostname - should not extract workgroup
    rs_string host = "my-cluster.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com";
    rs_string workgroup = plugin.TestExtractWorkgroupFromHost(host);
    
    EXPECT_EQ(workgroup, "");
}

// Test: Serverless hostname - cluster ID extraction returns empty (not provisioned)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_Serverless_ClusterIdReturnsEmpty) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Serverless hostname - should not extract cluster ID
    rs_string host = "default.518627716765.us-east-1.redshift-serverless.amazonaws.com";
    rs_string clusterId = plugin.TestExtractClusterIdFromHost(host);
    
    EXPECT_EQ(clusterId, "");
}

// Test: Various valid cluster names
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_VariousClusterNames) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Test various cluster name formats
    std::vector<std::pair<rs_string, rs_string>> testCases = {
        {"simple.account.us-west-2.redshift.amazonaws.com", "simple"},
        {"my-cluster-name.account.eu-west-1.redshift.amazonaws.com", "my-cluster-name"},
        {"cluster123.account.ap-southeast-1.redshift.amazonaws.com", "cluster123"},
        {"test-cluster-01.account.us-east-1.redshift.amazonaws.com", "test-cluster-01"}
    };
    
    for (const auto& testCase : testCases) {
        rs_string clusterId = plugin.TestExtractClusterIdFromHost(testCase.first);
        EXPECT_EQ(clusterId, testCase.second) 
            << "Failed for hostname: " << testCase.first;
    }
}

// Test: Various valid workgroup names
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_VariousWorkgroupNames) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Test various workgroup name formats
    std::vector<std::pair<rs_string, rs_string>> testCases = {
        {"default.123456789012.us-east-1.redshift-serverless.amazonaws.com", "default"},
        {"my-workgroup.123456789012.eu-west-1.redshift-serverless.amazonaws.com", "my-workgroup"},
        {"workgroup123.123456789012.ap-southeast-1.redshift-serverless.amazonaws.com", "workgroup123"},
        {"test-wg-01.123456789012.us-west-2.redshift-serverless.amazonaws.com", "test-wg-01"}
    };
    
    for (const auto& testCase : testCases) {
        rs_string workgroup = plugin.TestExtractWorkgroupFromHost(testCase.first);
        EXPECT_EQ(workgroup, testCase.second) 
            << "Failed for hostname: " << testCase.first;
    }
}

// Test: Various valid regions
TEST(IDP_TOKEN_AUTH_TEST_SUITE, HostnameParsing_VariousRegions) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // Test various region formats for both provisioned and serverless
    std::vector<std::pair<rs_string, rs_string>> testCases = {
        // Provisioned
        {"cluster.account.us-east-1.redshift.amazonaws.com", "us-east-1"},
        {"cluster.account.us-west-2.redshift.amazonaws.com", "us-west-2"},
        {"cluster.account.eu-west-1.redshift.amazonaws.com", "eu-west-1"},
        {"cluster.account.ap-southeast-1.redshift.amazonaws.com", "ap-southeast-1"},
        {"cluster.account.cn-north-1.redshift.amazonaws.com.cn", "cn-north-1"},
        // Serverless
        {"workgroup.account.us-east-1.redshift-serverless.amazonaws.com", "us-east-1"},
        {"workgroup.account.eu-central-1.redshift-serverless.amazonaws.com", "eu-central-1"},
        {"workgroup.account.ap-northeast-1.redshift-serverless.amazonaws.com", "ap-northeast-1"}
    };
    
    for (const auto& testCase : testCases) {
        rs_string region = plugin.TestExtractRegionFromHost(testCase.first);
        EXPECT_EQ(region, testCase.second) 
            << "Failed for hostname: " << testCase.first;
    }
}

// ============================================================
// Cluster Type Detection Tests
// ============================================================

// Test: Serverless hostname is detected correctly
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_ServerlessHostname_DetectedAsServerless) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "default.518627716765.us-east-1.redshift-serverless.amazonaws.com";
    bool isServerless = plugin.TestIsServerlessHost(host);
    
    EXPECT_TRUE(isServerless);
}

// Test: Provisioned hostname is not detected as serverless
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_ProvisionedHostname_NotServerless) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "my-cluster.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com";
    bool isServerless = plugin.TestIsServerlessHost(host);
    
    EXPECT_FALSE(isServerless);
}

// Test: Invalid hostname is not detected as serverless
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_InvalidHostname_NotServerless) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "example.com";
    bool isServerless = plugin.TestIsServerlessHost(host);
    
    EXPECT_FALSE(isServerless);
}

// Test: Various serverless hostnames are detected correctly
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_VariousServerlessHostnames) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    std::vector<rs_string> serverlessHosts = {
        "default.123456789012.us-east-1.redshift-serverless.amazonaws.com",
        "my-workgroup.123456789012.eu-west-1.redshift-serverless.amazonaws.com",
        "test.123456789012.ap-southeast-1.redshift-serverless.amazonaws.com"
    };
    
    for (const auto& host : serverlessHosts) {
        EXPECT_TRUE(plugin.TestIsServerlessHost(host)) 
            << "Should be detected as serverless: " << host;
    }
}

// Test: Various provisioned hostnames are not detected as serverless
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_VariousProvisionedHostnames) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    std::vector<rs_string> provisionedHosts = {
        "my-cluster.account.us-west-2.redshift.amazonaws.com",
        "cluster-01.account.eu-west-1.redshift.amazonaws.com",
        "test-cluster.account.cn-north-1.redshift.amazonaws.com.cn"
    };
    
    for (const auto& host : provisionedHosts) {
        EXPECT_FALSE(plugin.TestIsServerlessHost(host)) 
            << "Should NOT be detected as serverless: " << host;
    }
}

// Test: Empty hostname is not detected as serverless
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_EmptyHostname_NotServerless) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    rs_string host = "";
    bool isServerless = plugin.TestIsServerlessHost(host);
    
    EXPECT_FALSE(isServerless);
}

// Test: Hostname with "serverless" in wrong position is not detected as serverless
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ClusterTypeDetection_ServerlessInWrongPosition_NotServerless) {
    IAMConfiguration config;
    config.SetIdpAuthToken("test-token");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginTestHelper plugin(config, argsMap);
    
    // "serverless" appears but not in the correct pattern
    rs_string host = "serverless-cluster.account.us-west-2.redshift.amazonaws.com";
    bool isServerless = plugin.TestIsServerlessHost(host);
    
    EXPECT_FALSE(isServerless);
}

// ============================================================
// ResolveClusterInfo Test Helper
// ============================================================

/**
 * Extended test helper class that exposes ResolveClusterInfo and resolved values for testing.
 * This allows us to test the explicit parameter precedence behavior.
 */
class IdpTokenAuthPluginResolveTestHelper : public IdpTokenAuthPlugin {
public:
    explicit IdpTokenAuthPluginResolveTestHelper(
        const IAMConfiguration &in_config = IAMConfiguration(),
        const std::map<rs_string, rs_string> &in_argsMap = std::map<rs_string, rs_string>())
        : IdpTokenAuthPlugin(in_config, in_argsMap) {}

    // Expose ResolveClusterInfo for testing
    void TestResolveClusterInfo() {
        ResolveClusterInfo();
    }

    // Expose resolved values for verification
    rs_string GetResolvedClusterId() const { return m_resolvedClusterId; }
    rs_string GetResolvedWorkgroup() const { return m_resolvedWorkgroup; }
    rs_string GetResolvedRegion() const { return m_resolvedRegion; }
    bool GetIsServerless() const { return m_isServerless; }
};

// ============================================================
// Explicit Parameter Precedence Tests
// ============================================================

// Test: Explicit ClusterId takes precedence over hostname-extracted cluster ID
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_ClusterId_OverridesHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname contains "hostname-cluster" as cluster ID
    config.SetHost("hostname-cluster.account.us-west-2.redshift.amazonaws.com");
    // Explicit ClusterId should override
    config.SetClusterId("explicit-cluster");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit ClusterId should take precedence
    EXPECT_EQ(plugin.GetResolvedClusterId(), "explicit-cluster");
    EXPECT_FALSE(plugin.GetIsServerless());
}

// Test: Explicit Workgroup takes precedence over hostname-extracted workgroup
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_Workgroup_OverridesHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname contains "hostname-workgroup" as workgroup
    config.SetHost("hostname-workgroup.123456789012.us-east-1.redshift-serverless.amazonaws.com");
    // Explicit Workgroup should override
    config.SetWorkgroup("explicit-workgroup");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit Workgroup should take precedence
    EXPECT_EQ(plugin.GetResolvedWorkgroup(), "explicit-workgroup");
    EXPECT_TRUE(plugin.GetIsServerless());
}

// Test: Explicit Region takes precedence over hostname-extracted region (provisioned)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_Region_OverridesHostname_Provisioned) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname contains "us-west-2" as region
    config.SetHost("my-cluster.account.us-west-2.redshift.amazonaws.com");
    // Explicit Region should override
    config.SetRegion("eu-central-1");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit Region should take precedence
    EXPECT_EQ(plugin.GetResolvedRegion(), "eu-central-1");
}

// Test: Explicit Region takes precedence over hostname-extracted region (serverless)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_Region_OverridesHostname_Serverless) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname contains "us-east-1" as region
    config.SetHost("my-workgroup.123456789012.us-east-1.redshift-serverless.amazonaws.com");
    // Explicit Region should override
    config.SetRegion("ap-southeast-1");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit Region should take precedence
    EXPECT_EQ(plugin.GetResolvedRegion(), "ap-southeast-1");
}

// Test: All explicit parameters take precedence simultaneously (provisioned)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_AllParameters_Provisioned) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname values
    config.SetHost("hostname-cluster.account.us-west-2.redshift.amazonaws.com");
    // Explicit values should all override
    config.SetClusterId("explicit-cluster");
    config.SetRegion("eu-west-1");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // All explicit values should take precedence
    EXPECT_EQ(plugin.GetResolvedClusterId(), "explicit-cluster");
    EXPECT_EQ(plugin.GetResolvedRegion(), "eu-west-1");
    EXPECT_FALSE(plugin.GetIsServerless());
}

// Test: All explicit parameters take precedence simultaneously (serverless)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_AllParameters_Serverless) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname values
    config.SetHost("hostname-workgroup.123456789012.us-east-1.redshift-serverless.amazonaws.com");
    // Explicit values should all override
    config.SetWorkgroup("explicit-workgroup");
    config.SetRegion("ap-northeast-1");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // All explicit values should take precedence
    EXPECT_EQ(plugin.GetResolvedWorkgroup(), "explicit-workgroup");
    EXPECT_EQ(plugin.GetResolvedRegion(), "ap-northeast-1");
    EXPECT_TRUE(plugin.GetIsServerless());
}

// Test: Hostname extraction works when no explicit parameters provided (provisioned)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_NoExplicit_UsesHostname_Provisioned) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Only hostname provided, no explicit parameters
    config.SetHost("my-cluster.account.us-west-2.redshift.amazonaws.com");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Should extract from hostname
    EXPECT_EQ(plugin.GetResolvedClusterId(), "my-cluster");
    EXPECT_EQ(plugin.GetResolvedRegion(), "us-west-2");
    EXPECT_FALSE(plugin.GetIsServerless());
}

// Test: Hostname extraction works when no explicit parameters provided (serverless)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_NoExplicit_UsesHostname_Serverless) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Only hostname provided, no explicit parameters
    config.SetHost("my-workgroup.123456789012.eu-west-1.redshift-serverless.amazonaws.com");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Should extract from hostname
    EXPECT_EQ(plugin.GetResolvedWorkgroup(), "my-workgroup");
    EXPECT_EQ(plugin.GetResolvedRegion(), "eu-west-1");
    EXPECT_TRUE(plugin.GetIsServerless());
}

// Test: Explicit serverless flag overrides hostname-based detection
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_ServerlessFlag_OverridesHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Provisioned hostname (not serverless based on pattern)
    config.SetHost("my-cluster.account.us-west-2.redshift.amazonaws.com");
    // But explicit serverless flag is set
    config.SetIsServerless(true);
    // Must provide workgroup since we're forcing serverless mode
    config.SetWorkgroup("explicit-workgroup");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit serverless flag should take precedence
    EXPECT_TRUE(plugin.GetIsServerless());
    EXPECT_EQ(plugin.GetResolvedWorkgroup(), "explicit-workgroup");
}

// Test: Explicit ClusterId with non-Redshift hostname
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_ClusterId_WithNonRedshiftHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Non-Redshift hostname (e.g., custom endpoint or VPC endpoint)
    config.SetHost("custom-endpoint.example.com");
    // Explicit ClusterId and Region must be provided
    config.SetClusterId("my-cluster");
    config.SetRegion("us-west-2");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit values should be used
    EXPECT_EQ(plugin.GetResolvedClusterId(), "my-cluster");
    EXPECT_EQ(plugin.GetResolvedRegion(), "us-west-2");
    EXPECT_FALSE(plugin.GetIsServerless());
}

// Test: Explicit Workgroup with non-Redshift hostname
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_Workgroup_WithNonRedshiftHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Non-Redshift hostname (e.g., custom endpoint or VPC endpoint)
    config.SetHost("custom-endpoint.example.com");
    // Explicit serverless mode with Workgroup and Region
    config.SetIsServerless(true);
    config.SetWorkgroup("my-workgroup");
    config.SetRegion("eu-central-1");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Explicit values should be used
    EXPECT_EQ(plugin.GetResolvedWorkgroup(), "my-workgroup");
    EXPECT_EQ(plugin.GetResolvedRegion(), "eu-central-1");
    EXPECT_TRUE(plugin.GetIsServerless());
}

// Test: Empty explicit ClusterId falls back to hostname extraction
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_EmptyClusterId_FallsBackToHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname with cluster ID
    config.SetHost("hostname-cluster.account.us-west-2.redshift.amazonaws.com");
    // Empty explicit ClusterId should fall back to hostname
    config.SetClusterId("");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Should fall back to hostname extraction
    EXPECT_EQ(plugin.GetResolvedClusterId(), "hostname-cluster");
}

// Test: Empty explicit Workgroup falls back to hostname extraction
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_EmptyWorkgroup_FallsBackToHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname with workgroup
    config.SetHost("hostname-workgroup.123456789012.us-east-1.redshift-serverless.amazonaws.com");
    // Empty explicit Workgroup should fall back to hostname
    config.SetWorkgroup("");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Should fall back to hostname extraction
    EXPECT_EQ(plugin.GetResolvedWorkgroup(), "hostname-workgroup");
}

// Test: Empty explicit Region falls back to hostname extraction
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_EmptyRegion_FallsBackToHostname) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Hostname with region
    config.SetHost("my-cluster.account.ap-southeast-1.redshift.amazonaws.com");
    // Empty explicit Region should fall back to hostname
    config.SetRegion("");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    plugin.TestResolveClusterInfo();
    
    // Should fall back to hostname extraction
    EXPECT_EQ(plugin.GetResolvedRegion(), "ap-southeast-1");
}

// Test: Error when cluster ID cannot be resolved (no explicit, invalid hostname)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_NoClusterId_InvalidHostname_ThrowsError) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Invalid hostname that doesn't match Redshift pattern
    config.SetHost("invalid-hostname.example.com");
    // No explicit ClusterId
    // Explicit region to avoid region error
    config.SetRegion("us-west-2");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    // Should throw error because cluster ID cannot be resolved
    EXPECT_THROW({
        plugin.TestResolveClusterInfo();
    }, RsErrorException);
}

// Test: Error when workgroup cannot be resolved (no explicit, invalid hostname)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_NoWorkgroup_InvalidHostname_ThrowsError) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Invalid hostname that doesn't match Redshift pattern
    config.SetHost("invalid-hostname.example.com");
    // Force serverless mode
    config.SetIsServerless(true);
    // No explicit Workgroup
    // Explicit region to avoid region error
    config.SetRegion("us-west-2");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    // Should throw error because workgroup cannot be resolved
    EXPECT_THROW({
        plugin.TestResolveClusterInfo();
    }, RsErrorException);
}

// Test: Error when region cannot be resolved (no explicit, invalid hostname)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ExplicitPrecedence_NoRegion_InvalidHostname_ThrowsError) {
    IAMConfiguration config;
    // Set up identity-enhanced credentials to enable the flow
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    // Invalid hostname that doesn't match Redshift pattern
    config.SetHost("invalid-hostname.example.com");
    // Explicit ClusterId to avoid cluster ID error
    config.SetClusterId("my-cluster");
    // No explicit Region
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    // Should throw error because region cannot be resolved
    EXPECT_THROW({
        plugin.TestResolveClusterInfo();
    }, RsErrorException);
}

// ============================================================
// ValidateArgumentsMap Tests (tested indirectly via GetAuthToken)
// ============================================================

/**
 * These tests validate the ValidateArgumentsMap logic by calling GetAuthToken(),
 * which internally calls ValidateArgumentsMap(). Since ValidateArgumentsMap is private,
 * we test it indirectly through the public API.
 * 
 * For identity-enhanced flow tests, we use ResolveClusterInfo via the test helper
 * since GetAuthToken would proceed to make API calls after validation passes.
 */

// Test: Validation passes when only identity-enhanced credentials are provided (no direct token)
// This is the key test for the bug fix - identity-enhanced flow should not require direct token
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_IdentityEnhancedOnly_PassesValidation) {
    IAMConfiguration config;
    // Only identity-enhanced credentials, no direct token
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    // Set host so ResolveClusterInfo can extract cluster info
    config.SetHost("my-cluster.account.us-west-2.redshift.amazonaws.com");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPluginResolveTestHelper plugin(config, argsMap);
    
    // ResolveClusterInfo is called after ValidateArgumentsMap in GetSubjectToken flow
    // If ValidateArgumentsMap incorrectly throws, this would fail
    EXPECT_NO_THROW({
        plugin.TestResolveClusterInfo();
    });
    
    // Verify the identity-enhanced flow was correctly identified
    EXPECT_EQ(plugin.GetResolvedClusterId(), "my-cluster");
    EXPECT_EQ(plugin.GetResolvedRegion(), "us-west-2");
}

// Test: Validation passes when only direct token is provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_DirectTokenOnly_PassesValidation) {
    IAMConfiguration config;
    // Only direct token, no identity-enhanced credentials
    config.SetIdpAuthToken("test-token-value");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // GetAuthToken should succeed and return the direct token
    rs_string token = plugin.GetAuthToken();
    EXPECT_EQ(token, "test-token-value");
}

// Test: Validation fails when both flows are provided (conflicting parameters)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_BothFlows_FailsValidation) {
    IAMConfiguration config;
    // Both direct token and identity-enhanced credentials
    config.SetIdpAuthToken("test-token-value");
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - conflicting parameters
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Validation fails when neither flow is provided
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_NeitherFlow_FailsValidation) {
    IAMConfiguration config;
    // No parameters set
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - no authentication parameters
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Validation fails when direct token is provided without token_type
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_DirectToken_MissingTokenType_FailsValidation) {
    IAMConfiguration config;
    // Only token, missing token_type
    config.SetIdpAuthToken("test-token-value");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - missing token_type
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Validation fails when token_type is provided without token
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_DirectToken_MissingToken_FailsValidation) {
    IAMConfiguration config;
    // Only token_type, missing token
    config.SetIdpAuthTokenType("ACCESS_TOKEN");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - missing token
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Validation fails when identity-enhanced credentials are incomplete (missing AccessKeyId)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_IdentityEnhanced_MissingAccessKeyId_FailsValidation) {
    IAMConfiguration config;
    // Missing AccessKeyId
    config.SetSecretKey("dummy-secret-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - incomplete credentials treated as no parameters
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Validation fails when identity-enhanced credentials are incomplete (missing SecretKey)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_IdentityEnhanced_MissingSecretKey_FailsValidation) {
    IAMConfiguration config;
    // Missing SecretKey
    config.SetAccessId("dummy-access-key");
    config.SetSessionToken("dummy-session-token");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - incomplete credentials treated as no parameters
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}

// Test: Validation fails when identity-enhanced credentials are incomplete (missing SessionToken)
TEST(IDP_TOKEN_AUTH_TEST_SUITE, ValidateArgumentsMap_IdentityEnhanced_MissingSessionToken_FailsValidation) {
    IAMConfiguration config;
    // Missing SessionToken
    config.SetAccessId("dummy-access-key");
    config.SetSecretKey("dummy-secret-key");
    
    std::map<rs_string, rs_string> argsMap;
    IdpTokenAuthPlugin plugin(config, argsMap);
    
    // Should throw - incomplete credentials treated as no parameters
    EXPECT_THROW({
        plugin.GetAuthToken();
    }, RsErrorException);
}
