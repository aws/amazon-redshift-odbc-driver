#ifndef _IAMPLUGINFACTORY_H_
#define _IAMPLUGINFACTORY_H_

#include "rslog.h"
#include "IAMPluginCredentialsProvider.h"
#include "IAMAdfsCredentialsProvider.h"
#include "IAMAzureCredentialsProvider.h"
#include "IAMBrowserAzureCredentialsProvider.h"
#include "IAMBrowserSamlCredentialsProvider.h"
#include "IAMOktaCredentialsProvider.h"
#include "IAMPingCredentialsProvider.h"
#include "IAMExternalCredentialsProvider.h"
#include "IAMJwtBasicCredentialsProvider.h"
#include "IAMBrowserAzureOAuth2CredentialsProvider.h"
#include "JwtIamAuthPlugin.h"
#include "IdpTokenAuthPlugin.h"
#include "BrowserIdcAuthPlugin.h"

#include <map>
#include <memory>

#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    class IAMPluginFactory 
    {
    public:
        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_pluginName         The plug-in credentials provider to create
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMPluginCredentialsProvider> CreatePlugin(
            const rs_wstring& in_pluginName,
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());


        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMAdfsCredentialsProvider> CreateAdfsPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMAzureCredentialsProvider> CreateAzurePlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMBrowserAzureCredentialsProvider> CreateBrowserAzurePlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

		/// @brief Constructor           Construct credentials provider using argument map
		///
		/// @param in_config             The IAM Connection Configuration
		/// @param in_argsMap            Optional arguments map passed to the credentials provider
		///
		/// @return A credentials provider wrapped using smart pointer
		static std::unique_ptr<IAMBrowserAzureOAuth2CredentialsProvider> CreateBrowserAzureOAuth2Plugin(
						const IAMConfiguration& in_config = IAMConfiguration(),
			const std::map<rs_string, rs_string>& in_argsMap
			= std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMBrowserSamlCredentialsProvider> CreateBrowserSamlPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMPingCredentialsProvider> CreatePingPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMOktaCredentialsProvider> CreateOktaPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMJwtBasicCredentialsProvider> CreateJwtPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct Jwt IAM auth plugin using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<JwtIamAuthPlugin> CreateJwtIamAuthPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct IdP token auth plugin using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return IdP token auth plugin wrapped using smart pointer
        static std::unique_ptr<IdpTokenAuthPlugin> CreateIdpTokenAuthPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());
        
        /// @brief Constructor           Construct IdC Browser credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return Browser IdC credentials provider wrapped using smart pointer
        static std::unique_ptr<BrowserIdcAuthPlugin> CreateBrowserIdcAuthPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMExternalCredentialsProvider> CreateExternalPlugin(
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Destructor
        ~IAMPluginFactory() {};
  
    private:
        /// @brief Constructor.
        IAMPluginFactory();

    };
}
}

#endif
