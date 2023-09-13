#include "BrowserIdcAuthPlugin.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unordered_map>

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
    static const std::string DEFAULT_IDC_CLIENT_DISPLAY_NAME =
        "Amazon Redshift ODBC driver";
    static const std::string CLIENT_TYPE = "public";
    static const std::string GRANT_TYPE =
        "urn:ietf:params:oauth:grant-type:device_code";

    static const std::string IDC_SCOPE = "redshift:connect";

    /**
     * The default value of browser authentication timeout in seconds
     * It is used if user doesn't provide any value for {@code idc_response_timeout} option.
     */
    static const int DEFAULT_BROWSER_AUTH_VERIFY_TIMEOUT = 120;
    /**
     * The default time in seconds for which the client must wait between attempts
     * when polling for a session It is used if auth server doesn't provide any
     * value for {@code interval} in start device authorization response
     */
    static const int REQUEST_CREATE_TOKEN_DEFAULT_INTERVAL = 1;

    static std::unordered_map<std::string, RegisterClientResult> m_registerClientCache;
    std::string m_registerClientCacheKey;

    std::string m_idcClientDisplayName;
}

BrowserIdcAuthPlugin::BrowserIdcAuthPlugin(
    RsLogger *in_log, const IAMConfiguration &in_config,
    const std::map<rs_string, rs_string> &in_argsMap)
    : NativePluginCredentialsProvider(in_log, in_config, in_argsMap) {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::BrowserIdcAuthPlugin");
    RS_LOG(m_log)("Current UTC time:%ld", system_clock::to_time_t(system_clock::now()));
    InitArgumentsMap();
}

BrowserIdcAuthPlugin::~BrowserIdcAuthPlugin() {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::~BrowserIdcAuthPlugin");
}

rs_string BrowserIdcAuthPlugin::GetAuthToken() {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::GetAuthToken");
    return GetIdcToken();
}

void BrowserIdcAuthPlugin::InitArgumentsMap() {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::InitArgumentsMap");

    const rs_string startUrl = m_config.GetStartUrl();
    const rs_string idcRegion = m_config.GetIdcRegion();
    const short idcResponseTimeout  = m_config.GetIdcResponseTimeout();

    if (!IAMUtils::rs_trim(startUrl).empty()) {
        m_argsMap[KEY_IDC_START_URL] = startUrl;
        RS_LOG(m_log)("Setting start_url=%s", startUrl.c_str());
    }
    if (!IAMUtils::rs_trim(idcRegion).empty()) {
        m_argsMap[KEY_IDC_REGION] = idcRegion;
        RS_LOG(m_log)("Setting idc_region=%s", idcRegion.c_str());
    }
    
    if (idcResponseTimeout >= 10) { // minimum allowed timeout value is 10 secs 
        m_argsMap[KEY_IDC_RESPONSE_TIMEOUT] = std::to_string(idcResponseTimeout);
        RS_LOG(m_log)("Setting idc_response_timeout=%s", m_argsMap[KEY_IDC_RESPONSE_TIMEOUT].c_str());
    } else {
        m_argsMap[KEY_IDC_RESPONSE_TIMEOUT] = std::to_string(DEFAULT_BROWSER_AUTH_VERIFY_TIMEOUT);
        RS_LOG(m_log)("Setting idc_response_timeout=%s; provided value=%d", 
                        m_argsMap[KEY_IDC_RESPONSE_TIMEOUT].c_str(), idcResponseTimeout);
    }
    
    

    // Set values for other runtime variables
    // If client provides an application name, we use that, else use driver as default name
    m_idcClientDisplayName = m_config.GetIdcClientDisplayName().empty()
                                  ? DEFAULT_IDC_CLIENT_DISPLAY_NAME
                                  : m_config.GetIdcClientDisplayName();
    RS_LOG(m_log)("Setting display application name=%s", m_idcClientDisplayName.c_str());

    m_registerClientCacheKey =
        m_idcClientDisplayName + ":" + idcRegion;
}

rs_string BrowserIdcAuthPlugin::GetIdcToken() {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::GetIdcToken");
    ValidateArgumentsMap();

    SSOOIDCClient idc_client =
        InitializeIdcClient(m_argsMap[KEY_IDC_REGION]);

    RegisterClientResult registerClientResult =
        BrowserIdcAuthPlugin::RegisterClient(
            idc_client, m_registerClientCacheKey, m_idcClientDisplayName,
            CLIENT_TYPE, IDC_SCOPE);

    StartDeviceAuthorizationResult startDeviceAuthorizationResult =
        BrowserIdcAuthPlugin::StartDeviceAuthorization(
            idc_client, registerClientResult.GetClientId(),
            registerClientResult.GetClientSecret(),
            m_argsMap[KEY_IDC_START_URL]);

    LaunchBrowser(startDeviceAuthorizationResult.GetVerificationUriComplete());

    return PollForAccessToken(idc_client, registerClientResult,
                              startDeviceAuthorizationResult, GRANT_TYPE);
}

