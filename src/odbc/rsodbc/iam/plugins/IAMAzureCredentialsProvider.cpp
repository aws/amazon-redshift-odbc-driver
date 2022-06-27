#include "IAMAzureCredentialsProvider.h"
#include "IAMUtils.h"
#include "IAMHttpClient.h"

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/base64/Base64.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <sstream>
#include <regex>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace
{
    /* Azure specific request key and value */
    static const rs_string AZURE_ACCESS_TOKEN = "access_token";
    static const rs_string AZURE_ERROR_CODE = "error";
    static const rs_string AZURE_ERROR_DESCRIPTION = "error_description";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMAzureCredentialsProvider::IAMAzureCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("IAMAzureCredentialsProvider::IAMAzureCredentialsProvider");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMAzureCredentialsProvider::InitArgumentsMap()
{
    RS_LOG(m_log)("IAMAzureCredentialsProvider::InitArgumentsMap");
    /* We grab the parameters needed to get the SAML Assertion and get the temporary IAM Credentials.
       We are using the base class implementation but we override for logging purposes. */
    IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMAzureCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG(m_log)("IAMAzureCredentialsProvider::ValidateArgumentsMap");

    /* We validate the parameters passed in and make sure we have the required fields. */
    if (!m_argsMap.count(IAM_KEY_IDP_TENANT))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that IDP_TENANT is provided.");
    }
    else if (!m_argsMap.count(IAM_KEY_USER))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that USER is provided.");
    }
    else if (!m_argsMap.count(IAM_KEY_PASSWORD))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that PASSWORD is provided.");
    }
    else if (!m_argsMap.count(IAM_KEY_CLIENT_SECRET))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that CLIENT_SECRET is provided.");
    }
    else if (!m_argsMap.count(IAM_KEY_CLIENT_ID))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that CLIENT_ID is provided.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMAzureCredentialsProvider::GetSamlAssertion()
{
    RS_LOG(m_log)("IAMAzureCredentialsProvider::GetSamlAssertion");

    /* All plugins must have this method implemented. We need to return the SAML Response back to base class.
       It is also good to make an entrance log to this method. */
    return AzureOauthBasedAuthentication();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMAzureCredentialsProvider::AzureOauthBasedAuthentication()
{
    RS_LOG(m_log)("IAMAzureCredentialsProvider::AzureOauthBasedAuthentication");

    /* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
       verifying the server certificate (e.g., self-signed IDP server) */
    bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);

    RS_LOG(m_log)(
        "IAMAzureCredentialsProvider::AzureOauthBasedAuthentication ",
        + "verifySSL: %s",
        shouldVerifySSL ? "true" : "false");

    HttpClientConfig config;
    config.m_verifySSL = shouldVerifySSL;
    config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG(m_log)("IAMAzureCredentialsProvider::AzureOauthBasedAuthentication ",
		"HttpClientConfig.m_timeout: %ld",
		config.m_timeout);

    if (m_config.GetUsingHTTPSProxy() && m_config.GetUseProxyIdpAuth())
    {
        config.m_httpsProxyHost = m_config.GetHTTPSProxyHost();
        config.m_httpsProxyPort = m_config.GetHTTPSProxyPort();
        config.m_httpsProxyUserName = m_config.GetHTTPSProxyUser();
        config.m_httpsProxyPassword = m_config.GetHTTPSProxyPassword();
    }

    /* We get the shared ptr to the IAM Http Client similar to how all the other plugins do it. */
    std::shared_ptr<IAMHttpClient> client = GetHttpClient(config);

    /* This is the url used to grab the saml2 oauth token. This never changes. We need to pass
       in the idp_tenant either in string readable form or ID form. */
    rs_string uri = "https://login.microsoftonline.com/" +
        m_argsMap[IAM_KEY_IDP_TENANT] +
        "/oauth2/token";

    RS_LOG(m_log)(
       "IAMAzureCredentialsProvider::AzureOauthBasedAuthentication ",
        + "Using URI: %s",
        uri.c_str());

	// Enforce URL validation
	ValidateURL(uri);


    /* Setting the headers. */
    const std::map<rs_string, rs_string> requestHeader =
    {
        { "Content-Type", "application/x-www-form-urlencoded; charset=utf-8" },
        { "Accept", "application/json" }
    };

    /* Setting the query parameters needed to grab the SAML Assertion. */
    const std::map<rs_string, rs_string> paramMap =
    {
        { "grant_type", "password" },
        { "requested_token_type", "urn:ietf:params:oauth:token-type:saml2" },
        { "username", m_argsMap[IAM_KEY_USER] },
        { "password", m_argsMap[IAM_KEY_PASSWORD] },
        { "client_secret", m_argsMap[IAM_KEY_CLIENT_SECRET] },
        { "client_id", m_argsMap[IAM_KEY_CLIENT_ID] },
        { "resource", m_argsMap[IAM_KEY_CLIENT_ID] }
    };

    const rs_string requestBody = IAMHttpClient::CreateHttpFormRequestBody(paramMap);
    Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(
        uri,
        HttpMethod::HTTP_POST,
        requestHeader,
        requestBody);

	RS_LOG(m_log)("IAMAzureCredentialsProvider::AzureOauthBasedAuthentication: response %s\n", response.GetResponseBody().c_str());

    /* Check the response to see if we get a 200 back. If it fails, we throw an error and tell the user to
       double check their parameters. */
    IAMHttpClient::CheckHttpResponseStatus(response,
        "Authentication failed on the Azure server. Please check the IdP Tenant, User, Password, Client Secret and Client ID.");

    /* Convert response body to JSON and return session Token */
    Json::JsonValue res(response.GetResponseBody());
    if (!res.WasParseSuccessful())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to retrieve Azure json response. Please verify the connection settings.");
    }

    rs_string error_message = "Authentication failed on the Azure server. Please check the IdP Tenant, User, Password, Client Secret and Client ID.";
    rs_string error_description = GetValueByKeyFromJson(res, AZURE_ERROR_DESCRIPTION);

    if (!error_description.empty())
    {
        rs_wstring error_description_wstring = IAMUtils::convertFromUTF8(error_description);
        IAMUtils::ReplaceAll(error_description_wstring,L"\r\n", L' ');
        error_description = IAMUtils::convertToUTF8(error_description_wstring);

        rs_string error_code = GetValueByKeyFromJson(res, AZURE_ERROR_CODE);
        if (!error_code.empty())
        {
            error_message = error_code + ": " + error_description;
        }
        else
        {
            error_message = error_description;
        }
    }
    
    /* Check the response to see if we get a 200 back. If it fails, we throw an error and tell the user to
       double check their parameters. */
    IAMHttpClient::CheckHttpResponseStatus(response, error_message);

    /* Grab the SAML Assertion from the "access_token" node in the JSON */
    rs_string access_token = GetValueByKeyFromJson(res, AZURE_ACCESS_TOKEN);

    /* We get the string as a UTF8 string. Azure encodes it differently, replacing + and / with - and _, respectively.
       We also should replace &#x3d; with its symbol, =. */
    rs_wstring in_samlAssertion = IAMUtils::convertFromUTF8(access_token);
    IAMUtils::ReplaceAll(in_samlAssertion,L"-", L'+');
    IAMUtils::ReplaceAll(in_samlAssertion,L"_", L'/');
    IAMUtils::ReplaceAll(in_samlAssertion,L"&#x3d;", L'=');

    Base64::Base64 base64;
    rs_string samlAssertion = IAMUtils::convertToUTF8(in_samlAssertion);

	RS_LOG(m_log)(
		"IAMAzureCredentialsProvider::AzureOauthBasedAuthentication ",
		+"samlAssertion: %s",
		samlAssertion.c_str());

    /* The Base64 Decode method takes in an AWS::String so we convert from rs_string. */
    Aws::String aws_samlAssertion(samlAssertion.c_str(), samlAssertion.size());

    /* Azure sends us back the SAML Assertion not only in invalid UTF8 format but also without the correct base64 =
       padding so we have to pad the SAML Assertion we get with = at the end until we get a multiple of 4. Without
       this, the SAML content gets cut off and we don't send the complete SAML Response causing the code to break. */
    int remainder = aws_samlAssertion.size() % 4;
    if (remainder != 0)
    {
        for (int i = 4 - remainder; i > 0; i--)
        {
            aws_samlAssertion += "=";
        }
    }

    /* Decrypt the base64 SAML Assertion and convert to rs_string. */
    ByteBuffer samlByteBuffer = base64.Decode(aws_samlAssertion);
    const rs_string samlContent(
        reinterpret_cast<const char*>(
        samlByteBuffer.GetUnderlyingData()),
        samlByteBuffer.GetLength());

    /* What we get back from Azure is only the SAML Assertion. THe base class requires we pass in the full SAML
       Response. So we append the extra tags to the SAML Assertion to turn it into a SAML Response.*/
    std::stringstream samlStream;
    samlStream << "<samlp:Response xmlns:samlp=\"urn:oasis:names:tc:SAML:2.0:protocol\">"
               << "<samlp:Status>"
               << "<samlp:StatusCode Value=\"urn:oasis:names:tc:SAML:2.0:status:Success\"/>"
               << "</samlp:Status>"
               << samlContent
               << "</samlp:Response>";

    /* We pass the newly formulated SAML Response back into a ByteBuffer and pass that to Base64 Encode method to
       re-encrypt the SAML Response to be passed to the base class. */
    const rs_string samlResponseContent = samlStream.str();
    ByteBuffer samlResponseByteBuffer(reinterpret_cast<const unsigned char*>(samlResponseContent.c_str()), samlResponseContent.size());
    rs_string samlResponse = base64.Encode(samlResponseByteBuffer);

    return samlResponse;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMAzureCredentialsProvider::~IAMAzureCredentialsProvider()
{
    /* Do nothing */
}
