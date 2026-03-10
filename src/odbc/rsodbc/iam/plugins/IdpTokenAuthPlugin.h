#ifndef _IdpTokenAuthPlugin_H_
#define _IdpTokenAuthPlugin_H_

#include "NativePluginCredentialsProvider.h"
#include <aws/core/client/ClientConfiguration.h>

namespace Redshift
{
    namespace IamSupport
    {
        class IdpTokenAuthPlugin : public NativePluginCredentialsProvider 
        {
        public:
            explicit IdpTokenAuthPlugin(
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

            // ============================================================
            // Identity Enhanced Credentials Flow Methods
            // ============================================================

            /// @brief Check if identity-enhanced credentials flow should be used
            /// @return true if AWS credentials (AccessKeyID, SecretAccessKey, SessionToken) are provided
            bool IsUsingIdentityEnhancedCredentials() const;

            /// @brief Get subject token via GetIdentityCenterAuthToken API
            /// @return The subject token obtained from the API
            rs_string GetSubjectToken();

            /// @brief Call GetIdentityCenterAuthToken API for provisioned clusters
            /// @param clusterId The cluster identifier
            /// @param region The AWS region
            /// @return The subject token from the API response
            rs_string GetProvisionedAuthToken(const rs_string& clusterId, const rs_string& region);

            /// @brief Call GetIdentityCenterAuthToken API for serverless workgroups
            /// @param workgroup The workgroup name
            /// @param region The AWS region
            /// @return The subject token from the API response
            rs_string GetServerlessAuthToken(const rs_string& workgroup, const rs_string& region);

        protected:
            // ============================================================
            // Hostname Parsing Utilities (protected for testing)
            // ============================================================

            /// @brief Check if hostname matches serverless pattern
            /// @param host The hostname to check
            /// @return true if hostname matches *.redshift-serverless.amazonaws.com pattern
            bool IsServerlessHost(const rs_string& host) const;

            /// @brief Extract cluster ID from provisioned hostname
            /// @param host The hostname (format: cluster.account.region.redshift.amazonaws.com)
            /// @return The cluster ID or empty string if pattern doesn't match
            rs_string ExtractClusterIdFromHost(const rs_string& host) const;

            /// @brief Extract workgroup name from serverless hostname
            /// @param host The hostname (format: workgroup.account.region.redshift-serverless.amazonaws.com)
            /// @return The workgroup name or empty string if pattern doesn't match
            rs_string ExtractWorkgroupFromHost(const rs_string& host) const;

            /// @brief Extract region from hostname
            /// @param host The hostname
            /// @return The region or empty string if pattern doesn't match
            rs_string ExtractRegionFromHost(const rs_string& host) const;

            // ============================================================
            // Cluster Info Resolution (protected for testing)
            // ============================================================

            /// @brief Resolve cluster/workgroup information from hostname or explicit parameters
            void ResolveClusterInfo();

            // ============================================================
            // Member Variables (protected for testing)
            // ============================================================

            /// @brief Resolved cluster identifier (from explicit param or hostname)
            rs_string m_resolvedClusterId;

            /// @brief Resolved workgroup name (from explicit param or hostname)
            rs_string m_resolvedWorkgroup;

            /// @brief Resolved AWS region (from explicit param or hostname)
            rs_string m_resolvedRegion;

            /// @brief Flag indicating serverless mode
            bool m_isServerless = false;

        private:

            // ============================================================
            // AWS Client Configuration
            // ============================================================

            /// @brief Create AWS client configuration for API calls
            /// @param region The AWS region
            /// @return Configured ClientConfiguration object
            Aws::Client::ClientConfiguration CreateClientConfiguration(const rs_string& region) const;

            // ============================================================
            // Member Variables for Identity Enhanced Credentials Flow
            // ============================================================

            /// @brief AWS Access Key ID for identity-enhanced credentials flow
            rs_string m_accessKeyId;

            /// @brief AWS Secret Access Key for identity-enhanced credentials flow
            rs_string m_secretAccessKey;

            /// @brief AWS Session Token for identity-enhanced credentials flow
            rs_string m_sessionToken;

            /// @brief Optional custom endpoint URL for API calls
            rs_string m_endpointUrl;
        };
    }
}

#endif