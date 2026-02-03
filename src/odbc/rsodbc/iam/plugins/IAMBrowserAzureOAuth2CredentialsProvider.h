#ifndef _IAMBROWSERAZUREOAuth2CREDENTIALSPROVIDER_H_
#define _IAMBROWSERAZUREOAuth2CREDENTIALSPROVIDER_H_

#include "IAMPluginCredentialsProvider.h"

#include <map>

#include "../rs_iam_support.h"
#include "WEBServer.h"

namespace Redshift
{
	namespace IamSupport
	{
		struct JWTAssertion
		{
			rs_string header;
			rs_string payload;
			rs_string signature;
		};

		/// @brief IAMPluginCredentialsProvider implementation class.
		///        Retrieves AWSCredentials using Browser plug-in.
		class IAMBrowserAzureOAuth2CredentialsProvider : public IAMPluginCredentialsProvider
		{
		public:
			/// @brief Constructor          Construct credentials provider using argument map
			///
			/// @param in_config            The IAM Connection Configuration
			/// @param in_argsMap           Optional arguments map passed to the credentials provider
			explicit IAMBrowserAzureOAuth2CredentialsProvider(
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
			rs_string GetJwtAssertion();

			/// @brief Get AWS credentials for the given credentials provider
			///
			/// @return AwsCredentials from the credentials provider
			virtual Aws::Auth::AWSCredentials GetAWSCredentials() override;

			/// @brief Get the AWS credentials from the JWT assertion
			///
			/// @param in_jwtAssertion   JWT assertion used to retrieve AWSCredentials
			///
			/// @return AwsCredentials from the JWT assertion
			virtual Aws::Auth::AWSCredentials GetAWSCredentialsWithJwt(
				const rs_string& in_jwtAssertion);

			/// @brief Assume role using JWT Request
			///
			/// @param in_jwtAssertion        The JWT assertion
			/// @param in_roleArn             The AWS role arn
			/// @param in_roleSessionName     The AWS role session name
			///
			/// @return AWSCredentials
			///
			/// @exception ErrorException if assume role with jwt request failed
			virtual Aws::Auth::AWSCredentials AssumeRoleWithJwtRequest(
				const rs_string& in_jwtAssertion,
				const rs_string& in_roleArn,
				const rs_string& in_roleSessionName);

			/// @brief Decode base64 encoded JWT token
			///
			/// @param jwt    The JWT string
			///
			/// @return decoded JWT assertion
			JWTAssertion DecodeJwtToken(const rs_string& jwt);

			/// @brief Get DbUser field from JWT Assertion.
			///
			/// @param jwt    The JWT Assertion
			///
			/// @return void
			void RetrieveDbUserField(const JWTAssertion& jwt);

			/// @brief Decode base64 string
			///
			/// @param str    String to be decoded
			///
			/// @return decoded base64 string
			rs_string DecodeBase64String(const rs_string& str);

			/// @brief Align base64 encoded payload token
			///
			/// @param str    Payload Token string for Alignment
			///
			/// @return void
			void AlignPayloadToken(rs_string& str);

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

			/// @brief Waiting for the server to start listening
			///
			/// @param srv                  The HTTP web server
			///
			/// @exception Exception will be thrown if SERVER_START_TIMEOUT is reached
			void WaitForServer(WEBServer& srv);

			/// @brief Destructor
			~IAMBrowserAzureOAuth2CredentialsProvider();

		private:
			// @brief Disabled assignment operator to avoid warning.
			IAMBrowserAzureOAuth2CredentialsProvider& operator=(const IAMBrowserAzureOAuth2CredentialsProvider& in_browserProvider);
// LINUX is used in Mac build too, so order of LINUX and APPLE are important
#if (defined(_WIN32) || defined(_WIN64))
			const char* command_ = "start \"\" \"";
			const char* subcommand_ = "\"";
#elif (defined(__APPLE__) || defined(__MACH__) || defined(PLATFORM_DARWIN))
			const char* command_ = "open \"";
			const char* subcommand_ = "\"";
#elif (defined(LINUX) || defined(__linux__))
			const char* command_ = "URL=\"";
			// Trying so hard to open browser on Linux as some commands couldn't work.
			const char* subcommand_ = "\"; xdg-open $URL || sensible-browser $URL || x-www-browser $URL || gnome-open $URL";
#endif
		};
	}
}

#endif

