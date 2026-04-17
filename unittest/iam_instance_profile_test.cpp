// iam_instance_profile_test.cpp
//
// Unit tests for EC2 instance profile credentials and credential_source support
// in the Redshift ODBC driver's IAM authentication layer.
#include "common.h"
#include "iam/RsErrorException.h"
#include "iam/RsIamClient.h"
#include "iam/RsIamHelper.h"
#include "iam/RsSettings.h"
#include "iam/core/IAMConfiguration.h"
#include "iam/core/IAMFactory.h"
#include "iam/core/IAMProfileCredentialsProvider.h"

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <filesystem>
#include <process.h> // _getpid
#endif

using namespace Redshift::IamSupport;
using namespace Aws::Auth;

// Initialize the AWS SDK for the lifetime of the test process.
// Uses RsIamClient which manages Aws::InitAPI/ShutdownAPI via internal
// reference counting, so we don't need to link aws-cpp-sdk-core directly into
// the test executable on Windows.
class AwsSdkEnvironment : public ::testing::Environment {
  public:
    void SetUp() override {
        RsSettings settings;
        m_client = std::make_unique<RedshiftODBC::RsIamClient>(settings);
    }
    void TearDown() override { m_client.reset(); }

  private:
    std::unique_ptr<RedshiftODBC::RsIamClient> m_client;
};

static ::testing::Environment *const aws_env =
    ::testing::AddGlobalTestEnvironment(new AwsSdkEnvironment);

// =============================================================================
// Factory method tests
//
// The IAMFactory had a bug where CreateInstanceProfileCredentialsProvider and
// CreateDefaultCredentialsProvider used std::shared_ptr<T>() (null pointer)
// instead of std::make_shared<T>() (valid instance). These tests verify the
// fix by checking that the returned providers are non-null and of the correct
// concrete type.
// =============================================================================

TEST(IAM_INSTANCE_PROFILE_TEST_SUITE,
     CreateInstanceProfileCredentialsProvider_ReturnsNonNull) {
    auto provider = IAMFactory::CreateInstanceProfileCredentialsProvider();
    ASSERT_NE(provider, nullptr) << "CreateInstanceProfileCredentialsProvider "
                                    "must return a valid provider, not null";
}

TEST(IAM_INSTANCE_PROFILE_TEST_SUITE,
     CreateInstanceProfileCredentialsProvider_ReturnsCorrectType) {
    auto provider = IAMFactory::CreateInstanceProfileCredentialsProvider();
    ASSERT_NE(provider, nullptr);
    auto *concrete =
        dynamic_cast<InstanceProfileCredentialsProvider *>(provider.get());
    ASSERT_NE(concrete, nullptr)
        << "CreateInstanceProfileCredentialsProvider must return an "
           "InstanceProfileCredentialsProvider instance";
}

TEST(IAM_INSTANCE_PROFILE_TEST_SUITE,
     CreateDefaultCredentialsProvider_ReturnsNonNull) {
    auto provider = IAMFactory::CreateDefaultCredentialsProvider();
    ASSERT_NE(provider, nullptr) << "CreateDefaultCredentialsProvider must "
                                    "return a valid provider, not null";
}

TEST(IAM_INSTANCE_PROFILE_TEST_SUITE,
     CreateDefaultCredentialsProvider_ReturnsCorrectType) {
    auto provider = IAMFactory::CreateDefaultCredentialsProvider();
    ASSERT_NE(provider, nullptr);
    auto *concrete =
        dynamic_cast<DefaultAWSCredentialsProviderChain *>(provider.get());
    ASSERT_NE(concrete, nullptr)
        << "CreateDefaultCredentialsProvider must return a "
           "DefaultAWSCredentialsProviderChain instance";
}

TEST(IAM_INSTANCE_PROFILE_TEST_SUITE,
     CreateStaticCredentialsProvider_ReturnsNonNull) {
    IAMConfiguration config;
    config.SetAccessId("AKIAEXAMPLEKEYID1234");
    config.SetSecretKey("dummySecretKeyForTesting123");
    auto provider = IAMFactory::CreateStaticCredentialsProvider(config);
    ASSERT_NE(provider, nullptr);
}

