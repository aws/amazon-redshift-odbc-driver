// *NOTE*: This is a class borrowed and modified using the AWS C++ SDK on Github
//  Link: https://github.com/aws/aws-sdk-cpp

#ifndef _IAMWINHTTPCLIENT_H_
#define _IAMWINHTTPCLIENT_H_

#if defined(_WIN32)

#include "rs_iam_support.h"
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/windows/WinSyncHttpClient.h>
#include <aws/core/client/ClientConfiguration.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#include <winhttp.h>

namespace Redshift
{
namespace IamSupport
{
    class IAMWinHttpClient : public Aws::Http::WinSyncHttpClient
    {
    public:
        /// @brief Constructor        Construct PGOWinInetHttpClient object
        explicit IAMWinHttpClient(
            const Aws::Client::ClientConfiguration& config, 
            bool in_enableWIA = false);

        /// @brief Destructor
        ~IAMWinHttpClient();

        // Gets log tag for use in logging in the base class.
        const char* GetLogTag() const override { return "IAMWinHttpClient"; }

        // Set enableWIA
        void SetEnableWIA(bool in_enableWIA);

        // Send the http request using Windows Integrated Authentication
        static rs_string SendHttpRequestWithWIA(
            const rs_wstring& in_host,
            short in_port = INTERNET_DEFAULT_PORT,
            bool in_verifySSL = true,
            const rs_wstring& in_proxyUsername = L"",
            const rs_wstring& in_proxyPassword = L"",
            const rs_wstring& in_loginToRp = L"",
			int in_stsConnectionTimeout = (int)DEFAULT_TIMEOUT
        );

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMWinHttpClient& operator=(const IAMWinHttpClient& in_winInetHttpClient);
        
        // WinHttp specific implementations
        void* OpenRequest(
            const std::shared_ptr<Aws::Http::HttpRequest>& request,
            void* connection, 
            const Aws::StringStream& ss) const override;

        // Add headers to the http request handle 
        void DoAddHeaders(void* hHttpRequest, Aws::String& headerStr) const override;

        // Write request data to an HTTP server.
        uint64_t DoWriteData(void* hHttpRequest, 
							char* streamBuffer, 
							uint64_t bytesRead,
							bool isChunked) const override;

		// Write the trailing CRLF to the request data to an HTTP server.
		uint64_t FinalizeWriteData(void* hHttpRequest) const override;

        // Wait to receive the response to an HTTP request initiated by WinHttpSendRequest. 
        bool DoReceiveResponse(void* hHttpRequest) const override;

        // Retrieve header information associated with an HTTP request.
        bool DoQueryHeaders(
            void* hHttpRequest, 
            std::shared_ptr<Aws::Http::HttpResponse>& response, 
            Aws::StringStream& ss, uint64_t& read) const override;

        //  Send the specified request to the HTTP server.
        bool DoSendRequest(void* hHttpRequest) const override;

        // Read data from a handle opened by the WinHttpOpenRequest function.
        bool DoReadData(void* hHttpRequest, char* body, uint64_t size, uint64_t& read) const override;

        // Get WinHttpClient module
        void* GetClientModule() const override;

        // Enabled if need to verify the server certificate
        bool m_verifySSL;

        // True if Windows Integrated Authentication is enabled
        bool m_enableWIA;

        // True if a proxy server is used
        bool m_usingProxy;

        // Username to connect to the proxy server
        Aws::WString m_proxyUserName;

        // Password to connect to the proxy server
        Aws::WString m_proxyPassword;
    };
}
}

#endif
#endif
