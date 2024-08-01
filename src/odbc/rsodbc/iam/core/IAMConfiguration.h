#ifndef _IAMCONFIGURATION_H_
#define _IAMCONFIGURATION_H_

#include "IAMCredentials.h"

#include <map>
#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief Configuration used for constructing CredentialsProvider
    class IAMConfiguration
    {
    public:
        /// @brief Constructor
        /// 
        /// @param in_credentials        The IAM Credentials         
        explicit IAMConfiguration(const IAMCredentials& in_credentials = IAMCredentials());

        /// @brief Returns the AWSCredentials of the IAMConfiguration 
        ///
        /// @return the AWSCredentials of the credentials holder
        Aws::Auth::AWSCredentials GetAWSCredentials() const;

        /// @brief Sets the AWSCredentials of the IAMConfiguration
        ///
        /// @param in_credentials           The AWSCredentials
        void SetAWSCredentials(const Aws::Auth::AWSCredentials& in_credentials);

        /// @brief Returns the AccessId of the IAMConfiguration
        ///
        /// @return AccessId of the IAMConfiguration
        rs_string GetAccessId() const;

        /// @brief Sets the AccessId of the IAMConfiguration
        ///
        /// @param in_accessId      AccessId of the IAMConfiguration
        void SetAccessId(const rs_string& in_accessId);

        /// @brief Returns the SecretKey of the IAMConfiguration
        ///
        /// @return SecretKey of the IAMConfiguration
        rs_string GetSecretKey() const;

        /// @brief Sets the SecretKey of the IAMConfiguration
        ///
        /// @param in_secretKey      SecretKey of the IAMConfiguration
        void SetSecretKey(const rs_string& in_secretKey);

        /// @brief Returns the SessionToken of the IAMConfiguration
        ///
        /// @return SessionToken of the IAMConfiguration
        rs_string GetSessionToken() const;

        /// @brief Sets the SessionToken of the IAMConfiguration
        ///
        /// @param in_sessionToken      SessionToken of the IAMConfiguration
        void SetSessionToken(const rs_string& in_sessionToken);

        /// @brief Returns the dbUser of the IAMConfiguration
        ///
        /// @return the dbUser of the IAMConfiguration
        rs_string GetDbUser() const;

        /// @brief Sets the dbUser of the IAMConfiguration
        ///
        /// @param in_dbUser        The database user
        void SetDbUser(const rs_string& in_dbUser);

        /// @brief Returns the dbGroups of the IAMConfiguration
        ///
        /// @return the DbGroups of the IAMConfiguration
        rs_string GetDbGroups() const;

        /// @brief Sets the DbGroups of the IAMConfiguration
        ///
        /// @param in_dbGroups        The database groups
        void SetDbGroups(const rs_string& in_dbGroups);

        /// @brief Returns the ForceLowercase of the IAMConfiguration
        ///
        /// @return the ForceLowercase of the IAMConfiguration
        bool GetForceLowercase() const;
        
        /// @brief Sets the ForceLowercase of the IAMConfiguration
        ///
        /// @param in_forceLowercase        Database Group Names Force Lowercase
        void SetForceLowercase(bool in_forceLowercase);
        
        /// @brief Returns the AutoCreate of the IAMConfiguration
        ///
        /// @return the AutoCreate of the IAMConfiguration
        bool GetAutoCreate() const;

        /// @brief Sets the AutoCreate of the IAMConfiguration
        ///
        /// @param in_autoCreate        Database User Auto Create
        void SetAutoCreate(bool in_autoCreate);

        /// @brief Returns the ProfileName of the IAMConfiguration
        ///
        /// @return ProfileName of the IAMConfiguration
        rs_string GetProfileName() const;

        /// @brief Sets the ProfileName of the IAMConfiguration
        ///
        /// @param in_profileName      ProfileName of the IAMConfiguration
        void SetProfileName(const rs_string& in_profileName);

        /// @brief Returns the PluginName of the IAMConfiguration
        ///
        /// @return PluginName of the IAMConfiguration
        rs_string GetPluginName() const;

        /// @brief Sets the PluginName of the IAMConfiguration
        ///
        /// @param in_pluginName      PluginName of the IAMConfiguration
        void SetPluginName(rs_string in_pluginName);

        /// @brief Returns the Plugin user of the IAMConfiguration
        ///
        /// @return Plugin user of the IAMConfiguration
        rs_string GetUser() const;

        /// @brief Sets Plugin user of the IAMConfiguration
        ///
        /// @param in_user         Plugin user of the IAMConfiguration
        void SetUser(const rs_string& in_user);

        rs_string GetRegion() const;
        void SetRegion(const rs_string& in_region);

        /// @brief Returns the Plugin password of the IAMConfiguration
        ///
        /// @return Plugin password of the IAMConfiguration
        rs_string GetPassword() const;

        /// @brief Sets Plugin password of the IAMConfiguration
        ///
        /// @param in_password      Plugin password of the IAMConfiguration
        void SetPassword(const rs_string& in_password);

        /// @brief Returns the IdpHost of the IAMConfiguration
        ///
        /// @return IdpHost of the IAMConfiguration
        rs_string GetIdpHost() const;

        /// @brief Sets the IdpHost of the IAMConfiguration
        ///
        /// @param in_idpHost      IdpHost of the IAMConfiguration
        void SetIdpHost(const rs_string& in_idpHost);

        /// @brief Returns the IdpPort of the IAMConfiguration
        ///
        /// @return IdpPort of the IAMConfiguration
        short GetIdpPort() const;

        /// @brief Sets the IdpPort of the IAMConfiguration
        ///
        /// @param in_idpPort      IdpPort of the IAMConfiguration
        void SetIdpPort(short in_idpPort);

        /// @brief Returns the IdpTenant of the IAMConfiguration
        ///
        /// @return IdpTenant of the IAMConfiguration
        rs_string GetIdpTenant() const;

        /// @brief Sets the IdpTenant of the IAMConfiguration
        ///
        /// @param in_idpTenant      IdpTenant of the IAMConfiguration
        void SetIdpTenant(const rs_string& in_idpTenant);

        /// @brief Returns the ClientSecret of the IAMConfiguration
        ///
        /// @return ClientSecret of the IAMConfiguration
        rs_string GetClientSecret() const;

        /// @brief Sets the ClientSecret of the IAMConfiguration
        ///
        /// @param in_clientSecret      ClientSecret of the IAMConfiguration
        void SetClientSecret(const rs_string& in_clientSecret);

        /// @brief Returns the ClientId of the IAMConfiguration
        ///
        /// @return ClientId of the IAMConfiguration
        rs_string GetClientId() const;

        /// @brief Sets the ClientId of the IAMConfiguration
        ///
        /// @param in_clientId      ClientId of the IAMConfiguration
        void SetClientId(const rs_string& in_clientId);

		/// @brief Returns the Scope of the IAMConfiguration
		///
		/// @return scope of the IAMConfiguration
		rs_string GetScope() const;

		/// @brief Sets the Scope of the IAMConfiguration
		///
		/// @param in_scope      Scope of the IAMConfiguration
		void SetScope(const rs_string& in_scope);

        /// @brief Returns the IDP Response Timeout (in seconds) of the IAMConfiguration
        ///
        /// @return Timeout of the IAMConfiguration
        short GetIdpResponseTimeout() const;

        /// @brief Sets the IDP Response Timeout (in seconds) of the IAMConfiguration
        ///
        /// @param in_idp_response_timeout      in_idp_response_timeout of the IAMConfiguration
        void SetIdpResponseTimeout(short in_idp_response_timeout);

        /// @brief Returns the Listen port of the IAMConfiguration
        ///
        /// @return Listen port of the IAMConfiguration
        short GetListenPort() const;

        /// @brief Sets the Listen port of the IAMConfiguration
        ///
        /// @param in_listen_port      in_listen_port of the IAMConfiguration
        void SetListenPort(short in_listen_port);

        /// @brief Returns the Login URL of the IAMConfiguration
        ///
        /// @return Login URL of the IAMConfiguration
        rs_string GetLoginURL() const;

        /// @brief Sets the Login URL of the IAMConfiguration
        ///
        /// @param in_login_url      Login URL of the IAMConfiguration
        void SetLoginURL(const rs_string& in_login_url);

        /// @brief Returns the Role ARN of the IAMConfiguration
        ///
        /// @return Role ARN of the IAMConfiguration
        rs_string GetRoleARN() const;

        /// @brief Sets the Role ARN of the IAMConfiguration
        ///
        /// @param in_role_arn      Role ARN of the IAMConfiguration
        void SetRoleARN(const rs_string& in_role_arn);

        /// @brief Returns the Web Identity Token of the IAMConfiguration
        ///
        /// @return Web Identity Token of the IAMConfiguration
        rs_string GetWebIdentityToken() const;

        /// @brief Sets the Web Identity Token of the IAMConfiguration
        ///
        /// @param in_web_identity_token Web Identity Token of the IAMConfiguration
        void SetWebIdentityToken(const rs_string& in_web_identity_token);

        /// @brief Returns the duration, in seconds, of the IAMConfiguration
        ///
        /// @return duration, in seconds, of the IAMConfiguration
        short GetDuration() const;

        /// @brief Sets the duration, in seconds, of the IAMConfiguration
        ///
        /// @param in_duration duration, in seconds, of the IAMConfiguration
        void SetDuration(short in_duration);

        /// @brief Returns the Role Session Name of the IAMConfiguration
        ///
        /// @return Role Session Name of the IAMConfiguration
        rs_string GetRoleSessionName() const;

        /// @brief Sets the Role Session Name of the IAMConfiguration
        ///
        /// @param in_role_session_name Role Session Name of the IAMConfiguration
        void SetRoleSessionName(const rs_string& in_role_session_name);

        /// @brief Sets the dbGroups filter of the IAMConfiguration
        ///
        /// @return dbGroups filter of the IAMConfiguration
        rs_string GetDbGroupsFilter() const;

        /// @brief Sets the dbGroups filter of the IAMConfiguration
        ///
        /// @param in_group_filter      dbGroups filter of the IAMConfiguration
        void SetDbGroupsFilter(const rs_string& in_groups_filter);

        /// @brief Returns the PreferredRole of the IAMConfiguration
        ///
        /// @return PreferredRole of the IAMConfiguration
        rs_string GetPreferredRole() const;

        /// @brief Sets the PreferredRole of the IAMConfiguration
        ///
        /// @param in_preferredRole      PreferredRole of the IAMConfiguration
        void SetPreferredRole(const rs_string& in_preferredRole);

        /// @brief Returns the AppId of the IAMConfiguration
        ///
        /// @return AppId of the IAMConfiguration
        rs_string GetAppId() const;

        /// @brief Sets the AppId of the IAMConfiguration
        ///
        /// @param in_appId      AppId of the IAMConfiguration
        void SetAppId(const rs_string& in_appId);

        /// @brief Returns the Okta AppName of the IAMConfiguration
        ///
        /// @return Okta App Name of the IAMConfiguration
        rs_string GetAppName() const;

        /// @brief Sets the Okta AppName of the IAMConfiguration
        ///
        /// @param in_appName Okta AppName of the IAMConfiguration
        void SetAppName(const rs_string& in_appName);

        /// @brief Returns the PartnerSpId of the IAMConfiguration
        ///
        /// @return PartnerSpId of the IAMConfiguration
        rs_string GetPartnerSpId() const;

        /// @brief Sets the PartnerSpId of the IAMConfiguration
        ///
        /// @param in_partnerSpId      AppId of the IAMConfiguration
        void SetPartnerSpId(const rs_string& in_partnerSpId);

        /// @brief Returns the loginToRp of the IAMConfiguration
        ///
        /// @return loginToRp of the IAMConfiguration
        rs_string GetLoginToRp() const;
        
        /// @brief Sets the loginToRp of the IAMConfiguration
        ///
        /// @param in_loginToRp      loginToRp of the IAMConfiguration
        void SetLoginToRp(const rs_string& in_loginToRp);

        /// @brief Returns the SslInsecure of the IAMConfiguration
        ///
        /// @return SslInsecure of the IAMConfiguration
        bool GetSslInsecure() const;

        /// @brief Sets the SslInsecure of the IAMConfiguration
        ///
        /// @param in_sslInsecure      SslInsecure of the IAMConfiguration
        void SetSslInsecure(bool in_sslInsecure);

        /// @brief Returns the CaFile of the IAMConfiguration
        ///
        /// @return CaFile of the IAMConfiguration
        rs_string GetCaFile() const;

        /// @brief Sets the CaFile of the IAMConfiguration
        ///
        /// @param in_caFile      CaFile of the IAMConfiguration
        void SetCaFile(const rs_string& in_caFile);

        /// @brief Returns the Host of the IAMConfiguration Proxy
        ///
        /// @return Host of the IAMConfiguration HTTPS Proxy
        rs_string GetHTTPSProxyHost() const;

        /// @brief Sets the Host of the HTTPS Proxy
        ///
        /// @param in_proxyHost     Host of the HTTPS Proxy
        void SetHTTPSProxyHost(const rs_string& in_proxyHost);

        /// @brief Returns the Port of the HTTPS Proxy
        ///
        /// @return Port of the HTTPS Proxy
        short GetHTTPSProxyPort() const;

        /// @brief Sets the Port of the HTTPS Proxy
        ///
        /// @param in_proxyPort     Port of the HTTPS Proxy
        void SetHTTPSProxyPort(const short in_proxyPort);

        /// @brief Returns the Username of the HTTPS Proxy
        ///
        /// @return Username of the HTTPS Proxy
        rs_string GetHTTPSProxyUser() const;

        /// @brief Sets the Username of the HTTPS Proxy
        ///
        /// @param in_proxyUser     Username of the HTTPS Proxy
        void SetHTTPSProxyUser(const rs_string& in_proxyUser);

        /// @brief Returns the Password of the HTTPS Proxy
        ///
        /// @return Password of the HTTPS Proxy
        rs_string GetHTTPSProxyPassword() const;

        /// @brief Sets the Password of the HTTPS Proxy
        ///
        /// @param in_proxyPassword     Password of the HTTPS Proxy
        void SetHTTPSProxyPassword(const rs_string& in_proxyPassword);

        /// @brief Gets the boolean value whether to use IdP Auth for Proxy
        bool GetUseProxyIdpAuth() const;

        /// @brief Sets the boolean value whether to use IdP Auth for Proxy
        ///
        /// @param in_useProxy     Value of the whether to use IdP Auth for Proxy
        void SetUseProxyIdpAuth(bool in_useProxy);

		/// @brief Returns the URL of the AWS Endpoint
		rs_string GetEndpointUrl() const;

		/// @brief Sets the AWS Endpoint URL
		///
		/// @param in_endpointUrl     URL of the AWS Endpoint
		void SetEndpointUrl(const rs_string& in_endpointUrl);

		/// @brief Returns the URL of the AWS STS Endpoint
		rs_string GetStsEndpointUrl() const;

		/// @brief Sets the AWS STS Endpoint URL
		///
		/// @param in_stsEndpointUrl     URL of the AWS STS Endpoint
		void SetStsEndpointUrl(const rs_string& in_stsEndpointUrl);

		/// @brief Returns the STS connection timeout (in seconds)
		int GetStsConnectionTimeout() const;

		/// @brief Sets the STS connection timeout (in seconds)
		///
		/// @param in_stsConnectionTimeout     The STS connection timeout (in seconds)
		void SetStsConnectionTimeout(int in_stsConnectionTimeout);

		/// @brief Returns the AuthProfile used for common settings
		rs_string GetAuthProfile() const;

		/// @brief Sets the AuthProfile
		///
		/// @param in_authProfile     AuthProfile used for common settings
		void SetAuthProfile(const rs_string& in_authProfile);


        /// @brief Gets the boolean value whether a HTTPS proxy is used
        bool GetUsingHTTPSProxy() const;

        /// @brief Sets the boolean value whether a HTTPS proxy is used
        ///
        /// @param in_useProxy     Value of the whether a HTTPS proxy is used
        void SetUsingHTTPSProxy(bool in_useProxy);

        /// @brief Returns the IdP auth token used in IdP Auth Token plugin
        rs_string GetIdpAuthToken() const;

        /// @brief Sets the IdP auth token
        ///
        /// @param in_idpAuthToken Auth token used in IdP Auth Token plugin
        void SetIdpAuthToken(const rs_string& in_idpAuthToken);

        /// @brief Returns the token type used in IdP Auth Token plugin
        rs_string GetIdpAuthTokenType() const;

        /// @brief Sets the IdP auth token type
        ///
        /// @param in_idpAuthTokenType token type used in IdP Auth Token plugin
        void SetIdpAuthTokenType(const rs_string& in_idpAuthTokenType);

        /// @brief Gets the in_key setting of the IAMConfiguration
        ///
        /// @param in_key      Key setting of the IAMConfiguration
        /// 
        /// @return the setting value corresponding to the key in IAMConfiguration
        rs_string GetSetting(const rs_string& in_key) const;

        /// @brief Sets the setting of the IAMConfiguration
        ///
        /// @param in_key      Key setting of the IAMConfiguration
        /// @param in_value    Value of the key settings of the IAMConfiguration
        void SetSetting(const rs_string& in_key, const rs_string& in_value);

        /// @brief Removes the setting of the IAMConfiguration
        ///
        /// @param in_key      Key setting of the IAMConfiguration
        void RemoveSetting(const rs_string& in_key);

        /// @brief Returns the start url used in Browser IdC plugin
        rs_string GetIssuerUrl() const;

        /// @brief Sets the startUrl
        ///
        /// @param in_startUrl     Start url used in Browser IdC plugin
        void SetIssuerUrl(const rs_string& in_issuerUrl);

        /// @brief Returns the IdC region used in Browser IdC plugin
        rs_string GetIdcRegion() const;

        /// @brief Sets the idcRegion
        ///
        /// @param in_idcRegion     IdC region used in Browser IdC plugin
        void SetIdcRegion(const rs_string& in_idcRegion);

        /// @brief Returns the display name of the client that is using the IdC browser auth plugin
        rs_string GetIdcClientDisplayName() const;

        /// @brief Sets the display name of the client that is using the IdC browser auth plugin
        ///
        /// @param in_idcClientDisplayName     Client display name
        void SetIdcClientDisplayName(const rs_string& in_idcClientDisplayName);

        /// @brief Destructor.
        ~IAMConfiguration();

    private:
        /* IAMCredentials that contains AWSCredentials, dbUser, dbGroup, forceLowercase, autoCreate
           Used for Static AuthType (AWSCredentials) and additional parameter for 
           plugin and profile (dbUser, dbGroup, forceLowercase, autoCreate) */
        IAMCredentials m_credentials; 

        /* Additional customizable settings mainly used for AuthType Plugin and Profile  */
        std::map<rs_string, rs_string> m_settings;

        /* Flag to determine whether an HTTPS proxy is used */
        bool m_usingProxy = false;
    };
}
}

#endif
