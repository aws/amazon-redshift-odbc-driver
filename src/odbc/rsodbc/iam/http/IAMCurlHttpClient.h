// *NOTE*: This is a class borrowed and modified using the AWS C++ SDK on Github
//  Link: https://github.com/aws/aws-sdk-cpp

#ifndef _IAMCURLHTTPCLIENT_H_
#define _IAMCURLHTTPCLIENT_H_

#if !defined(_WIN32)

#include "../rs_iam_support.h"
#include "IAMHttpClient.h"
#include "curl/curl.h"

#include <aws/core/http/HttpTypes.h>
#include <map>
#include <sstream>

namespace Redshift
{
namespace IamSupport
{
    /// @brief Wrapper around a CURL connection pointer.
    struct CurlConnection
    {
        /// @brief Constructor.
        CurlConnection(CURL *in_curlConnection) : 
            m_curlConnection(in_curlConnection)
        {
            ; // Do nothing
        }

        /// @brief Destructor.
        ~CurlConnection()
        {
            if (NULL != m_curlConnection)
            {
                curl_easy_cleanup(m_curlConnection);
            }
        }

        /// @brief Returns the wrapped CURL connection pointer.
        ///
        /// @return The wrapped pointer. (NOT OWN)
        CURL* Get() const
        {
            return m_curlConnection;
        }

        // Private ==============================================================================================================
    private:
        // The CURL connection pointer being wrapped. (OWN)
        CURL *m_curlConnection;
    };

    /// @brief Wrapper around a curl_slist pointer.
    struct CurlStringList
    {
        /// @brief Constructor.
        CurlStringList() : m_list(NULL)
        {
            ; // Does nothing.
        }

        /// @brief Destructor.
        ~CurlStringList()
        {
            if (NULL != m_list)
            {
                curl_slist_free_all(m_list);
            }
        }

        /// @brief Returns the wrapped curl_slist pointer.
        ///
        /// @return The wrapped pointer. (NOT OWN)
        curl_slist* Get() const
        {
            return m_list;
        }

        /// @brief Appends a string value to the list.
        ///
        /// @param in_value     The value to append. (NOT OWN)
        /// @return *this.
        CurlStringList& Append(const char* in_value)
        {
            m_list = curl_slist_append(m_list, in_value);
            return *this;
        }

    // Private ==============================================================================================================
    private:
        // The pointer being wrapped. (OWN)
        curl_slist* m_list;
    };

    /// @brief IAMCurlHttpClient class for making Http request
    class IAMCurlHttpClient : public IAMHttpClient
    {
    public:
        /// @brief Constructor        Construct IAMCurlHttpClient object
        // Creates client, initializes curl handle if it hasn't been created already.
        explicit IAMCurlHttpClient(const HttpClientConfig& in_config);

        /// @brief Destructor
        ~IAMCurlHttpClient();

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMCurlHttpClient& operator=(const IAMCurlHttpClient& in_httpClient);

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

        // Curl connection handle used for make HTTP request
        CurlConnection m_connectionHandle;

        rs_string m_userAgent; /* Http User Agent */
        rs_string m_caPath; /* Http SSL CA Path */
        rs_string m_caFile; /* Http SSL CA File */

        bool m_verifySSL; /* Enable verifying Http server certificate */
		Aws::Client::FollowRedirectsPolicy m_followRedirects; /* Enable Http redirect */
        bool m_isUsingProxy; /* True if a proxy server is used */

        // Username to connect to the proxy server
        rs_wstring m_proxyUserName;

        // Password to connect to the proxy server
        rs_wstring m_proxyPassword;

        // Host name to connect to the proxy server
        rs_wstring m_proxyHost;
        
        // Port to connect to the proxy server
        short m_proxyPort;
    };
}
}

#endif
#endif
