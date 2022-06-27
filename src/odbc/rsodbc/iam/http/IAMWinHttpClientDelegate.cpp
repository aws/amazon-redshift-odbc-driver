#if defined(_WIN32)

#include "IAMWinHttpClientDelegate.h"

#include <aws/core/http/HttpTypes.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClientFactory.h>

using namespace Redshift::IamSupport;
using namespace Aws::Client;
using namespace Aws::Http;
using namespace Aws::Utils;

namespace
{
    static const char* LOG_TAG = "IAMWinHttpClientDelegate";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMWinHttpClientDelegate::IAMWinHttpClientDelegate(const HttpClientConfig& in_config) :
    m_client(Aws::MakeShared<IAMWinHttpClient>(LOG_TAG, GetClientConfiguration(in_config)))
{
    /* Do nothing */
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Redshift::IamSupport::HttpResponse IAMWinHttpClientDelegate::MakeHttpRequestWithWIA(
    const rs_wstring& in_host,
    short in_port,
    bool in_verifySSL,
    const rs_wstring& in_proxyUsername,
    const rs_wstring& in_proxyPassword,
    const rs_wstring& in_loginToRp,
	int in_stsConnectionTimeout)
{
    Redshift::IamSupport::HttpResponse response;
    response.SetResponseBody(
        IAMWinHttpClient::SendHttpRequestWithWIA(
            in_host,
            in_port,
            in_verifySSL,
            in_proxyUsername,
            in_proxyPassword,
            in_loginToRp,
			in_stsConnectionTimeout));
    return response;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Redshift::IamSupport::HttpResponse IAMWinHttpClientDelegate::DoMakeHttpRequest(
    const rs_string& in_host,
    HttpMethod in_requestMethod,
    const std::map<rs_string, rs_string>& in_requestHeaders,
    const rs_string& in_requestBody) const
{
    std::shared_ptr<HttpRequest> request = CreateHttpRequest(
        URI(in_host),
        in_requestMethod,
        Stream::DefaultResponseStreamFactoryMethod);


    for (const auto& header : in_requestHeaders)
    {
        request->SetHeaderValue(header.first, header.second);
    }


    if (!in_requestBody.empty())
    {
        SetHttpPostRequestBody(request, in_requestBody);
    }

    std::shared_ptr<Aws::Http::HttpResponse> response = m_client->MakeRequest(request);
    Redshift::IamSupport::HttpResponse result;

    if (response)
    {
        result.SetStatusCode(static_cast<short>(response->GetResponseCode()));
        
        HeaderValueCollection headers =  response->GetHeaders();
        for (const auto& header : headers)
        {
            result.AppendHeader(header.first + ": " + header.second + "\n");
        }
        
        result.SetResponseBody(GetHttpResponseContentBody(response));
    }
    
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMWinHttpClientDelegate::~IAMWinHttpClientDelegate()
{
    /* Do nothing */
}


#endif
