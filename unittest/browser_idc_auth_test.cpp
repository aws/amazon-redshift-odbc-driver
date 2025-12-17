#include "iam/plugins/BrowserIdcAuthPlugin.h"
#include "iam/core/IAMUtils.h"
#include "iam/core/IAMConfiguration.h"
#include "iam/RsErrorException.h"
#include "common.h"

using namespace Redshift::IamSupport;

// Simple test class that doesn't access private methods
class BrowserIdcAuthPluginTest {
public:
    static std::string TestRegionValidation(const std::string& region) {
        IAMConfiguration config;
        std::map<rs_string, rs_string> argsMap;
        
        BrowserIdcAuthPlugin plugin(config, argsMap);
        
        // Call BuildOidcHostUrl which validates the region
        return plugin.BuildOidcHostUrl(region);
    }
};

// Test valid regions - verify correct OIDC host URLs
TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_valid_us_region) {
    IAMConfiguration config;
    std::map<rs_string, rs_string> argsMap;
    BrowserIdcAuthPlugin plugin(config, argsMap);

    rs_string result = plugin.BuildOidcHostUrl("us-east-1");
    EXPECT_EQ(result, "oidc.us-east-1.amazonaws.com");
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_valid_china_region) {
    IAMConfiguration config;
    std::map<rs_string, rs_string> argsMap;
    BrowserIdcAuthPlugin plugin(config, argsMap);

    rs_string result = plugin.BuildOidcHostUrl("cn-north-1");
    EXPECT_EQ(result, "oidc.cn-north-1.amazonaws.com.cn");
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_valid_gov_region) {
    std::string result1 = BrowserIdcAuthPluginTest::TestRegionValidation("us-gov-west-1");
    std::string result2 = BrowserIdcAuthPluginTest::TestRegionValidation("us-gov-east-1");
    EXPECT_FALSE(result1.empty());
    EXPECT_FALSE(result2.empty());
    EXPECT_EQ(result1, "oidc.us-gov-west-1.amazonaws.com");
    EXPECT_EQ(result2, "oidc.us-gov-east-1.amazonaws.com");
}

// Test invalid regions - these should return empty strings
TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_injection_attack_prevention) {
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us-east-1; rm -rf /").empty());
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us-east-1/../../../etc/passwd").empty());
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us-east-1\nmalicious.com").empty());
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_invalid_characters) {
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us_east_1").empty());  // underscores
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us.east.1").empty());  // dots
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us@east@1").empty());  // special chars
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_length_validation) {
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us-1").empty());      // too short
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("a-very-very-very-very-long-region-name-that-exceeds-limits").empty());  // too long
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_double_hyphen_prevention) {
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us--east-1").empty());
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("us-east--1").empty());
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_empty_region) {
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("").empty());
    EXPECT_TRUE(BrowserIdcAuthPluginTest::TestRegionValidation("   ").empty());  // whitespace only
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_proxy_not_configured) {
    IAMConfiguration config;
    config.SetUsingHTTPSProxy(false);
    std::map<rs_string, rs_string> argsMap;
    argsMap["idp_region"] = "us-east-1";
    
    BrowserIdcAuthPlugin plugin(config, argsMap);
    EXPECT_FALSE(config.GetUsingHTTPSProxy());
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_proxy_configured_without_auth) {
    IAMConfiguration config;
    config.SetUsingHTTPSProxy(true);
    config.SetUseProxyIdpAuth(true);
    config.SetHTTPSProxyHost("proxy.example.com");
    config.SetHTTPSProxyPort(8080);
    
    EXPECT_TRUE(config.GetUsingHTTPSProxy());
    EXPECT_TRUE(config.GetUseProxyIdpAuth());
    EXPECT_EQ(config.GetHTTPSProxyHost(), "proxy.example.com");
    EXPECT_EQ(config.GetHTTPSProxyPort(), 8080);
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_proxy_configured_with_auth) {
    IAMConfiguration config;
    config.SetUsingHTTPSProxy(true);
    config.SetUseProxyIdpAuth(true);
    config.SetHTTPSProxyHost("proxy.example.com");
    config.SetHTTPSProxyPort(8080);
    config.SetHTTPSProxyUser("proxyuser");
    config.SetHTTPSProxyPassword("proxypass");
    
    EXPECT_EQ(config.GetHTTPSProxyUser(), "proxyuser");
    EXPECT_EQ(config.GetHTTPSProxyPassword(), "proxypass");
}

TEST(BROWSER_IDC_AUTH_TEST_SUITE, test_proxy_disabled_for_idp) {
    IAMConfiguration config;
    config.SetUsingHTTPSProxy(true);
    config.SetUseProxyIdpAuth(false);
    config.SetHTTPSProxyHost("proxy.example.com");
    config.SetHTTPSProxyPort(8080);
    
    EXPECT_TRUE(config.GetUsingHTTPSProxy());
    EXPECT_FALSE(config.GetUseProxyIdpAuth());
}
