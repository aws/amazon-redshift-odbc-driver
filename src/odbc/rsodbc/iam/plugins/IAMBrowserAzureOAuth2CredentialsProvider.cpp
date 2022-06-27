#include "IAMBrowserAzureOAuth2CredentialsProvider.h"
#include "IAMJwtBasicCredentialsProvider.h"
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
IAMBrowserAzureOAuth2CredentialsProvider::IAMBrowserAzureOAuth2CredentialsProvider(
	RsLogger* in_log,
	const IAMConfiguration& in_config,
	const std::map<rs_string, rs_string>& in_argsMap) :
	IAMJwtPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::IAMBrowserAzureOAuth2CredentialsProvider");
	InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::InitArgumentsMap()
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::InitArgumentsMap");

	/* We grab the parameters needed to get the OAUTH Assertion and get the temporary IAM Credentials.
	We are using the base class implementation but we override for logging purposes. */
	IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::ValidateArgumentsMap()
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::ValidateArgumentsMap");

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
rs_string IAMBrowserAzureOAuth2CredentialsProvider::GetJwtAssertion()
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::GetJwtAssertion");

	/* All plugins must have this method implemented. We need to return the OAUTH Response back to base class.
	It is also good to make an entrance log to this method. */
	return BrowserOauthBasedAuthentication();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int IAMBrowserAzureOAuth2CredentialsProvider::GenerateRandomInteger(int low, int high)
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::GenerateLength");

	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<> dist(low, high);

	return dist(generator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureOAuth2CredentialsProvider::GenerateState()
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::GenerateState");

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
void IAMBrowserAzureOAuth2CredentialsProvider::LaunchBrowser(const rs_string& uri)
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::LaunchBrowser");

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
void IAMBrowserAzureOAuth2CredentialsProvider::WaitForServer(WEBServer& srv)
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::WaitForServer");

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
rs_string IAMBrowserAzureOAuth2CredentialsProvider::RequestAuthorizationCode()
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::RequestAuthorizationCode");

	/*  Generate state to include in URI to prevent the cross-site request forgery attacks.
	This state will be verified in the token response.  */
	rs_string state = GenerateState();

	/* Let the server to listen on the random free port on the system. */
	rs_string random_port = "0";

	WEBServer srv(m_log, state, random_port, m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT]);

	/* Launch WEB Server to wait the response with the authorization code from /oauth2/authorize/. */
	srv.LaunchServer();

	try
	{
		WaitForServer(srv);

		/* Save the listen port from the server to know where to redirect the response. */
		int port = srv.GetListenPort();
		m_argsMap[IAM_KEY_LISTEN_PORT] = std::to_string(port);
		rs_string scope = "openid%20" + m_argsMap[IAM_KEY_SCOPE];

		/* Generate URI to request an authorization code.  */
		const rs_string uri = "https://login.microsoftonline.com/" +
			m_argsMap[IAM_KEY_IDP_TENANT] +
			"/oauth2/authorize?client_id=" +
			m_argsMap[IAM_KEY_CLIENT_ID] +
			"&response_type=code&redirect_uri=http%3A%2F%2Flocalhost%3A" +
			m_argsMap[IAM_KEY_LISTEN_PORT] +
			"%2Fredshift%2F" +
			"&response_mode=form_post&scope=" + scope +
			"&state=" +
			state;

		RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::RequestAuthorizationCode",
			"uri=%s\n",uri.c_str());


		// Enforce URL validation
		ValidateURL(uri);

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
rs_string IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken(const rs_string& authCode)
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken");

	/* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
	verifying the server certificate (e.g., self-signed IDP server) */
	bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);
	const rs_string scope = "openid " + m_argsMap[IAM_KEY_SCOPE];


	const std::map<rs_string, rs_string> requestHeader =
	{
		{ "Content-Type", "application/x-www-form-urlencoded; charset=utf-8" },
		{ "Accept", "application/json" }
	};

	const std::map<rs_string, rs_string> paramMap =
	{
		{ "grant_type", "authorization_code" },
//		{ "requested_token_type", "urn:ietf:params:oauth:token-type:jwt" },
		{ "response_type", "token" },
		{ "scope", scope },
		{ "client_id", m_argsMap[IAM_KEY_CLIENT_ID] },
		{ "code", authCode },
		{ "redirect_uri", "http://localhost:" + m_argsMap[IAM_KEY_LISTEN_PORT] + "/redshift/" }
//		{ "resource", m_argsMap[IAM_KEY_CLIENT_ID] }
	};

	HttpClientConfig config;
	config.m_verifySSL = shouldVerifySSL;
	config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken ",
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

	/* Generate URI to redeem the code for an access_token. */
	rs_string reduri = "https://login.microsoftonline.com/" +
		m_argsMap[IAM_KEY_IDP_TENANT] +
		"/oauth2/v2.0/token";

	// Enforce URL regex in LOGIN_URL to avoid possible remote code execution
	ValidateURL(reduri);

	const rs_string requestBody = IAMHttpClient::CreateHttpFormRequestBody(paramMap);

	Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(
		reduri,
		HttpMethod::HTTP_POST,
		requestHeader,
		requestBody);

	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken: response %s\n", response.GetResponseBody().c_str());

	IAMHttpClient::CheckHttpResponseStatus(response,
		"Authentication failed on the Browser server. Please check the IdP Tenant and Client ID.");

	/* Convert response body to JSON and return Access Token if parse was successful. */
	Json::JsonValue res(response.GetResponseBody());

	return res.WasParseSuccessful() ? GetValueByKeyFromJson(res, ACCESS_TOKEN) : "";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureOAuth2CredentialsProvider::BrowserOauthBasedAuthentication()
{
	RS_LOG(m_log)("IAMBrowserAzureOAuth2CredentialsProvider::BrowserOauthBasedAuthentication");

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

	return accessToken;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMBrowserAzureOAuth2CredentialsProvider::~IAMBrowserAzureOAuth2CredentialsProvider()
{
	; // Do nothing.
}
