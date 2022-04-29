#include "IAMProfileConfigLoader.h"

#include <aws/core/internal/AWSHttpResourceClient.h>
#include <aws/core/utils/memory/stl/AWSList.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <fstream>

using namespace Redshift::IamSupport;
using namespace Aws;
using namespace Aws::Utils;
using namespace Aws::Auth;

namespace
{
// Internal variables and methods ==================================================================
    static const char* const CONFIG_LOADER_TAG = "IAMProfileConfigLoader";
    static const char* const REGION_KEY = "region";
    static const char* const ACCESS_KEY_ID_KEY = "aws_access_key_id";
    static const char* const SECRET_KEY_KEY = "aws_secret_access_key";
    static const char* const SESSION_TOKEN_KEY = "aws_session_token";
    static const char* const ROLE_ARN_KEY = "role_arn";
    static const char* const SOURCE_PROFILE_KEY = "source_profile";
    static const char* const PROFILE_PREFIX = "profile ";
    static const char EQ = '=';
    static const char LEFT_BRACKET = '[';
    static const char RIGHT_BRACKET = ']';
    static const char* const PARSER_TAG = "IAMConfigFileProfileFSM";
    static const char* const CONFIG_FILE_LOADER = "IAMConfigFileProfileConfigLoader";

    class ConfigFileProfileFSM
    {
        
    public:
        ConfigFileProfileFSM() : m_parserState(START) {}

        const Aws::Map<String, IAMProfile>& GetProfiles() const { return m_foundProfiles; }

        void ParseStream(Aws::IStream& stream)
        {
            static const size_t ASSUME_EMPTY_LEN = 3;

            Aws::String line;
            while (std::getline(stream, line) && m_parserState != FAILURE)
            {
                if (line.empty() || line.length() < ASSUME_EMPTY_LEN)
                {
                    continue;
                }

                auto openPos = line.find(LEFT_BRACKET);
                auto closePos = line.find(RIGHT_BRACKET);

                switch (m_parserState)
                {

                case START:
                    if (openPos != std::string::npos && closePos != std::string::npos)
                    {
                        FlushProfileAndReset(line, openPos, closePos);
                        m_parserState = PROFILE_FOUND;
                    }
                    break;

                    //fall through here is intentional to reduce duplicate logic
                case PROFILE_KEY_VALUE_FOUND:
                    if (openPos != std::string::npos && closePos != std::string::npos)
                    {
                        m_parserState = PROFILE_FOUND;
                        FlushProfileAndReset(line, openPos, closePos);
                        break;
                    }
                    // fall through
                case PROFILE_FOUND:
                {
                    auto keyValuePair = StringUtils::Split(line, EQ);
                    if (keyValuePair.size() == 2)
                    {
                        String profileAttrKey = StringUtils::ToLower(keyValuePair[0].c_str());
                        m_profileKeyValuePairs[StringUtils::Trim(profileAttrKey.c_str())] =
                            StringUtils::Trim(keyValuePair[1].c_str());
                        m_parserState = PROFILE_KEY_VALUE_FOUND;
                    }

                    break;
                }
                default:
                    m_parserState = FAILURE;
                    break;
                }
            }

            FlushProfileAndReset(line, 0, 0);
        }

    private:

