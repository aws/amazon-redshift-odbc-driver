#ifndef _IAMCREDENTIALS_H_
#define _IAMCREDENTIALS_H_

#include <aws/core/auth/AWSCredentialsProvider.h>

#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    class IAMCredentials
    {
    public:
        /// @brief Constructor
        ///
        /// @param in_accessId            The AWS Access ID
        /// @param in_secretKey           The AWS Secret Access Key
        /// @param in_sessionToken        The AWS Session Token
        explicit IAMCredentials(
            const rs_string& in_accessId     = rs_string(),
            const rs_string& in_secretKey    = rs_string(),
            const rs_string& in_sessionToken = rs_string());

        /// @brief Constructor
        ///
        /// @param in_credentials          The AWS credentials
        explicit IAMCredentials(
            const Aws::Auth::AWSCredentials& in_credentials);

        /// @brief Returns the AWSCredentials of the IAMCredentials
        ///
        /// @return the AWSCredentials of the credentials holder
        virtual Aws::Auth::AWSCredentials GetAWSCredentials() const;

        /// @brief Sets the AWSCredentials of the IAMCredentials
        ///
        /// @param in_credentials           The AWSCredentials
        virtual void SetAWSCredentials(const Aws::Auth::AWSCredentials& in_credentials);

        /// @brief Returns the AccessId of the IAMCredentials
        ///
        /// @return AccessId of the IAMCredentials
        virtual rs_string GetAccessId() const;

        /// @brief Sets the AccessId of the IAMCredentials
        ///
        /// @param in_accessId      AccessId of the IAMCredentials
        virtual void SetAccessId(const rs_string& in_accessId);

        /// @brief Returns the SecretKey of the IAMCredentials
        ///
        /// @return SecretKey of the IAMCredentials
        virtual rs_string GetSecretKey() const;

        /// @brief Sets the SecretKey of the IAMCredentials
        ///
        /// @param in_secretKey      SecretKey of the IAMCredentials
        virtual void SetSecretKey(const rs_string& in_secretKey);

        /// @brief Returns the SessionToken of the IAMCredentials
        ///
        /// @return SessionToken of the IAMCredentials
        virtual rs_string GetSessionToken() const;

        /// @brief Sets the SessionToken of the IAMCredentials
        ///
        /// @param in_sessionToken      SessionToken of the IAMCredentials
        virtual void SetSessionToken(const rs_string& in_sessionToken);

        /// @brief Returns the dbUser of the IAMCredentials
        ///
        /// @return the dbUser of the IAMCredentials
        virtual rs_string GetDbUser() const;

        /// @brief Sets the dbUser of the IAMCredentials
        ///
        /// @param in_dbUser        The database user
        virtual void SetDbUser(const rs_string& in_dbUser);

        /// @brief Returns the dbGroups of the IAMCredentials
        ///
        /// @return the DbGroups of the IAMCredentials
        virtual rs_string GetDbGroups() const;

        /// @brief Sets the DbGroups of the IAMCredentials
        ///
        /// @param in_dbGroups        The database groups
        virtual void SetDbGroups(const rs_string& in_dbGroups);

        /// @brief Returns the ForceLowercase of the IAMCredentials
        ///
        /// @return the ForceLowercase of the IAMCredentials
        virtual bool GetForceLowercase() const;
        
        /// @brief Sets the ForceLowercase of the IAMCredentials
        ///
        /// @param in_forceLowercase       Database Group Names Force Lowercase
        virtual void SetForceLowercase(bool in_forceLowercase);

        /// @brief Returns the AutoCreate of the IAMCredentials
        ///
        /// @return the AutoCreate of the IAMCredentials
        virtual bool GetAutoCreate() const;

        /// @brief Sets the AutoCreate of the IAMCredentials
        ///
        /// @param in_autoCreate        Database User Auto Create
        virtual void SetAutoCreate(bool in_autoCreate);

        /// @brief Destructor.
        virtual ~IAMCredentials();

    private:
        Aws::Auth::AWSCredentials m_credentials;  /* AWSCredentials */

        rs_string m_dbUser;   /* dbUser retrieved from plug-in or profile */
        rs_string m_dbGroups; /* dbGroups retrieved from plug-in or profile */
        bool m_forceLowercase;   /* Database Group Names Force Lowercase retrieved from 
                                    plug-in or profile */
        bool m_autoCreate;       /* User AutoCreate retrieved from plug-in or profile */
    };
}
}

#endif
