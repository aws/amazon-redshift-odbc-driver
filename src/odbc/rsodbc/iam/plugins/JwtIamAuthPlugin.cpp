#include "JwtIamAuthPlugin.h"
#include "IAMUtils.h"

using namespace Redshift::IamSupport;

namespace
{
    static const rs_string DEFAULT_ROLE_SESSION_NAME = "jwt_redshift_session";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
JwtIamAuthPlugin::JwtIamAuthPlugin(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMJwtPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("JwtIamAuthPlugin::JwtIamAuthPlugin");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void JwtIamAuthPlugin::InitArgumentsMap()
{
    RS_LOG(m_log)( "JwtIamAuthPlugin::InitArgumentsMap");
    IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void JwtIamAuthPlugin::ValidateArgumentsMap()
{
    RS_LOG(m_log)("JwtIamAuthPlugin::ValidateArgumentsMap");

    if (!m_argsMap.count(IAM_KEY_WEB_IDENTITY_TOKEN))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Authentication failed, please verify that web_identity_token is provided.");
    }

    if (!m_argsMap.count(IAM_KEY_ROLE_ARN))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Authentication failed, please verify that role_arn is provided.");
    }

    if (!m_argsMap.count(IAM_KEY_ROLE_SESSION_NAME))
    {
        m_argsMap[IAM_KEY_ROLE_SESSION_NAME] = DEFAULT_ROLE_SESSION_NAME;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string JwtIamAuthPlugin::GetJwtAssertion()
{
    RS_LOG(m_log)("JwtIamAuthPlugin::GetJwtAssertion");
    rs_string jwt = m_argsMap[IAM_KEY_WEB_IDENTITY_TOKEN];
    return jwt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
JwtIamAuthPlugin::~JwtIamAuthPlugin()
{
    RS_LOG(m_log)("JwtIamAuthPlugin::~JwtIamAuthPlugin");
    /* Do nothing */
}
