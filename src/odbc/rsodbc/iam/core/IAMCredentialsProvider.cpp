#include "IAMCredentialsProvider.h"
#include "IAMUtils.h"

#include <regex>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMCredentialsProvider::IAMCredentialsProvider(
        const IAMConfiguration& in_config) :
    m_config(in_config)
{
    RS_LOG_DEBUG("Redshift::IamSupport::%s::%s()", "IAMCredentialsProvider", "IAMCredentialsProvider");
}


////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMCredentialsProvider::GetAWSCredentials()
{
    /* return cached AWSCredentials */
    if (CanUseCachedAwsCredentials())
    {
        return m_credentials.GetAWSCredentials();
    }

    return AWSCredentials();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMCredentialsProvider::SaveSettings(const Aws::Auth::AWSCredentials& in_credentials)
{
    m_credentials.SetAWSCredentials(in_credentials);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMCredentialsProvider::GetConnectionSettings(IAMCredentials& out_credentials)
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
IAMCredentials IAMCredentialsProvider::GetIAMCredentials()
{
    /* Call GetAWSCredentials before calling GetIAMCredentials to ensure that
    the correct credentials (e.g., AWSCredentials, dbuser, dbGroup) are cached */
    GetAWSCredentials();

    return m_credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMCredentialsProvider::CanUseCachedAwsCredentials()
{
    return
        !m_credentials.GetAWSCredentials().GetAWSAccessKeyId().empty() &&
        !m_credentials.GetAWSCredentials().GetAWSSecretKey().empty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMCredentialsProvider::ValidateURL(const rs_string & in_url)
{
	// Enforce URL regex in LOGIN_URL to avoid possible remote code execution
	if (!std::regex_match(in_url, std::regex(IAM_URL_PATTERN)))
	{
		IAMUtils::ThrowConnectionExceptionWithInfo("LOGIN_URL is not a valid URL starting with https://.");
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
IAMCredentialsProvider::~IAMCredentialsProvider()
{
    RS_LOG_DEBUG("Redshift::IamSupport::%s::%s()", "IAMCredentialsProvider", "~IAMCredentialsProvider");
}
