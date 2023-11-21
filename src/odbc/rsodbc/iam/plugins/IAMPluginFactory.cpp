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

    if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_ADFS), false))
    {
        credProvider = CreateAdfsPlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_AZUREAD), false))
    {
        credProvider = CreateAzurePlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_BROWSER_AZURE), false))
    {
        credProvider = CreateBrowserAzurePlugin( in_config, in_argsMap);
    }
	else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_BROWSER_AZURE_OAUTH2), false))
	{
		credProvider = CreateBrowserAzureOAuth2Plugin( in_config, in_argsMap);
	}
	else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_BROWSER_SAML), false))
    {
        credProvider = CreateBrowserSamlPlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_PING), false))
    {
        credProvider = CreatePingPlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_OKTA), false))
    {
        credProvider = CreateOktaPlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(IAM_PLUGIN_JWT), false))
    {
        credProvider = CreateJwtPlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(JWT_IAM_AUTH_PLUGIN), false))
    {
        credProvider = CreateJwtIamAuthPlugin( in_config, in_argsMap);
    }
    else if (IAMUtils::isEqual(in_pluginName, IAMUtils::convertCharStringToWstring(PLUGIN_IDP_TOKEN_AUTH), false))
    {
        credProvider = CreateIdpTokenAuthPlugin( in_config, in_argsMap);
    }
    else
    {
        credProvider = CreateExternalPlugin( in_config, in_argsMap); // .Detach()
    }

    return credProvider;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMAdfsCredentialsProvider> IAMPluginFactory::CreateAdfsPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateAdfsPlugin");

    return std::unique_ptr<IAMAdfsCredentialsProvider>(
        new IAMAdfsCredentialsProvider( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMAzureCredentialsProvider> IAMPluginFactory::CreateAzurePlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateAzurePlugin");

    return std::unique_ptr<IAMAzureCredentialsProvider>(
        new IAMAzureCredentialsProvider( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMBrowserAzureCredentialsProvider> IAMPluginFactory::CreateBrowserAzurePlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateBrowserAzurePlugin");

    return std::unique_ptr<IAMBrowserAzureCredentialsProvider>(
        new IAMBrowserAzureCredentialsProvider( in_config, in_argsMap));
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

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMBrowserSamlCredentialsProvider> IAMPluginFactory::CreateBrowserSamlPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateBrowserSamlPlugin");

    return std::unique_ptr<IAMBrowserSamlCredentialsProvider>(
        new IAMBrowserSamlCredentialsProvider( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMPingCredentialsProvider> IAMPluginFactory::CreatePingPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreatePingPlugin");

    return std::unique_ptr<IAMPingCredentialsProvider>(
        new IAMPingCredentialsProvider( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMOktaCredentialsProvider> IAMPluginFactory::CreateOktaPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateOktaPlugin");

    return std::unique_ptr<IAMOktaCredentialsProvider>(
        new IAMOktaCredentialsProvider( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMJwtBasicCredentialsProvider> IAMPluginFactory::CreateJwtPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateJwtPlugin");

    return std::unique_ptr<IAMJwtBasicCredentialsProvider>(
        new IAMJwtBasicCredentialsProvider( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<JwtIamAuthPlugin> IAMPluginFactory::CreateJwtIamAuthPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateJwtIamAuthPlugin");

    return std::unique_ptr<JwtIamAuthPlugin>(
        new JwtIamAuthPlugin( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IdpTokenAuthPlugin> IAMPluginFactory::CreateIdpTokenAuthPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateBasicPlugin");

    return std::unique_ptr<IdpTokenAuthPlugin>(
        new IdpTokenAuthPlugin( in_config, in_argsMap));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IAMExternalCredentialsProvider> IAMPluginFactory::CreateExternalPlugin(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap)
{
    RS_LOG_DEBUG("IAM", "IAMPluginFactory::CreateExternalPlugin");

    return std::unique_ptr<IAMExternalCredentialsProvider>(
        new IAMExternalCredentialsProvider( in_config, in_argsMap));
}
