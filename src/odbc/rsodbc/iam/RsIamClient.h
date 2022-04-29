#ifndef _RS_IAMCLIENT_H_
#define _RS_IAMCLIENT_H_

#include "RsIamHelper.h"
#include "rs_iam_support.h"
#include "IAMFactory.h"
#include "IAMConfiguration.h"
#include "RsCredentials.h"
#include "rslock.h"

#include <aws/core/Aws.h>
#include <aws/redshift/RedshiftClient.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralServiceClient.h>

namespace RedshiftODBC
{
    class RsIamClient
    {
    public:
        /// @brief Constructor.
        ///
        /// @param in_settings      The connection settings.
        /// @param in_logger        The logger. (NOT OWN)
        RsIamClient(const RsSettings& in_settings, RsLogger* in_logger);

		/// @brief Connect to retrieve AWS IAM Credentials.
		void Connect();

        /// @brief Gets the PGOCredentials of the Iam Client.
        ///
        /// @return the the PGOCredentials of the Iam Client.
        RsCredentials GetCredentials() const;

        /// @brief Set the PGOCredentials of the Iam client.
        ///
        /// @param in_credentials
        void SetCredentials(const RsCredentials& in_credentials);

		rs_string ReadAuthProfile(rs_string auth_profile_name,
			rs_string accessKey,
			rs_string secretKey,
			rs_string host,
			rs_string region);


        // count the number of PGOIamClient be initialized, used to InitAPI and ShutdownAPI
        static int s_iamClientCount;

        // The critical section used for Aws InitAPI LOCK
        static MUTEX_HANDLE s_criticalSection;

        /// @brief Destructor.
        ~RsIamClient();

    private:

        /// @brief Send Describe cluster request to retrieve host and port
        ///
        /// @param in_client          The Redshift client
        /// @param in_clusterId       The cluster identifier to be described
        /// 
        /// @return The endpoint that the cluster belongs (domain and port)
        /// 
        /// @exception ErrorException if cannot describe cluster or cluster is not fully created.
        Aws::Redshift::Model::Endpoint DescribeCluster(
            const Aws::Redshift::RedshiftClient& in_client,
            const rs_string& in_clusterId);

		/// @brief Send Describe configuration request to retrieve host and port
		///
		/// @param in_client          The Redshift client
		/// 
		/// @return The endpoint of the configuration (domain and port)
		/// 
		/// @exception ErrorException if cannot describe configuration.
		Aws::RedshiftArcadiaCoralService::Model::Endpoint DescribeConfiguration(
			const Aws::RedshiftArcadiaCoralService::RedshiftArcadiaCoralServiceClient& in_client);

        /// @brief Send cluster credentials request 
        ///
        /// @param in_credentialsProvider   
        /// 
        /// @return The GetClusterCredentialsOutcome upon no exception occurs.
        Aws::Redshift::Model::GetClusterCredentialsOutcome SendClusterCredentialsRequest(
            const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider);

		/// @brief Send serverless credentials request 
		///
		/// @param in_credentialsProvider   
		/// 
		/// @return The GetCredentialsOutcome upon no exception occurs.
		Aws::RedshiftArcadiaCoralService::Model::GetCredentialsOutcome SendCredentialsRequest(
			const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider);

        /// @brief Process the cluster credentials request outcome
        ///
        /// @param in_outcome
        /// 
        /// @exception ErrorException when GetClusterCredentialsOutcome indicates failure.
        void ProcessClusterCredentialsOutcome(
            const Aws::Redshift::Model::GetClusterCredentialsOutcome& in_outcome);

		/// @brief Process the serverless credentials request outcome
		///
		/// @param in_outcome
		/// 
		/// @exception ErrorException when GetCredentialsOutcome indicates failure.
		void ProcessServerlessCredentialsOutcome(
			const Aws::RedshiftArcadiaCoralService::Model::GetCredentialsOutcome& in_outcome);

        /// @brief Get cluster credentials request 
        ///
        /// @param in_credentialsProvider   
        ///
        /// @exception ErrorException when GetClusterCredentialsOutcome indicates failure.
        void GetClusterCredentials(
            const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider);

		/// @brief Get serverless credentials request 
		///
		/// @param in_credentialsProvider   
		///
		/// @exception ErrorException when GetCredentialsOutcome indicates failure.
		void GetServerlessCredentials(
			const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& in_credentialsProvider);

        /// @brief Throw a PGOConnectError exception with error message
        ///
        /// @param in_error
        /// 
        /// @exception ErrorException with the error message indicated.
        void ThrowExceptionWithError(
            const Aws::Client::AWSError<Aws::Redshift::RedshiftErrors>& in_error);

        /// @brief Validate the connection attributes in PGOSettings
        /// 
        /// @exception ErrorException when any attribute in PGOSettings is not valid.
        void ValidateConnectionAttributes();

        /// @brief Infer the credentials provider to used given the connection attribute
        rs_string InferCredentialsProvider();

        /// @brief Create IAM Configuration based on the AuthType given
        ///
        /// @param in_authType          IAM Auth Type
        ///
        /// @return A IAMConfiguration object for the given AuthType
        Redshift::IamSupport::IAMConfiguration CreateIAMConfiguration(
            const rs_string& in_authType = rs_string());

        /// @brief Disabled assignment operator to avoid warning.
        ///
        /// @param in_pgoIamClient   The PGOIamClient to assign to the current PGOIamClient.
        ///
        /// @return A reference to the current PGOIamClient.
        RsIamClient& operator=(const RsIamClient& in_pgoIamClient);

        // The settings contains all connection attributes for overall PGOSetting (NOT OWN).
        const RsSettings& m_settings;

        /* credentials holder that stores all IAM authentication related credentials */
        RsCredentials m_credentials;

        // The driver log. (NOT OWN)
        RsLogger* m_log;
    };

}


#endif
