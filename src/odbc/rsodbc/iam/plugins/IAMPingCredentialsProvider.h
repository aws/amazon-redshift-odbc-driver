#ifndef _IAMPINGCREDENTIALSPROVIDER_H_
#define _IAMPINGCREDENTIALSPROVIDER_H_

#include "../rs_iam_support.h"
#include "IAMSamlPluginCredentialsProvider.h"
#include "IAMConfiguration.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief IAMPluginCredentialsProvider implementation class.
    ///        Retrieves AWSCredentials using Ping plug-in.
    class IAMPingCredentialsProvider : public IAMSamlPluginCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        explicit IAMPingCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// Initialize default values for argument map
        /// 
        /// @exception ErrorException if required arguments do not exist
        void InitArgumentsMap() override;

        /// @brief  Get saml assertion from given connection settings
        /// 
        /// @return Saml assertion
        rs_string GetSamlAssertion() override;

        /// @brief  Get the name-value pair from the input tag vector
        /// 
        /// @param  in_inputTag       The input tags vector
        /// 
        /// @return A map contains name-value pair extracted from the input tag vector
        virtual std::map<rs_string, rs_string> GetNameValuePairFromInputTag(
            const std::vector<rs_string>& in_inputTags) override;

        /// @brief Destructor
        ~IAMPingCredentialsProvider();

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMPingCredentialsProvider& operator=(const IAMPingCredentialsProvider& in_oktaProvider);
    };
}
}

#endif
