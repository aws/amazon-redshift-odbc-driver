#ifndef _IAMPLUGINFACTORY_H_
#define _IAMPLUGINFACTORY_H_

#include "RsLogger.h"
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
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMPluginCredentialsProvider> CreatePlugin(
            const rs_wstring& in_pluginName,
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());


        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMAdfsCredentialsProvider> CreateAdfsPlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMAzureCredentialsProvider> CreateAzurePlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMBrowserAzureCredentialsProvider> CreateBrowserAzurePlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

		/// @brief Constructor           Construct credentials provider using argument map
		///
		/// @param in_log                The logger. (NOT OWN)
		/// @param in_config             The IAM Connection Configuration
		/// @param in_argsMap            Optional arguments map passed to the credentials provider
		///
		/// @return A credentials provider wrapped using smart pointer
		static std::unique_ptr<IAMBrowserAzureOAuth2CredentialsProvider> CreateBrowserAzureOAuth2Plugin(
			RsLogger* in_log,
			const IAMConfiguration& in_config = IAMConfiguration(),
			const std::map<rs_string, rs_string>& in_argsMap
			= std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMBrowserSamlCredentialsProvider> CreateBrowserSamlPlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMPingCredentialsProvider> CreatePingPlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMOktaCredentialsProvider> CreateOktaPlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMJwtBasicCredentialsProvider> CreateJwtPlugin(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// @brief Constructor           Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        /// 
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMExternalCredentialsProvider> CreateExternalPlugin(
            RsLogger* in_log,
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
