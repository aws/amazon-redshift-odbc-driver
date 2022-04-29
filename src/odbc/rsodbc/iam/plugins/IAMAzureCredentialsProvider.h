#ifndef _IAMAZURECREDENTIALSPROVIDER_H_
#define _IAMAZURECREDENTIALSPROVIDER_H_

#include "IAMSamlPluginCredentialsProvider.h"

#include <map>
#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief IAMPluginCredentialsProvider implementation class.
    ///        Retrieves AWSCredentials using Azure plug-in.
    class IAMAzureCredentialsProvider : public IAMSamlPluginCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        explicit IAMAzureCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// Initialize default values for argument map
        /// 
        /// @exception ErrorException if required arguments do not exist
        void InitArgumentsMap() override;

        /// Validate values for argument map. For Azure Oauth, we should
        /// override the method to ignore unused values.
        ///
        /// @exception ErrorException if required arguments do not exist
        void ValidateArgumentsMap() override;

        /// @brief  Get saml assertion from given connection settings
        /// 
        /// @return Saml assertion
        rs_string GetSamlAssertion() override;

        /// @brief  Azure Oauth based authentication.
        /// 
        /// @return Saml assertion string
        /// 
        /// @exception ErrorException if authentication failed
        rs_string AzureOauthBasedAuthentication();

        /// @brief Destructor
        ~IAMAzureCredentialsProvider();

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMAzureCredentialsProvider& operator=(const IAMAzureCredentialsProvider& in_azureProvider);
    };
}
}

#endif
