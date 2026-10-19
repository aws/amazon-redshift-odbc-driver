/**
 * Unit tests for the IMDS-disabled STS client configuration in the IAM layer.
 *
 * The AWS SDK's default ClientConfiguration constructor queries the EC2
 * instance metadata service (IMDS) for the region when none is found in the
 * environment or AWS config file. On non-EC2 machines that blocks for ~10s per
 * attempt. The driver builds every client configuration through
 * IAMUtils::CreateClientConfiguration, which disables IMDS, and resolves the
 * STS region itself (DSN Region, else the Redshift hostname, else the SDK
 * default).
 *
 * The IMDS tests point the SDK's metadata client at a local listener and
 * count connections, so they are deterministic and need no network access.
 * A positive control confirms the SDK default constructor does reach the
 * listener, so the tests would fail if the driver reverted to it.
 */

#include "common.h"

// Constructing an Aws::Client::ClientConfiguration pulls in the AWS SDK HTTP
// stack, which on Windows requires system HTTP libraries that the unit-test
// binary does not link. The behavior under test is platform-independent, so the
// tests run on non-Windows platforms; the fix itself applies on all platforms.
#ifndef _WIN32

#include "iam/RsIamClient.h"
#include "iam/RsSettings.h"
#include "iam/core/IAMConfiguration.h"
#include "iam/core/IAMProfileCredentialsProvider.h"
#include "iam/core/IAMUtils.h"
#include "iam/http/IAMHttpClient.h"
#include "iam/plugins/BrowserIdcAuthPlugin.h"
#include "iam/plugins/IAMJwtPluginCredentialsProvider.h"

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/config/ConfigAndCredentialsCacheManager.h>
#include <aws/core/internal/AWSHttpResourceClient.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Redshift::IamSupport;

#define IAM_IMDS_DISABLED_TEST_SUITE IamImdsDisabledTest

namespace
{
    const char* const PROVISIONED_HOST =
        "redshiftj-iam-test.c2mf5zd3u3sv.us-west-2.redshift.amazonaws.com";
    const char* const SERVERLESS_HOST =
        "default.518627716765.eu-central-1.redshift-serverless.amazonaws.com";
    const char* const CNAME_HOST = "redshift-odbc-imds-test.invalid";

    // Test helpers exposing the STS client-config builders used by
    // IAMProfileCredentialsProvider::AssumeRole and
    // IAMJwtPluginCredentialsProvider::AssumeRoleWithJwtRequest.
    class ProfileStsConfigTestHelper : public IAMProfileCredentialsProvider
    {
    public:
        explicit ProfileStsConfigTestHelper(const IAMConfiguration& in_config)
            : IAMProfileCredentialsProvider(in_config) {}

        Aws::Client::ClientConfiguration BuildConfig() { return BuildStsClientConfig(); }
    };

    class JwtStsConfigTestHelper : public IAMJwtPluginCredentialsProvider
    {
    public:
        explicit JwtStsConfigTestHelper(const IAMConfiguration& in_config)
            : IAMJwtPluginCredentialsProvider(in_config, std::map<rs_string, rs_string>()) {}

        Aws::Client::ClientConfiguration BuildConfig() { return BuildStsClientConfig(); }
    };

    // Test helper exposing the SSO-OIDC client-config builder used by
    // BrowserIdcAuthPlugin::InitializeIdcClient.
    class IdcClientConfigTestHelper : public BrowserIdcAuthPlugin
    {
    public:
        explicit IdcClientConfigTestHelper(const IAMConfiguration& in_config)
            : BrowserIdcAuthPlugin(in_config) {}

        Aws::Client::ClientConfiguration BuildConfig(const std::string& in_region)
        {
            return BuildIdcClientConfig(in_region);
        }
    };

    // Sets an environment variable for the lifetime of the object and restores
    // its previous value (or absence) on destruction.
    class ScopedEnvVar
    {
    public:
        ScopedEnvVar(const char* in_name, const char* in_value) : m_name(in_name)
        {
            const char* previous = getenv(in_name);
            m_hadValue = previous != nullptr;
            if (m_hadValue)
            {
                m_previous = previous;
            }

            if (in_value)
            {
                setenv(in_name, in_value, 1);
            }
            else
            {
                unsetenv(in_name);
            }
        }

        ~ScopedEnvVar()
        {
            if (m_hadValue)
            {
                setenv(m_name.c_str(), m_previous.c_str(), 1);
            }
            else
            {
                unsetenv(m_name.c_str());
            }
        }

    private:
        std::string m_name;
        std::string m_previous;
        bool m_hadValue;
    };

