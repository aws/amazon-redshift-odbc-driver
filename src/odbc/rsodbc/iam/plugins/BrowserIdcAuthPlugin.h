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
                const std::map<rs_string, rs_string>& in_argsMap = std::map<rs_string, rs_string>());

            // @brief Get the IdC token
            rs_string GetAuthToken() override;

            // @brief Destructor
            ~BrowserIdcAuthPlugin();

        private:
            // @brief Client to call IdC functions
            Aws::SSOOIDC::SSOOIDCClient idc_client;

            // @brief Initialize the arguments map
            void InitArgumentsMap() override;

            // @brief Validate the arguments map
	        void ValidateArgumentsMap() override;

            // @brief Gets an IdC token using browser based IdC authentication
            std::string GetIdcToken();

            // @brief Initialize the IdC client
            Aws::SSOOIDC::SSOOIDCClient InitializeIdcClient(const std::string& in_region);

            // @brief Register the client with IdC
            Aws::SSOOIDC::Model::RegisterClientResult GetRegisterClientResult();

            // @brief Create the code verifier
            std::string GenerateCodeVerifier();

            // @brief Create the code challenge
            std::string GenerateCodeChallenge(const std::string& codeVerifier);

            // @brief Retrieve the authorization code from the IdC server
            std::string FetchAuthorizationCode(
                const std::string& codeVerifier,
                Aws::SSOOIDC::Model::RegisterClientResult& registerClientResult);

            // @brief Generates a random state to be used in browser authentication
            std::string GenerateState();

            // @brief Generates a random integer between low and high
            int GenerateRandomInteger(int low, int high);

            // @brief Constructs and validates the URI used to launch the browser
            void OpenBrowser(
                const std::string& state,
                const std::string& codeChallenge,
                Aws::SSOOIDC::Model::RegisterClientResult& registerClientResult
            );

            // @brief Launches browser at the provided uri
            void LaunchBrowser(const std::string& uri);

            // @brief Fetch the access token from the IdC server
            std::string FetchAccessToken(
                const Aws::SSOOIDC::Model::RegisterClientResult& in_registerClientResult,
                const std::string &authCode,
                const std::string &codeVerifier);

            // @brief Handle the register client API error
            void HandleRegisterClientError(
                const Aws::SSOOIDC::Model::RegisterClientOutcome &outcome);

            // @brief Handle create access token API errors
            void HandleCreateTokenError(
                const Aws::SSOOIDC::Model::CreateTokenOutcome &outcome);

            // @brief Logs IdC API failure responses
            template <typename T, typename R> 
            void LogFailureResponse(
                const Aws::Utils::Outcome<T, R> &outcome, const std::string &operation) {
                int responseCode = static_cast<int>(outcome.GetError().GetResponseCode());
                std::string exceptionName = outcome.GetError().GetExceptionName();
                std::string errorMessage = (outcome.GetError()).GetMessage();
                RS_LOG_DEBUG("IAMIDC", "%s - Response code:%d; Exception name:%s; Error message:%s", 
                    operation.c_str(), responseCode, exceptionName.c_str(), errorMessage.c_str());
            }

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