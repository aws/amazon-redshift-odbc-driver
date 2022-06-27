#ifndef _IAMWINHTTPCLIENTDELEGATE_H_
#define _IAMWINHTTPCLIENTDELEGATE_H_

#if defined(_WIN32)

#include "rs_iam_support.h"
#include "IAMHttpClient.h"
#include "IAMWinHttpClient.h"

namespace Redshift
{
namespace IamSupport
{
    /// @brief Wrapper/Delegate for PGOWinHttpClient
    class IAMWinHttpClientDelegate : public IAMHttpClient
    {
    public:
        /// @brief Constructor        Construct PGOWinInetHttpClient object
        /// 
        /// @param  in_config         HttpClientConfig object
        explicit IAMWinHttpClientDelegate(
            const HttpClientConfig& in_config);

        /// @brief  Makes HTTP request using WIA
        /// 
        /// @param  in_host                 Request host URI
        /// @param  in_port                 Request host port, 0 if using default
        /// @param  in_verifySSL            Verify server certificate
        /// @param  in_proxyUsername        Username to connect to the proxy
        /// @param  in_proxyPassword        Password to connect to the proxy
        /// 
        /// @return HttpResponse
        static Redshift::IamSupport::HttpResponse MakeHttpRequestWithWIA(
            const rs_wstring& in_host,
            short in_port = 0,
            bool in_verifySSL = true,
            const rs_wstring& in_proxyUsername = L"",
            const rs_wstring& in_proxyPassword = L"",
            const rs_wstring& in_loginToRp = L"",
			int in_stsConnectionTimeout = (int)DEFAULT_TIMEOUT);

        /// @brief Destructor
        ~IAMWinHttpClientDelegate();

    private:
        /// @brief  Makes HTTP request and retrieves the HttpResponse 
        /// 
        /// @param  in_host                 Request host URI
        /// @param  in_requestMethod        Request method  
        /// @param  in_requestHeaders       Request headers map
        /// @param  in_requestBody          Request body 
        /// 
        /// @return HttpResponse
        virtual HttpResponse DoMakeHttpRequest(
            const rs_string& in_host,
            Aws::Http::HttpMethod in_requestMethod,
            const std::map<rs_string, rs_string>& in_requestHeaders,
            const rs_string& in_requestBody) const override;

        // The actual WinHttpClient used to make Http request
        std::shared_ptr<IAMWinHttpClient> m_client;
    };
}
}

#endif
#endif
