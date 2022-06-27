#include "IAMFactory.h"
#include "IAMUtils.h"
#include <aws/core/auth/AWSCredentialsProviderChain.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;

// Public Static ===================================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Aws::Auth::AWSCredentialsProvider> IAMFactory::CreateCredentialsProvider(
    const IamSettings& in_settings,
    RsLogger& in_log)
{
  in_log.log(
        "Redshift::IamSupport::%s::%s()",
        "IAMFactory",
        "CreateCredentialsProvider");

    std::shared_ptr<AWSCredentialsProvider> credentialsProvider;

    IAMConfiguration config;

    // Configure DB user settings if enabled.
    if (in_settings.m_enableDbUserDbGroups)
    {
        config.SetDbUser(in_settings.m_dbUser);
        config.SetDbGroups(in_settings.m_dbGroups);
        config.SetForceLowercase(in_settings.m_forceLowercase);
        config.SetAutoCreate(in_settings.m_userAutoCreate);
    }

    // Username and password for Profile and Plugin.
    config.SetUser(in_settings.m_uidPwdSettingsForProfileOrPlugin.m_uid);
    config.SetPassword(in_settings.m_uidPwdSettingsForProfileOrPlugin.m_pwd);
    
    // Set Customized CA file for SSL connection.
    config.SetCaFile(in_settings.m_caFile);

    config.SetUseProxyIdpAuth(in_settings.m_useProxyForIdpAuth);
    if (!in_settings.m_proxySettings.m_proxyHost.empty())
    {
        config.SetUsingHTTPSProxy(true);
        config.SetHTTPSProxyHost(in_settings.m_proxySettings.m_proxyHost);
        config.SetHTTPSProxyPort(in_settings.m_proxySettings.m_proxyPort);
        config.SetHTTPSProxyUser(in_settings.m_proxySettings.m_uidPwdSettings.m_uid);
        config.SetHTTPSProxyPassword(in_settings.m_proxySettings.m_uidPwdSettings.m_pwd);
    }

    if (PLUGIN == in_settings.m_authType)
    {
        // Plugin AuthType specific connection attributes.
        config.SetPluginName(in_settings.m_pluginName);
        config.SetIdpHost(in_settings.m_idpHost);
        config.SetIdpPort(in_settings.m_idpPort);
        config.SetIdpTenant(in_settings.m_idpTenant);
        config.SetClientSecret(in_settings.m_clientSecret);
        config.SetClientId(in_settings.m_clientId);
		config.SetScope(in_settings.m_scope);
		config.SetIdpResponseTimeout(in_settings.m_idp_response_timeout);
        config.SetLoginURL(in_settings.m_login_url);
        config.SetListenPort(in_settings.m_listen_port);
        config.SetAppId(in_settings.m_appId);
        config.SetPreferredRole(in_settings.m_preferredRole);
        config.SetSslInsecure(in_settings.m_sslInsecure);
        config.SetPartnerSpId(in_settings.m_partnerSpId);
        config.SetAppName(in_settings.m_oktaAppName);
        config.SetDbGroupsFilter(in_settings.m_dbGroupsFilter);
        config.SetRoleARN(in_settings.m_role_arn);
        config.SetWebIdentityToken(in_settings.m_web_identity_token);
        config.SetDuration(in_settings.m_duration);
        config.SetRoleSessionName(in_settings.m_role_session_name);

        credentialsProvider = CreatePluginCredentialsProvider(&in_log, config);
    }
    else if (PROFILE == in_settings.m_authType)
    {
        // Profile AuthType specific connection attributes.
        config.SetProfileName(in_settings.m_awsProfile);

        credentialsProvider = CreateProfileCredentialsProvider(&in_log, config);
    }
    else if (STATIC == in_settings.m_authType)
    {
        // Static AuthType specific connection attributes.
        config.SetAWSCredentials(
            AWSCredentials(
                in_settings.m_accessKeyId, 
                in_settings.m_secretAccessKey, 
                in_settings.m_sessionToken));

        credentialsProvider = CreateStaticCredentialsProvider(config);
    }
    else if (INSTANCE_PROFILE == in_settings.m_authType)
    {
        credentialsProvider = CreateInstanceProfileCredentialsProvider();
    }
    else
    {
        credentialsProvider = CreateDefaultCredentialsProvider();
    }

    return credentialsProvider;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<AWSCredentialsProvider> IAMFactory::CreateStaticCredentialsProvider(
    const IAMConfiguration& in_config)
{
    return std::make_shared<SimpleAWSCredentialsProvider>(
        in_config.GetAccessId(),
        in_config.GetSecretKey(),
        in_config.GetSessionToken());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<AWSCredentialsProvider> IAMFactory::CreateDefaultCredentialsProvider()
{
    return std::shared_ptr<DefaultAWSCredentialsProviderChain>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Aws::Auth::AWSCredentialsProvider> IAMFactory::CreateInstanceProfileCredentialsProvider()
{
    return std::shared_ptr<InstanceProfileCredentialsProvider>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMProfileCredentialsProvider> IAMFactory::CreateProfileCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport",
        "IAMFactory",
        "CreateProfileCredentialsProvider");

    return std::make_shared<IAMProfileCredentialsProvider>(in_log, in_config);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMPluginCredentialsProvider> IAMFactory::CreatePluginCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  std::map<rs_string, rs_string> argsMap;
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreatePluginCredentialsProvider");

    return std::shared_ptr<IAMPluginCredentialsProvider>(
        IAMPluginFactory::CreatePlugin(IAMUtils::convertStringToWstring(in_config.GetPluginName()), in_log, in_config, argsMap));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMAdfsCredentialsProvider> IAMFactory::CreateAdfsCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreateAdfsCredentialsProvider");

    return std::shared_ptr<IAMAdfsCredentialsProvider>(
        IAMPluginFactory::CreateAdfsPlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMAzureCredentialsProvider> IAMFactory::CreateAzureCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s:%s()",
        "IAMFactory",
        "CreateAzureCredentialsProvider");

    return std::shared_ptr<IAMAzureCredentialsProvider>(
        IAMPluginFactory::CreateAzurePlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMBrowserAzureCredentialsProvider> IAMFactory::CreateBrowserAzureCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s:%s()",
        "IAMFactory",
        "CreateBrowserAzureCredentialsProvider");

    return std::shared_ptr<IAMBrowserAzureCredentialsProvider>(
        IAMPluginFactory::CreateBrowserAzurePlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMJwtBasicCredentialsProvider> IAMFactory::CreateJwtCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreateJwtCredentialsProvider");

    return std::shared_ptr<IAMJwtBasicCredentialsProvider>(
        IAMPluginFactory::CreateJwtPlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMBrowserSamlCredentialsProvider> IAMFactory::CreateBrowserSamlCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreateBrowserSamlCredentialsProvider");

    return std::shared_ptr<IAMBrowserSamlCredentialsProvider>(
        IAMPluginFactory::CreateBrowserSamlPlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMOktaCredentialsProvider> IAMFactory::CreateOktaCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreateOktaCredentialsProvider");

    return std::shared_ptr<IAMOktaCredentialsProvider>(
        IAMPluginFactory::CreateOktaPlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMPingCredentialsProvider> IAMFactory::CreatePingCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreatePingCredentialsProvider");

    return std::shared_ptr<IAMPingCredentialsProvider>(
        IAMPluginFactory::CreatePingPlugin(in_log, in_config)); // .Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<IAMExternalCredentialsProvider> IAMFactory::CreateExternalCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config)
{
  in_log->log(
        "Redshift:IamSupport::%s::%s()",
        "IAMFactory",
        "CreateExternalCredentialsProvider");

    return std::shared_ptr<IAMExternalCredentialsProvider>(
        IAMPluginFactory::CreateExternalPlugin(in_log, in_config));//.Detach());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMFactory::~IAMFactory()
{
    /* Do nothing */
}

