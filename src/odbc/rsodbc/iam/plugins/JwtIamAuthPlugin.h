#ifndef _JWTIAMAUTHPLUGIN_H_
#define _JWTIAMAUTHPLUGIN_H_

#include "IAMJwtPluginCredentialsProvider.h"

#include <map>

#include "../rs_iam_support.h"

namespace Redshift
{
    namespace IamSupport
    {
        class JwtIamAuthPlugin : public IAMJwtPluginCredentialsProvider
        {
        public:
            /// @brief Constructor        Construct auth plugin using argument map
            ///
            /// @param in_log                The logger.
            /// @param in_config             The IAM Connection Configuration
            /// @param in_argsMap            Optional arguments map passed to the credentials provider
            explicit JwtIamAuthPlugin(
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

            /// @brief  Get JWT assertion from given connection settings
            ///
            /// @return JWT assertion
            rs_string GetJwtAssertion() override;

            /// @brief Destructor
            ~JwtIamAuthPlugin();

            /// @brief Disabled assignment operator to avoid warning.
            JwtIamAuthPlugin& operator=(
                const JwtIamAuthPlugin& in_jwtProvider) = delete;
        };
    }
}

#endif
