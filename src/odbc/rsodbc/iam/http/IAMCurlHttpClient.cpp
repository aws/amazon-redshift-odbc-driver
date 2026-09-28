// *NOTE*: This is a class borrowed and modified using the AWS C++ SDK on Github
//  Link: https://github.com/aws/aws-sdk-cpp

#if !defined(_WIN32)

#include "IAMCurlHttpClient.h"
#include "IAMUtils.h"
#include <rslog.h>

using namespace Redshift::IamSupport;
using namespace Aws::Client;
using namespace Aws::Http;

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
    // Default Http user agent used for making Http call
    const rs_string DEFAULT_USER_AGENT = "curl/7.44.0";

    /// @brief  Callback function used for write the data returned from Curl Http request
    /// 
    /// @param  in_contents        Http response content
    /// @param  in_size            Size of each memory block
    /// @param  in_nmemb           Total number of memory blocks
    /// @param  in_userp           User pointer
    /// 
    /// @return Total size of the content received
    static size_t WriteDataCallback(void *in_contents, size_t in_size, size_t in_nmemb, void *in_userp)
    {
        static_cast<rs_string*>(in_userp)->append(
            static_cast<char*>(in_contents), in_size * in_nmemb);
        return in_size * in_nmemb;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMCurlHttpClient::IAMCurlHttpClient(const HttpClientConfig& in_config) :
    m_connectionHandle(curl_easy_init()),
    m_userAgent(in_config.m_userAgent),
    m_caPath(in_config.m_caPath),
    m_caFile(in_config.m_caFile),
    m_verifySSL(in_config.m_verifySSL),
    m_followRedirects(in_config.m_followRedirects),
    m_isUsingProxy(!in_config.m_httpsProxyHost.empty()),
    m_proxyUserName(IAMUtils::convertStringToWstring(in_config.m_httpsProxyUserName)),
    m_proxyPassword(IAMUtils::convertStringToWstring(in_config.m_httpsProxyPassword)),
    m_proxyHost(IAMUtils::convertStringToWstring(in_config.m_httpsProxyHost)),
    m_proxyPort(in_config.m_httpsProxyPort)
{
    if (!m_connectionHandle.Get())
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Failed to initialize CurlHttpClient.");
    }

    if (!m_userAgent.empty())
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_USERAGENT, m_userAgent.c_str());
    }
    else
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_USERAGENT, DEFAULT_USER_AGENT.c_str());
    }
    
    if (!m_caPath.empty())
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_CAPATH, m_caPath.c_str());
    }

    // Resolve the CA file: caller-supplied path, or the driver default converted
    // to UTF-8. See ResolveCaFile for why the conversion is required.
    const rs_string caFile = ResolveCaFile(m_caFile);

    if (!caFile.empty())
    {
        RS_LOG_DEBUG("IAMCurlHttpClient", "Using CA file: %s", caFile.c_str());
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_CAINFO, caFile.c_str());
    }

    if (m_verifySSL)
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_SSL_VERIFYHOST, 2L);
    }
    else
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    if (m_followRedirects != FollowRedirectsPolicy::NEVER)
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_MAXREDIRS, 50L);
    }
    
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_TCP_KEEPALIVE, 1L);

    if (m_isUsingProxy)
    {
        m_proxyHost = L"http://" + m_proxyHost;
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_PROXY, IAMUtils::convertToUTF8(m_proxyHost).c_str());
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_PROXYPORT, (long)m_proxyPort);
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_PROXYUSERNAME, IAMUtils::convertToUTF8(m_proxyUserName).c_str());
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_PROXYPASSWORD, IAMUtils::convertToUTF8(m_proxyPassword).c_str());
    }
    else
    {
        curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_PROXY, "");
    }


}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMCurlHttpClient::ResolveCaFile(const rs_string& in_caFile)
{
    if (!in_caFile.empty())
    {
        return in_caFile;
    }

    // GetDefaultCaFile() returns an rs_wstring; convert to UTF-8 so libcurl
    // receives a valid const char* path instead of a truncated wide string.
    return IAMUtils::convertToUTF8(IAMUtils::GetDefaultCaFile());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Redshift::IamSupport::HttpResponse IAMCurlHttpClient::DoMakeHttpRequest(
    const rs_string& in_host,
    HttpMethod in_requestMethod,
    const std::map<rs_string, rs_string>& in_requestHeaders,
    const rs_string& in_requestBody) const
{
    CURLcode ret;
    
    CurlStringList headerList;

    long statusCode;
    rs_string responseHeader;
    rs_string responseBody;

    switch (in_requestMethod)
    {
        case HttpMethod::HTTP_GET:
        {
            curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_HTTPGET, 1L);
            break;
        }

        case HttpMethod::HTTP_POST:
        {
            curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_POST, 1L);
            curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_POSTFIELDS, in_requestBody.c_str());
            curl_easy_setopt(
                m_connectionHandle.Get(), 
                CURLOPT_POSTFIELDSIZE, 
                static_cast<long>(in_requestBody.size()));
            break;
        }
        default:
            break;
    }

    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_URL, in_host.c_str());

    for (const auto& header: in_requestHeaders)
    {
        rs_string headerValue = header.first + ": " + header.second;
        headerList.Append(headerValue.c_str());
    }

    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_HTTPHEADER, headerList.Get());
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_WRITEFUNCTION, WriteDataCallback);
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(m_connectionHandle.Get(), CURLOPT_HEADERDATA, &responseHeader);

    ret = curl_easy_perform(m_connectionHandle.Get());

    if (ret != CURLE_OK)
    {
        rs_string message(curl_easy_strerror(ret));
        IAMUtils::ThrowConnectionExceptionWithInfo("Failed to authenticate server: " + message);
    }

    curl_easy_getinfo(m_connectionHandle.Get(), CURLINFO_RESPONSE_CODE, &statusCode);

    HttpResponse response(
        static_cast<short>(statusCode),
        responseHeader,
        responseBody);   

    return response;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMCurlHttpClient::~IAMCurlHttpClient()
{
    ; // Do nothing
}

#endif
