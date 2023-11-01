#ifndef _IAMJWTBASICCREDENTIALSPROVIDER_H_
#define _IAMJWTBASICCREDENTIALSPROVIDER_H_

#include "IAMJwtPluginCredentialsProvider.h"

#include <map>

#include "../rs_iam_support.h"

namespace Redshift
{
    namespace IamSupport
    {
        class IAMJwtBasicCredentialsProvider : public IAMJwtPluginCredentialsProvider
        {
        public:
            /// @brief Constructor        Construct credentials provider using argument map
            ///
            /// @param in_config             The IAM Connection Configuration
            /// @param in_argsMap            Optional arguments map passed to the credentials provider
            explicit IAMJwtBasicCredentialsProvider(
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

            /// @brief  Get JWT assertion from given connection settings
            ///
            /// @return JWT assertion
            rs_string GetJwtAssertion() override;

            /// @brief Destructor
            ~IAMJwtBasicCredentialsProvider();

            /// @brief Disabled assignment operator to avoid warning.
            IAMJwtBasicCredentialsProvider& operator=(
                const IAMJwtBasicCredentialsProvider& in_jwtProvider) = delete;
        };
    }
}

#endif
