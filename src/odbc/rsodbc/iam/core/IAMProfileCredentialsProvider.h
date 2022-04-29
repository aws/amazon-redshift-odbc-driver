#ifndef _IAMPROFILECRNDENTIALSPROVIDER_H_
#define _IAMPROFILECRNDENTIALSPROVIDER_H_

#include "IAMProfileConfigLoader.h"
#include "IAMCredentials.h"
#include "IAMConfiguration.h"
#include "RsLogger.h"

#include <set>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief .
    class IAMProfileCredentialsProvider : public Aws::Auth::ProfileConfigFileAWSCredentialsProvider
    {
    public:
        /// @brief Constructor.
        ///
        /// @param in_log            The log. (NOT OWN)
        /// @param in_config         The IAM Connection Configuration
        explicit IAMProfileCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Get the AWS credentials for ProfileCredentialsProvider
        /// 
        /// @return AwsCredentials read and calculated from the profile files or cache
        Aws::Auth::AWSCredentials GetAWSCredentials() override;

        /// @brief Assume role given the role arn and credentials
        /// 
        /// @param in_profile    profile used to look up for AwsCredentials
        /// @return AWSCredentials that read and calculated from the profile files
        /// 
        /// @exception ErrorException if AssumeRoleOutcome indicates failure.
        Aws::Auth::AWSCredentials GetAWSCredentials(const rs_string& in_profile);

        /// @brief Gets IAMCredentials cached by the credentials provider
        /// 
        /// @return IAMCredentials cached by the credentials provider
        IAMCredentials GetIAMCredentials();

        /// @brief Gets the profile information from credentials/config file
        /// 
        /// @param in_profile    Profile to look up in the credentials/config file
        /// 
        /// @return IAMProfile read from the credentials/config file
        /// 
        /// @exception ErrorException if no AWS profile found or chained profile loop
        IAMProfile LoadProfile(const rs_string& in_profile);

        /// @brief Assume role given the role arn and credentials
        /// 
        /// @param in_roleArn
        /// @param in_credentialsProvider
        /// 
        /// @return AWSCredentials that contains the result of AssumeRoleRequest
        /// 
        /// @exception ErrorException if AssumeRoleOutcome indicates failure.
        Aws::Auth::AWSCredentials AssumeRole(
            const rs_string& in_roleArn,
            const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider);

        /// @brief  Save AWSCredentials and related settings to m_credentialsHoder. 
        /// 
        /// @param  in_credentials    The AWS credentials to be saved
        void SaveSettings(const Aws::Auth::AWSCredentials& in_credentials);

        /// @brief  Get and save connection attribute settings from m_credentials
        ///         to out_credentials, if any saved attributes are available. 
        /// 
        /// @param  out_credentials       The output credentials holder
        void GetConnectionSettings(IAMCredentials& out_credentials);

        /// @brief Check the cached m_credentials can be used
        /// 
        /// @return True if we can use the cached AWSCredentials, else false
        bool CanUseCachedAwsCredentials();
        
        /// @brief Destructor.
        ~IAMProfileCredentialsProvider();

    private:
        // The connection log. (NOT OWN)
        RsLogger* m_log;

        // The IAM related connection configurations (NOT OWN).
        const IAMConfiguration& m_config;

        /* the initial profile used to look up AwsCredentials */
        rs_string m_profileToUse;

        /* ProfileConfigLoaders used to read profiles from file system */
        std::shared_ptr<IAMProfileConfigLoader> m_configFileLoader;
        std::shared_ptr<IAMProfileConfigLoader> m_credentialsFileLoader;

        /* credentials used to cache the AWSCredentials, so that for chained profile, 
           driver does not have to go through the entire chained request / response cycle */
        IAMCredentials m_credentials;

        /* used for keeping track of chained-role profiles */
        std::set<rs_string> m_chainedProfiles;
    };
}
}

#endif
