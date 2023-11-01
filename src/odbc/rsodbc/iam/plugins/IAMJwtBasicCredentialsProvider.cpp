#include "IAMJwtBasicCredentialsProvider.h"
#include "IAMUtils.h"

using namespace Redshift::IamSupport;

namespace
{
    static const rs_string DEFAULT_ROLE_SESSION_NAME = "jwt_redshift_session";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMJwtBasicCredentialsProvider::IAMJwtBasicCredentialsProvider(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMJwtPluginCredentialsProvider( in_config, in_argsMap)
{
    RS_LOG_DEBUG("IAMCRD", "IAMJwtBasicCredentialsProvider::IAMJwtBasicCredentialsProvider");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMJwtBasicCredentialsProvider::InitArgumentsMap()
{
    RS_LOG_DEBUG( "IAMJwtBasicCredentialsProvider", "InitArgumentsMap");
    IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMJwtBasicCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "IAMJwtBasicCredentialsProvider::ValidateArgumentsMap");

    if (!m_argsMap.count(IAM_KEY_WEB_IDENTITY_TOKEN))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Authentication failed, please verify that WEB_IDENTITY_TOKEN is provided.");
    }

    if (!m_argsMap.count(IAM_KEY_ROLE_ARN))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Authentication failed, please verify that ROLE_ARN is provided.");
    }

    if (!m_argsMap.count(IAM_KEY_ROLE_SESSION_NAME))
    {
        m_argsMap[IAM_KEY_ROLE_SESSION_NAME] = DEFAULT_ROLE_SESSION_NAME;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMJwtBasicCredentialsProvider::GetJwtAssertion()
{
    RS_LOG_DEBUG("IAMCRD", "IAMJwtBasicCredentialsProvider::GetJwtAssertion");

    rs_string jwt = m_argsMap[IAM_KEY_WEB_IDENTITY_TOKEN];

    RS_LOG_DEBUG("IAMCRD", "IAMJwtBasicCredentialsProvider::GetJwtAssertion JWT Assertion: %s", jwt.c_str());

    return jwt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMJwtBasicCredentialsProvider::~IAMJwtBasicCredentialsProvider()
{
    RS_LOG_DEBUG("IAMCRD", "IAMJwtBasicCredentialsProvider::~IAMJwtBasicCredentialsProvider");
    /* Do nothing */
}
