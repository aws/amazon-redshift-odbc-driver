#ifndef _IAMJWTPLUGINCREDENTIALSPROVIDER_H_
#define _IAMJWTPLUGINCREDENTIALSPROVIDER_H_

#include "../rs_iam_support.h"
#include "IAMPluginCredentialsProvider.h"

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
        class IAMJwtPluginCredentialsProvider : public IAMPluginCredentialsProvider
        {
        public:
            /// @brief Constructor        Construct credentials provider using argument map
            ///
            /// @param in_config          The IAM Connection Configuration
            /// @param in_argsMap         Optional arguments map passed to the credentials provider
            explicit IAMJwtPluginCredentialsProvider(
                                const IAMConfiguration& in_config = IAMConfiguration(),
                const std::map<rs_string, rs_string>& in_argsMap
                = std::map<rs_string, rs_string>());

            /// @brief  Get AWS credentials for the given credentials provider
            ///
            /// @return AwsCredentials from the credentials provider
            virtual Aws::Auth::AWSCredentials GetAWSCredentials() override;

            /// @brief  Validate if the arguments map contains all required arguments
            ///         that later will be used by plugin.
            ///
            /// @exception ErrorException with the error message indicated,
            ///            If any required arguments are missing.
            virtual void ValidateArgumentsMap() override;

            /// @brief Get DbUser field from JWT Assertion.
            ///
            /// @param jwt    The JWT Assertion
            ///
            /// @return void
            void RetrieveDbUserField(const JWTAssertion& jwt);

            /// @brief Align base64 encoded payload token
            ///
            /// @param str    Payload Token string for Alignment
            ///
            /// @return void
            void AlignPayloadToken(rs_string& str);

            /// @brief Decode base64 string
            ///
            /// @param str    String to be decoded
            ///
            /// @return decoded basse64 string
            rs_string DecodeBase64String(const rs_string& str);

            /// @brief Decode base64 encoded JWT token
            ///
            /// @param jwt    The JWT string
            ///
            /// @return decoded JWT assertion
            JWTAssertion DecodeJwtToken(const rs_string& jwt);

            /// @brief Get the AWS credentials from the JWT assertion
            ///
            /// @param in_jwtAssertion   JWT assertion used to retrieve AWSCredentials
            ///
            /// @return AwsCredentials from the JWT assertion
            virtual Aws::Auth::AWSCredentials GetAWSCredentialsWithJwt(
                const rs_string& in_jwtAssertion);

            /// @brief  Assume role using JWT Request
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

            /// @brief  Get JWT assertion from given connection settings
            ///
            /// @return JWT assertion
            virtual rs_string GetJwtAssertion()
            {
                return rs_string();
            }

            /// @brief Destructor
            virtual ~IAMJwtPluginCredentialsProvider();

        protected:
            /// @brief Disabled assignment operator to avoid warning.
            IAMJwtPluginCredentialsProvider& operator=(
                const IAMJwtPluginCredentialsProvider& in_jwtProvider) = delete;
        };
    }
}

#endif
