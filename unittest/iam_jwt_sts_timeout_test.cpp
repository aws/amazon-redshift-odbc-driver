/**
 * Unit tests for STS client configuration in the JWT IAM plugin.
 *
 * The AssumeRoleWithWebIdentity call made during JWT/OAuth authentication must
 * use the connection-level StsConnectionTimeout and StsEndpointUrl, matching the
 * behavior of the other IAM credential providers. These tests apply those
 * settings to a client configuration via ApplyStsClientConnectionSettings and
 * assert that they are honored.
 */

#include "common.h"

// Constructing an Aws::Client::ClientConfiguration pulls in the AWS SDK HTTP
// stack, which on Windows requires system HTTP libraries that the unit-test
// binary does not link. The behavior under test is platform-independent, so the
// tests run on non-Windows platforms; the fix itself applies on all platforms.
#ifndef _WIN32

#include "iam/plugins/IAMJwtPluginCredentialsProvider.h"
#include "iam/core/IAMConfiguration.h"

#include <aws/core/client/ClientConfiguration.h>

using namespace Redshift::IamSupport;

#define IAM_JWT_STS_TIMEOUT_TEST_SUITE IamJwtStsTimeoutTest

namespace
{
    // Test helper exposing the STS client-config builder that
    // AssumeRoleWithJwtRequest uses to create its STS client.
    class JwtStsConfigTestHelper : public IAMJwtPluginCredentialsProvider
    {
    public:
        explicit JwtStsConfigTestHelper(
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
                = std::map<rs_string, rs_string>())
            : IAMJwtPluginCredentialsProvider(in_config, in_argsMap) {}

        Aws::Client::ClientConfiguration BuildConfig() { return BuildStsClientConfig(); }
    };
}

// An explicit StsConnectionTimeout must be applied to every timeout field.
TEST(IAM_JWT_STS_TIMEOUT_TEST_SUITE, ExplicitTimeoutIsAppliedToStsClientConfig)
{
    IAMConfiguration config;
    config.SetStsConnectionTimeout(5000 /* ms */);

    Aws::Client::ClientConfiguration stsConfig;
    ApplyStsClientConnectionSettings(stsConfig, config);

    EXPECT_EQ(stsConfig.connectTimeoutMs, 5000);
    EXPECT_EQ(stsConfig.requestTimeoutMs, 5000);
    EXPECT_EQ(stsConfig.httpRequestTimeoutMs, 5000);
}

// When the user omits StsConnectionTimeout, the driver initializes the IAM
// configuration with a zero value (RsIamClient::CreateIAMConfiguration), which
// IAMConfiguration resolves to the driver default. The STS client must then use
// that default rather than the AWS SDK's built-in connect timeout.
TEST(IAM_JWT_STS_TIMEOUT_TEST_SUITE, DefaultTimeoutIsAppliedWhenUnset)
{
    IAMConfiguration config;
    config.SetStsConnectionTimeout(0);  // mirrors the driver's handling of an unset value

    Aws::Client::ClientConfiguration stsConfig;
    ApplyStsClientConnectionSettings(stsConfig, config);

    EXPECT_EQ(stsConfig.connectTimeoutMs, DEFAULT_TIMEOUT);
    EXPECT_EQ(stsConfig.requestTimeoutMs, DEFAULT_TIMEOUT);
    EXPECT_EQ(stsConfig.httpRequestTimeoutMs, DEFAULT_TIMEOUT);
}

// A connection-level StsEndpointUrl must be applied as the endpoint override.
TEST(IAM_JWT_STS_TIMEOUT_TEST_SUITE, StsEndpointOverrideIsApplied)
{
    IAMConfiguration config;
    config.SetStsEndpointUrl("https://sts.example.com");

    Aws::Client::ClientConfiguration stsConfig;
    ApplyStsClientConnectionSettings(stsConfig, config);

    EXPECT_EQ(stsConfig.endpointOverride, "https://sts.example.com");
}

// The STS client configuration that AssumeRoleWithJwtRequest builds
// (BuildStsClientConfig) must carry the connection-level timeout and endpoint,
// i.e. the wiring from the request path through to ApplyStsClientConnectionSettings.
TEST(IAM_JWT_STS_TIMEOUT_TEST_SUITE, BuildStsClientConfigAppliesConnectionSettings)
{
    IAMConfiguration config;
    config.SetStsConnectionTimeout(7000 /* ms */);
    config.SetStsEndpointUrl("https://sts.example.com");

    JwtStsConfigTestHelper plugin(config);
    Aws::Client::ClientConfiguration stsConfig = plugin.BuildConfig();

    EXPECT_EQ(stsConfig.connectTimeoutMs, 7000);
    EXPECT_EQ(stsConfig.requestTimeoutMs, 7000);
    EXPECT_EQ(stsConfig.httpRequestTimeoutMs, 7000);
    EXPECT_EQ(stsConfig.endpointOverride, "https://sts.example.com");
}

#endif // !_WIN32
