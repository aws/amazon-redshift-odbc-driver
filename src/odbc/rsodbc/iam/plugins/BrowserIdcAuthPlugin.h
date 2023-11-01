#ifndef _BrowserIdcAuthPlugin_H_
#define _BrowserIdcAuthPlugin_H_

#include "NativePluginCredentialsProvider.h"

#include <aws/sso-oidc/model/CreateTokenRequest.h>
#include <aws/sso-oidc/model/RegisterClientRequest.h>
#include <aws/sso-oidc/model/StartDeviceAuthorizationRequest.h>
#include <aws/sso-oidc/model/CreateTokenResult.h>
#include <aws/sso-oidc/model/RegisterClientResult.h>
#include <aws/sso-oidc/model/StartDeviceAuthorizationResult.h>
#include <aws/sso-oidc/SSOOIDCClient.h>

namespace Redshift
{
    namespace IamSupport
    {
        class BrowserIdcAuthPlugin : public NativePluginCredentialsProvider
        {
        public:
            explicit BrowserIdcAuthPlugin(
                                const IAMConfiguration& in_config = IAMConfiguration(),
                const std::map<rs_string, rs_string>& in_argsMap
                = std::map<rs_string, rs_string>());

            // @brief Get the IdC token
            rs_string GetAuthToken() override;

            // @brief Destructor
            ~BrowserIdcAuthPlugin();

        private:

            // @brief Initialize the arguments map
            void InitArgumentsMap() override;

            // @brief Validate the arguments map
            void ValidateArgumentsMap() override;

            /**
             * @brief     Get IdC token using browser based IdC auth
             * @return    An IdC token
             */
            rs_string GetIdcToken();

            // @brief Initialize the IdC client
            Aws::SSOOIDC::SSOOIDCClient InitializeIdcClient(const std::string& in_idcRegion);

            // @brief Template function for logging error response
            template <typename T, typename R>
            void LogFailureResponse(const Aws::Utils::Outcome<T, R> &outcome,
                                   const std::string &operation);

            // @brief Register the client with IdC
            Aws::SSOOIDC::Model::RegisterClientResult RegisterClient(
                Aws::SSOOIDC::SSOOIDCClient& idc_client,
                const std::string& in_registerClientCacheKey,
                const std::string& in_clientApplicationName,
                const std::string& in_clientType,
                const std::string& in_scope);

            // @brief Handle the register client API error
            template <typename T, typename R>
            void HandleRegisterClientError(
                const Aws::Utils::Outcome<T, R> &outcome);

            // @brief Start the device authorization flow
            Aws::SSOOIDC::Model::StartDeviceAuthorizationResult StartDeviceAuthorization(
                Aws::SSOOIDC::SSOOIDCClient& idc_client,
                const std::string& in_clientId,
                const std::string& in_clientSecret,
                const std::string& in_startUrl);

            // @brief Handle the start device authorization API error
            template <typename T, typename R>
            void HandleStartDeviceAuthorizationError(
                const Aws::Utils::Outcome<T, R> &outcome);

            // @brief Launch the browser with the given URL
            void LaunchBrowser(const std::string& in_url);

            // @brief Poll for the access token
            std::string PollForAccessToken(
                Aws::SSOOIDC::SSOOIDCClient& idc_client,
                const Aws::SSOOIDC::Model::RegisterClientResult& in_registerClientResult,
                const Aws::SSOOIDC::Model::StartDeviceAuthorizationResult& in_startDeviceAuthorizationResult,
                const std::string& in_grantType);

            // @brief Handle the create access token API error
            template <typename T, typename R>
            void HandleCreateTokenError(
                const Aws::Utils::Outcome<T, R> &outcome);

            // @brief Disabled assignment operator to avoid warning.
            BrowserIdcAuthPlugin& operator=(const BrowserIdcAuthPlugin& in_browserProvider);

#if (defined(_WIN32) || defined(_WIN64))
			const char* command_ = "start \"\" \"";
			const char* subcommand_ = "\"";
#elif (defined(LINUX) || defined(__linux__))
			const char* command_ = "URL=\"";
			// Trying so hard to open browser on Linux as some commands couldn't work.
			const char* subcommand_ = "\"; xdg-open $URL || sensible-browser $URL || x-www-browser $URL || gnome-open $URL";
#elif (defined(__APPLE__) || defined(__MACH__) || defined(PLATFORM_DARWIN))
			const char* command_ = "open \"";
			const char* subcommand_ = "\"";
#endif

        };
    }
}

#endif