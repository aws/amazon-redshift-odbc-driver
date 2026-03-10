#include "NativePluginCredentialsProvider.h"

using namespace Redshift::IamSupport;
using namespace Aws::Auth;

namespace 
{
    static const char* LOG_TAG = "NativePluginCredentialsProvider";
}

NativePluginCredentialsProvider::NativePluginCredentialsProvider(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMPluginCredentialsProvider( in_config, in_argsMap)
{
    RS_LOG_DEBUG("IAMCRD", "NativePluginCredentialsProvider::NativePluginCredentialsProvider");
}

AWSCredentials NativePluginCredentialsProvider::GetAWSCredentials()
{
    RS_LOG_DEBUG("IAMCRD", "NativePluginCredentialsProvider::GetAWSCredentials");
    return AWSCredentials();
}

void NativePluginCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "NativePluginCredentialsProvider::ValidateArgumentsMap");
}

rs_string NativePluginCredentialsProvider::GetAuthTokenType()
{
    RS_LOG_DEBUG("IAMCRD", "NativePluginCredentialsProvider::GetAuthTokenType");
    // Default implementation returns the token_type from m_argsMap if present
    auto it = m_argsMap.find(KEY_IDP_AUTH_TOKEN_TYPE);
    if (it != m_argsMap.end()) {
        return it->second;
    }
    return "";
}

NativePluginCredentialsProvider::~NativePluginCredentialsProvider()
{
    RS_LOG_DEBUG("IAMCRD", "NativePluginCredentialsProvider::~NativePluginCredentialsProvider");
}