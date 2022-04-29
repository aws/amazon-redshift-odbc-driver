#include "IAMJwtPluginCredentialsProvider.h"
#include "IAMUtils.h"
#include <sstream>

#include <aws/sts/STSClient.h>
#include <aws/core/utils/base64/Base64.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/sts/model/AssumeRoleWithWebIdentityRequest.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::STS;
using namespace Aws::Utils;

namespace
{
    static const char* LOG_TAG = "IAMJwtPluginCredentialsProvider";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMJwtPluginCredentialsProvider::IAMJwtPluginCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::IAMJwtPluginCredentialsProvider");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMJwtPluginCredentialsProvider::GetAWSCredentials()
{
    RS_LOG(m_log)( "IAMJwtPluginCredentialsProvider", "GetAWSCredentials");
    /* return cached AWSCredentials from the IAMCredentialsHolder */
    if (CanUseCachedAwsCredentials())
    {
        return m_credentials.GetAWSCredentials();
    }

    /* Validate that all required arguments for plugin are provided */
    ValidateArgumentsMap();

    AWSCredentials credentials = GetAWSCredentialsWithJwt(GetJwtAssertion());
    SaveSettings(credentials); /* cache returned credentials in IAMCredentials Holder */
    return credentials;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMJwtPluginCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::ValidateArgumentsMap");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMJwtPluginCredentialsProvider::DecodeBase64String(const rs_string& str)
{
    RS_LOG(m_log)( "IAMJwtPluginCredentialsProvider", "DecodeBase64String");
    Base64::Base64 base64;
    ByteBuffer buf = base64.Decode(str);
    return rs_string(
        reinterpret_cast<const char*>(buf.GetUnderlyingData()),
        buf.GetLength());
}

void IAMJwtPluginCredentialsProvider::AlignPayloadToken(rs_string& str)
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::AlignPayloadToken");

    int padding = str.size() % 4;
    if (padding != 0)
    {
        for (int i = 4 - padding; i > 0; i--)
        {
            str += "=";
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
JWTAssertion IAMJwtPluginCredentialsProvider::DecodeJwtToken(const rs_string& jwt)
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::DecodeJwtToken");

    std::vector<rs_string> tokens;
    std::stringstream ss(jwt);
    rs_string token;

    while (std::getline(ss, token, '.'))
    {
        RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::DecodeJwtToken ",
            + "token: %s", token.c_str());
        tokens.push_back(token);
    }

    if (tokens.size() != 3)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Invalid number of tokens inside JWT assertion.");
    }

    AlignPayloadToken(tokens[1]);

    JWTAssertion jwtAssertion{
        DecodeBase64String(tokens[0]),
        DecodeBase64String(tokens[1]),
        tokens[2]
    };

    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::DecodeJwtToken ",
          + "Header: %s, Payload: %s, Signature: %s",
        jwtAssertion.header.c_str(),
        jwtAssertion.payload.c_str(),
        jwtAssertion.signature.c_str());

    return jwtAssertion;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMJwtPluginCredentialsProvider::RetrieveDbUserField(const JWTAssertion& jwt)
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::ParseJWTAssertion");

    Json::JsonValue json(jwt.payload);

    if (!json.WasParseSuccessful())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to get JSON from JWT assertion.");
    }

    std::vector<rs_string> fields = {
        "DbUser", "upn", "preferred_username", "email"
    };
    rs_string dbuser;

    for (const auto& f : fields)
    {
        dbuser = GetValueByKeyFromJson(json, f);

        RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::RetrieveDbUserField ",
            + "%s: %s", f.c_str(), dbuser.c_str());

        if (!dbuser.empty())
        {
            break;
        }
    }

    if (dbuser.empty())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "DbUser is missing in the JWT assertion.");
    }

    /* Replace dbuser and autocreate. */
    m_argsMap[IAM_KEY_DBUSER] = dbuser;
    m_argsMap[IAM_KEY_AUTOCREATE] = "1";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMJwtPluginCredentialsProvider::GetAWSCredentialsWithJwt(
    const rs_string& in_jwtAssertion)
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::GetAWSCredentialsWithJwt");

    if (in_jwtAssertion.empty())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to retrieve JWT assertion. Please verify the connection settings.");
    }

    JWTAssertion jwt = DecodeJwtToken(in_jwtAssertion);
    RetrieveDbUserField(jwt);

    return AssumeRoleWithJwtRequest(in_jwtAssertion,
        m_argsMap[IAM_KEY_ROLE_ARN],
        m_argsMap[IAM_KEY_ROLE_SESSION_NAME]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
AWSCredentials IAMJwtPluginCredentialsProvider::AssumeRoleWithJwtRequest(
    const rs_string& in_jwtAssertion,
    const rs_string& in_roleArn,
    const rs_string& in_roleSessionName)
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::AssumeRoleWithJwtRequest");

    ClientConfiguration config;

#ifndef _WIN32
    // Added CA file to the config for verifying STS server certificate
    if (!m_config.GetCaFile().empty())
    {
        config.caFile = m_config.GetCaFile();
    }
    else
    {
        config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile()); // .GetAsPlatformString()
    }

#endif // !_WIN32

    if (m_config.GetUsingHTTPSProxy())
    {
        config.proxyHost = m_config.GetHTTPSProxyHost();
        config.proxyPort = m_config.GetHTTPSProxyPort();
        config.proxyUserName = m_config.GetHTTPSProxyUser();
        config.proxyPassword = m_config.GetHTTPSProxyPassword();
    }

    STSClient client(Aws::MakeShared<AnonymousAWSCredentialsProvider>(LOG_TAG), config);

    Model::AssumeRoleWithWebIdentityRequest request;
    request.SetWebIdentityToken(in_jwtAssertion);
    request.SetRoleArn(in_roleArn);
    request.SetRoleSessionName(in_roleSessionName);

    int durationSecond = 0;
    if (m_argsMap.count(IAM_KEY_DURATION))
    {
        durationSecond = atoi(m_argsMap[IAM_KEY_DURATION].c_str());
    }

    if (durationSecond > 0)
    {
        request.SetDurationSeconds(durationSecond);
    }

    Model::AssumeRoleWithWebIdentityOutcome outcome = client.AssumeRoleWithWebIdentity(request);

    if (!outcome.IsSuccess())
    {
        const AWSError<STSErrors>& error = outcome.GetError();
        const rs_string& exceptionName = error.GetExceptionName();
        const rs_string& errorMessage = error.GetMessage();

        rs_string fullErrorMsg =
            exceptionName +
            ": " +
            errorMessage +
            " (HTTP response code: " +
                std::to_string(static_cast<short>(
                    error.GetResponseCode())) +
            ")";
        IAMUtils::ThrowConnectionExceptionWithInfo(fullErrorMsg);
    }

    const Model::Credentials& credentials = outcome.GetResult().GetCredentials();

    return AWSCredentials(
        credentials.GetAccessKeyId(),
        credentials.GetSecretAccessKey(),
        credentials.GetSessionToken());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMJwtPluginCredentialsProvider::~IAMJwtPluginCredentialsProvider()
{
    RS_LOG(m_log)("IAMJwtPluginCredentialsProvider::~IAMJwtPluginCredentialsProvider");
    /* Do nothing */
}
