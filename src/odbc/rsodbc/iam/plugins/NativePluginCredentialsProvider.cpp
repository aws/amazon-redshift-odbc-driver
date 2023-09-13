#include "NativePluginCredentialsProvider.h"

using namespace Redshift::IamSupport;
using namespace Aws::Auth;

namespace 
{
    static const char* LOG_TAG = "NativePluginCredentialsProvider";
}

NativePluginCredentialsProvider::NativePluginCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("NativePluginCredentialsProvider::NativePluginCredentialsProvider");
}

AWSCredentials NativePluginCredentialsProvider::GetAWSCredentials()
{
    RS_LOG(m_log)("NativePluginCredentialsProvider::GetAWSCredentials");
    return AWSCredentials();
}

void NativePluginCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG(m_log)("NativePluginCredentialsProvider::ValidateArgumentsMap");
}

NativePluginCredentialsProvider::~NativePluginCredentialsProvider()
{
    RS_LOG(m_log)("NativePluginCredentialsProvider::~NativePluginCredentialsProvider");
}