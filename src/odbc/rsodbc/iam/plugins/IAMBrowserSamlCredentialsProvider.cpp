#include "IAMBrowserSamlCredentialsProvider.h"
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
#include <aws/core/utils/StringUtils.h>

#include <random>
#include <sstream>
#include <unordered_map>

using namespace Redshift::IamSupport;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::Utils;
using namespace Aws::Http;

namespace
{
    // Wait 10 seconds for the server to start listening
    static const int SERVER_START_TIMEOUT = 10;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMBrowserSamlCredentialsProvider::IAMBrowserSamlCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::IAMBrowserSamlCredentialsProvider");

    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserSamlCredentialsProvider::InitArgumentsMap()
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::InitArgumentsMap");
    
    /* We grab the parameters needed to get the SAML Assertion and get the temporary IAM Credentials.
    We are using the base class implementation but we override for logging purposes. */
    IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserSamlCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::ValidateArgumentsMap");
    
    /* We validate the parameters passed in and make sure we have the required fields. */
    if (!m_argsMap.count(IAM_KEY_LOGIN_URL))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Authentication failed, please verify that LOGIN_URL is provided.");
    }

    /* Use default timeout parameter if user didn't provide timeout in configuration field. */
    if (!m_argsMap.count(IAM_KEY_IDP_RESPONSE_TIMEOUT))
    {
        m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT] = std::to_string(IAM_DEFAULT_BROWSER_PLUGIN_TIMEOUT);
    }
    /* Use default listen port parameter if user didn't provide listen port in configuration field. */
    if (!m_argsMap.count(IAM_KEY_LISTEN_PORT))
    {
        m_argsMap[IAM_KEY_LISTEN_PORT] = std::to_string(IAM_DEFAULT_LISTEN_PORT);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserSamlCredentialsProvider::GetSamlAssertion()
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::GetSamlAssertion");
    
    /* All plugins must have this method implemented. We need to return the SAML Response back to base class.
    It is also good to make an entrance log to this method. */
    return RequestSamlAssertion();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int IAMBrowserSamlCredentialsProvider::GenerateRandomInteger(int low, int high)
 {
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::GenerateLength");

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(low, high);

    return dist(generator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMBrowserSamlCredentialsProvider::GenerateState()
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::GenerateState");

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
void IAMBrowserSamlCredentialsProvider::LaunchBrowser(const rs_string& uri)
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::LaunchBrowser");

    rs_string open_uri = command_ + uri + subcommand_;

    if (system(open_uri.c_str()) == -1)
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Couldn't open a URI or some error occurred.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserSamlCredentialsProvider::EraseLineFeeds(rs_string& str)
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::EraseLineFeeds");

    const rs_string linefeed = "\r\n";
    size_t pos = 0;

    while ((pos = str.find(linefeed)) != std::string::npos)
    {
        str.erase(pos, linefeed.size());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMBrowserSamlCredentialsProvider::WaitForServer(WEBServer& srv)
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::WaitForServer");

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
rs_string IAMBrowserSamlCredentialsProvider::RequestSamlAssertion()
{
    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::RequestSamlAssertion");
    
    /*  Generate state to include in URI to prevent the cross-site request forgery attacks.
    This state will be verified in the token response.  */
    rs_string state = GenerateState();
    
    WEBServer srv(m_log, state,
    m_argsMap[IAM_KEY_LISTEN_PORT],
    m_argsMap[IAM_KEY_IDP_RESPONSE_TIMEOUT]);
    
    /* Launch WEB Server to wait for the SAML response. */
    srv.LaunchServer();
    
    WaitForServer(srv);

    LaunchBrowser(m_argsMap[IAM_KEY_LOGIN_URL]);
    
    srv.Join();
    
    if (srv.IsTimeout())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Connection timeout. Please verify the connection settings.");
    }
    
    rs_string SAMLResponse(srv.GetSamlResponse());
    
    SAMLResponse = StringUtils::URLDecode(SAMLResponse.c_str());

    EraseLineFeeds(SAMLResponse);

    RS_LOG(m_log)("IAMBrowserSamlCredentialsProvider::RequestSamlAssertion %s",
        SAMLResponse.c_str());
    
    return SAMLResponse;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMBrowserSamlCredentialsProvider::~IAMBrowserSamlCredentialsProvider()
{
    ; // Do nothing.
}
