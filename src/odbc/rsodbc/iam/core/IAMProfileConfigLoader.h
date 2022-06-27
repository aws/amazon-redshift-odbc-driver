#ifndef _IAMPROFILECONFIGLOADER_H_
#define _IAMPROFILECONFIGLOADER_H_

#include <aws/core/Aws.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/utils/DateTime.h>

#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief Class used for storing AWS profiles
    class IAMProfile
    {
    public:
        /* Start of default AWS Profile implementation */
        inline const Aws::String& GetName() const
        { 
            return m_name; 
        }

        inline void SetName(const Aws::String& value)
        { 
            m_name = value; 
        }

        inline const Aws::Auth::AWSCredentials& GetCredentials() const 
        { 
            return m_credentials; 
        }

        inline void SetCredentials(const Aws::Auth::AWSCredentials& value) 
        { 
            m_credentials = value; 
        }

        inline const Aws::String& GetRegion() const
        { 
            return m_region; 
        }

        inline void SetRegion(const Aws::String& value)
        { 
            m_region = value; 
        }

        inline const Aws::String& GetRoleArn() const
        { 
            return m_roleArn; 
        }

        inline void SetRoleArn(const Aws::String& value)
        { 
            m_roleArn = value; 
        }

		inline const Aws::String& GetRoleSessionName() const
		{
			return m_roleSessionName;
		}

		inline void SetRoleSessionName(const Aws::String& value)
		{
			m_roleSessionName = value;
		}

        inline const Aws::String& GetSourceProfile() const
        { 
            return m_sourceProfile; 
        }

        inline void SetSourceProfile(const Aws::String& value)
        { 
            m_sourceProfile = value; 
        }
        /* End of default AWS Profile implementation*/

        /// @brief Set the profile attribute with the given key and value
        ///
        /// @param in_key       The key of profile attribute 
        /// @param in_value     The value of profile attribute 
        inline void SetProfileAttribute(const Aws::String& in_key, const Aws::String& in_value)
        {
            m_profile[in_key] = in_value;
        }

        /// @brief Get the profile attribute with the given key and value
        ///
        /// @param in_key       The key of profile attribute 
        /// 
        ///@return The profile attribute value for the given key if exists, else empty string
        inline Aws::String GetProfileAttribute(const Aws::String& in_key) const
        {
            if (m_profile.count(in_key))
            {
                return m_profile.at(in_key);
            }
            return "";
        }

        /// @brief Get all the customized the profile attributes
        ///
        ///@return All the customized profile attribute
        inline const Aws::Map<Aws::String, Aws::String>& GetProfileAttributes() const
        {
            return m_profile;
        }

        /// @brief Get all the customized the profile attributes
        ///
        ///@return All the customized profile attribute
        inline Aws::Map<Aws::String, Aws::String> GetProfileAttributes()
        {
            return m_profile;
        }

    private:
        Aws::String m_name;
        Aws::String m_region;
        Aws::Auth::AWSCredentials m_credentials;
        Aws::String m_roleArn;
		Aws::String m_roleSessionName;
        Aws::String m_sourceProfile;

        /* Profile attributes map that contains additional information such as plugin_name, dbUser */
        Aws::Map<Aws::String, Aws::String> m_profile;
    };

    /// @brief Class used for loading AWS profiles
    class IAMProfileConfigLoader
    {
        
    public:
        virtual ~IAMProfileConfigLoader() = default;
        /**
        * Load the configuration
        */
        bool Load();

        /**
        * Over writes the entire config source with the newly configured profile data.
        */
        bool PersistProfiles(const Aws::Map<Aws::String, IAMProfile>& profiles);

        /**
        * Gets all profiles from the configuration file.
        */
        inline const Aws::Map<Aws::String, IAMProfile>& GetProfiles() const { return m_profiles; };

        /**
        * the timestamp from the last time the profile information was loaded from file.
        */
        inline const Aws::Utils::DateTime& LastLoadTime() const { return m_lastLoadTime; }

    protected:
        /**
        * Subclasses override this method to implement fetching the profiles.
        */
        virtual bool LoadInternal() = 0;

        /**
        * Subclasses override this method to implement persisting the profiles. Default returns false.
        */
        virtual bool PersistInternal(const Aws::Map<Aws::String, IAMProfile>&) { return false; }

        Aws::Map<Aws::String, IAMProfile> m_profiles;
        Aws::Utils::DateTime m_lastLoadTime;

    };

    /**
    * Reads configuration from a config file (e.g. $HOME/.aws/config or $HOME/.aws/credentials
    */
    class IAMConfigFileProfileConfigLoader : public IAMProfileConfigLoader
    {
    public:
        /**
        * fileName - file to load config from
        * useProfilePrefix - whether or not the profiles are prefixed with "profile", credentials file is not
        * while the config file is. Defaults to off.
        */
        IAMConfigFileProfileConfigLoader(const Aws::String& fileName, bool useProfilePrefix = false);

        virtual ~IAMConfigFileProfileConfigLoader() = default;

        /**
        * File path being used for the config loader.
        */
        const Aws::String& GetFileName() const { return m_fileName; }

    protected:
        virtual bool LoadInternal() override;
        virtual bool PersistInternal(const Aws::Map<Aws::String, IAMProfile>&) override;

    private:
        Aws::String m_fileName;
        bool m_useProfilePrefix;
    };
}
}

#endif
