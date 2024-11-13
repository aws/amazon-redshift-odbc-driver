#include "BrowserIdcAuthPlugin.h"
	 
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <rslog.h>
#include <random>
#include <string>
#include <algorithm>
#include <openssl/sha.h>
#include <cstring>
#include "IAMUtils.h"
#include "WEBServer.h"
#include <aws/core/client/ClientConfiguration.h>

#if (defined(_WIN32) || defined(_WIN64))
#include <windows.h>
#include <shellapi.h>
#ifdef GetMessage
#undef GetMessage
#endif
#elif (defined(__APPLE__) || defined(__MACH__) || defined(PLATFORM_DARWIN))
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace Redshift::IamSupport;
using namespace Aws::SSOOIDC;
using namespace Aws::SSOOIDC::Model;
using namespace std::chrono;

namespace {
    /**
     * Client name variable.
     */
    static const std::string CLIENT_NAME = "Amazon Redshift ODBC driver";
    /**
     * Client type of client application
     */
    static const std::string CLIENT_TYPE = "public";
    /**
     * Application grant types variable.
     */
    static const std::string AUTH_CODE_GRANT_TYPE = "authorization_code";
    /**
     * Application scope variable.
     */
    static const std::string REDSHIFT_IDC_CONNECT_SCOPE = "redshift:connect";
    /**
     * Application scope variable.
     */
    static const std::string REDSHIFT_IDC_CONNECT_SCOPE_URL = "redshift%3Aconnect";
    /**
     * Key for authorization code.
     */
    static const std::string AUTH_CODE_PARAMETER_NAME = "code";
    /**
     * String containing HTTPS.
     */
    static const std::string CURRENT_INTERACTION_SCHEMA = "https";
    /**
     * String containing OIDC used for building the authorization server endpoint.
     */
    static const std::string OIDC_SCHEMA = "oidc";
    /**
     * String containing amazonaws.com used for building the authorization server endpoint.
     */
    static const std::string AMAZON_COM_SCHEMA = "amazonaws.com";
    /**
     * Redirect URI of client application
     */
    static const std::string REDIRECT_URI = "http://127.0.0.1";
    /**
     * Redirect URI of client application used for URL building
     */
    static const std::string REDIRECT_URI_URL = "http%3A%2F%2F127.0.0.1%3A";
    /**
     * Authorize endpoint to get authorization code
     */
    static const std::string AUTHORIZE_ENDPOINT = "/authorize";
    /**
     * Method used to hash the code verifier
     */
    static const std::string CHALLENGE_METHOD = "S256";
    /**
     * SHA256 hash used to hash the code verifier
     */
    static const std::string SHA256_METHOD = "SHA-256";
    /**
     * Predefined length of code verifier
     */
    static const short CODE_VERIFIER_LENGTH = 60;
    /**
     * Predefined length of code challenge
     */
    static const short CODE_CHALLENGE_LENGTH = 43;
    /**
     * Valid base64 encoding characters
     */
    static const std::string BASE64_CHAR_LIST = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    /**
     * Valid characters used to generate the state to prevent the cross-site request forgery attacks
     */
    static const char STATE_CHAR_LIST[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    /**
     * The default time in seconds for which the client must wait between attempts
     * when polling for a session. 
     */
    static const int REQUEST_CREATE_TOKEN_INTERVAL = 1;
    /**
     * The default value of browser authentication timeout in seconds
     */
    static const int DEFAULT_BROWSER_AUTH_VERIFY_TIMEOUT = 120;
    /**
     * Cache used to hold the result of the RegisterClient API call
     */
    static std::unordered_map<std::string, RegisterClientResult> m_registerClientCache;
    /**
     * Key used for the register client result cache
     */
    std::string m_registerClientCacheKey;

    std::string m_idcClientDisplayName;
}

BrowserIdcAuthPlugin::BrowserIdcAuthPlugin(
    const IAMConfiguration &in_config,
    const std::map<rs_string, rs_string> &in_argsMap)
    : NativePluginCredentialsProvider(in_config, in_argsMap) {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::BrowserIdcAuthPlugin");
    RS_LOG_DEBUG("IAMIDC", "Current UTC time:%ld", system_clock::to_time_t(system_clock::now()));
    InitArgumentsMap();
}

BrowserIdcAuthPlugin::~BrowserIdcAuthPlugin() {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::~BrowserIdcAuthPlugin");
}

void BrowserIdcAuthPlugin::InitArgumentsMap() {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::InitArgumentsMap");

    const std::string issuer_url = m_config.GetIssuerUrl();
    const std::string idc_region = m_config.GetIdcRegion();
    const short listen_port = m_config.GetListenPort();
    const short idpResponseTimeout  = m_config.GetIdpResponseTimeout();

    if(!IAMUtils::rs_trim(issuer_url).empty()) {
        m_argsMap[KEY_IDC_ISSUER_URL] = issuer_url;
        RS_LOG_DEBUG("IAMIDC", "Setting issuer_url=%s", m_argsMap[KEY_IDC_ISSUER_URL].c_str());
    }

    if(!IAMUtils::rs_trim(idc_region).empty()) {
        m_argsMap[KEY_IDC_REGION] = idc_region;
        RS_LOG_DEBUG("IAMIDC", "Setting idc_region=%s", m_argsMap[KEY_IDC_REGION].c_str());
    }

    if(listen_port != 0) {
        m_argsMap[IAM_KEY_LISTEN_PORT] = std::to_string(listen_port);
        RS_LOG_DEBUG("IAMIDC", "Setting listen_port=%s", m_argsMap[IAM_KEY_LISTEN_PORT].c_str());
    } else {
        m_argsMap[IAM_KEY_LISTEN_PORT] = std::to_string(IAM_DEFAULT_LISTEN_PORT);
        RS_LOG_DEBUG("IAMIDC", "Setting default listen_port=%s", m_argsMap[IAM_KEY_LISTEN_PORT].c_str());
    }

    if (idpResponseTimeout >= 10) { // minimum allowed timeout value is 10 secs 
        m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT] = std::to_string(idpResponseTimeout);
        RS_LOG_DEBUG("IAMIDC", "Setting idp_response_timeout=%s", m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT].c_str());
    } else {
        m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT] = std::to_string(DEFAULT_BROWSER_AUTH_VERIFY_TIMEOUT);
        RS_LOG_DEBUG("IAMIDC", "Setting default idp_response_timeout=%s; provided value=%d", 
            m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT].c_str(), idpResponseTimeout);
    }

    m_idcClientDisplayName = m_config.GetIdcClientDisplayName().empty()
	                                   ? CLIENT_NAME
	                                   : m_config.GetIdcClientDisplayName();

    m_registerClientCacheKey = issuer_url + ":" + idc_region + ":" + std::to_string(listen_port);
}

