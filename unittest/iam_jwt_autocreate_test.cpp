/**
 * Unit tests for connection-level AutoCreate handling in the JWT IAM plugin.
 *
 * When the JWT plugin extracts the database user from the token assertion, it
 * must apply that DbUser without altering the caller's AutoCreate preference.
 * These tests exercise RetrieveDbUserField directly and assert that the
 * AutoCreate entry in the arguments map is left exactly as the connection
 * configured it -- present or absent, "0" or "1".
 */

#include "common.h"
#include "iam/plugins/IAMJwtPluginCredentialsProvider.h"
#include "iam/core/IAMConfiguration.h"

using namespace Redshift::IamSupport;

#define IAM_JWT_AUTOCREATE_TEST_SUITE IamJwtAutoCreateTest

namespace
{
    /**
     * Test helper exposing the protected arguments map and the DbUser-retrieval
     * step so the test can inspect what the JWT plugin writes to the args map.
     */
    class JwtAutoCreateTestHelper : public IAMJwtPluginCredentialsProvider
    {
    public:
        explicit JwtAutoCreateTestHelper(
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
                = std::map<rs_string, rs_string>())
            : IAMJwtPluginCredentialsProvider(in_config, in_argsMap) {}

        const std::map<rs_string, rs_string>& GetArguments() const { return m_argsMap; }

        void CallRetrieveDbUserField(const JWTAssertion& jwt) { RetrieveDbUserField(jwt); }
    };

    // RetrieveDbUserField parses the (already-decoded) JSON payload for DbUser,
    // so a plain JSON string is all that is required here.
    JWTAssertion MakeJwtAssertion(const rs_string& dbuser)
    {
        JWTAssertion jwt;
        jwt.payload = "{\"DbUser\":\"" + dbuser + "\"}";
        return jwt;
    }
}

// The database user encoded in the JWT assertion must be written to the args map.
TEST(IAM_JWT_AUTOCREATE_TEST_SUITE, DbUserIsRetrievedFromJwt)
{
    std::map<rs_string, rs_string> argsMap;
    JwtAutoCreateTestHelper plugin(IAMConfiguration(), argsMap);

    plugin.CallRetrieveDbUserField(MakeJwtAssertion("jwt-user"));

    ASSERT_EQ(plugin.GetArguments().count(IAM_KEY_DBUSER), 1u);
    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_DBUSER), "jwt-user");
}

// AutoCreate explicitly disabled on the connection must stay disabled after the
// DbUser is extracted from the assertion.
TEST(IAM_JWT_AUTOCREATE_TEST_SUITE, ExplicitFalseIsNotOverwrittenByJwtPlugin)
{
    std::map<rs_string, rs_string> argsMap;
    argsMap[IAM_KEY_AUTOCREATE] = "0";
    JwtAutoCreateTestHelper plugin(IAMConfiguration(), argsMap);

    plugin.CallRetrieveDbUserField(MakeJwtAssertion("jwt-user"));

    ASSERT_EQ(plugin.GetArguments().count(IAM_KEY_AUTOCREATE), 1u);
    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_AUTOCREATE), "0");
}

// When AutoCreate is not set on the connection, the plugin must not add it; an
// absent entry preserves the driver's default (disabled).
TEST(IAM_JWT_AUTOCREATE_TEST_SUITE, DefaultAutoCreateIsNotInjectedByJwtPlugin)
{
    std::map<rs_string, rs_string> argsMap;
    JwtAutoCreateTestHelper plugin(IAMConfiguration(), argsMap);

    plugin.CallRetrieveDbUserField(MakeJwtAssertion("jwt-user"));

    EXPECT_EQ(plugin.GetArguments().count(IAM_KEY_AUTOCREATE), 0u);
}

// AutoCreate explicitly enabled on the connection must stay enabled.
TEST(IAM_JWT_AUTOCREATE_TEST_SUITE, ExplicitTrueRemainsEnabledThroughJwtPlugin)
{
    std::map<rs_string, rs_string> argsMap;
    argsMap[IAM_KEY_AUTOCREATE] = "1";
    JwtAutoCreateTestHelper plugin(IAMConfiguration(), argsMap);

    plugin.CallRetrieveDbUserField(MakeJwtAssertion("jwt-user"));

    ASSERT_EQ(plugin.GetArguments().count(IAM_KEY_AUTOCREATE), 1u);
    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_AUTOCREATE), "1");
}
