
#include "IAMProfileCredentialsProvider.h"
#include "IAMUtils.h"
#include "IAMPluginCredentialsProvider.h"
#include "IAMPluginFactory.h"

#include <aws/sts/STSClient.h>
#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/core/platform/Environment.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Config;
using namespace Aws::Client;
using namespace Aws::STS;
using namespace Aws::Utils;

#if (defined(_WIN32) || defined(_WIN64))
#ifdef GetMessage
#undef GetMessage
#endif
#endif
namespace {

    /* Allocation log tag for Profile Credentials Provider */
    static const char* PROFILE_LOG_TAG = "IAMProfileCredentialsProvider";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
IAMProfileCredentialsProvider::IAMProfileCredentialsProvider(
        const IAMConfiguration& in_config) :
    ProfileConfigFileAWSCredentialsProvider(
        in_config.GetProfileName().c_str(), 
        Aws::Auth::REFRESH_THRESHOLD),
    m_config(in_config),
    m_profileToUse(in_config.GetProfileName()),
    m_configFileLoader(
        Aws::MakeShared<IAMConfigFileProfileConfigLoader>(
        PROFILE_LOG_TAG,
        GetConfigProfileFilename(),
        true)),
    m_credentialsFileLoader(Aws::MakeShared<IAMConfigFileProfileConfigLoader>(
        PROFILE_LOG_TAG,
        GetCredentialsProfileFilename()))
{
    RS_LOG_DEBUG("IAM", "Redshift::IamSuppor::%s::%s()t",
                 "IAMProfileCredentialsProvider",
                 "IAMProfileCredentialsProvider");

    /* Use default profile to look up IAM profile configurations */
    if (m_profileToUse.empty())
    {
        auto profileFromVar = Aws::Environment::GetEnv(AWS_PROFILE_ENVIRONMENT_VARIABLE);
        if (!profileFromVar.empty())
        {
            m_profileToUse = profileFromVar;
        }
        else
        {
            m_profileToUse = IAM_DEFAULT_PROFILE;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMProfileCredentialsProvider::GetAWSCredentials()
{
    RS_LOG_DEBUG("IAM", "Redshift::IamSupport::%s::%s()",
                 "IAMProfileCredentialsProvider", "GetAWSCredentials");
    /* return cached AWSCredentials */
    if (CanUseCachedAwsCredentials())
    {
        return m_credentials.GetAWSCredentials();
    }
    
    const AWSCredentials credentials = GetAWSCredentials(m_profileToUse);
    /* cache returned credentials */
    SaveSettings(credentials);
    return credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMProfileCredentialsProvider::GetAWSCredentials(const rs_string& in_profile)
{
    RS_LOG_DEBUG("IAM", "Redshift::IamSupport::%s::%s()",
                 "IAMProfileCredentialsProvider", "GetAWSCredentials");
    /* check for profile type:
    1. Default profile
    2. Role-based profile
    3. Plugin-based profile 
    */

    IAMProfile profile = LoadProfile(in_profile);
    const rs_string& roleArn = profile.GetRoleArn();
	const rs_string& roleSessionName = profile.GetRoleSessionName();
    rs_string temp = profile.GetProfileAttribute(IAM_KEY_PLUGIN_NAME);
	rs_wstring pluginName = IAMUtils::convertStringToWstring(temp);
	rs_wstring credential_process = IAMUtils::convertStringToWstring(profile.GetProfileAttribute("credential_process"));

    if (!IAMUtils::isEmpty(pluginName))
    {
        RS_LOG_DEBUG("IAM",
                     "IAMProfileCredentialsProvider.GetAWSCredentials() Using "
                     "plugin based profile: %s",
                     pluginName.c_str());

        std::unique_ptr<IAMPluginCredentialsProvider> plugin = IAMPluginFactory::CreatePlugin(
            IAMUtils::trim(pluginName),
            m_config,
            profile.GetProfileAttributes());

        if (!plugin.get())
        {
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "Plugin " + IAMUtils::convertToUTF8(pluginName) + " is not supported.");
        }

        plugin->GetConnectionSettings(m_credentials);
        return plugin->GetAWSCredentials();
    }
    else if (!roleArn.empty())
    {
        RS_LOG_DEBUG("IAM",
                     "IAMProfileCredentialsProvider.GetAWSCredentials Using "
                     "role based profile: %s",
                     roleArn.c_str());

        /* role-based profile */
        const rs_string sourceProfile = profile.GetSourceProfile();
        AWSCredentials credentials = GetAWSCredentials(sourceProfile);
        auto simpleProvider = Aws::MakeShared<SimpleAWSCredentialsProvider>(PROFILE_LOG_TAG, credentials);
        return AssumeRole(roleArn, roleSessionName, simpleProvider);
    }
	else if (!IAMUtils::isEmpty(credential_process))
	{
        RS_LOG_DEBUG("IAM",
                     "IAMProfileCredentialsProvider.GetAWSCredentials Using "
                     "credential process based profile: %s",
                     credential_process.c_str());

        /* credential process profile */
        ProcessCredentialsProvider processCredentialsProvider(
            profile.GetName());
        return processCredentialsProvider.GetAWSCredentials();
	}

    
    /* default profile */
    return profile.GetCredentials();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMProfile IAMProfileCredentialsProvider::LoadProfile(const rs_string& in_profile)
{
    /* check for possible chained role base profile loop */
    if (m_chainedProfiles.count(in_profile))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Chained profiles loop, please update profile settings.");
    }

    /* Limit the maximum chained profile to 100 */
    if (m_chainedProfiles.size() > 100)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Maximum chained profiles reached, please update profile settings.");
    }

    /* keep track of each chained profile */
    m_chainedProfiles.insert(in_profile);

    RS_LOG_DEBUG("IAM", "Redshift::IamSupport::%s::%s() Loading profile: %s",
                 "IAMProfileCredentialsProvider", "LoadProfile",
                 in_profile.c_str());

    /* load profiles from credentials file if credentials file loader is empty */
    if (m_credentialsFileLoader->GetProfiles().empty())
    {
        m_credentialsFileLoader->Load();
    }

    auto profileToUseIter = m_credentialsFileLoader->GetProfiles().find(in_profile);

    if (profileToUseIter == m_credentialsFileLoader->GetProfiles().end())
    {
        /* Load profiles from config file if configFileLoader is empty */
        if (m_configFileLoader->GetProfiles().empty())
        {
            m_configFileLoader->Load();
        }

        profileToUseIter = m_configFileLoader->GetProfiles().find(in_profile);

        if (profileToUseIter == m_configFileLoader->GetProfiles().end())
        {
            /* no profile found, throw exception */
			rs_wstring profile = IAMUtils::convertStringToWstring(in_profile);
            IAMUtils::ThrowConnectionExceptionWithInfo("No AWS profile found: " + IAMUtils::convertToUTF8(profile));
        }
    }

    return profileToUseIter->second;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMProfileCredentialsProvider::AssumeRole(
    const rs_string& in_roleArn,
	const rs_string& in_roleSessionName,
    const std::shared_ptr<AWSCredentialsProvider>& in_credentialsProvider)
{
    RS_LOG_DEBUG("IAM","Redshift::IamSupport::%s::%s()", "IAMProfileCredentialsProvider", "AssumeRole");

    ClientConfiguration config;

#ifndef _WIN32
    // Added CA file to the config for verifying STS server certificate
    if (!m_config.GetCaFile().empty())
    {
        config.caFile = m_config.GetCaFile();
    }
    else
    {
        config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile()); // .GetAsPlatformString()
    }

#endif // !_WIN32

    if (m_config.GetUsingHTTPSProxy())
    {
        config.proxyHost = m_config.GetHTTPSProxyHost(); 
        config.proxyPort = m_config.GetHTTPSProxyPort();
        config.proxyUserName = m_config.GetHTTPSProxyUser();
        config.proxyPassword = m_config.GetHTTPSProxyPassword();
    }

	config.endpointOverride = m_config.GetStsEndpointUrl();
	config.httpRequestTimeoutMs = m_config.GetStsConnectionTimeout();
	config.connectTimeoutMs = m_config.GetStsConnectionTimeout();
	config.requestTimeoutMs = m_config.GetStsConnectionTimeout();

    RS_LOG_DEBUG("IAM",
                    "Redshift::IamSupport::%s::%s(): httpRequestTimeoutMs: "
                    "%ld, connectTimeoutMs: %ld, requestTimeoutMs: %ld",
                    "IAMProfileCredentialsProvider", "AssumeRole",
                    config.httpRequestTimeoutMs, config.connectTimeoutMs,
                    config.requestTimeoutMs);

        STSClient client(in_credentialsProvider, config);
        Model::AssumeRoleRequest request = Model::AssumeRoleRequest();
        request.SetRoleArn(in_roleArn);

	// Support role_session_name in the AWS Profile
	if (!in_roleSessionName.empty())
	{
		request.SetRoleSessionName(in_roleSessionName);
	}
	else
	{
		const rs_string roleSessionName = "odbc-" + std::to_string(DateTime::Now().Millis());
		request.SetRoleSessionName(roleSessionName);
	}

        RS_LOG_DEBUG(
            "IAM",
            "IAMProfileCredentialsProvider::AssumeRole: Calling "
            "client.AssumeRole with role_arn: %s and role_session_name: %s",
            request.GetRoleArn().c_str(), request.GetRoleSessionName().c_str());

        Model::AssumeRoleOutcome outcome = client.AssumeRole(request);

        if (!outcome.IsSuccess()) {
            const AWSError<STSErrors> &error = outcome.GetError();
            const rs_string &exceptionName = error.GetExceptionName();
            const rs_string &errorMessage = error.GetMessage();

            rs_string fullErrorMsg = exceptionName + ": " + errorMessage;
            IAMUtils::ThrowConnectionExceptionWithInfo(fullErrorMsg);
    }

    const Model::Credentials& credentials = outcome.GetResult().GetCredentials();
    
    return AWSCredentials(
        credentials.GetAccessKeyId(), 
        credentials.GetSecretAccessKey(),
        credentials.GetSessionToken());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMProfileCredentialsProvider::SaveSettings(const Aws::Auth::AWSCredentials& in_credentials)
{
    m_credentials.SetAWSCredentials(in_credentials);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMProfileCredentialsProvider::GetConnectionSettings(IAMCredentials& out_credentials)
{
    /* Call GetAWSCredentials before calling GetConnectionSettings to ensure that
    the correct credentials (e.g., AWSCredentials, dbuser, dbGroup) are cached */
    GetAWSCredentials();

    /* For now we only save the following connection attribute from the existing credentials
    holder to the output credentials holder: dbuser (username), dbgroups, forceLowercase, and 
    autocreate */
    const rs_string& dbUser = m_credentials.GetDbUser();
    const rs_string& dbGroups = m_credentials.GetDbGroups();
    bool forceLowercase = m_credentials.GetForceLowercase();
    bool userAutoCreate = m_credentials.GetAutoCreate();

    if (!dbUser.empty())
    {
        out_credentials.SetDbUser(dbUser);
    }

    if (!dbGroups.empty())
    {
        out_credentials.SetDbGroups(dbGroups);
    }

    if (forceLowercase)
    {
        out_credentials.SetForceLowercase(true);
    }

    if (userAutoCreate)
    {
        out_credentials.SetAutoCreate(true);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMCredentials IAMProfileCredentialsProvider::GetIAMCredentials()
{
    /* Call GetAWSCredentials before calling GetIAMCredentials to ensure that
    the correct credentials (e.g., AWSCredentials, dbuser, dbGroup) are cached */
    GetAWSCredentials();

    return m_credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMProfileCredentialsProvider::CanUseCachedAwsCredentials()
{
    RS_LOG_DEBUG("IAM", "Redshift::IamSupport::%s::%s()",
                 "IAMProfileCredentialsProvider", "CanUseCachedAwsCredentials");
    return 
        ((!m_credentials.GetAWSCredentials().GetAWSAccessKeyId().empty())
            && (!m_credentials.GetAWSCredentials().GetAWSSecretKey().empty()));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
IAMProfileCredentialsProvider::~IAMProfileCredentialsProvider()
{
    RS_LOG_DEBUG("IAM",
                 "Redshift::IamSupport::%s::%s()"
                 "IAMProfileCredentialsProvider",
                 "~IAMProfileCredentialsProvider");
    /* Do nothing */
}
