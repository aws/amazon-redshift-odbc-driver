#ifndef _IAMHTTPCLIENT_H_
#define _IAMHTTPCLIENT_H_

#include <aws/core/http/HttpTypes.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>

#include <map>
#include "../rs_iam_support.h"

namespace Redshift
{
namespace IamSupport
{
    /* Configuration used to initialize a HttpClient */
    struct HttpClientConfig
    {
        rs_string m_userAgent; /* Http request user agent */
        rs_string m_caPath; /* Http SSL CA path */
        rs_string m_caFile; /* Http SSL CA file */

        rs_string m_httpsProxyHost; /* Host to connect to the proxy server */
        rs_string m_httpsProxyUserName; /* Username to connect to the proxy server */
        rs_string m_httpsProxyPassword; /* Password name to connect to the proxy server */
        short m_httpsProxyPort = 0; /* Port to connect to the proxy server*/

        // Port to connect to the proxy server
        bool m_verifySSL; /* Enable verify server certificate */
		Aws::Client::FollowRedirectsPolicy m_followRedirects; /* Enable http re-direct */

        long m_timeout; /* The timeout value in milliseconds. */

        HttpClientConfig() :
            m_verifySSL(true),
            m_followRedirects(Aws::Client::FollowRedirectsPolicy::DEFAULT),
            m_timeout(DEFAULT_TIMEOUT)
        {
            /* Do nothing */
        }
    };

    /* Http Response class returned from MakeHttpRequest call */
    class HttpResponse
    {
    public:
        /// @brief Constructor
        /// 
        /// @param in_statusCode            Http status code
        /// @param in_resHeader             Http response header
        /// @param in_resBody               Http response body
        explicit HttpResponse(
            short in_statusCode = 0,
            const rs_string& in_resHeader = rs_string(),
            const rs_string& in_resBody = rs_string()) :
            m_statusCode(in_statusCode),
            m_resHeader(in_resHeader),
            m_resBody(in_resBody)
        {
            // Do nothing
        }

        /// @brief Gets the Http status code from the Http response
        /// 
        /// @return The Http status code
        inline short GetStatusCode() const
        {
            return m_statusCode;
        }

        /// @brief Sets the Http status code from the Http response
        /// 
        /// @param in_statusCode            Http status code
        inline void SetStatusCode(short in_statusCode)
        {
            m_statusCode = in_statusCode;
        }

        /// @brief Gets the Http response header from the Http response
        /// 
        /// @return The Http response header
        inline rs_string GetResponseHeader() const
        {
            return m_resHeader;
        }

        /// @brief Sets the Http response header from the Http response
        /// 
        /// @param in_resHeader            Http response header
        inline void SetResponseHeader(const rs_string& in_resHeader)
        {
            m_resHeader = in_resHeader;
        }

        /// @brief Appends the Http response header
        /// 
        /// @param in_resHeader            Http header
        inline void AppendHeader(const rs_string& in_resHeader)
        {
            m_resHeader += in_resHeader;
        }

        /// @brief Gets the Http response body from the Http response
        /// 
        /// @return The Http response body
        inline rs_string GetResponseBody() const
        {
            return m_resBody;
        }

        /// @brief Sets the Http response body
        /// 
        /// @param in_resBody            Http response body
        inline void SetResponseBody(const rs_string& in_resBody)
        {
            m_resBody = in_resBody;
        }

    private:
        short m_statusCode; /* Http response status code */
        rs_string m_resHeader; /* Http response header */
        rs_string m_resBody; /* Http response body */
    };

    /// @brief IAMHttpClient class for CURL and WinHttp.
    class IAMHttpClient 
    {
    public:
        /// @brief  Makes HTTP request and retrieves the HttpResponse 
        /// 
        /// @param  in_host                 Request host URI
        /// @param  in_requestMethod        Request method  
        /// @param  in_requestHeaders        Request header vector
        /// @param  in_requestBody          Request body
        /// 
        /// @return HttpResponse
        Redshift::IamSupport::HttpResponse MakeHttpRequest(
            const rs_string& in_host,
            Aws::Http::HttpMethod in_requestMethod = Aws::Http::HttpMethod::HTTP_GET,
            const std::map<rs_string, rs_string>& in_requestHeaders
            = std::map<rs_string, rs_string>(),
            const rs_string& in_requestBody = rs_string()) const;

        /// @brief  Get the Aws ClientConfiguration based on PGO HttpClientConfig
        /// 
        /// @param  in_config   The HttpClientConfig
        /// 
        /// @return Aws ClientConfiguration
        static Aws::Client::ClientConfiguration GetClientConfiguration(
            const HttpClientConfig& in_config);
       
        /// @brief  Get the content body from the given Http response 
        /// 
        /// @param  in_response   The Http response from the Http request
        /// 
        /// @return The content body of the Http response
        static rs_string GetHttpResponseContentBody(
            const std::shared_ptr<Aws::Http::HttpResponse>& in_response);

        /// @brief  Creates the Http POST JSON request body 
        /// 
        /// @param  in_paramMap  Key-value parameter map
        /// 
        /// @return The request content body string generated from the parameter map
        static rs_string CreateHttpJsonRequestBody(
            const std::map<rs_string, rs_string>& in_paramMap);

        /// @brief  Creates the Http POST form url-encoded request body 
        /// 
        /// @param  in_paramMap  Key-value parameter map
        /// 
        /// @return The request content body string generated from the parameter map
        static rs_string CreateHttpFormRequestBody(
            const std::map<rs_string, rs_string>& in_paramMap);

        /// @brief  Set the HttpRequest's request body and length
        /// 
        /// @param  in_request           Http request
        /// @param  in_requestBody       Http request body to be set
        static void SetHttpPostRequestBody(
            const std::shared_ptr<Aws::Http::HttpRequest>& in_request,
            const rs_string& in_requestBody);

        /// @brief  Checks the http response
        /// 
        /// @param  in_response       Http response
        /// @param  in_message        Customized error message (Optional)
        /// 
        /// @Exception if response's status code is not 200
        static void CheckHttpResponseStatus(
            const Redshift::IamSupport::HttpResponse& in_response,
            const rs_string& in_message = rs_string());

        // @brief Destructor
        virtual ~IAMHttpClient();

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
            const rs_string& in_requestBody ) const = 0;

    };
}
}

#endif