void BrowserIdcAuthPlugin::ValidateArgumentsMap() {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::ValidateArgumentsMap");

    // Validate the parameters passed in and make sure we have the required fields.
    if (!m_argsMap.count(KEY_IDC_ISSUER_URL)) {
        RS_LOG_ERROR("IAMIDC",
        "IdC authentication failed: issuer_url needs to be provided in "
        "connection params");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: The issuer URL must be included in the "
            "connection parameters.");
    } else if (!m_argsMap.count(KEY_IDC_REGION)) {
        RS_LOG_ERROR("IAMIDC",
        "IdC authentication failed: idc_region needs to be provided in "
        "connection params");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: The IdC region must be included in the "
            "connection parameters.");
    }
}

rs_string BrowserIdcAuthPlugin::GetAuthToken() {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::GetAuthToken");
	return GetIdcToken();
}

std::string BrowserIdcAuthPlugin::GetIdcToken() {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::GetIdcToken");
    ValidateArgumentsMap();

    idc_client = InitializeIdcClient(m_argsMap[KEY_IDC_REGION]);
    RegisterClientResult registerClientResult = GetRegisterClientResult();
    std::string codeVerifier = GenerateCodeVerifier();
    std::string codeChallenge = GenerateCodeChallenge(codeVerifier);
    std::string authorizationCode = FetchAuthorizationCode(codeChallenge, registerClientResult);
    std::string accessToken = FetchAccessToken(registerClientResult, authorizationCode, codeVerifier);
    
    return accessToken;
}

SSOOIDCClient BrowserIdcAuthPlugin::InitializeIdcClient(const std::string& in_region) {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::InitializeIdcClient");
    Aws::Client::ClientConfiguration client_config;
    client_config.region = in_region;
    SSOOIDCClient client(client_config);
    return client;
}

