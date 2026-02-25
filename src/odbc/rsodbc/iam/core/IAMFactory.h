#ifndef _IAMFACTORY_H_
#define _IAMFACTORY_H_

#include "IAMConfiguration.h"
#include "IAMPluginFactory.h"
#include "IAMProfileCredentialsProvider.h"
#include "rslog.h"

#include <aws/core/auth/AWSCredentialsProvider.h>

#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief Factory for creating AWSCredentialsProvider
    class IAMFactory
    {
    public:
        /// @brief Creates the correct type of AWSCredentialsProvider according to the specified
        /// settings.
        ///
        /// @param in_settings              The settings.

        static std::shared_ptr<Aws::Auth::AWSCredentialsProvider> CreateCredentialsProvider(
            const IamSettings& in_settings);

        /// @brief Creates a static AWSCredentialsProvider.
        ///
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a static AWSCredentialsProvider
        static std::shared_ptr<Aws::Auth::AWSCredentialsProvider> CreateStaticCredentialsProvider(
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a default AWSCredentialsProvider.
        ///
        /// @return A shared pointer to a default AWSCredentialsProvider
        static std::shared_ptr<Aws::Auth::AWSCredentialsProvider> CreateDefaultCredentialsProvider();

        /// @brief Creates an Instance Profile AWSCredentialsProvider.
        ///
        /// @return A shared pointer to an Instance Profile AWSCredentialsProvider
        static std::shared_ptr<Aws::Auth::AWSCredentialsProvider> CreateInstanceProfileCredentialsProvider();

        /// @brief Creates a profile AWSCredentialsProvider.
        ///
                /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a profile AWSCredentialsProvider
        static std::shared_ptr<IAMProfileCredentialsProvider> CreateProfileCredentialsProvider(
                        const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a plugin AWSCredentialsProvider according to
        ///        the pluginName set in the IAMConfiguration parameter.
        ///
                /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a plugin AWSCredentialsProvider
        static std::shared_ptr<IAMPluginCredentialsProvider> CreatePluginCredentialsProvider(
                        const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Destructor.
        ~IAMFactory();
    private:
        /// @brief Constructor.
        IAMFactory();
    };
}
}

#endif
