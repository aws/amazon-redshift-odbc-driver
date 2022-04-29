#ifndef _RS_CREDENTIALS_H_
#define _RS_CREDENTIALS_H_


// #include "RsIamHelper.h"
#include "IAMCredentials.h"
#include "RsSettings.h"

#include <aws/core/auth/AWSCredentialsProvider.h>

namespace RedshiftODBC
{
    class RsCredentials : public Redshift::IamSupport::IAMCredentials
    {
    public:
        /// @brief Constructor
        ///
        /// @param in_accessId            The AWS Access ID
        /// @param in_secretKey           The AWS Secret Access Key
        /// @param in_sessionToken        The AWS Session Token
        explicit RsCredentials(
            const rs_string& in_accessId     = rs_string(),
            const rs_string& in_secretKey    = rs_string(),
            const rs_string& in_sessionToken = rs_string());

        /// @brief Constructor
        ///
        /// @param in_dbUser               The Redshift cluster database user
        /// @param in_dbPassword           The Redshift cluster database password
        /// @param in_expirationTime       The Redshift cluster dbUser and dbPassword expiration time
        RsCredentials(
            const rs_string& in_dbUser,
            const rs_string& in_dbPassword,
            long in_expirationTime);

        /// @brief Constructor
        ///
        /// @param in_credentials          The AWS credentials
        explicit RsCredentials(
            const Aws::Auth::AWSCredentials& in_credentials);

        /// @brief Returns the settings of the credentials holder
        ///
        /// @return the connection settings of the credentials holder.
        const RsSettings & GetSettings() const;

        /// @brief Returns the database user of the credentials holder
        ///
        /// @return the dbUser of the credentials holder.
        rs_string GetDbUser() const;

        /// @brief Sets the database user of the credentials holder
        ///
        /// @param in_dbUser               The Redshift cluster database user
        void SetDbUser(const rs_string& in_dbUser);

        /// @brief Returns the database password of the credentials holder
        ///
        /// @return the dbPassword of the credentials holder.
        rs_string GetDbPassword() const;

        /// @brief Sets the database password of the credentials holder
        ///
        /// @param in_dbPassword           The Redshift cluster database password
        void SetDbPassword(const rs_string& in_dbPassword);

        /// @brief Sets the expiration time of the credentials holder
        ///
        /// @param in_expirationTime       The Redshift cluster credentials expiration time
        void SetExpirationTime(long in_expirationTime);

        /// @brief Returns the Redshift cluster credentials expiration time of the credentials holder
        ///
        /// @return The Redshift cluster credentials expiration time of the credentials holder.
        long GetExpirationTime() const;

        /// @brief Sets the host of the credentials holder
        ///
        /// @param in_host       The Redshift cluster host address
        void SetHost(const rs_string& in_host);

        /// @brief Returns the Redshift cluster host address of the credentials holder
        ///
        /// @return The Redshift cluster host address expiration time of the credentials holder.
        rs_string GetHost() const;

        /// @brief Sets the port of the credentials holder
        ///
        /// @param in_port       The Redshift cluster host port
        void SetPort(short in_port);

        /// @brief Returns the Redshift cluster host port of the credentials holder
        ///
        /// @return The Redshift cluster host port expiration time of the credentials holder.
        short GetPort() const;

        /// @brief Destructor.
        ~RsCredentials();

    private:
        /* Connection settings cached by the credentials holder. [Read-only]
           Used to verify if the cached IAM credentials is valid */
        RsSettings  m_settings;

        /* Redshift cluster credentials cached by the credentials holder */
        rs_string m_dbUser;
        rs_string m_dbPassword;
        long  m_expirationTime;

        /* Host and port retrieved from DescribeCluster call */
        rs_string m_host;
        short m_port;
    };
}

#endif