RegisterClientResult BrowserIdcAuthPlugin:: GetRegisterClientResult() {
    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::RegisterClient");

    // Check if a RegisterClientResult exists in the cache and if it is not expired
    if (m_registerClientCache.count(m_registerClientCacheKey) > 0) {
        // check if RegisterClientResult found from cache is expired
        if (m_registerClientCache[m_registerClientCacheKey].GetClientSecretExpiresAt() >
            system_clock::to_time_t(system_clock::now())) {
            RS_LOG_DEBUG("IAMIDC", "Valid RegisterClientResult found from cache");
            return m_registerClientCache[m_registerClientCacheKey];
        } else {
            RS_LOG_DEBUG("IAMIDC", "RegisterClientResult found from cache is expired");
            m_registerClientCache.erase(m_registerClientCacheKey);
        }
    }

    // If the cached RegisterClientResult is not found or has expired, call IdC and register new client
    RegisterClientRequest request;
    request.SetClientName(m_idcClientDisplayName);
    request.SetClientType(CLIENT_TYPE);
    Aws::Vector<Aws::String> idcScopes(1, REDSHIFT_IDC_CONNECT_SCOPE);
    request.SetScopes(idcScopes);
    request.SetIssuerUrl(m_argsMap[KEY_IDC_ISSUER_URL]);
    std::string redirectUri = REDIRECT_URI + ":" + m_argsMap[IAM_KEY_LISTEN_PORT];
    Aws::Vector<Aws::String> redirectUriVector(1, redirectUri);
    request.SetRedirectUris(redirectUriVector);
    Aws::Vector<Aws::String> grantTypes(1, AUTH_CODE_GRANT_TYPE);
    request.SetGrantTypes(grantTypes);

    RegisterClientOutcome outcome = idc_client.RegisterClient(request);

    if(!outcome.IsSuccess()) {
        RS_LOG_DEBUG("IAMIDC", "Failed to register client with IdC");
        HandleRegisterClientError(outcome);
    }

    m_registerClientCache[m_registerClientCacheKey] = outcome.GetResult();
    return outcome.GetResult();
}

std::string BrowserIdcAuthPlugin::GenerateCodeVerifier() {
    std::string codeVerifier = "";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, BASE64_CHAR_LIST.size() - 1);

    for (int i = 0; i < CODE_VERIFIER_LENGTH; ++i) {
        codeVerifier += BASE64_CHAR_LIST[dis(gen)];
    }

    return codeVerifier;
}

std::string BrowserIdcAuthPlugin::GenerateCodeChallenge(const std::string& codeVerifier) {
    // Calculate SHA256 hash
    unsigned char sha256Hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(codeVerifier.c_str()), codeVerifier.length(), sha256Hash);

    return IAMUtils::base64urlEncode(sha256Hash, SHA256_DIGEST_LENGTH);
}

std::string BrowserIdcAuthPlugin::FetchAuthorizationCode(
    const std::string& codeChallenge,
    RegisterClientResult& registerClientResult) {
    //  Generate state to include in URI to prevent the cross-site request forgery attacks.
	std::string state = GenerateState();

	WEBServer srv(state, m_argsMap[IAM_KEY_LISTEN_PORT], m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT]);

	// Launch WEB Server to wait the response with the authorization code from authorize/ endpoint
	srv.LaunchServer();
    
    try
	{
        OpenBrowser(state, codeChallenge, registerClientResult);
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

void BrowserIdcAuthPlugin::OpenBrowser(
    const std::string& state,
    const std::string& codeChallenge,
    RegisterClientResult& registerClientResult) {
    // Generate URI to request an authorization code
    const std::string uri = CURRENT_INTERACTION_SCHEMA + "://" + OIDC_SCHEMA + "." +
        m_argsMap[KEY_IDC_REGION] + "." + AMAZON_COM_SCHEMA +
        "/authorize?response_type=" + AUTH_CODE_PARAMETER_NAME +
        "&client_id=" + registerClientResult.GetClientId().c_str() +
        "&redirect_uri=" + REDIRECT_URI_URL + m_argsMap[IAM_KEY_LISTEN_PORT] +
        "&scopes=" + REDSHIFT_IDC_CONNECT_SCOPE_URL +
        "&state=" + state +
        "&code_challenge=" + codeChallenge +
        "&code_challenge_method=" + CHALLENGE_METHOD;

    // Enforce URL validation
    IAMUtils::ValidateURL(uri);

#if (defined(_WIN32) || defined(_WIN64))
	try {
        LaunchBrowser(uri);
    } 
    catch(RsErrorException & e) {
        RS_LOG_ERROR("IAMIDC", "Unable to open the browser. Display environment is not supported");
    }
#else
    char* display = std::getenv("DISPLAY");
    if (display == nullptr || std::strlen(display) == 0) {
        RS_LOG_ERROR("IAMIDC", "Unable to open the browser. Display environment is not supported");
    } 
    else {
        LaunchBrowser(uri);
    }
#endif
}

void BrowserIdcAuthPlugin::LaunchBrowser(const std::string &uri)
{
	RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::LaunchBrowser");

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
        RS_LOG_ERROR("IAMIDC", "Failed to open the URL in a web browser");
		IAMUtils::ThrowConnectionExceptionWithInfo("Couldn't open a URI or some error occurred.");
	}
}

 std::string BrowserIdcAuthPlugin::FetchAccessToken(
    const RegisterClientResult &in_registerClientResult,
    const std::string &authCode,
    const std::string &codeVerifier) {

    RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::FetchAccessToken");

    int browser_auth_timeout_in_sec = std::stoi(m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT]);
    auto pollingEndTime = system_clock::now() + seconds(browser_auth_timeout_in_sec);

    CreateTokenRequest request;
    request.WithClientId(in_registerClientResult.GetClientId());
    request.WithClientSecret(in_registerClientResult.GetClientSecret());
    request.WithCode(authCode);
    request.WithGrantType(AUTH_CODE_GRANT_TYPE);
    request.WithCodeVerifier(codeVerifier);
    std::string redirectUri = REDIRECT_URI + ":" + m_argsMap[IAM_KEY_LISTEN_PORT];

    request.WithRedirectUri(redirectUri);
    while (system_clock::now() < pollingEndTime) {
        CreateTokenOutcome createTokenOutcome = idc_client.CreateToken(request);

        if(createTokenOutcome.IsSuccess()) {
            std::string access_token =
                createTokenOutcome.GetResult().GetAccessToken();
            if (access_token.empty()) {
                IAMUtils::ThrowConnectionExceptionWithInfo(
                    "IdC authentication failed : The credential token couldn't "
                    "be created.");
            }
            RS_LOG_DEBUG("IAMIDC", "Fetched an IdC token successfully");
            return access_token;
        } 
        else {
            const auto errorType = createTokenOutcome.GetError().GetErrorType();
            if (errorType == Aws::SSOOIDC::SSOOIDCErrors::AUTHORIZATION_PENDING) {
                RS_LOG_DEBUG("IAMIDC", "Browser authorization pending from user");
                std::this_thread::sleep_for(seconds(REQUEST_CREATE_TOKEN_INTERVAL));
            } 
            else {
                HandleCreateTokenError(createTokenOutcome);
            }
        }
    }

    // Failed to get access token because of polling timeout
    IAMUtils::ThrowConnectionExceptionWithInfo(
        "IdC authentication failed : The request timed out. Authentication "
        "wasn't completed.");
}

