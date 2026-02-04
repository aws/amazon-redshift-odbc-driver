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

NativePluginCredentialsProvider::~NativePluginCredentialsProvider()
{
    RS_LOG_DEBUG("IAMCRD", "NativePluginCredentialsProvider::~NativePluginCredentialsProvider");
}