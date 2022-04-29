#ifndef _IAMBROWSERSAMLCREDENTIALSPROVIDER_H_
#define _IAMBROWSERSAMLCREDENTIALSPROVIDER_H_

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
        ///        Retrieves AWSCredentials using URI plug-in.
        class IAMBrowserSamlCredentialsProvider : public IAMSamlPluginCredentialsProvider
        {
        public:
            /// @brief Constructor          Construct credentials provider using argument map
            ///
            /// @param in_log               The logger. (NOT OWN)
            /// @param in_config            The IAM Connection Configuration
            /// @param in_argsMap           Optional arguments map passed to the credentials provider
            explicit IAMBrowserSamlCredentialsProvider(
                RsLogger* in_log,
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
            /// @return Saml assertion
            rs_string GetSamlAssertion() override;

            /// @brief  Launch Browser to open URI link
            ///
            /// @param uri                  The URI used to login IdP
            void LaunchBrowser(const rs_string& uri);

            /// @brief Get random number between low and high
            ///
            /// @param low                  The minimum integer to generate
            /// @param high                 The maximum integer to generate
            ///
            /// @return generated integer
            int GenerateRandomInteger(int low, int high);
                
            /// @brief Generate random string to use in URI state parameter
            ///
            /// @return generated string
            rs_string GenerateState();

            /// @brief Request an authorization code from /oauth2/authorize/
            ///
            /// @return authorization code
            rs_string RequestSamlAssertion();

            /// @brief Erase \r\n symbols from string
            ///
            /// @param str                  The string to be modified
            void EraseLineFeeds(rs_string& str);

            /// @brief Waiting for the server to start listening
            ///
            /// @param srv                  The HTTP web server
            ///
            /// @exception Exception will be thrown if SERVER_START_TIMEOUT is reached
            void WaitForServer(WEBServer& srv);

            /// @brief Destructor
            ~IAMBrowserSamlCredentialsProvider();

        private:
            /// @brief Disabled assignment operator to avoid warning.
            IAMBrowserSamlCredentialsProvider& operator=(const IAMBrowserSamlCredentialsProvider& in_URIProvider);

            #if (defined(_WIN32) || defined(_WIN64))
            const char* command_ = "start \"\" \"";
            const char* subcommand_ = "\"";
            #elif (defined(LINUX) || defined(__linux__))
            const char* command_ = "URL=\"";
            // Trying so hard to open URI on Linux as some commands couldn't work.
            const char* subcommand_ = "\"; xdg-open $URL || sensible-URI $URL || x-www-URI $URL || gnome-open $URL";
            #elif (defined(__APPLE__) || defined(__MACH__) || defined(PLATFORM_DARWIN))
            const char* command_ = "open \"";
            const char* subcommand_ = "\"";
            #endif
        };
    }
}

#endif
