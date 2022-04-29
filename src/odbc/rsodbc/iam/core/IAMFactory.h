#ifndef _IAMFACTORY_H_
#define _IAMFACTORY_H_

#include "IAMConfiguration.h"
#include "IAMPluginFactory.h"
#include "IAMProfileCredentialsProvider.h"
#include "RsLogger.h"

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
        /// @param in_log                   The logger.

        static std::shared_ptr<Aws::Auth::AWSCredentialsProvider> CreateCredentialsProvider(
            const IamSettings& in_settings,
            RsLogger& in_log);

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
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a profile AWSCredentialsProvider
        static std::shared_ptr<IAMProfileCredentialsProvider> CreateProfileCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a plugin AWSCredentialsProvider according to
        ///        the pluginName set in the IAMConfiguration parameter.
        ///
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a plugin AWSCredentialsProvider
        static std::shared_ptr<IAMPluginCredentialsProvider> CreatePluginCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a ADFS plugin AWSCredentialsProvider.
        ///
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a ADFS plugin AWSCredentialsProvider
        static std::shared_ptr<IAMAdfsCredentialsProvider> CreateAdfsCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a Azure plugin AWSCredentialsProvider.
        ///
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a Azure plugin AWSCredentialsProvider
        static std::shared_ptr<IAMAzureCredentialsProvider> CreateAzureCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

		/// @brief Creates a Browser Azure plugin AWSCredentialsProvider.
		///
		/// @param in_log          The connection or driver log
		/// @param in_config       IAMConfigurations
		///
		/// @return A shared pointer to a Browser Azure plugin AWSCredentialsProvider
		static std::shared_ptr<IAMBrowserAzureCredentialsProvider> CreateBrowserAzureCredentialsProvider(
			RsLogger* in_log,
			const IAMConfiguration& in_config = IAMConfiguration());

		/// @brief Creates a Browser SAML plugin AWSCredentialsProvider.
		///
		/// @param in_log          The connection or driver log
		/// @param in_config       IAMConfigurations
		///
		/// @return A shared pointer to a Browser SAML plugin AWSCredentialsProvider
		static std::shared_ptr<IAMBrowserSamlCredentialsProvider> CreateBrowserSamlCredentialsProvider(
		RsLogger* in_log,
		const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a Okta plugin AWSCredentialsProvider.
        ///
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a Okta plugin AWSCredentialsProvider
        static std::shared_ptr<IAMOktaCredentialsProvider> CreateOktaCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a JWT plugin AWSCredentialsProvider.
        ///
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a JWT plugin AWSCredentialsProvider
        static std::shared_ptr<IAMJwtBasicCredentialsProvider> CreateJwtCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates a Ping plugin AWSCredentialsProvider.
        ///
        /// @param in_log          The connection or driver log
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a Ping plugin AWSCredentialsProvider
        static std::shared_ptr<IAMPingCredentialsProvider> CreatePingCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration());

        /// @brief Creates an External plugin AWSCredentialsProvider.
        ///
        /// @param in_log          The connection or driver log  (NOT OWN)
        /// @param in_config       IAMConfigurations
        ///
        /// @return A shared pointer to a Ping plugin AWSCredentialsProvider
        static std::shared_ptr<IAMExternalCredentialsProvider> CreateExternalCredentialsProvider(
            RsLogger* in_log,
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
