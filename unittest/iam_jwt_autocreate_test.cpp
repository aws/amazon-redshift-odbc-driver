/**
 * Regression tests for connection-level AutoCreate propagation through the
 * JWT IAM plugin.
 */

#include "common.h"
#include "iam/RsSettings.h"
#include "iam/plugins/JwtIamAuthPlugin.h"

using namespace Redshift::IamSupport;

#define IAM_JWT_AUTOCREATE_TEST_SUITE IamJwtAutoCreateTest

namespace
{
class JwtIamAuthPluginTestHelper : public JwtIamAuthPlugin
{
public:
    explicit JwtIamAuthPluginTestHelper(
        const IAMConfiguration& in_config = IAMConfiguration(),
        const std::map<rs_string, rs_string>& in_argsMap =
            std::map<rs_string, rs_string>())
        : JwtIamAuthPlugin(in_config, in_argsMap)
    {
    }

    const std::map<rs_string, rs_string>& GetArguments() const
    {
        return m_argsMap;
    }
};

JWTAssertion MakeJwtAssertion()
{
    return JWTAssertion{"", R"({"DbUser":"jwt-user"})", ""};
}
} // namespace

TEST(IAM_JWT_AUTOCREATE_TEST_SUITE,
     ExplicitFalseIsNotOverwrittenByJwtPlugin)
{
    std::map<rs_string, rs_string> argsMap = {
        {IAM_KEY_AUTOCREATE, "0"}
    };
    IAMConfiguration config;
    JwtIamAuthPluginTestHelper plugin(config, argsMap);

    plugin.RetrieveDbUserField(MakeJwtAssertion());

    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_AUTOCREATE), "0");
    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_DBUSER), "jwt-user");
}

TEST(IAM_JWT_AUTOCREATE_TEST_SUITE,
     DefaultAutoCreateRemainsEnabledThroughJwtPlugin)
{
    RsSettings settings;
    ASSERT_TRUE(settings.m_userAutoCreate);

    IAMConfiguration config;
    config.SetAutoCreate(settings.m_userAutoCreate);
    JwtIamAuthPluginTestHelper plugin(config);

    plugin.RetrieveDbUserField(MakeJwtAssertion());

    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_AUTOCREATE), "1");
}

TEST(IAM_JWT_AUTOCREATE_TEST_SUITE,
     ExplicitTrueRemainsEnabledThroughJwtPlugin)
{
    IAMConfiguration config;
    config.SetAutoCreate(true);
    JwtIamAuthPluginTestHelper plugin(config);

    plugin.RetrieveDbUserField(MakeJwtAssertion());

    EXPECT_EQ(plugin.GetArguments().at(IAM_KEY_AUTOCREATE), "1");
}
