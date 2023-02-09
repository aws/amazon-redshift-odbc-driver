#include "IAMPingCredentialsProvider.h"
#include "IAMUtils.h"
#include "IAMHttpClient.h"

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
    static const rs_string DEFAULT_PARTNER_SPID = "urn:amazon:webservices";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMPingCredentialsProvider::IAMPingCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("IAMPingCredentialsProvider::IAMPingCredentialsProvider");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMPingCredentialsProvider::InitArgumentsMap()
{
    RS_LOG(m_log)("IAMPingCredentialsProvider::InitArgumentsMap");

    IAMPluginCredentialsProvider::InitArgumentsMap();

    const rs_string partnerSpid = m_config.GetPartnerSpId();
    SetArgumentKeyValuePair(IAM_KEY_PARTNER_SPID, partnerSpid, DEFAULT_PARTNER_SPID, true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMPingCredentialsProvider::GetSamlAssertion()
{
    RS_LOG(m_log)("IAMPingCredentialsProvider::GetSamlAssertion");

    /* By default we enable verifying server certificate, use argument ssl_insecure = true to
    disable verifying the server certificate (e.g., self-signed IDP server). */
    
    bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);

    RS_LOG(m_log)("IAMPingCredentialsProvider::GetSamlAssertion "
         "verifySSL: %s",
        shouldVerifySSL ? "true" : "false");

    HttpClientConfig config;

    config.m_verifySSL = shouldVerifySSL;
    config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG(m_log)("IAMPingCredentialsProvider::GetSamlAssertion",
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
    
    const rs_string uri = "https://" + m_argsMap[IAM_KEY_IDP_HOST] + ":" +
        m_argsMap[IAM_KEY_IDP_PORT] + "/idp/startSSO.ping?PartnerSpId=" +
        m_argsMap[IAM_KEY_PARTNER_SPID];
        

    RS_LOG(m_log)("IAMPingCredentialsProvider::GetSamlAssertion "
         "Using URI: %s",
        uri.c_str());

	// Enforce URL validation
	ValidateURL(uri);

    Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(uri);

	RS_LOG(m_log)("IAMPingCredentialsProvider::GetSamlAssertion: response %s\n", response.GetResponseBody().c_str());

    IAMHttpClient::CheckHttpResponseStatus(
        response,
        "Connection to the Ping server failed. Please check the IdP Host, IdP Port and Partner SPID.");

    const std::vector<rs_string> inputTags = GetInputTagsFromHtml(response.GetResponseBody());
    const std::map<rs_string, rs_string> paramMap = GetNameValuePairFromInputTag(inputTags);

    const std::map<rs_string, rs_string> requestHeader =
    {
        { "Content-Type", "application/x-www-form-urlencoded; charset=utf-8" },
    };

    const rs_string requestBody = IAMHttpClient::CreateHttpFormRequestBody(paramMap);
 // In plugin "Identity Provider: PingFederate", we need to add another
  // parameter(skip.simultaneous.authn.req.check=true) with the URL when driver
  // does a POST request.
  //  This is to avoid PingFederate speed bump template.
  const rs_string uri_post = uri + "&skip.simultaneous.authn.req.check=true";

  response = client->MakeHttpRequest(uri_post, HttpMethod::HTTP_POST,
                                     requestHeader, requestBody);

  RS_LOG(m_log)
  ("IAMPingCredentialsProvider::PostSamlAssertion "
   "Using URI: %s",
   uri_post.c_str());
    IAMHttpClient::CheckHttpResponseStatus(response,
        "Authentication failed on the Ping server. Please check the user and password.");
    return ExtractSamlAssertion(response.GetResponseBody(), IAM_PLUGIN_SAML_PATTERN);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
std::map<rs_string, rs_string> IAMPingCredentialsProvider::GetNameValuePairFromInputTag(
    const std::vector<rs_string>& in_inputTags)
{
    RS_LOG(m_log)("IAMPingCredentialsProvider::GetNameValuePairFromInputTag");

    std::map<rs_string, rs_string> paramMap;
    std::pair<rs_string, rs_string> username;
    bool isUsernameFound = false;
    bool isPasswordFound = false;

    for (const rs_string& input : in_inputTags)
    {
        const rs_string name = GetValueByKeyFromInput(input, "name");
        const rs_string value = GetValueByKeyFromInput(input, "value");
        const rs_string id = GetValueByKeyFromInput(input, "id");
        const rs_string type = GetValueByKeyFromInput(input, "type");
        rs_string password_tag;

        const bool isText = type == "text";
        const bool isPassword = type == "password";
        
        if (!isUsernameFound && (id == "username") && isText)
        {
            paramMap[name] = m_argsMap[IAM_KEY_USER];
            isUsernameFound = true;
        }
        else if ((name.find("pass") != rs_string::npos) && isPassword)
        {
            if (isPasswordFound)
            {
                RS_LOG(m_log)("IAMPingCredentialsProvider::GetNameValuePairFromInputTag "
                     "pass field: %s, has conflict with field: %s",
                    password_tag.c_str(), name.c_str());
                
                IAMUtils::ThrowConnectionExceptionWithInfo(
                    "Duplicate password fields on login page.");
            }
            
            paramMap[name] = m_argsMap[IAM_KEY_PASSWORD];
            isPasswordFound = true;
            
            /* Save password tag name to log it, in case of duplicate
             * password is found. */
            password_tag = name;
            
        }
        else if ((name.find("user") != rs_string::npos ||
                  name.find("email") != rs_string::npos) && isText)
        {
            /* It is possible that id text field with username doesn't exist,
             * so save name field that equal user or email. */
            username = std::make_pair(name, m_argsMap[IAM_KEY_USER]);
        
        }
        else if (!name.empty() && !value.empty())
        {
            paramMap[name] = value;
        }
    }

    if (!isUsernameFound && !username.first.empty())
    {
        paramMap.insert(username);
        isUsernameFound = true;
    }
    
    if (!isUsernameFound || !isPasswordFound)
    {
        RS_LOG(m_log)("IAMPingCredentialsProvider::GetNameValuePairFromInputTag "
             "username found: %s, password found: %s",
            std::to_string(isUsernameFound).c_str(),
            std::to_string(isPasswordFound).c_str());
        IAMUtils::ThrowConnectionExceptionWithInfo("Failed to parse login form.");
    }

    return paramMap;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMPingCredentialsProvider::~IAMPingCredentialsProvider()
{
    RS_LOG(m_log)("IAMPingCredentialsProvider::~IAMPingCredentialsProvider");
    /* Do nothing */
}