void BrowserIdcAuthPlugin::ValidateArgumentsMap() {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::ValidateArgumentsMap");

    // Validate the parameters passed in and make sure we have the required fields.
    if (!m_argsMap.count(KEY_IDC_START_URL)) {
        RS_LOG(m_log)
        ("IdC authentication failed: start_url needs to be provided in "
         "connection params");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: The start URL must be included in the "
            "connection parameters.");
    } else if (!m_argsMap.count(KEY_IDC_REGION)) {
        RS_LOG(m_log)
        ("IdC authentication failed: idc_region needs to be provided in "
         "connection params");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed: The IdC region must be included in the "
            "connection parameters.");
    }
}

SSOOIDCClient BrowserIdcAuthPlugin::InitializeIdcClient(
    const std::string &in_idcRegion) {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::InitializeIdcClient");
    Aws::Client::ClientConfiguration client_config;
    client_config.region = in_idcRegion;
    SSOOIDCClient client(client_config);
    return client;
}

template <typename T, typename R> 
void BrowserIdcAuthPlugin::LogFailureResponse(
    const Aws::Utils::Outcome<T, R> &outcome, const std::string &operation) {
    int responseCode = static_cast<int>(outcome.GetError().GetResponseCode());
    std::string exceptionName = outcome.GetError().GetExceptionName();
    std::string errorMessage = (outcome.GetError()).GetMessage();
    RS_LOG(m_log)("%s - Response code:%d; Exception name:%s; Error message:%s", 
        operation.c_str(), responseCode, exceptionName.c_str(), errorMessage.c_str());
}

RegisterClientResult BrowserIdcAuthPlugin::RegisterClient(
    SSOOIDCClient &idc_client, const std::string &in_registerClientCacheKey,
    const std::string &in_idcClientDisplayName,
    const std::string &in_clientType, const std::string &in_scope) {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::RegisterClient");

    // Check if a RegisterClientResult exists in the cache and if it is not expired
    if (m_registerClientCache.count(in_registerClientCacheKey) > 0) {
        // check if RegisterClientResult found from cache is expired
        if (m_registerClientCache[in_registerClientCacheKey].GetClientSecretExpiresAt() >
            system_clock::to_time_t(system_clock::now())) {
            RS_LOG(m_log)("Valid RegisterClientResult found from cache");
            return m_registerClientCache[in_registerClientCacheKey];
        } else {
            RS_LOG(m_log)("RegisterClientResult found from cache is expired");
            m_registerClientCache.erase(in_registerClientCacheKey);
        }
    }

    /** 
     * If the cached RegisterClientResult is not found or has expired, call IdC
     * and register new client
     */
    RegisterClientRequest request;
    request.SetClientName(in_idcClientDisplayName);
    request.SetClientType(in_clientType);
    Aws::Vector<Aws::String> idcScopes(1, in_scope);
    request.SetScopes(idcScopes);

    auto outcome = idc_client.RegisterClient(request);

    if(!outcome.IsSuccess()) {
        RS_LOG(m_log)("Failed to register client with IdC");
        HandleRegisterClientError(outcome);
    }

    // Cache the result and return it
	m_registerClientCache[in_registerClientCacheKey] = outcome.GetResult();
    return outcome.GetResult();
}

template <typename T, typename R>
void BrowserIdcAuthPlugin::HandleRegisterClientError(
    const Aws::Utils::Outcome<T, R> &outcome) {
    int errorType = static_cast<int>(outcome.GetError().GetErrorType());
    std::string errorMessage = (outcome.GetError()).GetMessage();
    switch (errorType) {
    case static_cast<int>(Aws::SSOOIDC::SSOOIDCErrors::INTERNAL_SERVER):
        LogFailureResponse(
            outcome, "Error: Unexpected server error while registering client");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : An error occurred during the "
            "request.");
        break;
    default:
        LogFailureResponse(outcome, "Error: Unexpected register client error");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : There was an error during "
            "authentication.");
    }
}

StartDeviceAuthorizationResult BrowserIdcAuthPlugin::StartDeviceAuthorization(
    SSOOIDCClient &idc_client, const std::string &in_clientId,
    const std::string &in_clientSecret, const std::string &in_startUrl) {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::StartDeviceAuthorization");

    StartDeviceAuthorizationRequest request;
    request.SetClientId(in_clientId);
    request.SetClientSecret(in_clientSecret);
    request.SetStartUrl(in_startUrl);

    auto outcome = idc_client.StartDeviceAuthorization(request);

    if(!outcome.IsSuccess()) {
        RS_LOG(m_log)("Failed to start device authorization with IdC");
        HandleStartDeviceAuthorizationError(outcome);
    }

    return outcome.GetResult();
}