    // Minimal HTTP listener on 127.0.0.1 standing in for IMDS. Every accepted
    // connection is counted and answered with 404, which the SDK does not
    // retry, so a positive control finishes quickly.
    class FakeImdsListener
    {
    public:
        // Binds and starts serving. Uses fatal assertions so a setup failure
        // stops the test instead of running it against a dead endpoint; call
        // via ASSERT_NO_FATAL_FAILURE.
        void Start()
        {
            m_socket = socket(AF_INET, SOCK_STREAM, 0);
            ASSERT_GE(m_socket, 0);

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = 0;
            ASSERT_EQ(bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
            ASSERT_EQ(listen(m_socket, 16), 0);

            socklen_t len = sizeof(addr);
            ASSERT_EQ(getsockname(m_socket, reinterpret_cast<sockaddr*>(&addr), &len), 0);
            ASSERT_NE(addr.sin_port, 0);
            m_endpoint = "http://127.0.0.1:" + std::to_string(ntohs(addr.sin_port));

            m_thread = std::thread([this]() { Serve(); });
        }

        ~FakeImdsListener()
        {
            m_stop = true;
            if (m_thread.joinable())
            {
                m_thread.join();
            }
            if (m_socket >= 0)
            {
                close(m_socket);
            }
        }

        const std::string& Endpoint() const { return m_endpoint; }
        int Connections() const { return m_connections; }

    private:
        void Serve()
        {
            while (!m_stop)
            {
                pollfd pfd = { m_socket, POLLIN, 0 };
                if (poll(&pfd, 1, 50 /* ms */) <= 0)
                {
                    continue;
                }

                int client = accept(m_socket, nullptr, nullptr);
                if (client < 0)
                {
                    continue;
                }
                m_connections++;

                char buffer[4096];
                pollfd cpfd = { client, POLLIN, 0 };
                if (poll(&cpfd, 1, 1000 /* ms */) > 0)
                {
                    (void)recv(client, buffer, sizeof(buffer), 0);
                }

                static const char RESPONSE[] =
                    "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                (void)send(client, RESPONSE, sizeof(RESPONSE) - 1, MSG_NOSIGNAL);
                close(client);
            }
        }

        int m_socket = -1;
        std::string m_endpoint;
        std::thread m_thread;
        std::atomic<bool> m_stop{ false };
        std::atomic<int> m_connections{ 0 };
    };

    // Fixture that clears every non-IMDS region source and redirects the SDK's
    // IMDS client to a local FakeImdsListener, so a ClientConfiguration that
    // consults IMDS is observable as a connection to the listener.
    class IamImdsDisabledTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // Keeps the SDK initialized for the duration of the test.
            m_sdk.reset(new RedshiftODBC::RsIamClient(m_settings));

            m_listener.reset(new FakeImdsListener());
            ASSERT_NO_FATAL_FAILURE(m_listener->Start());
            m_envVars.emplace_back(new ScopedEnvVar("AWS_REGION", nullptr));
            m_envVars.emplace_back(new ScopedEnvVar("AWS_DEFAULT_REGION", nullptr));
            m_envVars.emplace_back(new ScopedEnvVar("AWS_PROFILE", nullptr));
            m_envVars.emplace_back(new ScopedEnvVar("AWS_EC2_METADATA_DISABLED", nullptr));
            m_envVars.emplace_back(new ScopedEnvVar(
                "AWS_CONFIG_FILE", "/nonexistent/redshift-odbc-imds-test/config"));
            m_envVars.emplace_back(new ScopedEnvVar(
                "AWS_EC2_METADATA_SERVICE_ENDPOINT", m_listener->Endpoint().c_str()));

            ReloadSdkState();
        }

        void TearDown() override
        {
            m_envVars.clear();
            ReloadSdkState();
            m_listener.reset();
            m_sdk.reset();
        }

        int ImdsConnections() const { return m_listener->Connections(); }

    private:
        // The SDK reads the IMDS endpoint and AWS config file at InitAPI;
        // reload both so the environment set up above takes effect.
        static void ReloadSdkState()
        {
            Aws::Config::ReloadCachedConfigFile();
            Aws::Internal::CleanupEC2MetadataClient();
            Aws::Internal::InitEC2MetadataClient();
        }

        RsSettings m_settings;
        std::unique_ptr<RedshiftODBC::RsIamClient> m_sdk;
        std::unique_ptr<FakeImdsListener> m_listener;
        std::vector<std::unique_ptr<ScopedEnvVar>> m_envVars;
    };
}

