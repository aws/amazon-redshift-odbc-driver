#include "IAMAdfsCredentialsProvider.h"
#include "IAMUtils.h"
#include "IAMHttpClient.h"
#include "IAMWinHttpClientDelegate.h"

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace
{
    static const rs_string DEFAULT_LOGINTORP = "urn:amazon:webservices";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMAdfsCredentialsProvider::IAMAdfsCredentialsProvider(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider( in_config, in_argsMap)
{
    RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::IAMAdfsCredentialsProvider");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMAdfsCredentialsProvider::InitArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::InitArgumentsMap");
    IAMPluginCredentialsProvider::InitArgumentsMap();

    const rs_string loginToRp = m_config.GetLoginToRp();
    SetArgumentKeyValuePair(IAM_KEY_LOGINTORP, loginToRp, DEFAULT_LOGINTORP, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMAdfsCredentialsProvider::GetSamlAssertion()
{
    RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::GetSamlAssertion");

    if (m_argsMap[IAM_KEY_USER].empty() || m_argsMap[IAM_KEY_PASSWORD].empty())
    {
        return WindowsIntegratedAuthentication();
    }
    return FormBasedAuthentication();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMAdfsCredentialsProvider::WindowsIntegratedAuthentication()
{
  RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::WindowsIntegratedAuthentication");

#ifndef _WIN32
    /* ADFS Integrated Authentication is supported on Windows, throw an exception on other platforms */
    IAMUtils::ThrowConnectionExceptionWithInfo(
        "ADFS Integrated Authentication is only supported on Windows. Please use Form Based ADFS Authentication.");

    return rs_string();

#else
    /* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
    verifying the server certificate (e.g., self-signed IDP server) */
    bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);

    RS_LOG_DEBUG("IAMCRD", 
        "IAMAdfsCredentialsProvider::WindowsIntegratedAuthentication ",
        + "verifySSL: %s",
        shouldVerifySSL ? "true" : "false");

    const bool useProxyForIdP = m_config.GetUsingHTTPSProxy() && m_config.GetUseProxyIdpAuth();

    Redshift::IamSupport::HttpResponse response =
        IAMWinHttpClientDelegate::MakeHttpRequestWithWIA(
            rs_wstring(IAMUtils::convertFromUTF8(m_argsMap[IAM_KEY_IDP_HOST])),
            to_short(m_argsMap[IAM_KEY_IDP_PORT]), // NumberConverter::ConvertStringToUInt16
            shouldVerifySSL,
            useProxyForIdP ? rs_wstring(IAMUtils::convertFromUTF8(m_config.GetHTTPSProxyUser())) : rs_wstring(),
            useProxyForIdP ? rs_wstring(IAMUtils::convertFromUTF8(m_config.GetHTTPSProxyPassword())) : rs_wstring(),
            IAMUtils::convertFromUTF8(m_argsMap[IAM_KEY_LOGINTORP]),
			to_int(m_argsMap[IAM_KEY_STS_CONNECTION_TIMEOUT]));

    return ExtractSamlAssertion(response.GetResponseBody(), IAM_PLUGIN_SAML_PATTERN);

#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMAdfsCredentialsProvider::FormBasedAuthentication()
{
    RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::FormBasedAuhentication");

    /* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
       verifying the server certificate (e.g., self-signed IDP server) */
    bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);

    RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::FormBasedAuthentication "
         "verifySSL: %s",
        (shouldVerifySSL ? "true" : "false"));

    HttpClientConfig config;
    config.m_verifySSL = shouldVerifySSL;
    config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::FormBasedAuthentication ",
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

    rs_string uri = "https://" + m_argsMap[IAM_KEY_IDP_HOST] + ":" + m_argsMap[IAM_KEY_IDP_PORT] +
        "/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=" + m_argsMap[IAM_KEY_LOGINTORP];
        
	// Enforce URL validation
	IAMUtils::ValidateURL(uri);

    /* Test the availability of the server by sending a simple GET request */
    Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(uri);
	
	RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::FormBasedAuthentication: response %s\n", response.GetResponseBody().c_str());
	
	IAMHttpClient::CheckHttpResponseStatus(response,
        "Connection to the AD FS server failed. Please check the IdP Host and IdP Port.");

    const rs_string htmlBody = response.GetResponseBody();


    const std::map<rs_string, rs_string> requestHeader =
    {
        { "Content-Type", "application/x-www-form-urlencoded; charset=utf-8" },
    };

    const std::vector<rs_string> inputTags = GetInputTagsFromHtml(htmlBody);
    const std::map<rs_string, rs_string> paramMap = GetNameValuePairFromInputTag(inputTags);

    /* This is the alternative URI used for making Http request, if available */
    const rs_string action = GetFormActionFromHtml(htmlBody);

    if (!action.empty() && '/' == action[0])
    {
        uri = "https://" + m_argsMap[IAM_KEY_IDP_HOST] + ":" + m_argsMap[IAM_KEY_IDP_PORT] + action;

		// Enforce URL validation
		IAMUtils::ValidateURL(uri);
    }

    const rs_string requestBody = IAMHttpClient::CreateHttpFormRequestBody(paramMap);
    response = client->MakeHttpRequest(uri, HttpMethod::HTTP_POST, requestHeader, requestBody);
	RS_LOG_DEBUG("IAMCRD", "IAMAdfsCredentialsProvider::FormBasedAuthentication: response2 %s\n", response.GetResponseBody().c_str());

    IAMHttpClient::CheckHttpResponseStatus(response,
        "Authentication failed on the AD FS server. Please check the user, password, or Login To RP.");
    return ExtractSamlAssertion(response.GetResponseBody(), IAM_PLUGIN_SAML_PATTERN);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMAdfsCredentialsProvider::~IAMAdfsCredentialsProvider()
{
    /* Do nothing */
}
