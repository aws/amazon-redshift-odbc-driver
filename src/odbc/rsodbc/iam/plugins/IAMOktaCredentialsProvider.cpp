#include "IAMOktaCredentialsProvider.h"
#include "IAMUtils.h"

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/json/JsonSerializer.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace 
{
    /* Okta specific request key and value */
    static const rs_string OKTA_SESSION_TOKEN_STATUS = "status";
    static const rs_wstring OKTA_SESSION_TOKEN_STATUS_SUCCESS = L"SUCCESS";
    static const rs_string OKTA_SESSION_TOKEN = "sessionToken";
    static const rs_string OKTA_DEFAULT_APP_NAME = "amazon_aws";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMOktaCredentialsProvider::IAMOktaCredentialsProvider(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider( in_config, in_argsMap)
{
    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::IAMOktaCredentialsProvider");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMOktaCredentialsProvider::InitArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvide::InitArgumentsMap");
    IAMPluginCredentialsProvider::InitArgumentsMap();

    const rs_string appId = m_config.GetAppId();
    const rs_string appName = m_config.GetAppName();

    if (!appId.empty())
    {
        m_argsMap[IAM_KEY_APP_ID] = appId;
    }

    SetArgumentKeyValuePair(IAM_KEY_APP_NAME, appName, OKTA_DEFAULT_APP_NAME, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMOktaCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::ValidateArgumentsMap");

    IAMSamlPluginCredentialsProvider::ValidateArgumentsMap();

    if (!m_argsMap.count(IAM_KEY_APP_ID))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Missing required connection attribute: app_id");
    }

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMOktaCredentialsProvider::GetSamlAssertion()
{
    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider", "GetSamlAssertion");
    /* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
    verifying the server certificate (e.g., self-signed IDP server) */
    bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);

    RS_LOG_DEBUG("IAMCRD",
                 "IAMOktaCredentialsProvider::GetSamlAssertion verifySSL: %s",
                 shouldVerifySSL ? "true" : "false");

    HttpClientConfig config;
    
    config.m_verifySSL = shouldVerifySSL;
    config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG_DEBUG("IAMCRD",
                     "IAMOktaCredentialsProvider::GetSamlAssertion "
                     "HttpClientConfig.m_timeout: %ld",
                     config.m_timeout);

    if (m_config.GetUsingHTTPSProxy() && m_config.GetUseProxyIdpAuth())
    {
        config.m_httpsProxyHost = m_config.GetHTTPSProxyHost();
        config.m_httpsProxyPort = m_config.GetHTTPSProxyPort();
        config.m_httpsProxyUserName = m_config.GetHTTPSProxyUser();
        config.m_httpsProxyPassword = m_config.GetHTTPSProxyPassword();
    }

    std::shared_ptr<IAMHttpClient> client = GetHttpClient(config);
    const rs_string sessionToken = GetAuthSessionToken(client);

    const rs_string uri =
        "https://" + 
        m_argsMap[IAM_KEY_IDP_HOST] + 
        "/home/" + 
        m_argsMap[IAM_KEY_APP_NAME] + 
        "/" + 
        m_argsMap[IAM_KEY_APP_ID] + 
        "?onetimetoken=" + 
        sessionToken;

    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::GetSamlAssertion ",
        + "Using URI: %s",
        uri.c_str());

	// Enforce URL validation
	ValidateURL(uri);

    Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(uri);
	RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::GetSamlAssertion: response %s\n", response.GetResponseBody().c_str());

    IAMHttpClient::CheckHttpResponseStatus(response,
        "Failed to retrieve SAML assertion from the Okta server. Please check App ID and App Name.");

    /* Extract SAML assertion from the response body, and replace the url-encoded string
    with its original character, e.g, + and = */
    rs_wstring samlAssertion = IAMUtils::convertFromUTF8(
        ExtractSamlAssertion(response.GetResponseBody(), IAM_PLUGIN_SAML_PATTERN));
    IAMUtils::ReplaceAll(samlAssertion,L"&#x2b;", L'+');
    IAMUtils::ReplaceAll(samlAssertion,L"&#x3d;", L'=');

    return IAMUtils::convertToUTF8(samlAssertion);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMOktaCredentialsProvider::GetAuthSessionToken(
    const std::shared_ptr<IAMHttpClient>& in_httpClient)
{
    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider", "GetAuthSessionToken");

    if (!in_httpClient)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Invalid HttpClient Exception.");
    }

    const rs_string uri = "https://" + m_argsMap[IAM_KEY_IDP_HOST] + "/api/v1/authn";

    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::GetAuthSessionToken "
         "Using URI: %s",
        uri.c_str());

	// Enforce URL validation
	ValidateURL(uri);

    const std::map<rs_string, rs_string> requestHeader =
    {
        { "Accept", "application/json" },
        { "Content-Type", "application/json; charset=utf-8" },
        { "Cache-Control", "no-cache" }
    };
    
    const std::map<rs_string, rs_string> requestParamMap =
    {
        { "username", m_argsMap[IAM_KEY_USER] },
        { "password", m_argsMap[IAM_KEY_PASSWORD] }
    };

    const rs_string requestBody = IAMHttpClient::CreateHttpJsonRequestBody(requestParamMap);
    
    Redshift::IamSupport::HttpResponse response =
        in_httpClient->MakeHttpRequest(uri, HttpMethod::HTTP_POST, requestHeader, requestBody);

    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::GetAuthSessionToken "
         "Response Code: %d, Response Header: %s, Response Body: %s",
        response.GetStatusCode(),
        response.GetResponseHeader().c_str(),
        response.GetResponseBody().c_str());

    IAMHttpClient::CheckHttpResponseStatus(response,
        "Connection or authentication failed to the Okta server. Please check the IdP Host, user and password.");

    /* Convert response body to JSON and return session Token */
    Json::JsonValue res(response.GetResponseBody());
    if (!res.WasParseSuccessful())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to retrieve Okta session token. Please verify the connection settings.");
    }

    rs_string status = GetValueByKeyFromJson(res, OKTA_SESSION_TOKEN_STATUS);
    if (IAMUtils::isEqual(IAMUtils::convertStringToWstring(status),OKTA_SESSION_TOKEN_STATUS_SUCCESS, false))
    {
        return GetValueByKeyFromJson(res, OKTA_SESSION_TOKEN);
    }

    return rs_string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMOktaCredentialsProvider::~IAMOktaCredentialsProvider()
{
    RS_LOG_DEBUG("IAMCRD", "IAMOktaCredentialsProvider::~IAMOktaCredentialsProvider");
    /* Do nothing */
}