// Positive control: the SDK default constructor does consult IMDS when no
// other region source is available. Validates that the listener observes it.
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, SdkDefaultConstructorQueriesImds)
{
    Aws::Client::ClientConfiguration config;
    EXPECT_GT(ImdsConnections(), 0);
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, CreateClientConfigurationDoesNotQueryImds)
{
    Aws::Client::ClientConfiguration config = IAMUtils::CreateClientConfiguration();
    EXPECT_EQ(ImdsConnections(), 0);
    EXPECT_TRUE(config.disableIMDS);
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ProfileStsConfigDoesNotQueryImds)
{
    IAMConfiguration iamConfig;
    ProfileStsConfigTestHelper provider(iamConfig);
    Aws::Client::ClientConfiguration config = provider.BuildConfig();
    EXPECT_EQ(ImdsConnections(), 0);
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, JwtStsConfigDoesNotQueryImds)
{
    IAMConfiguration iamConfig;
    JwtStsConfigTestHelper provider(iamConfig);
    Aws::Client::ClientConfiguration config = provider.BuildConfig();
    EXPECT_EQ(ImdsConnections(), 0);
}

// IdP HTTP client used by the SAML/IdP plugins (and IAMWinHttpClientDelegate).
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, IdpHttpClientConfigDoesNotQueryImds)
{
    HttpClientConfig httpConfig;
    Aws::Client::ClientConfiguration config = IAMHttpClient::GetClientConfiguration(httpConfig);
    EXPECT_EQ(ImdsConnections(), 0);
    EXPECT_TRUE(config.disableIMDS);
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, IdcClientConfigDoesNotQueryImds)
{
    IAMConfiguration iamConfig;
    IdcClientConfigTestHelper plugin(iamConfig);
    Aws::Client::ClientConfiguration config = plugin.BuildConfig("us-west-2");
    EXPECT_EQ(ImdsConnections(), 0);
    EXPECT_EQ(config.region, "us-west-2");
}

// STS region resolution: configured Region -> config.region.
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ProfileStsConfigUsesConfiguredRegion)
{
    IAMConfiguration iamConfig;
    iamConfig.SetRegion("ap-northeast-1");
    iamConfig.SetHost(PROVISIONED_HOST);
    ProfileStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "ap-northeast-1");
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, JwtStsConfigUsesConfiguredRegion)
{
    IAMConfiguration iamConfig;
    iamConfig.SetRegion("ap-northeast-1");
    iamConfig.SetHost(PROVISIONED_HOST);
    JwtStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "ap-northeast-1");
}

// Unset Region -> region inferred from the Redshift hostname.
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ProfileStsConfigInfersRegionFromHost)
{
    IAMConfiguration iamConfig;
    iamConfig.SetHost(PROVISIONED_HOST);
    ProfileStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "us-west-2");
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, JwtStsConfigInfersRegionFromServerlessHost)
{
    IAMConfiguration iamConfig;
    iamConfig.SetHost(SERVERLESS_HOST);
    JwtStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "eu-central-1");
}

// Unset Region and no inferable host -> SDK default, us-east-1.
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ProfileStsConfigDefaultsToUsEast1)
{
    IAMConfiguration iamConfig;
    iamConfig.SetHost("my-cluster.example.com");
    ProfileStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "us-east-1");
    EXPECT_EQ(ImdsConnections(), 0);
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, JwtStsConfigDefaultsToUsEast1)
{
    IAMConfiguration iamConfig;
    JwtStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "us-east-1");
    EXPECT_EQ(ImdsConnections(), 0);
}

TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ResolveStsRegion)
{
    EXPECT_EQ(IAMUtils::ResolveStsRegion("ap-south-1", PROVISIONED_HOST, false), "ap-south-1");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", PROVISIONED_HOST, false), "us-west-2");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", SERVERLESS_HOST, false), "eu-central-1");
    EXPECT_EQ(IAMUtils::ResolveStsRegion(
        "", "cluster.123456789012.cn-north-1.redshift.amazonaws.com.cn", false), "cn-north-1");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", "my-cluster.example.com", false), "");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", "", false), "");
}

// For a CNAME, the configured Region and a Redshift-pattern hostname still
// take precedence over the DNS lookup; an unresolvable CNAME (.invalid is
// guaranteed not to resolve, RFC 6761) falls through to empty.
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ResolveStsRegionCname)
{
    EXPECT_EQ(IAMUtils::ResolveStsRegion("ap-south-1", CNAME_HOST, true), "ap-south-1");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", PROVISIONED_HOST, true), "us-west-2");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", CNAME_HOST, true), "");
    EXPECT_EQ(IAMUtils::ResolveStsRegion("", "", true), "");
}

// An unresolvable CNAME with no Region keeps the SDK default.
TEST_F(IAM_IMDS_DISABLED_TEST_SUITE, ProfileStsConfigUnresolvableCnameDefaultsToUsEast1)
{
    IAMConfiguration iamConfig;
    iamConfig.SetHost(CNAME_HOST);
    iamConfig.SetIsCname(true);
    ProfileStsConfigTestHelper provider(iamConfig);
    EXPECT_EQ(provider.BuildConfig().region, "us-east-1");
    EXPECT_EQ(ImdsConnections(), 0);
}

#endif // !_WIN32
