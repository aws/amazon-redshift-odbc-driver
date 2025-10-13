#include "IAMBrowserAzureCredentialsProvider.h"
#include "IAMUtils.h"
#include "IAMHttpClient.h"
#include "WEBServer.h"

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/base64/Base64.h>
#include <aws/core/utils/json/JsonSerializer.h>

#include <random>
#include <sstream>

#if (defined(_WIN32) || defined(_WIN64))
#include <shellapi.h>
#elif (defined(__APPLE__) || defined(__MACH__) || defined(PLATFORM_DARWIN))
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace
{
    // Browser specific request key and value
    static const rs_string ACCESS_TOKEN = "access_token";

    // Wait 10 seconds for the server to start listening
    static const int SERVER_START_TIMEOUT = 10;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMBrowserAzureCredentialsProvider::IAMBrowserAzureCredentialsProvider(
        const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider( in_config, in_argsMap)
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::IAMBrowserAzureCredentialsProvider");
    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureCredentialsProvider::InitArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::InitArgumentsMap");
    
    /* We grab the parameters needed to get the SAML Assertion and get the temporary IAM Credentials.
    We are using the base class implementation but we override for logging purposes. */
    IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::ValidateArgumentsMap");
    
    /* We validate the parameters passed in and make sure we have the required fields. */
    if (!m_argsMap.count(IAM_KEY_IDP_TENANT))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that IDP_TENANT is provided.");
    }
    else if (!m_argsMap.count(IAM_KEY_CLIENT_ID))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that CLIENT_ID is provided.");
    }

    /* Use default timeout parameter if user didn't provide timeout in configuration field. */
    if (!m_argsMap.count(IAM_KEY_IDP_RESPONSE_TIMEOUT))
    {
        m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT] = std::to_string(IAM_DEFAULT_BROWSER_PLUGIN_TIMEOUT);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureCredentialsProvider::GetSamlAssertion()
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::GetSamlAssertion");
    
    /* All plugins must have this method implemented. We need to return the SAML Response back to base class.
    It is also good to make an entrance log to this method. */
    return BrowserOauthBasedAuthentication();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int IAMBrowserAzureCredentialsProvider::GenerateRandomInteger(int low, int high)
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::GenerateLength");

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(low, high);

    return dist(generator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureCredentialsProvider::GenerateState()
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::GenerateState");

    const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    const int chars_size = (sizeof(chars) / sizeof(*chars)) - 1;
    const int rand_size = GenerateRandomInteger(9, chars_size - 1);
    rs_string state;

    state.reserve(rand_size);

    for (int i = 0; i < rand_size; ++i)
    {
        state.push_back(chars[GenerateRandomInteger(0, rand_size)]);
    }

    return state;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureCredentialsProvider::LaunchBrowser(const rs_string& uri)
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::LaunchBrowser");

	//  Avoid system calls where possible for LOGIN_URL to help avoid possible remote code execution
#if (defined(_WIN32) || defined(_WIN64))
	if (static_cast<int>(
		reinterpret_cast<intptr_t>(
			ShellExecute(
				NULL,
				NULL,
				uri.c_str(),
				NULL,
				NULL,
				SW_SHOWNORMAL))) <= 32)
#elif (defined(LINUX) || defined(__linux__))

    rs_string open_uri = command_ + uri + subcommand_;

    if (system(open_uri.c_str()) == -1)
#elif (defined(__APPLE__) || defined(__MACH__) || defined(PLATFORM_DARWIN))
	CFURLRef url = CFURLCreateWithBytes(
		NULL,                        // allocator
		(UInt8*)uri.c_str(),         // URLBytes
		uri.length(),                // length
		kCFStringEncodingASCII,      // encoding
		NULL                         // baseURL
	);
	if (url)
	{
		LSOpenCFURLRef(url, 0);
		CFRelease(url);
	}
	else
#endif
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Couldn't open a URI or some error occurred.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureCredentialsProvider::WaitForServer(WEBServer& srv)
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::WaitForServer");

    auto start = std::chrono::system_clock::now();

    while ((std::chrono::system_clock::now() - start < std::chrono::seconds(SERVER_START_TIMEOUT)) && !srv.IsListening())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!srv.IsListening())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Server couldn't start listening.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureCredentialsProvider::RequestAuthorizationCode()
{
   RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::RequestAuthorizationCode");
    
    /*  Generate state to include in URI to prevent the cross-site request forgery attacks.
    This state will be verified in the token response.  */
    rs_string state = GenerateState();

    /* Let the server to listen on the random free port on the system. */
    rs_string random_port = "0";

    WEBServer srv(state, random_port, m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT]);
    
    /* Launch WEB Server to wait the response with the authorization code from /oauth2/authorize/. */
    srv.LaunchServer();
    
	try
	{
		WaitForServer(srv);

		/* Save the listen port from the server to know where to redirect the response. */
		int port = srv.GetListenPort();
		m_argsMap[IAM_KEY_LISTEN_PORT] = std::to_string(port);

		/* Generate URI to request an authorization code.  */
		const rs_string uri = "https://login.microsoftonline.com/" +
			m_argsMap[IAM_KEY_IDP_TENANT] +
			"/oauth2/authorize?client_id=" +
			m_argsMap[IAM_KEY_CLIENT_ID] +
			"&response_type=code&redirect_uri=http%3A%2F%2Flocalhost%3A" +
			m_argsMap[IAM_KEY_LISTEN_PORT] +
			"%2Fredshift%2F" +
			"&response_mode=form_post&scope=openid" +
			"&state=" +
			state;

		// Enforce URL validation
		IAMUtils::ValidateURL(uri);

		LaunchBrowser(uri);
	}
	catch (RsErrorException & e)
	{
		srv.Join();
		throw e;
	}

    
    srv.Join();
    
    if (srv.IsTimeout())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Connection timeout. Please verify the connection settings.");
    }
    
    return srv.GetCode();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureCredentialsProvider::RequestAccessToken(const rs_string& authCode)
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::RequestAccessToken");
    
    /* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
    verifying the server certificate (e.g., self-signed IDP server) */
    bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);
    
    const std::map<rs_string, rs_string> requestHeader =
    {
        { "Content-Type", "application/x-www-form-urlencoded; charset=utf-8" },
        { "Accept", "application/json" }
    };
    
    const std::map<rs_string, rs_string> paramMap =
    {
        { "grant_type", "authorization_code" },
        { "requested_token_type", "urn:ietf:params:oauth:token-type:saml2" },
        { "scope", "openid" },
        { "client_id", m_argsMap[IAM_KEY_CLIENT_ID] },
        { "code", authCode },
        { "redirect_uri", "http://localhost:" + m_argsMap[IAM_KEY_LISTEN_PORT] + "/redshift/" },
        { "resource", m_argsMap[IAM_KEY_CLIENT_ID] }
    };
    
    HttpClientConfig config;
    config.m_verifySSL = shouldVerifySSL;
    config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG_DEBUG("IAMCRD",
		"Setting HttpClientConfig.m_timeout: %ld", config.m_timeout);

    if (m_config.GetUsingHTTPSProxy() && m_config.GetUseProxyIdpAuth())
    {
        config.m_httpsProxyHost = m_config.GetHTTPSProxyHost();
        config.m_httpsProxyPort = m_config.GetHTTPSProxyPort();
        config.m_httpsProxyUserName = m_config.GetHTTPSProxyUser();
        config.m_httpsProxyPassword = m_config.GetHTTPSProxyPassword();
    }
    
    std::shared_ptr<IAMHttpClient> client = GetHttpClient(config);
    
    /* Generate URI to redeem the code for an access_token. */
    rs_string reduri = "https://login.microsoftonline.com/" +
        m_argsMap[IAM_KEY_IDP_TENANT] +
        "/oauth2/token";
    
	// Enforce URL regex in LOGIN_URL to avoid possible remote code execution
	IAMUtils::ValidateURL(reduri);

    const rs_string requestBody = IAMHttpClient::CreateHttpFormRequestBody(paramMap);
    
    Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(
        reduri,
        HttpMethod::HTTP_POST,
        requestHeader,
        requestBody);
    
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::RequestAccessToken: response %s\n", response.GetResponseBody().c_str());

    IAMHttpClient::CheckHttpResponseStatus(response,
        "Authentication failed on the Browser server. Please check the IdP Tenant and Client ID.");
    
    /* Convert response body to JSON and return Access Token if parse was successful. */
    Json::JsonValue res(response.GetResponseBody());
    
    return res.WasParseSuccessful() ? GetValueByKeyFromJson(res, ACCESS_TOKEN) : "";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureCredentialsProvider::RetrieveSamlFromAccessToken(const rs_string& accessToken)
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::RetrieveSamlFromAccessToken");
    
    /* accessToken formated as UTF8 string. As browser encodes it differently, we should replace:
    and / with - and _, respectively;
    &#x3d, with its symbol, =. */
    rs_wstring in_samlAssertion = IAMUtils::convertFromUTF8(accessToken);
    IAMUtils::ReplaceAll(in_samlAssertion,L"-", L'+');
    IAMUtils::ReplaceAll(in_samlAssertion,L"_", L'/');
    IAMUtils::ReplaceAll(in_samlAssertion,L"&#x3d;", L'=');
    
    Base64::Base64 base64;
    rs_string samlAssertion = IAMUtils::convertToUTF8(in_samlAssertion);
    
    /* The Base64 Decode method takes in an AWS::String so we convert from rs_string. */
    Aws::String aws_samlAssertion(samlAssertion.c_str(), samlAssertion.size());
    
    /* Browser sends us back the SAML Assertion not only in invalid UTF8 format but also without the correct base64 =
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
    
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::RetrieveSamlFromAccessToken: samlContent %s\n", samlContent.c_str());

    /* What we get back from Browser is only the SAML Assertion. THe base class requires we pass in the full SAML
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
    ByteBuffer samlResponseByteBuffer(reinterpret_cast<const unsigned char*>(samlResponseContent.c_str()),
    samlResponseContent.size());
    
    return base64.Encode(samlResponseByteBuffer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureCredentialsProvider::BrowserOauthBasedAuthentication()
{
    RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureCredentialsProvider::BrowserOauthBasedAuthentication");

    const rs_string authCode = RequestAuthorizationCode();

    if (authCode.empty())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to retrieve authorization code token. Please verify the connection settings.");
    }

    const rs_string accessToken = RequestAccessToken(authCode);

    if (accessToken.empty())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "Failed to retrieve access token. Please verify the connection settings.");
    }

    return RetrieveSamlFromAccessToken(accessToken);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMBrowserAzureCredentialsProvider::~IAMBrowserAzureCredentialsProvider()
{
    ; // Do nothing.
}
