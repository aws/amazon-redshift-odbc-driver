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

#include <rslog.h>

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
        // Begin Code copy
        // Taken from:
        // https://github.com/aws/aws-sdk-cpp/blob/main/src/aws-cpp-sdk-core/source/http/windows/WinHttpSyncHttpClient.cpp
        // until we find a solution
        void AzWinHttpLogLastError(const char* FuncName) const
        {
        #define AZ_WINHTTP_ERROR(win_http_error) \
            {win_http_error, #win_http_error}
        #define AZ_WINHTTP_ERROR_FALLBACK(win_http_error, value) \
            {value, #win_http_error}

            static const std::pair<DWORD, const char*> WIN_HTTP_ERRORS[] = {
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_OUT_OF_HANDLES),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_TIMEOUT),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INTERNAL_ERROR),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INVALID_URL),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_UNRECOGNIZED_SCHEME),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_NAME_NOT_RESOLVED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INVALID_OPTION),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_OPTION_NOT_SETTABLE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SHUTDOWN),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_LOGIN_FAILURE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_OPERATION_CANCELLED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INCORRECT_HANDLE_STATE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CANNOT_CONNECT),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CONNECTION_ERROR),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_RESEND_REQUEST),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CANNOT_CALL_AFTER_SEND),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CANNOT_CALL_AFTER_OPEN),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_HEADER_NOT_FOUND),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INVALID_SERVER_RESPONSE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INVALID_HEADER),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_INVALID_QUERY_REQUEST),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_HEADER_ALREADY_EXISTS),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_REDIRECT_FAILED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_UNHANDLED_SCRIPT_TYPE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SCRIPT_EXECUTION_ERROR),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_NOT_INITIALIZED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_FAILURE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_CERT_DATE_INVALID),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_CERT_CN_INVALID),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_INVALID_CA),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_CERT_REV_FAILED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_CHANNEL_ERROR),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_INVALID_CERT),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_CERT_REVOKED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_AUTODETECTION_FAILED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_HEADER_COUNT_EXCEEDED),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_HEADER_SIZE_OVERFLOW),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_RESPONSE_DRAIN_OVERFLOW),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CLIENT_CERT_NO_PRIVATE_KEY),
                    AZ_WINHTTP_ERROR(ERROR_WINHTTP_CLIENT_CERT_NO_ACCESS_PRIVATE_KEY),
                    AZ_WINHTTP_ERROR_FALLBACK(ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED_PROXY, (WINHTTP_ERROR_BASE + 187)),
                    AZ_WINHTTP_ERROR_FALLBACK(ERROR_WINHTTP_SECURE_FAILURE_PROXY, (WINHTTP_ERROR_BASE + 188)),
                    AZ_WINHTTP_ERROR_FALLBACK(ERROR_WINHTTP_RESERVED_189, (WINHTTP_ERROR_BASE + 189)),
                    AZ_WINHTTP_ERROR_FALLBACK(ERROR_WINHTTP_HTTP_PROTOCOL_MISMATCH, (WINHTTP_ERROR_BASE + 190)),
                    AZ_WINHTTP_ERROR_FALLBACK(ERROR_WINHTTP_GLOBAL_CALLBACK_FAILED, (WINHTTP_ERROR_BASE + 191)),
                    AZ_WINHTTP_ERROR_FALLBACK(ERROR_WINHTTP_FEATURE_DISABLED, (WINHTTP_ERROR_BASE + 192)),
                    AZ_WINHTTP_ERROR(ERROR_INVALID_PARAMETER),
                    AZ_WINHTTP_ERROR(ERROR_NOT_ENOUGH_MEMORY)
            };
        #undef AZ_WINHTTP_ERROR
        #undef AZ_WINHTTP_ERROR_FALLBACK

            static const size_t WIN_HTTP_ERRORS_SZ = sizeof(WIN_HTTP_ERRORS) / sizeof(WIN_HTTP_ERRORS[0]);

            const auto lastError = GetLastError();
            size_t errorIdx = 0;
            for(; errorIdx < WIN_HTTP_ERRORS_SZ; ++errorIdx)
            {
                if(WIN_HTTP_ERRORS[errorIdx].first == lastError)
                {
                    RS_LOG_ERROR("WinHttp", "Failed to %s with an error code: ", FuncName, WIN_HTTP_ERRORS[errorIdx].second);
                    break;
                }
            }
            if (errorIdx == WIN_HTTP_ERRORS_SZ)
            {
                RS_LOG_ERROR("WinHttp", "Failed to %s with an error code: ", FuncName, lastError);
            }
        }
        template<typename WinHttpFunc, typename... Args>
        bool AzCallWinHttp(const char* FuncName, WinHttpFunc func, Args &&... args) const
        {
            bool success = func(std::forward<Args>(args)...);
            if (!success)
            {
                AzWinHttpLogLastError(FuncName);
            }
            return success;
        }

        bool DoQueryDataAvailable(void *hHttpRequest,
                                  uint64_t &available) const {
            
            return (WinHttpQueryDataAvailable(hHttpRequest,
                                              (LPDWORD)&available) != 0);
        }
        const char* GetActualHttpVersionUsed(void* hHttpRequest) const
        {
            DWORD httpVersion = 0xFFF;
            DWORD ioLen = sizeof(httpVersion);
            this->AzCallWinHttp("WinHttpSetOption", WinHttpQueryOption, hHttpRequest, WINHTTP_OPTION_HTTP_PROTOCOL_USED, &httpVersion, &ioLen);

            switch (httpVersion)
            {
                case 0x0:
                    return "1.1 or 1.0";
                case 0x1:
                    return "2.0";
                case 0x2:
                    return "3.0";
                default:
                    break;
            }
            return "Unknown";
        }
        // End Code copy
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
