#ifndef _NATIVEPLUGINCREDENTIALSPROVIDER_H_
#define _NATIVEPLUGINCREDENTIALSPROVIDER_H_

#include <map>
#include "../rs_iam_support.h"
#include "IAMUtils.h"
#include "IAMPluginCredentialsProvider.h"

namespace Redshift
{
    namespace IamSupport
    {
        class NativePluginCredentialsProvider : public IAMPluginCredentialsProvider
        {
        public:
            explicit NativePluginCredentialsProvider(
                                const IAMConfiguration& in_config = IAMConfiguration(),
                const std::map<rs_string, rs_string>& in_argsMap
                    = std::map<rs_string, rs_string>());

            /**
             * @brief Get AWS credentials for the given credentials provider
             * @return AwsCredentials from the credentials provider
             */ 
            virtual Aws::Auth::AWSCredentials GetAWSCredentials() override;

            // @brief Validate the arguments map for the credential provider
            virtual void ValidateArgumentsMap() override;

            /**
             * @brief  Get auth token from the plugin
             * 
             * @return auth token
             */  
            virtual rs_string GetAuthToken() = 0;

            /// @brief Destructor
            virtual ~NativePluginCredentialsProvider();

        protected:
            // @brief Disabled assignment operator to avoid warning.
            NativePluginCredentialsProvider& operator=(const NativePluginCredentialsProvider& in_credProvider);

        };
    }
}

#endif