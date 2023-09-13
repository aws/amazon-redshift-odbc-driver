#ifndef _IdpTokenAuthPlugin_H_
#define _IdpTokenAuthPlugin_H_

#include "NativePluginCredentialsProvider.h"

namespace Redshift
{
    namespace IamSupport
    {
        class IdpTokenAuthPlugin : public NativePluginCredentialsProvider 
        {
        public:
            explicit IdpTokenAuthPlugin(
                RsLogger *in_log,
                const IAMConfiguration &in_config = IAMConfiguration(),
                const std::map<rs_string, rs_string> &in_argsMap =
                    std::map<rs_string, rs_string>());

            // @brief Get the auth token
            rs_string GetAuthToken() override;

            // @brief Destructor
            ~IdpTokenAuthPlugin();

        private:
            // @brief Initialize the arguments map
            void InitArgumentsMap() override;
                
            // @brief Validate the arguments map
            void ValidateArgumentsMap() override;

            // @brief Disabled assignment operator to avoid warning.
            IdpTokenAuthPlugin& operator=(const IdpTokenAuthPlugin& in_basicProvider);
        };
    }
}

#endif