        void FlushProfileAndReset(Aws::String& line, size_t openPos, size_t closePos)
        {
            if (!m_currentWorkingProfile.empty() && !m_profileKeyValuePairs.empty())
            {
                IAMProfile profile;
                profile.SetName(m_currentWorkingProfile);

                auto regionIter = m_profileKeyValuePairs.find(REGION_KEY);
                if (regionIter != m_profileKeyValuePairs.end())
                {
                    profile.SetRegion(regionIter->second);
                }

                auto accessKeyIdIter = m_profileKeyValuePairs.find(ACCESS_KEY_ID_KEY);
                Aws::String accessKey, secretKey, sessionToken;
                if (accessKeyIdIter != m_profileKeyValuePairs.end())
                {
                    accessKey = accessKeyIdIter->second;

                    auto secretAccessKeyIter = m_profileKeyValuePairs.find(SECRET_KEY_KEY);
                    auto sessionTokenIter = m_profileKeyValuePairs.find(SESSION_TOKEN_KEY);
                    if (secretAccessKeyIter != m_profileKeyValuePairs.end())
                    {
                        secretKey = secretAccessKeyIter->second;
                    }

                    if (sessionTokenIter != m_profileKeyValuePairs.end())
                    {
                        sessionToken = sessionTokenIter->second;
                    }

                    profile.SetCredentials(Aws::Auth::AWSCredentials(accessKey, secretKey, sessionToken));
                }

                auto assumeRoleArnIter = m_profileKeyValuePairs.find(ROLE_ARN_KEY);
                if (assumeRoleArnIter != m_profileKeyValuePairs.end())
                {
                    profile.SetRoleArn(assumeRoleArnIter->second);
                }

                auto sourceProfileIter = m_profileKeyValuePairs.find(SOURCE_PROFILE_KEY);
                if (sourceProfileIter != m_profileKeyValuePairs.end())
                {
                    profile.SetSourceProfile(sourceProfileIter->second);
                }

                /* read all customized profile attributes from the AWS profile */
                for (const auto& el : m_profileKeyValuePairs)
                {
                    profile.SetProfileAttribute(el.first, el.second);
                }

                m_foundProfiles[profile.GetName()] = profile;
                m_currentWorkingProfile.clear();
                m_profileKeyValuePairs.clear();
            }

            if (!line.empty() && openPos != std::string::npos && closePos != std::string::npos)
            {
                m_currentWorkingProfile = StringUtils::Trim(line.substr(openPos + 1, closePos - openPos - 1).c_str());
                StringUtils::Replace(m_currentWorkingProfile, PROFILE_PREFIX, "");
            }
        }

        enum State
        {
            START = 0,
            PROFILE_FOUND,
            PROFILE_KEY_VALUE_FOUND,
            FAILURE
        };

        Aws::String m_currentWorkingProfile;
        Aws::Map<String, String> m_profileKeyValuePairs;
        State m_parserState;
        Aws::Map<String, IAMProfile> m_foundProfiles;
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMProfileConfigLoader::Load()
{
    if (LoadInternal())
    {
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMProfileConfigLoader::PersistProfiles(const Aws::Map<Aws::String, IAMProfile>& profiles)
{
    if (PersistInternal(profiles))
    {
        m_profiles = profiles;
        m_lastLoadTime = DateTime::Now();
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMConfigFileProfileConfigLoader::IAMConfigFileProfileConfigLoader(
    const Aws::String& fileName, 
    bool useProfilePrefix) :
    m_fileName(fileName),
    m_useProfilePrefix(useProfilePrefix)
{
    
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMConfigFileProfileConfigLoader::LoadInternal()
{
    m_profiles.clear();

    Aws::IFStream inputFile(m_fileName.c_str());
    if (inputFile)
    {
        ConfigFileProfileFSM parser;
        parser.ParseStream(inputFile);
        m_profiles = parser.GetProfiles();
        return m_profiles.size() > 0;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMConfigFileProfileConfigLoader::PersistInternal(const Aws::Map<Aws::String, IAMProfile>& profiles)
{
    Aws::OFStream outputFile(m_fileName.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (outputFile)
    {
        for (auto& profile : profiles)
        {
            Aws::String prefix = m_useProfilePrefix ? PROFILE_PREFIX : "";

            outputFile << LEFT_BRACKET << prefix << profile.second.GetName() << RIGHT_BRACKET << std::endl;
            const Aws::Auth::AWSCredentials& credentials = profile.second.GetCredentials();
            outputFile << ACCESS_KEY_ID_KEY << EQ << credentials.GetAWSAccessKeyId() << std::endl;
            outputFile << SECRET_KEY_KEY << EQ << credentials.GetAWSSecretKey() << std::endl;

            if (!credentials.GetSessionToken().empty())
            {
                outputFile << SESSION_TOKEN_KEY << EQ << credentials.GetSessionToken() << std::endl;
            }

            if (!profile.second.GetRegion().empty())
            {
                outputFile << REGION_KEY << EQ << profile.second.GetRegion() << std::endl;
            }

            if (!profile.second.GetRoleArn().empty())
            {
                outputFile << ROLE_ARN_KEY << EQ << profile.second.GetRoleArn() << std::endl;
            }

            if (!profile.second.GetSourceProfile().empty())
            {
                outputFile << SOURCE_PROFILE_KEY << EQ << profile.second.GetSourceProfile() << std::endl;
            }

            outputFile << std::endl;
        }

        return true;
    }

    return false;
}

