#ifndef _IAMOKTACREDENTIALSPROVIDER_H_
#define _IAMOKTACREDENTIALSPROVIDER_H_

#include "../rs_iam_support.h"
#include "IAMSamlPluginCredentialsProvider.h"
#include "IAMHttpClient.h"
#include "IAMConfiguration.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief IAMPluginCredentialsProvider implementation class.
    ///        Retrieves AWSCredentials using Okta plug-in.
    class IAMOktaCredentialsProvider : public IAMSamlPluginCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        explicit IAMOktaCredentialsProvider(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// Initialize default values for argument map
        /// 
        /// @exception ErrorException if required arguments do not exist
        void InitArgumentsMap() override;

        /// @brief  Validate if the arguments map contains all required arguments
        ///         that later will be used by plugin. E.g., app_id in Okta
        /// 
        /// @exception ErrorException with the error message indicated,
        ///            If any required arguments are missing.
        void ValidateArgumentsMap() override;

        /// @brief  Get saml assertion from given connection settings
        /// 
        /// @return Saml assertion
        rs_string GetSamlAssertion() override;

        /// @brief  Get Okta authentication session token from given connection settings
        /// 
        /// @param  in_httpClient    Http Client used to make the request
        /// 
        /// @return Okta authentication token
        rs_string GetAuthSessionToken(
            const std::shared_ptr<IAMHttpClient>& in_httpClient);

        /// @brief Destructor
        ~IAMOktaCredentialsProvider();

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMOktaCredentialsProvider& operator=(const IAMOktaCredentialsProvider& in_oktaProvider);
    };
}
}

#endif
