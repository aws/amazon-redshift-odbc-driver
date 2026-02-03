#ifndef _IAMPLUGINFACTORY_H_
#define _IAMPLUGINFACTORY_H_

#include "rslog.h"
#include "IAMPluginCredentialsProvider.h"
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
        /// @brief CreatePlugin - Create the appropriate plugin based on plugin name
        ///
        /// @param in_pluginName         The plug-in credentials provider to create (only BrowserAzureADOAuth2 supported)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        ///
        /// @return A credentials provider wrapped using smart pointer
        static std::unique_ptr<IAMPluginCredentialsProvider> CreatePlugin(
            const rs_wstring& in_pluginName,
                        const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

		/// @brief CreateBrowserAzureOAuth2Plugin - Construct Azure AD OAuth2 credentials provider
		///
		/// @param in_config             The IAM Connection Configuration
		/// @param in_argsMap            Optional arguments map passed to the credentials provider
		///
		/// @return A credentials provider wrapped using smart pointer
		static std::unique_ptr<IAMBrowserAzureOAuth2CredentialsProvider> CreateBrowserAzureOAuth2Plugin(
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