template <typename T, typename R>
void BrowserIdcAuthPlugin::HandleStartDeviceAuthorizationError(
    const Aws::Utils::Outcome<T, R> &outcome) {
    int errorType = static_cast<int>(outcome.GetError().GetErrorType());
    std::string errorMessage = (outcome.GetError()).GetMessage();
    switch (errorType) {
    case static_cast<int>(Aws::SSOOIDC::SSOOIDCErrors::SLOW_DOWN):
        LogFailureResponse(outcome,
                           "Error: Too frequent requests made by client");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : Requests to the IdC service are too "
            "frequent.");
        break;
    case static_cast<int>(Aws::SSOOIDC::SSOOIDCErrors::INTERNAL_SERVER):
        LogFailureResponse(outcome,
                           "Error: Server error in start device authorization");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : An error occurred during the "
            "request.");
        break;
    default:
        LogFailureResponse(
            outcome, "Error: Unexpected error in start device authorization");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : There was an error during "
            "authentication.");
    }
}

void BrowserIdcAuthPlugin::LaunchBrowser(const std::string &uri) {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::LaunchBrowser");

    ValidateURL(uri);

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
		RS_LOG(m_log)("Failed to open the URL in a web browser");
		IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : The URL can't be opened in a web browser.");
        }
}

std::string BrowserIdcAuthPlugin::PollForAccessToken(
    SSOOIDCClient &idc_client,
    const RegisterClientResult &in_registerClientResult,
    const StartDeviceAuthorizationResult &in_startDeviceAuthorizationResult,
    const std::string &in_grantType) {
    RS_LOG(m_log)("BrowserIdcAuthPlugin::PollForAccessToken");

    int browser_auth_timeout_in_sec = std::stoi(m_argsMap[KEY_IDC_RESPONSE_TIMEOUT]);
    auto pollingEndTime =
        system_clock::now() + seconds(browser_auth_timeout_in_sec);

    int pollingIntervalInSec = REQUEST_CREATE_TOKEN_DEFAULT_INTERVAL;
    if (in_startDeviceAuthorizationResult.GetInterval()) {
        pollingIntervalInSec = in_startDeviceAuthorizationResult.GetInterval();
    }

    CreateTokenRequest request;
    request.WithClientId(in_registerClientResult.GetClientId());
    request.WithClientSecret(in_registerClientResult.GetClientSecret());
    request.WithDeviceCode(in_startDeviceAuthorizationResult.GetDeviceCode());
    request.WithGrantType(in_grantType);

    while (system_clock::now() < pollingEndTime) {

        auto createTokenOutcome = idc_client.CreateToken(request);

        if(createTokenOutcome.IsSuccess()) {
            std::string access_token =
                createTokenOutcome.GetResult().GetAccessToken();
            if (access_token.empty()) {
                IAMUtils::ThrowConnectionExceptionWithInfo(
                    "IdC authentication failed : The credential token couldn't "
                    "be created.");
            }
            RS_LOG(m_log)("Fetched an IdC token successfully");
            return access_token;
        } else {
            const auto errorType = createTokenOutcome.GetError().GetErrorType();
            if (errorType == Aws::SSOOIDC::SSOOIDCErrors::AUTHORIZATION_PENDING) {
	            RS_LOG(m_log)("Browser authorization pending from user");
	            std::this_thread::sleep_for(seconds(pollingIntervalInSec));
	        } else {
                HandleCreateTokenError(createTokenOutcome);
            }
        }
    }

    // Failed to get access token because of polling timeout
    IAMUtils::ThrowConnectionExceptionWithInfo(
        "IdC authentication failed : The request timed out. Authentication "
        "wasn't completed.");
}

template <typename T, typename R>
void BrowserIdcAuthPlugin::HandleCreateTokenError(
    const Aws::Utils::Outcome<T, R> &outcome) {
    int errorType = static_cast<int>(outcome.GetError().GetErrorType());
    std::string errorMessage = (outcome.GetError()).GetMessage();
    switch (errorType) {
    case static_cast<int>(Aws::SSOOIDC::SSOOIDCErrors::SLOW_DOWN):
        LogFailureResponse(
            outcome, "Error: Too frequent createToken requests made by client");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : Requests to the IdC service are too "
            "frequent.");
        break;
    case static_cast<int>(Aws::SSOOIDC::SSOOIDCErrors::ACCESS_DENIED):
        LogFailureResponse(outcome, "Error: Access denied, please ensure app "
                                    "assignment is done for the user");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : You don't have sufficient permission "
            "to perform the action.");
        break;
    case static_cast<int>(Aws::SSOOIDC::SSOOIDCErrors::INTERNAL_SERVER):
        LogFailureResponse(outcome, "Error: Server error in creating token");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : An error occurred during the "
            "request.");
        break;
    default:
        LogFailureResponse(outcome, "Error: Unexpected error in create token");
        IAMUtils::ThrowConnectionExceptionWithInfo(
            "IdC authentication failed : There was an error during "
            "authentication.");
    }
}
