#include "IAMBrowserAzureOAuth2CredentialsProvider.h"
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
#include <aws/sts/STSClient.h>
#include <aws/sts/model/AssumeRoleWithWebIdentityRequest.h>

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
		const IAMConfiguration& in_config,
	const std::map<rs_string, rs_string>& in_argsMap) :
	IAMPluginCredentialsProvider( in_config, in_argsMap)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::IAMBrowserAzureOAuth2CredentialsProvider");
	InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::InitArgumentsMap()
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::InitArgumentsMap");

	/* We grab the parameters needed to get the OAUTH Assertion and get the temporary IAM Credentials.
	We are using the base class implementation but we override for logging purposes. */
	IAMPluginCredentialsProvider::InitArgumentsMap();
	
	// Add IDP partition from configuration to argsMap
	rs_string idp_partition = m_config.GetSetting(IAM_KEY_IDP_PARTITION);
	if (!idp_partition.empty()) {
		m_argsMap[IAM_KEY_IDP_PARTITION] = idp_partition;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::ValidateArgumentsMap()
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::ValidateArgumentsMap");

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
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::GetJwtAssertion");

	/* All plugins must have this method implemented. We need to return the OAUTH Response back to base class.
	It is also good to make an entrance log to this method. */
	return BrowserOauthBasedAuthentication();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int IAMBrowserAzureOAuth2CredentialsProvider::GenerateRandomInteger(int low, int high)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::GenerateLength");

	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<> dist(low, high);

	return dist(generator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureOAuth2CredentialsProvider::GenerateState()
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::GenerateState");

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
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::LaunchBrowser");

	//  Avoid system calls where possible for LOGIN_URL to help avoid possible remote code execution
// LINUX is used in Mac build too, so order of LINUX and APPLE are important
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
#elif (defined(LINUX) || defined(__linux__))

	rs_string open_uri = command_ + uri + subcommand_;

	if (system(open_uri.c_str()) == -1)
#endif
	{
		IAMUtils::ThrowConnectionExceptionWithInfo("Couldn't open a URI or some error occurred.");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::WaitForServer(WEBServer& srv)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::WaitForServer");

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
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::RequestAuthorizationCode");

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

		// Build scope with URL encoding - add 'openid' if not present
		rs_string scopeParam = m_argsMap[IAM_KEY_SCOPE];
		rs_string scope;
		if (scopeParam.find("openid") == rs_string::npos) {
			scope = "openid%20" + scopeParam;
			RS_LOG_DEBUG("IAMCRD", "RequestAuthorizationCode: Added 'openid' to scope");
		} else {
			scope = scopeParam;
			RS_LOG_DEBUG("IAMCRD", "RequestAuthorizationCode: Scope already contains 'openid'");
		}

		/* Generate URI to request an authorization code.  */
		rs_string idpHostUrl;
		IAMUtils::GetMicrosoftIdpHost(m_argsMap.count(IAM_KEY_IDP_PARTITION) ? m_argsMap.at(IAM_KEY_IDP_PARTITION) : "", idpHostUrl);
		const rs_string uri = idpHostUrl + "/" +
			m_argsMap[IAM_KEY_IDP_TENANT] +
			"/oauth2/v2.0/authorize?client_id=" +
			m_argsMap[IAM_KEY_CLIENT_ID] +
			"&response_type=code&redirect_uri=http%3A%2F%2Flocalhost%3A" +
			m_argsMap[IAM_KEY_LISTEN_PORT] +
			"%2Fredshift%2F" +
			"&response_mode=form_post&scope=" + scope +
			"&state=" + state;



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
rs_string IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken(const rs_string& authCode)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken");

	/* By default we enable verifying server certificate, use argument ssl_insecure = true to disable
	verifying the server certificate (e.g., self-signed IDP server) */
	bool shouldVerifySSL = !IAMUtils::ConvertStringToBool(m_argsMap[IAM_KEY_SSL_INSECURE]);

	// Get scope from connection parameters
	rs_string scopeParam = m_argsMap[IAM_KEY_SCOPE];

	// Add 'openid' to scope if not already present (matching JDBC driver behavior)
	rs_string scope;
	if (scopeParam.find("openid") == rs_string::npos) {
		scope = "openid " + scopeParam;
		RS_LOG_DEBUG("IAMCRD", "Added 'openid' prefix to scope. Final scope: %s", scope.c_str());
	} else {
		scope = scopeParam;
		RS_LOG_DEBUG("IAMCRD", "Scope already contains 'openid': %s", scope.c_str());
	}


	const std::map<rs_string, rs_string> requestHeader =
	{
		{ "Content-Type", "application/x-www-form-urlencoded; charset=utf-8" },
		{ "Accept", "application/json" }
	};

	std::map<rs_string, rs_string> paramMap =
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

	// Add client_secret if provided (fixes GitHub Issue #16)
	if (m_argsMap.find(IAM_KEY_CLIENT_SECRET) != m_argsMap.end() &&
		!m_argsMap[IAM_KEY_CLIENT_SECRET].empty()) {
		paramMap["client_secret"] = m_argsMap[IAM_KEY_CLIENT_SECRET];
		RS_LOG_DEBUG("IAMCRD", "client_secret parameter added to token request");
	} else {
		RS_LOG_DEBUG("IAMCRD", "client_secret NOT provided - using public client flow");
	}

	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: Sending token request (JDBC-compatible, no PKCE)");
	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: scope='%s', grant_type='authorization_code', redirect_uri='%s'",
		scope.c_str(),
		("http://localhost:" + m_argsMap[IAM_KEY_LISTEN_PORT] + "/redshift/").c_str());

	HttpClientConfig config;
	config.m_verifySSL = shouldVerifySSL;
	config.m_caFile = m_config.GetCaFile();
	config.m_timeout = m_config.GetStsConnectionTimeout();

	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken ",
		"HttpClientConfig.m_timeout: %ld",
		config.m_timeout);

	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: Proxy check - UsingHTTPSProxy=%d, UseProxyIdpAuth=%d",
		m_config.GetUsingHTTPSProxy() ? 1 : 0,
		m_config.GetUseProxyIdpAuth() ? 1 : 0);

	if (m_config.GetUsingHTTPSProxy() && m_config.GetUseProxyIdpAuth())
	{
		config.m_httpsProxyHost = m_config.GetHTTPSProxyHost();
		config.m_httpsProxyPort = m_config.GetHTTPSProxyPort();
		config.m_httpsProxyUserName = m_config.GetHTTPSProxyUser();
		config.m_httpsProxyPassword = m_config.GetHTTPSProxyPassword();
		RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: Using HTTPS proxy - Host=%s, Port=%d",
			config.m_httpsProxyHost.c_str(),
			config.m_httpsProxyPort);
	} else {
		RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: NOT using proxy for IDP auth");
	}

	std::shared_ptr<IAMHttpClient> client = GetHttpClient(config);

	/* Generate URI to redeem the code for an access_token. */
	rs_string idpHostUrl;
	IAMUtils::GetMicrosoftIdpHost(m_argsMap.count(IAM_KEY_IDP_PARTITION) ? m_argsMap.at(IAM_KEY_IDP_PARTITION) : "", idpHostUrl);
	rs_string reduri = idpHostUrl + "/" +
		m_argsMap[IAM_KEY_IDP_TENANT] +
		"/oauth2/v2.0/token";

	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: Token endpoint URL: %s", reduri.c_str());

	// Enforce URL regex in LOGIN_URL to avoid possible remote code execution
	IAMUtils::ValidateURL(reduri);

	const rs_string requestBody = IAMHttpClient::CreateHttpFormRequestBody(paramMap);
	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: Request body length: %zu bytes", requestBody.length());

	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: Making HTTP POST request...");
	Redshift::IamSupport::HttpResponse response = client->MakeHttpRequest(
		reduri,
		HttpMethod::HTTP_POST,
		requestHeader,
		requestBody);

	RS_LOG_DEBUG("IAMCRD", "RequestAccessToken: HTTP request completed. Status code: %d", response.GetStatusCode());

	IAMHttpClient::CheckHttpResponseStatus(response,
		"Authentication failed on the Browser server. Please check the IdP Tenant and Client ID.");

	std::string maskedResponse = IAMUtils::maskCredentials(response.GetResponseBody());
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::RequestAccessToken: response %s\n", maskedResponse.c_str());

	/* Convert response body to JSON and return Access Token if parse was successful. */
	Json::JsonValue res(response.GetResponseBody());

	return res.WasParseSuccessful() ? GetValueByKeyFromJson(res, ACCESS_TOKEN) : "";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserAzureOAuth2CredentialsProvider::BrowserOauthBasedAuthentication()
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::BrowserOauthBasedAuthentication");

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
AWSCredentials IAMBrowserAzureOAuth2CredentialsProvider::GetAWSCredentials()
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider", "GetAWSCredentials");
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
rs_string IAMBrowserAzureOAuth2CredentialsProvider::DecodeBase64String(const rs_string& str)
{
	RS_LOG_DEBUG("IAMBrowserAzureOAuth2CredentialsProvider", "DecodeBase64String");
	Base64::Base64 base64;
	ByteBuffer buf = base64.Decode(str);
	return rs_string(
		reinterpret_cast<const char*>(buf.GetUnderlyingData()),
		buf.GetLength());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::AlignPayloadToken(rs_string& str)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::AlignPayloadToken");

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
JWTAssertion IAMBrowserAzureOAuth2CredentialsProvider::DecodeJwtToken(const rs_string& jwt)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::DecodeJwtToken");

	std::vector<rs_string> tokens;
	std::stringstream ss(jwt);
	rs_string token;

	while (std::getline(ss, token, '.'))
	{
		RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::DecodeJwtToken ",
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

	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::DecodeJwtToken ",
		  + "Header: %s, Payload: %s, Signature: %s",
		jwtAssertion.header.c_str(),
		jwtAssertion.payload.c_str(),
		jwtAssertion.signature.c_str());

	return jwtAssertion;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserAzureOAuth2CredentialsProvider::RetrieveDbUserField(const JWTAssertion& jwt)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::ParseJWTAssertion");

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

		RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::RetrieveDbUserField ",
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
AWSCredentials IAMBrowserAzureOAuth2CredentialsProvider::GetAWSCredentialsWithJwt(
	const rs_string& in_jwtAssertion)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::GetAWSCredentialsWithJwt");

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
AWSCredentials IAMBrowserAzureOAuth2CredentialsProvider::AssumeRoleWithJwtRequest(
	const rs_string& in_jwtAssertion,
	const rs_string& in_roleArn,
	const rs_string& in_roleSessionName)
{
	RS_LOG_DEBUG("IAMCRD", "IAMBrowserAzureOAuth2CredentialsProvider::AssumeRoleWithJwtRequest");

	ClientConfiguration config;

#ifndef _WIN32
	// Added CA file to the config for verifying STS server certificate
	if (!m_config.GetCaFile().empty())
	{
		config.caFile = m_config.GetCaFile();
	}
	else
	{
		config.caFile = IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile());
	}
#endif // !_WIN32

	if (m_config.GetUsingHTTPSProxy())
	{
		config.proxyHost = m_config.GetHTTPSProxyHost();
		config.proxyPort = m_config.GetHTTPSProxyPort();
		config.proxyUserName = m_config.GetHTTPSProxyUser();
		config.proxyPassword = m_config.GetHTTPSProxyPassword();
	}

	static const char* LOG_TAG = "IAMBrowserAzureOAuth2CredentialsProvider";
	Aws::STS::STSClient client(Aws::MakeShared<AnonymousAWSCredentialsProvider>(LOG_TAG), config);

	Aws::STS::Model::AssumeRoleWithWebIdentityRequest request;
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

	Aws::STS::Model::AssumeRoleWithWebIdentityOutcome outcome = client.AssumeRoleWithWebIdentity(request);

	if (!outcome.IsSuccess())
	{
		const Aws::Client::AWSError<Aws::STS::STSErrors>& error = outcome.GetError();
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

	const Aws::STS::Model::Credentials& credentials = outcome.GetResult().GetCredentials();

	return AWSCredentials(
		credentials.GetAccessKeyId(),
		credentials.GetSecretAccessKey(),
		credentials.GetSessionToken());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMBrowserAzureOAuth2CredentialsProvider::~IAMBrowserAzureOAuth2CredentialsProvider()
{
	; // Do nothing.
}
