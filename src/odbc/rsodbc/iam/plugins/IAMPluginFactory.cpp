#include "IAMPluginFactory.h"
#include "IAMUtils.h"

using namespace Redshift::IamSupport;

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMPluginCredentialsProvider> IAMPluginFactory::CreatePlugin(
    const rs_wstring& in_pluginName,
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreatePlugin");

    std::unique_ptr<IAMPluginCredentialsProvider> credProvider;

    // Only support Browser Azure AD OAuth2
	if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_BROWSER_AZURE_OAUTH2), false))
	{
		credProvider = CreateBrowserAzureOAuth2Plugin( in_config, in_argsMap);
	}
    else
    {
        std::string errorMsg = "Unsupported authentication provider: " + IAMUtils::convertToUTF8(in_pluginName);
        RS_LOG_ERROR("IAM", errorMsg.c_str());
        IAMUtils::ThrowConnectionExceptionWithInfo("Unsupported authentication provider. Only BrowserAzureADOAuth2 is supported.");
    }

    return credProvider;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMBrowserAzureOAuth2CredentialsProvider> IAMPluginFactory::CreateBrowserAzureOAuth2Plugin(
		const IAMConfiguration& in_config,
	const std::map<rs_string, rs_string>& in_argsMap)
{
	RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateBrowserAzureOAuth2Plugin");

	return std::unique_ptr<IAMBrowserAzureOAuth2CredentialsProvider>(
		new IAMBrowserAzureOAuth2CredentialsProvider( in_config, in_argsMap));
}
