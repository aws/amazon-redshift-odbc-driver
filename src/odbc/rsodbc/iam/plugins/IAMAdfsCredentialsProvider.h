#ifndef _IAMADFSCREDENTIALSPROVIDER_H_
#define _IAMADFSCREDENTIALSPROVIDER_H_

#include "IAMSamlPluginCredentialsProvider.h"

#include <map>
#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief IAMPluginCredentialsProvider implementation class.
    ///        Retrieves AWSCredentials using ADFS plug-in.
    class IAMAdfsCredentialsProvider : public IAMSamlPluginCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        explicit IAMAdfsCredentialsProvider(
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

        /// @brief  ADFS Windows native integrated authentication.
        /// 
        /// @return Saml assertion string
        /// 
        /// @exception ErrorException if authentication failed
        rs_string WindowsIntegratedAuthentication();

        /// @brief  ADFS form based authentication.
        /// 
        /// @return Saml assertion string
        /// 
        /// @exception ErrorException if authentication failed
        rs_string FormBasedAuthentication();

        /// @brief Destructor
        ~IAMAdfsCredentialsProvider();

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMAdfsCredentialsProvider& operator=(const IAMAdfsCredentialsProvider& in_adfsProvider);
    };
}
}

#endif
