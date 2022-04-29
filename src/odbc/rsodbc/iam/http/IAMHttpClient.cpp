#include "IAMHttpClient.h"
#include "IAMUtils.h"

#include <aws/core/utils/StringUtils.h>

/* Json::JsonValue class contains a member function: GetObject. There is a predefined
MACRO GetObject in wingdi.h that will cause the conflict. We need to undef GetObject
in order to use the GetObject memeber function from Json::JsonValue */
#ifdef GetObject
    #undef GetObject
#endif

#include <aws/core/utils/json/JsonSerializer.h>


using namespace Redshift::IamSupport;
using namespace Aws::Client;
using namespace Aws::Http;
using namespace Aws::Utils;

namespace
{
    static const char* LOG_TAG = "IAMHttpClient";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Redshift::IamSupport::HttpResponse IAMHttpClient::MakeHttpRequest(
    const rs_string& in_host,
    HttpMethod in_requestMethod,
    const std::map<rs_string, rs_string>& in_requestHeaders,
    const rs_string& in_requestBody) const
{
    return DoMakeHttpRequest(
        in_host,
        in_requestMethod,
        in_requestHeaders,
        in_requestBody);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ClientConfiguration IAMHttpClient::GetClientConfiguration(const HttpClientConfig& in_config) 
{
    ClientConfiguration clientConfig;

    if (!in_config.m_userAgent.empty())
    {
        clientConfig.userAgent = in_config.m_userAgent;
    }

    if (!in_config.m_caPath.empty())
    {
        clientConfig.caPath = in_config.m_caPath;
    }

    if (!in_config.m_caFile.empty())
    {
        clientConfig.caFile = in_config.m_caFile;
    }

    if (!in_config.m_httpsProxyHost.empty())
    {
        clientConfig.proxyHost = in_config.m_httpsProxyHost;
        clientConfig.proxyPort = in_config.m_httpsProxyPort;

        if (!in_config.m_httpsProxyUserName.empty())
        {
            clientConfig.proxyUserName = in_config.m_httpsProxyUserName;
        }

        if (!in_config.m_httpsProxyPassword.empty())
        {
            clientConfig.proxyPassword = in_config.m_httpsProxyPassword;
        }
    }

    clientConfig.verifySSL = in_config.m_verifySSL;
    clientConfig.followRedirects = in_config.m_followRedirects;

    // Override the connection and request timeout
    clientConfig.connectTimeoutMs = in_config.m_timeout;
    clientConfig.requestTimeoutMs = in_config.m_timeout;

    return clientConfig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMHttpClient::GetHttpResponseContentBody(
    const std::shared_ptr<Aws::Http::HttpResponse>& in_response)
{
    if (!in_response)
    {
        return rs_string();
    }

    Aws::StringStream responseStream;
    responseStream << in_response->GetResponseBody().rdbuf();
    return responseStream.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMHttpClient::CreateHttpJsonRequestBody(
    const std::map<rs_string, rs_string>& in_paramMap)
{
    Json::JsonValue requestBody;

    for (const auto& param : in_paramMap)
    {
        requestBody.WithString(param.first, param.second);
    }
    Json::JsonView requestBodyView(requestBody);

    return requestBodyView.WriteReadable();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMHttpClient::CreateHttpFormRequestBody(
    const std::map<rs_string, rs_string>& in_paramMap)
{
    size_t counter = 0, paramSize = in_paramMap.size();
    rs_string requestBody;

    /* For instance, if the paramter map = {username: abcd, password:redshift}
    this will be composed as: username=abcd&password=redshift.
    We also need to encode the key and value field according to the standard
    */

    for (const auto& param : in_paramMap)
    {
        requestBody +=
            (StringUtils::URLEncode(param.first.c_str()) 
            + "=" 
            + StringUtils::URLEncode(param.second.c_str()));

        /* Append ampersand except for last parameter */
        if (++counter < paramSize)
        {
            requestBody += "&";
        }
    }

    /* Comply with the application/x-www-form-urlencoded:
    https://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.1 */
    IAMUtils::ReplaceAll(requestBody, "%20", '+');

    return requestBody;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMHttpClient::SetHttpPostRequestBody(
    const std::shared_ptr<HttpRequest>& in_request,
    const rs_string& in_requestBody)
{
    std::shared_ptr<Aws::StringStream> requestBodyStream = Aws::MakeShared<Aws::StringStream>(LOG_TAG);
    *requestBodyStream << in_requestBody;
    in_request->AddContentBody(requestBodyStream);

    /* Calculate the length of the POST content body */
    Aws::StringStream intConverter;
    intConverter << requestBodyStream->tellp();
    in_request->SetContentLength(intConverter.str());
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMHttpClient::CheckHttpResponseStatus(
    const Redshift::IamSupport::HttpResponse& in_response,
    const rs_string& in_message /*= rs_string()*/)
{
    if (HttpResponseCode::OK != static_cast<HttpResponseCode>(in_response.GetStatusCode()))
    {
        rs_string responseCode = " Response code: " + std::to_string(in_response.GetStatusCode());
        IAMUtils::ThrowConnectionExceptionWithInfo(
            in_message +
            responseCode
            );
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMHttpClient:: ~IAMHttpClient()
{
    ; // Do nothing
}