std::string BrowserIdcAuthPlugin::GenerateState()
{
	RS_LOG_DEBUG("IAMIDC", "BrowserIdcAuthPlugin::GenerateState");

	const int chars_size = (sizeof(STATE_CHAR_LIST) / sizeof(*STATE_CHAR_LIST)) - 1;
	const int rand_size = GenerateRandomInteger(9, chars_size - 1);
	std::string state;

	state.reserve(rand_size);

	for (int i = 0; i < rand_size; ++i)
	{
		state.push_back(STATE_CHAR_LIST[GenerateRandomInteger(0, rand_size)]);
	}

	return state;
}

int BrowserIdcAuthPlugin::GenerateRandomInteger(int low, int high)
{
	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<> dist(low, high);

	return dist(generator);
}

void BrowserIdcAuthPlugin::HandleRegisterClientError(const RegisterClientOutcome &outcome) {
    switch (outcome.GetError().GetErrorType()) {
    case Aws::SSOOIDC::SSOOIDCErrors::INVALID_REDIRECT_URI:
        LogFailureResponse(
            outcome, "Error: Invalid redirect URI provided");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: Invalid redirect URI provided.");
        break;
    case Aws::SSOOIDC::SSOOIDCErrors::INVALID_REQUEST_REGION:
        LogFailureResponse(
            outcome, "Error: Invalid IdC region provided");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: Invalid IdC region provided.");
        break;
    default:
        LogFailureResponse(outcome, "Error: Unexpected register client error");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: There was an error during "
            "authentication.");
    }
}

void BrowserIdcAuthPlugin::HandleCreateTokenError(
    const CreateTokenOutcome &outcome) {
    switch (outcome.GetError().GetErrorType()) {
    case Aws::SSOOIDC::SSOOIDCErrors::SLOW_DOWN:
        LogFailureResponse(
            outcome, "Error: Too frequent createToken requests made by client");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: Requests to the IdC service are too "
            "frequent.");
        break;
    case Aws::SSOOIDC::SSOOIDCErrors::ACCESS_DENIED:
        LogFailureResponse(outcome, "Error: Access denied, please ensure app "
                                    "assignment is done for the user");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: You don't have sufficient permission "
            "to perform the action.");
        break;
    case Aws::SSOOIDC::SSOOIDCErrors::INTERNAL_SERVER:
        LogFailureResponse(outcome, "Error: Server error in creating token");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: An error occurred during the "
            "create token request.");
        break;
    default:
        LogFailureResponse(outcome, "Error: Unexpected error in create token");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: There was an error during "
            "authentication.");
    }
}