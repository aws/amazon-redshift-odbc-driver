#ifndef _IAMBROWSERAZURECREDENTIALSPROVIDER_H_
#define _IAMBROWSERAZURECREDENTIALSPROVIDER_H_

#include "IAMSamlPluginCredentialsProvider.h"

#include <map>
#include <aws/core/utils/xml/XmlSerializer.h>

#include "../rs_iam_support.h"
#include "WEBServer.h"

namespace Redshift
{
    namespace IamSupport
    {
        /// @brief IAMPluginCredentialsProvider implementation class.
        ///        Retrieves AWSCredentials using Browser plug-in.
        class IAMBrowserAzureCredentialsProvider : public IAMSamlPluginCredentialsProvider
        {
            public:
                /// @brief Constructor          Construct credentials provider using argument map
                ///
                /// @param in_config            The IAM Connection Configuration
                /// @param in_argsMap           Optional arguments map passed to the credentials provider
                explicit IAMBrowserAzureCredentialsProvider(
                                        const IAMConfiguration& in_config = IAMConfiguration(),
                    const std::map<rs_string, rs_string>& in_argsMap
                        = std::map<rs_string, rs_string>());
                
                /// Initialize default values for argument map
                ///
                /// @exception ErrorException if required arguments do not exist
                void InitArgumentsMap() override;
                
                /// Validate values for argument map.
                ///
                /// @exception ErrorException if required arguments do not exist
                void ValidateArgumentsMap() override;
                
                /// @brief Get saml assertion from given connection settings
                ///
                /// @return SAML assertion
                rs_string GetSamlAssertion() override;
                
                /// @brief Browser OAuth based authentication.
                ///
                /// @return SAML assertion string
                ///
                /// @exception ErrorException if authentication failed
                rs_string BrowserOauthBasedAuthentication();
                
                /// @brief Launch browser to open URI link
                ///
                /// @param uri                  The URI used to login IdP
                void LaunchBrowser(const rs_string& uri);
                
                /// @brief Get random number between low and high
                ///
                /// @param low                  The minimum integer to generate
                /// @param high                 The maximum integer to generate
                ///
                /// @return Generated integer
                int GenerateRandomInteger(int low, int high);
                
                /// @brief Generate random string to use in URI state parameter
                ///
                /// @return generated string
                rs_string GenerateState();
                
                /// @brief Request an authorization code from /oauth2/authorize/
                ///
                /// @return Authorization code
                rs_string RequestAuthorizationCode();
                
                /// @brief Request an access token from /oauth2/token/
                ///
                /// @param authCode             The authorization code token
                ///
                /// @return Access token
                rs_string RequestAccessToken(const rs_string& authCode);
                
                /// @brief Retrieve encoded SAML assertion from access token
                ///
                /// @param accessToken          The access token
                ///
                /// @return SAML assertion
                rs_string RetrieveSamlFromAccessToken(const rs_string& accessToken);
                
                /// @brief Waiting for the server to start listening
                ///
                /// @param srv                  The HTTP web server
                ///
                /// @exception Exception will be thrown if SERVER_START_TIMEOUT is reached
                void WaitForServer(WEBServer& srv);
                
                /// @brief Destructor
                ~IAMBrowserAzureCredentialsProvider();
                
            private:
                // @brief Disabled assignment operator to avoid warning.
                IAMBrowserAzureCredentialsProvider& operator=(const IAMBrowserAzureCredentialsProvider& in_browserProvider);

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