// =============================================================================
// Cross-platform helpers for temp files and environment variables
// =============================================================================
namespace {

inline int getPid() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

inline std::string getTempDir() {
#ifdef _WIN32
    return (std::filesystem::temp_directory_path() / "").string();
#else
    return "/tmp/";
#endif
}

inline void setEnvVar(const std::string &name, const std::string &value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

inline void unsetEnvVar(const std::string &name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

} // namespace

// RAII helper: writes content to a temp file on construction, deletes it on
// destruction. Ensures cleanup even if the test throws.
class ScopedTempFile {
  public:
    ScopedTempFile(const std::string &prefix, const std::string &content)
        : m_path(getTempDir() + prefix + "_" + std::to_string(getPid())) {
        std::ofstream ofs(m_path);
        ofs << content;
        ofs.close();
    }
    ~ScopedTempFile() { std::remove(m_path.c_str()); }
    const std::string &path() const { return m_path; }

  private:
    std::string m_path;
};

// RAII helper: sets an environment variable on construction, restores original
// on destruction.
class ScopedEnvVar {
  public:
    ScopedEnvVar(const std::string &name, const std::string &value)
        : m_name(name) {
        const char *old = std::getenv(name.c_str());
        m_hadOldValue = (old != nullptr);
        if (m_hadOldValue)
            m_oldValue = old;
        setEnvVar(name, value);
    }
    ~ScopedEnvVar() {
        if (m_hadOldValue)
            setEnvVar(m_name, m_oldValue);
        else
            unsetEnvVar(m_name);
    }

  private:
    std::string m_name;
    std::string m_oldValue;
    bool m_hadOldValue;
};

// =============================================================================
// credential_source error handling tests
//
// Per the AWS SDK specification, credential_source and source_profile are
// mutually exclusive, and credential_source requires role_arn. These tests
// verify the driver returns clear error messages for invalid configurations.
// =============================================================================

TEST(CREDENTIAL_SOURCE_ERROR_TEST_SUITE,
     MutualExclusivity_CredentialSourceAndSourceProfile) {
    // A profile with both credential_source and source_profile is invalid.
    // The driver should reject this with a clear error before attempting any
    // STS calls.
    std::string credContent =
        "[test_mutual_excl]\n"
        "role_arn = arn:aws:iam::123456789012:role/TestRole\n"
        "credential_source = Ec2InstanceMetadata\n"
        "source_profile = default\n"
        "\n"
        "[default]\n"
        "aws_access_key_id = AKIAEXAMPLEKEYID1234\n"
        "aws_secret_access_key = dummySecretKey123\n";

    ScopedTempFile credFile("rs_test_credentials", credContent);
    ScopedTempFile configFile("rs_test_config", "");

    ScopedEnvVar credEnv("AWS_SHARED_CREDENTIALS_FILE", credFile.path());
    ScopedEnvVar configEnv("AWS_CONFIG_FILE", configFile.path());

    IAMConfiguration config;
    config.SetProfileName("test_mutual_excl");
    IAMProfileCredentialsProvider provider(config);

    bool exceptionThrown = false;
    std::string errorMessage;
    try {
        provider.GetAWSCredentials();
        FAIL() << "Expected RsErrorException for mutual exclusivity violation";
    } catch (const RsErrorException &e) {
        exceptionThrown = true;
        errorMessage = e.what();
    }

    EXPECT_TRUE(exceptionThrown);
    if (exceptionThrown) {
        EXPECT_NE(errorMessage.find("credential_source"), std::string::npos)
            << "Error message should mention credential_source, got: "
            << errorMessage;
        EXPECT_NE(errorMessage.find("source_profile"), std::string::npos)
            << "Error message should mention source_profile, got: "
            << errorMessage;
    }
}

TEST(CREDENTIAL_SOURCE_ERROR_TEST_SUITE, CredentialSourceRequiresRoleArn) {
    // credential_source only makes sense with role_arn — it provides base
    // credentials for an AssumeRole call. Without role_arn there's nothing to
    // assume.
    std::string credContent = "[test_no_role]\n"
                              "credential_source = Ec2InstanceMetadata\n";

    ScopedTempFile credFile("rs_test_credentials", credContent);
    ScopedTempFile configFile("rs_test_config", "");

    ScopedEnvVar credEnv("AWS_SHARED_CREDENTIALS_FILE", credFile.path());
    ScopedEnvVar configEnv("AWS_CONFIG_FILE", configFile.path());

    IAMConfiguration config;
    config.SetProfileName("test_no_role");
    IAMProfileCredentialsProvider provider(config);

    bool exceptionThrown = false;
    std::string errorMessage;
    try {
        provider.GetAWSCredentials();
        FAIL() << "Expected RsErrorException when credential_source has no "
                  "role_arn";
    } catch (const RsErrorException &e) {
        exceptionThrown = true;
        errorMessage = e.what();
    }

    EXPECT_TRUE(exceptionThrown);
    if (exceptionThrown) {
        EXPECT_NE(errorMessage.find("credential_source"), std::string::npos)
            << "Error message should mention credential_source, got: "
            << errorMessage;
        EXPECT_NE(errorMessage.find("role_arn"), std::string::npos)
            << "Error message should mention role_arn, got: " << errorMessage;
    }
}

// =============================================================================
// Fuzz test: unrecognized credential_source values
//
// Generates 100 random printable-ASCII strings (none matching the three valid
// values) and verifies that CreateCredentialSourceProvider throws an error
// containing the invalid value. Uses a fixed RNG seed so the test is
// deterministic and reproducible across runs.
// =============================================================================

static std::vector<std::string>
GenerateRandomInvalidCredentialSources(size_t count) {
    static const std::vector<std::string> validValues = {
        "Ec2InstanceMetadata", "Environment", "EcsContainer"};

    // Fixed seed for deterministic, reproducible test runs
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> lenDist(1, 50);
    std::uniform_int_distribution<int> charDist(32, 126); // printable ASCII

    std::vector<std::string> results;
    results.reserve(count);

    while (results.size() < count) {
        int len = lenDist(rng);
        std::string s;
        s.reserve(len);
        for (int i = 0; i < len; ++i) {
            s.push_back(static_cast<char>(charDist(rng)));
        }

        bool isValid = false;
        for (const auto &v : validValues) {
            if (s == v) {
                isValid = true;
                break;
            }
        }
        if (!isValid) {
            results.push_back(std::move(s));
        }
    }
    return results;
}

class CredentialSourceInvalidValueTest
    : public ::testing::TestWithParam<std::string> {};

TEST_P(CredentialSourceInvalidValueTest,
       UnrecognizedValueThrowsErrorContainingValue) {
    const std::string &invalidValue = GetParam();

    IAMConfiguration config;
    IAMProfileCredentialsProvider provider(config);

    bool exceptionThrown = false;
    std::string errorMessage;
    try {
        provider.CreateCredentialSourceProvider(invalidValue);
        FAIL() << "Expected RsErrorException for invalid credential_source: "
               << invalidValue;
    } catch (const RsErrorException &e) {
        exceptionThrown = true;
        errorMessage = e.what();
    }

    EXPECT_TRUE(exceptionThrown)
        << "CreateCredentialSourceProvider should throw for invalid value: "
        << invalidValue;
    if (exceptionThrown) {
        EXPECT_NE(errorMessage.find(invalidValue), std::string::npos)
            << "Error message should contain the invalid value '"
            << invalidValue << "', got: " << errorMessage;
    }
}

INSTANTIATE_TEST_SUITE_P(
    RandomInvalidCredentialSources, CredentialSourceInvalidValueTest,
    ::testing::ValuesIn(GenerateRandomInvalidCredentialSources(100)));

// =============================================================================
// Valid credential_source mapping tests
//
// Each valid credential_source value must map to the correct AWS SDK provider:
//   "Ec2InstanceMetadata" → InstanceProfileCredentialsProvider
//   "Environment"         → EnvironmentAWSCredentialsProvider
//   "EcsContainer"        → GeneralHTTPCredentialsProvider (ECS task role)
// =============================================================================

TEST(CREDENTIAL_SOURCE_VALID_TEST_SUITE,
     Ec2InstanceMetadata_ReturnsNonNullInstanceProfileProvider) {
    IAMConfiguration config;
    IAMProfileCredentialsProvider provider(config);

    auto result =
        provider.CreateCredentialSourceProvider("Ec2InstanceMetadata");
    ASSERT_NE(result, nullptr);

    auto *concrete =
        dynamic_cast<InstanceProfileCredentialsProvider *>(result.get());
    EXPECT_NE(concrete, nullptr) << "Ec2InstanceMetadata should produce an "
                                    "InstanceProfileCredentialsProvider";
}

TEST(CREDENTIAL_SOURCE_VALID_TEST_SUITE,
     Environment_ReturnsNonNullEnvironmentProvider) {
    IAMConfiguration config;
    IAMProfileCredentialsProvider provider(config);

    auto result = provider.CreateCredentialSourceProvider("Environment");
    ASSERT_NE(result, nullptr);

    auto *concrete =
        dynamic_cast<EnvironmentAWSCredentialsProvider *>(result.get());
    EXPECT_NE(concrete, nullptr)
        << "Environment should produce an EnvironmentAWSCredentialsProvider";
}

TEST(CREDENTIAL_SOURCE_VALID_TEST_SUITE,
     EcsContainer_ReturnsNonNullTaskRoleProvider) {
    IAMConfiguration config;
    IAMProfileCredentialsProvider provider(config);

    auto result = provider.CreateCredentialSourceProvider("EcsContainer");
    ASSERT_NE(result, nullptr) << "EcsContainer should produce a non-null "
                                  "provider (GeneralHTTPCredentialsProvider)";
}

// =============================================================================
// Auth type inference tests
//
// When InstanceProfile=1 is set in the connection string/DSN and no explicit
// auth type, plugin, AWS profile, or static credentials are provided,
// InferCredentialsProvider() must return "InstanceProfile".
//
// InferCredentialsProvider is protected (moved from private) to allow testing
// via the TestableRsIamClient subclass below. This is safe because:
//   - No external code subclasses RsIamClient (it's an internal implementation
//   class)
//   - The method has no side effects; it only reads RsSettings and returns a
//   string
//   - The alternative (friend class or #define private public) is worse
// =============================================================================

// Subclass that exposes the protected InferCredentialsProvider for testing.
class TestableRsIamClient : public RedshiftODBC::RsIamClient {
  public:
    TestableRsIamClient(const RsSettings &settings)
        : RedshiftODBC::RsIamClient(settings) {}

    rs_string TestInferCredentialsProvider() {
        return InferCredentialsProvider();
    }
};

// Create RsSettings with m_useInstanceProfile=true and all auth-relevant fields
// empty.
static RsSettings MakeInstanceProfileSettings() {
    RsSettings s;
    s.m_useInstanceProfile = true;
    s.m_authType = "";
    s.m_pluginName = L"";
    s.m_awsProfile = "";
    s.m_accessKeyID = "";
    s.m_secretAccessKey = "";
    return s;
}

// Basic case: all auth fields empty, useInstanceProfile=true →
// "InstanceProfile"
TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_BasicEmpty_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

// AuthType=Profile should still resolve to InstanceProfile when the flag is set
TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_AuthTypeProfile_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_authType = "Profile";
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

// The following tests verify that unrelated connection fields (host, database,
// region, etc.) do NOT affect the inference result. The inference logic should
// only look at m_useInstanceProfile, m_authType, m_pluginName, m_awsProfile,
// and m_accessKeyID — other fields must be ignored.

TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_WithHost_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_host = "my-cluster.abc123.us-east-1.redshift.amazonaws.com";
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_WithDatabase_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_database = "mydb";
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_WithDbUser_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_dbUser = "admin";
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_WithRegion_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_awsRegion = "us-west-2";
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

TEST(INFER_CREDENTIALS_PROVIDER_TEST_SUITE,
     InstanceProfile_WithClusterId_ReturnsInstanceProfile) {
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_clusterIdentifer = "my-cluster";
    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile");
}

// Randomized parameterized test: combines random values for all unrelated
// fields to ensure inference is robust across many combinations.
struct InstanceProfileVariation {
    std::string description;
    std::string host;
    std::string database;
    std::string dbUser;
    std::string region;
    std::string clusterId;
    std::string
        authType; // empty or "Profile" — both should yield "InstanceProfile"
};

class InstanceProfileInferenceTest
    : public ::testing::TestWithParam<InstanceProfileVariation> {};

TEST_P(InstanceProfileInferenceTest, ReturnsInstanceProfile) {
    const auto &param = GetParam();
    RsSettings settings = MakeInstanceProfileSettings();
    settings.m_host = param.host;
    settings.m_database = param.database;
    settings.m_dbUser = param.dbUser;
    settings.m_awsRegion = param.region;
    settings.m_clusterIdentifer = param.clusterId;
    settings.m_authType = param.authType;

    TestableRsIamClient client(settings);
    EXPECT_EQ(client.TestInferCredentialsProvider(), "InstanceProfile")
        << "Failed for variation: " << param.description;
}

static std::vector<InstanceProfileVariation>
GenerateInstanceProfileVariations() {
    std::mt19937 rng(12345); // fixed seed for reproducibility

    static const std::vector<std::string> hosts = {
        "", "cluster.abc.us-east-1.redshift.amazonaws.com", "localhost",
        "10.0.0.1"};
    static const std::vector<std::string> databases = {"", "dev", "production",
                                                       "test_db"};
    static const std::vector<std::string> dbUsers = {
        "", "admin", "readonly_user", "iam:testuser"};
    static const std::vector<std::string> regions = {
        "", "us-east-1", "eu-west-1", "ap-southeast-2"};
    static const std::vector<std::string> clusterIds = {
        "", "my-cluster", "prod-redshift-01", "test-cluster-xyz"};
    // authType: empty or "Profile" — both should resolve to InstanceProfile
    // when the flag is set
    static const std::vector<std::string> authTypes = {"", "Profile"};

    std::vector<InstanceProfileVariation> variations;
    variations.reserve(100);

    for (size_t i = 0; i < 100; ++i) {
        std::uniform_int_distribution<size_t> hostDist(0, hosts.size() - 1);
        std::uniform_int_distribution<size_t> dbDist(0, databases.size() - 1);
        std::uniform_int_distribution<size_t> userDist(0, dbUsers.size() - 1);
        std::uniform_int_distribution<size_t> regionDist(0, regions.size() - 1);
        std::uniform_int_distribution<size_t> clusterDist(0, clusterIds.size() -
                                                                 1);
        std::uniform_int_distribution<size_t> authDist(0, authTypes.size() - 1);

        InstanceProfileVariation v;
        v.host = hosts[hostDist(rng)];
        v.database = databases[dbDist(rng)];
        v.dbUser = dbUsers[userDist(rng)];
        v.region = regions[regionDist(rng)];
        v.clusterId = clusterIds[clusterDist(rng)];
        v.authType = authTypes[authDist(rng)];
        v.description = "variation_" + std::to_string(i) + " authType=" +
                        (v.authType.empty() ? "(empty)" : v.authType) +
                        " host=" + (v.host.empty() ? "(empty)" : v.host);
        variations.push_back(std::move(v));
    }
    return variations;
}

INSTANTIATE_TEST_SUITE_P(
    RandomInstanceProfileVariations, InstanceProfileInferenceTest,
    ::testing::ValuesIn(GenerateInstanceProfileVariations()));

// =============================================================================
// Credential caching tests
//
// Instance profile credentials come from IMDS and are temporary (rotated by
// EC2). The driver must NOT cache them — IsValidIamCachedSettings must return
// false whenever m_useInstanceProfile is true, regardless of any other
// settings.
//
// IsValidIamCachedSettings is a public static method (moved from private). This
// is safe because it's a pure function with no side effects — it takes a const
// RsSettings reference and a bool, and returns a bool. There's no encapsulation
// concern with making a stateless validation function public.
// =============================================================================

TEST(CACHE_DISABLED_TEST_SUITE, InstanceProfile_DisablesCache_NonNativeAuth) {
    RsSettings settings;
    settings.m_useInstanceProfile = true;
    EXPECT_FALSE(RsIamHelper::IsValidIamCachedSettings(settings, false))
        << "IsValidIamCachedSettings must return false when "
           "m_useInstanceProfile=true";
}

TEST(CACHE_DISABLED_TEST_SUITE, InstanceProfile_DisablesCache_NativeAuth) {
    RsSettings settings;
    settings.m_useInstanceProfile = true;
    EXPECT_FALSE(RsIamHelper::IsValidIamCachedSettings(settings, true))
        << "IsValidIamCachedSettings must return false when "
           "m_useInstanceProfile=true (native auth)";
}

// Randomized: verify caching stays disabled regardless of other settings
// combinations.
struct CacheDisabledVariation {
    std::string description;
    std::string host;
    std::string database;
    std::string dbUser;
    std::string region;
    std::string authType;
    std::string accessKeyID;
    std::string secretAccessKey;
    bool disableCache;
    bool isNativeAuth;
};

class InstanceProfileCacheDisabledTest
    : public ::testing::TestWithParam<CacheDisabledVariation> {};

TEST_P(InstanceProfileCacheDisabledTest, AlwaysReturnsFalse) {
    const auto &param = GetParam();
    RsSettings settings;
    settings.m_useInstanceProfile = true;
    settings.m_host = param.host;
    settings.m_database = param.database;
    settings.m_dbUser = param.dbUser;
    settings.m_awsRegion = param.region;
    settings.m_authType = param.authType;
    settings.m_accessKeyID = param.accessKeyID;
    settings.m_secretAccessKey = param.secretAccessKey;
    settings.m_disableCache = param.disableCache;

    EXPECT_FALSE(
        RsIamHelper::IsValidIamCachedSettings(settings, param.isNativeAuth))
        << "Failed for variation: " << param.description;
}

static std::vector<CacheDisabledVariation> GenerateCacheDisabledVariations() {
    std::mt19937 rng(99887); // fixed seed for reproducibility
    static const std::vector<std::string> hosts = {
        "", "cluster.abc.us-east-1.redshift.amazonaws.com", "localhost"};
    static const std::vector<std::string> databases = {"", "dev", "prod"};
    static const std::vector<std::string> dbUsers = {"", "admin", "testuser"};
    static const std::vector<std::string> regions = {"", "us-east-1",
                                                     "eu-west-1"};
    static const std::vector<std::string> authTypes = {"", "Profile", "Static"};
    static const std::vector<std::string> accessKeys = {"",
                                                        "AKIAEXAMPLEKEYID1234"};
    static const std::vector<std::string> secretKeys = {"",
                                                        "dummySecretKey123"};

    std::vector<CacheDisabledVariation> variations;
    variations.reserve(100);

    for (size_t i = 0; i < 100; ++i) {
        CacheDisabledVariation v;
        v.host = hosts[rng() % hosts.size()];
        v.database = databases[rng() % databases.size()];
        v.dbUser = dbUsers[rng() % dbUsers.size()];
        v.region = regions[rng() % regions.size()];
        v.authType = authTypes[rng() % authTypes.size()];
        v.accessKeyID = accessKeys[rng() % accessKeys.size()];
        v.secretAccessKey = secretKeys[rng() % secretKeys.size()];
        v.disableCache = (rng() % 2 == 0);
        v.isNativeAuth = (rng() % 2 == 0);
        v.description =
            "cache_var_" + std::to_string(i) +
            " native=" + (v.isNativeAuth ? "true" : "false") +
            " authType=" + (v.authType.empty() ? "(empty)" : v.authType) +
            " disableCache=" + (v.disableCache ? "true" : "false");
        variations.push_back(std::move(v));
    }
    return variations;
}

INSTANTIATE_TEST_SUITE_P(
    RandomCacheDisabledVariations, InstanceProfileCacheDisabledTest,
    ::testing::ValuesIn(GenerateCacheDisabledVariations()));
