#ifndef _IAMCREDENTIALSPROVIDER_H_
#define _IAMCREDENTIALSPROVIDER_H_

#include "IAMCredentials.h"
#include "IAMConfiguration.h"
#include "rslog.h"
#include <aws/core/auth/AWSCredentialsProvider.h>
#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief IAMCredentialsProvider class for IAM Authentication
    class IAMCredentialsProvider : public Aws::Auth::AWSCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider
        /// @param in_config          The IAMConfiguration 
        explicit IAMCredentialsProvider(
                        const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief  Get AWS credentials for the given credentials provider
        /// 
        /// @return AwsCredentials from the credentials provider
        virtual Aws::Auth::AWSCredentials GetAWSCredentials() override;

        /// @brief Gets IAMCredentials cached by the credentials provider
        /// 
        /// @return IAMCredentials cached by the credentials provider
        virtual IAMCredentials GetIAMCredentials();

        /// @brief Check the cached m_credentials can be used
        /// 
        /// @return True if we can use the cached AWSCredentials, else false
        virtual bool CanUseCachedAwsCredentials();

        /// @brief  Save AWSCredentials and related settings to m_credentials. 
        /// 
        /// @param  in_credentials    The AWS credentials to be saved
        virtual void SaveSettings(const Aws::Auth::AWSCredentials& in_credentials);

        /// @brief  Get and save connection attribute settings from m_credentials
        ///         to out_credentials, if any saved attributes are available. 
        /// 
        /// @param  out_credentials    The output IAMCredentials
        virtual void GetConnectionSettings(IAMCredentials& out_credentials);

        /// @brief Destructor
        ~IAMCredentialsProvider();

    protected:
        /// @brief Disabled assignment operator to avoid warning.
        IAMCredentialsProvider& operator=(const IAMCredentialsProvider& in_credProvider);

		/// Validates the URL string
		///
		/// URL string should start with https, and not include potential redirection 
		///
		/// @param in_url           The URL to the login page
		///
		/// @exception ErrorException if in_url is does not start with https or has forbidden characters
		void ValidateURL(const rs_string & in_url);


        // The connection log. (NOT OWN)

        // The IAMConfiguration contains all IAM related settings.
        const IAMConfiguration m_config;

        /* Credential holders used to cache the AWSCredentials and additional connection settings */
        IAMCredentials m_credentials;
    };
}
}

#endif
