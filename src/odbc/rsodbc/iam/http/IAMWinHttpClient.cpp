// *NOTE*: This is a class borrowed and modified using the AWS C++ SDK on Github
//  Link: https://github.com/aws/aws-sdk-cpp

#if defined(_WIN32)

#include "IAMWinHttpClient.h"
#include "IAMUtils.h"
#include "RsErrorException.h"
#include "../rs_iam_support.h"

#include <aws/core/Http/HttpRequest.h>
#include <aws/core/http/standard/StandardHttpResponse.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/ratelimiter/RateLimiterInterface.h>
#include <aws/core/http/windows/WinHttpConnectionPoolMgr.h>
#include <aws/core/utils/Array.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <Windows.h>
#include <sstream>
#include <iostream>

using namespace Redshift::IamSupport;

#define DIAG_GENERAL_ERROR 63
#define DIAG_INVALID_AUTH_SPEC 47

// Throw an RsErrorException with the given state key, component id IAM_ERROR, the given message
// id and the given message parameter.
void IAMTHROW1(int key, rs_string id, rs_wstring param)                                                                 
{                                                                                                 
    std::vector<rs_wstring> msgParams;                                                          
    msgParams.push_back(param);                                                                 
    throw RsErrorException(key, IAM_ERROR, id, msgParams);                           
}

// Throw an RsErrorException with state key DIAG_GENERAL_ERROR, component id IAM_ERROR, and the given
// message id and the given message parameters.
void IAMTHROWGEN2(rs_string id, rs_wstring param1, rs_wstring param2)                                                           
{                                                                                                  
    std::vector<rs_wstring> msgParams;   
    msgParams.push_back(param1);                                                                
    msgParams.push_back(param2);                                                                
    throw RsErrorException(                                                          
        DIAG_GENERAL_ERROR,                                                        
        (int)IAM_ERROR,                                                                 
        id,
        msgParams);                                                                
}


using namespace Redshift::IamSupport;
using namespace Aws::Client;
using namespace Aws::Http;
using namespace Aws::Http::Standard;
using namespace Aws::Utils;

namespace
{
    static const uint32_t HTTP_REQUEST_WRITE_BUFFER_LENGTH = 8192;

    const wchar_t* USER_AGENT = L"Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.2; WOW64; Trident/6.0)";
    const wchar_t* ADFS_RESOURCE = L"/adfs/ls/IdpInitiatedSignOn.aspx?loginToRp=";

    const size_t MAX_HTTP_ATTEMPT = 5;

    /// @brief  Retrieves the content of the Http response and save as strings
    /// 
    /// @param  in_requestHandle         WinHttp request handle
    /// 
    /// @return Http response body in string format
    static std::string GetContent(HINTERNET in_requestHandle)
    {
        std::string content;

        DWORD available;
        char buffer[1024];
        while (WinHttpQueryDataAvailable(in_requestHandle, &available) && available)
        {
            DWORD chunk = (std::min)(available, static_cast<DWORD>(sizeof(buffer) - 1));

            DWORD read;
            if (WinHttpReadData(in_requestHandle, buffer, chunk, &read))
            {
                content.append(buffer, read);
            }
        }

        return content;
    }

    /// @brief  Retrieves the headers of the Http response the save as wstring
    /// 
    /// @param  in_requestHandle         WinHttp request handle
    /// 
    /// @return Http response headers in wstring format
    static std::wstring GetHeaders(HINTERNET in_requestHandle)
    {
        DWORD size = 0;
        std::wstring headers;

        BOOL success = WinHttpQueryHeaders(
            in_requestHandle,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            NULL,
            &size,
            WINHTTP_NO_HEADER_INDEX);

        if (!success && (ERROR_INSUFFICIENT_BUFFER == GetLastError()))
        {
            headers.resize(size / sizeof(headers[0]) + 1);
            if (WinHttpQueryHeaders(
                in_requestHandle,
                WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &headers[0],
                &size,
                WINHTTP_NO_HEADER_INDEX))
            {
                headers.resize(wcslen(&headers[0]));
            }
        }

        return headers;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IAMWinHttpClient::IAMWinHttpClient(const ClientConfiguration& config, bool in_enableWIA) :
    WinSyncHttpClient(),
    m_verifySSL(config.verifySSL),
    m_enableWIA(in_enableWIA)
{
    DWORD winhttpFlags = WINHTTP_ACCESS_TYPE_NO_PROXY;

	if (config.followRedirects == FollowRedirectsPolicy::NEVER ||
		(config.followRedirects == FollowRedirectsPolicy::DEFAULT && config.region == Aws::Region::AWS_GLOBAL))
	{
		m_allowRedirects = false;
	}
	else
	{
		m_allowRedirects = true;
	}

    m_usingProxy = !config.proxyHost.empty();
    const char* proxyHosts = nullptr;
    Aws::String strProxyHosts;
    Aws::WString proxyString;

    if (m_usingProxy)
    {
        const char* const proxySchemeString = Aws::Http::SchemeMapper::ToString(config.proxyScheme);

        winhttpFlags = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        Aws::StringStream ss;
        const char* schemeString = Aws::Http::SchemeMapper::ToString(config.scheme);
        ss << StringUtils::ToUpper(schemeString) << "=" << proxySchemeString << "://" << config.proxyHost << ":" << config.proxyPort;
        strProxyHosts.assign(ss.str());
        proxyHosts = strProxyHosts.c_str();

        proxyString = StringUtils::ToWString(proxyHosts);

        m_proxyUserName = StringUtils::ToWString(config.proxyUserName.c_str());
        m_proxyPassword = StringUtils::ToWString(config.proxyPassword.c_str());
    }

    Aws::WString openString = StringUtils::ToWString(config.userAgent.c_str());

    if (m_usingProxy)
    {
        SetOpenHandle(WinHttpOpen(openString.c_str(), winhttpFlags, proxyString.c_str(), nullptr, 0));
    }
    else
    {
        SetOpenHandle(WinHttpOpen(openString.c_str(), winhttpFlags, nullptr, nullptr, 0));
    }
    

    SetConnectionPoolManager(Aws::New<WinHttpConnectionPoolMgr>(
        GetLogTag(),
        GetOpenHandle(),
        config.maxConnections,
        config.requestTimeoutMs,
        config.connectTimeoutMs));

    if (!WinHttpSetTimeouts(GetOpenHandle(), config.connectTimeoutMs, config.connectTimeoutMs, -1, config.requestTimeoutMs))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("IAMWinHttpClient: Failed to set HTTP timeout.");
    }

    if (m_verifySSL)
    {
        //disable insecure tls protocols, otherwise you might as well turn ssl verification off.
        DWORD flags = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 |
            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

        if (!WinHttpSetOption(GetOpenHandle(), WINHTTP_OPTION_SECURE_PROTOCOLS, &flags, sizeof(flags)))
        {
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "IAMWinHttpClient: Failed setting secure crypto protocols with error code: " +
                std::to_string(GetLastError()));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* IAMWinHttpClient::OpenRequest(
    const std::shared_ptr<Aws::Http::HttpRequest>& request,
    void* connection, 
    const Aws::StringStream& ss) const
{
    LPCWSTR accept[2] = { nullptr, nullptr };

    DWORD requestFlags = WINHTTP_FLAG_REFRESH |
        (request->GetUri().GetScheme() == Scheme::HTTPS ? WINHTTP_FLAG_SECURE : 0);

    Aws::WString acceptHeader(L"*/*");

    if (request->HasHeader(Aws::Http::ACCEPT_HEADER))
    {
        acceptHeader = Aws::Utils::StringUtils::ToWString(request->GetHeaderValue(Aws::Http::ACCEPT_HEADER).c_str());
    }

    accept[0] = acceptHeader.c_str();

    Aws::WString wss = StringUtils::ToWString(ss.str().c_str());

	// WinHttpOpenRequest uses a connection handle to create a request handle
    HINTERNET hHttpRequest = WinHttpOpenRequest(
        connection, 
        StringUtils::ToWString(HttpMethodMapper::GetNameForHttpMethod(request->GetMethod())).c_str(),
        wss.c_str(), 
        nullptr, 
        nullptr, 
        accept, 
        requestFlags);

    //DISABLE_FEATURE settings need to be made after OpenRequest but before SendRequest
    if (!m_allowRedirects)
    {
        requestFlags = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(hHttpRequest, WINHTTP_OPTION_DISABLE_FEATURE, &requestFlags, sizeof(requestFlags));
    }

    //add proxy auth credentials to everything using this handle.
    if (m_usingProxy)
    {
        if (!m_proxyUserName.empty() && !WinHttpSetOption(hHttpRequest, WINHTTP_OPTION_PROXY_USERNAME, (LPVOID)m_proxyUserName.c_str(), (DWORD)m_proxyUserName.length()))
        {
            IAMUtils::ThrowConnectionExceptionWithInfo("Failed setting username for proxy with error code: ", std::to_string(GetLastError()));
        }

        if (!m_proxyPassword.empty() && !WinHttpSetOption(hHttpRequest, WINHTTP_OPTION_PROXY_PASSWORD, (LPVOID)m_proxyPassword.c_str(), (DWORD)m_proxyPassword.length()))
        {
            IAMUtils::ThrowConnectionExceptionWithInfo("Failed setting password for proxy with error code: ", std::to_string(GetLastError()));
        }

    }

    if (!m_verifySSL)
    {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        if (!WinHttpSetOption(hHttpRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags)))
        {
            IAMUtils::ThrowConnectionExceptionWithInfo(
                "IAMWinHttpClient: Failed to turn off self-signed certificate verification. " +
                std::to_string(GetLastError()));
        };
    }

    if (m_enableWIA)
    {
        /* Authenticate using NTLM protocol for Windows Integrated Authentication.
        WinHttp Options Flag API:
        https://msdn.microsoft.com/en-us/library/windows/desktop/aa384066(v=vs.85).aspx
        */
        DWORD logonPolicy = WINHTTP_AUTOLOGON_SECURITY_LEVEL_LOW;
        if (!WinHttpSetOption(hHttpRequest,
            WINHTTP_OPTION_AUTOLOGON_POLICY,
            &logonPolicy,
            sizeof(logonPolicy)))
        {
            IAMUtils::ThrowConnectionExceptionWithInfo("IAMWinHttpClient: Failed to set AutoLogon policy.");
        }

        if (!WinHttpSetCredentials(hHttpRequest, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_NTLM, NULL, NULL, NULL))
        {
            IAMUtils::ThrowConnectionExceptionWithInfo("IAMWinHttpClient: Failed to set WINHTTP_AUTH_SCHEME_NEGOTIATE.");
        };
    }

    return hHttpRequest;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMWinHttpClient::DoAddHeaders(void* hHttpRequest, Aws::String& headerStr) const
{
    Aws::WString wHeaderString = StringUtils::ToWString(headerStr.c_str());

    if (!WinHttpAddRequestHeaders(hHttpRequest,
        wHeaderString.c_str(),
        (DWORD)wHeaderString.length(),
        WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD))
    {
        IAMUtils::ThrowConnectionExceptionWithInfo("Failed to add HTTP request headers with error code: " + GetLastError());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t IAMWinHttpClient::DoWriteData(void* hHttpRequest, 
	char* streamBuffer, 
	uint64_t bytesRead,
	bool isChunked) const
{
	DWORD bytesWritten = 0;
	uint64_t totalBytesWritten = 0;
	const char CRLF[] = "\r\n";

	if (isChunked)
	{
		Aws::String chunkSizeHexString = StringUtils::ToHexString(bytesRead) + CRLF;

		if (!WinHttpWriteData(hHttpRequest, chunkSizeHexString.c_str(), (DWORD)chunkSizeHexString.size(), &bytesWritten))
		{
			return totalBytesWritten;
		}
		totalBytesWritten += bytesWritten;
		if (!WinHttpWriteData(hHttpRequest, streamBuffer, (DWORD)bytesRead, &bytesWritten))
		{
			return totalBytesWritten;
		}
		totalBytesWritten += bytesWritten;
		if (!WinHttpWriteData(hHttpRequest, CRLF, (DWORD)(sizeof(CRLF) - 1), &bytesWritten))
		{
			return totalBytesWritten;
		}
		totalBytesWritten += bytesWritten;
	}
	else
	{
		if (!WinHttpWriteData(hHttpRequest, streamBuffer, (DWORD)bytesRead, &bytesWritten))
		{
			return totalBytesWritten;
		}
		totalBytesWritten += bytesWritten;
	}

	return totalBytesWritten;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t IAMWinHttpClient::FinalizeWriteData(void* hHttpRequest) const
{
	DWORD bytesWritten = 0;
	const char trailingCRLF[] = "0\r\n\r\n";
	if (!WinHttpWriteData(hHttpRequest, trailingCRLF, (DWORD)(sizeof(trailingCRLF) - 1), &bytesWritten))
	{
		return 0;
	}

	return bytesWritten;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMWinHttpClient::DoReceiveResponse(void* hHttpRequest) const
{
    return (WinHttpReceiveResponse(hHttpRequest, nullptr) != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMWinHttpClient::DoQueryHeaders(void* hHttpRequest, std::shared_ptr<HttpResponse>& response, Aws::StringStream& ss, uint64_t& read) const
{
    wchar_t dwStatusCode[256];
    DWORD dwSize = sizeof(dwStatusCode);
    wmemset(dwStatusCode, 0, static_cast<size_t>(dwSize / sizeof(wchar_t)));

    WinHttpQueryHeaders(hHttpRequest, WINHTTP_QUERY_STATUS_CODE, nullptr, &dwStatusCode, &dwSize, 0);
    int responseCode = _wtoi(dwStatusCode);
    response->SetResponseCode(static_cast<HttpResponseCode>(responseCode));

    wchar_t contentTypeStr[1024];
    dwSize = sizeof(contentTypeStr);
    wmemset(contentTypeStr, 0, static_cast<size_t>(dwSize / sizeof(wchar_t)));

    WinHttpQueryHeaders(hHttpRequest, WINHTTP_QUERY_CONTENT_TYPE, nullptr, &contentTypeStr, &dwSize, 0);
    if (contentTypeStr[0] != NULL)
    {
        Aws::String contentStr = StringUtils::FromWString(contentTypeStr);
        response->SetContentType(contentStr);
    }

    BOOL queryResult = false;
    WinHttpQueryHeaders(
        hHttpRequest, 
        WINHTTP_QUERY_RAW_HEADERS_CRLF, 
        WINHTTP_HEADER_NAME_BY_INDEX, 
        nullptr, 
        &dwSize, 
        WINHTTP_NO_HEADER_INDEX);

    //I know it's ugly, but this is how MSFT says to do it so....
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        Aws::Utils::Array<wchar_t> headerRawString(dwSize / sizeof(wchar_t));

        queryResult = WinHttpQueryHeaders(
            hHttpRequest, 
            WINHTTP_QUERY_RAW_HEADERS_CRLF, 
            WINHTTP_HEADER_NAME_BY_INDEX, 
            headerRawString.GetUnderlyingData(), 
            &dwSize, 
            WINHTTP_NO_HEADER_INDEX);

        if (queryResult)
        {
            Aws::String headers(StringUtils::FromWString(headerRawString.GetUnderlyingData()));
            ss << std::move(headers);
            read = dwSize;
        }
    }

    return queryResult == TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMWinHttpClient::DoSendRequest(void* hHttpRequest) const
{
    return (WinHttpSendRequest(hHttpRequest, NULL, NULL, 0, 0, 0, NULL) != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMWinHttpClient::DoReadData(void* hHttpRequest, char* body, uint64_t size, uint64_t& read) const
{
    return (WinHttpReadData(hHttpRequest, body, (DWORD)size, (LPDWORD)&read) != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* IAMWinHttpClient::GetClientModule() const
{
    return GetModuleHandle(TEXT("winhttp.dll"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMWinHttpClient::SetEnableWIA(bool in_enableWIA)
{
    m_enableWIA = in_enableWIA;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMWinHttpClient::~IAMWinHttpClient()
{
    /* Do nothing */
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMWinHttpClient::SendHttpRequestWithWIA(
    const rs_wstring& in_host,
    short in_port,
    bool in_verifySSL,
    const rs_wstring& in_proxyUsername,
    const rs_wstring& in_proxyPassword,
    const rs_wstring& in_loginToRp
    )
{
    // Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/aa383880(v=vs.85).aspx
    HINTERNET sessionHandle = NULL,
        connectHandle = NULL,
        requestHandle = NULL;

    DWORD dwProxyAuthScheme = 0;

    size_t httpRequestAttempt = 0;
    rs_string httpContentBody;
    bool m_usingProxy = !IAMUtils::isEmpty(in_proxyUsername);

    if (m_usingProxy)
    {
        sessionHandle = WinHttpOpen(
            USER_AGENT,
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
    }
    else
    {
        sessionHandle = WinHttpOpen(
            USER_AGENT,
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
    }

    if (!sessionHandle)
    {
        return httpContentBody;
    }

    if (in_verifySSL)
    {
        //disable insecure tls protocols, otherwise you might as well turn ssl verification off.
        DWORD flags = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 |
            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
            WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;

        WinHttpSetOption(sessionHandle, WINHTTP_OPTION_SECURE_PROTOCOLS, &flags, sizeof(flags));
    }

    // Set timeouts
    WinHttpSetTimeouts(sessionHandle, DEFAULT_TIMEOUT, DEFAULT_TIMEOUT, DEFAULT_TIMEOUT, DEFAULT_TIMEOUT);

    connectHandle = WinHttpConnect(
        sessionHandle,
        in_host.c_str(), // GetAsPlatformWString()
        in_port,
        0);

    if (connectHandle)
    {
        LPCWSTR accept[] = { L"*/*", NULL };

        rs_wstring adfs_url = ADFS_RESOURCE + in_loginToRp;
        std::wstring adfs_url_wstring = adfs_url; // .GetAsPlatformWString()
        requestHandle = WinHttpOpenRequest(
            connectHandle,
            L"GET",
            const_cast<wchar_t*>(adfs_url_wstring.c_str()),
            NULL,
            WINHTTP_NO_REFERER,
            accept,
            WINHTTP_FLAG_REFRESH | WINHTTP_FLAG_SECURE);
    }

    if (requestHandle)
    {
        if (!in_verifySSL)
        {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
            WinHttpSetOption(requestHandle, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
        }
    }

    bool isDone = false;
    bool isUnauthorized = false;
    BOOL success = FALSE;

    while (++httpRequestAttempt < MAX_HTTP_ATTEMPT && requestHandle && !isDone)
    {
        if (0 != dwProxyAuthScheme && m_usingProxy)
        {
            success = WinHttpSetCredentials(
                requestHandle,
                WINHTTP_AUTH_TARGET_PROXY,
                dwProxyAuthScheme,
                in_proxyUsername.c_str(), // GetAsPlatformWString().
                in_proxyPassword.c_str(), // GetAsPlatformWString().
                NULL);
        }


        success = WinHttpSendRequest(
            requestHandle,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0);

        if (success)
        {
            success = WinHttpReceiveResponse(requestHandle, 0);
        }

		if (!success)
		{
			if (ERROR_WINHTTP_RESEND_REQUEST == GetLastError())
			{
				continue;
			}
			else if (ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED == GetLastError())
			{
				// [PGODBC-1351] If the server requests the certificate, but does not require it,
				// try indicate that it does not have a certificate
				WinHttpSetOption(
					requestHandle,
					WINHTTP_OPTION_CLIENT_CERT_CONTEXT,
					WINHTTP_NO_CLIENT_CERT_CONTEXT,
					0);

				continue;
			}
		}


        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        if (success)
        {
            success = WinHttpQueryHeaders(
                requestHandle,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                NULL,
                &statusCode,
                &size,
                NULL);
        }

        if (success)
        {
            httpContentBody = GetContent(requestHandle);

            HttpResponseCode responseCode = static_cast<HttpResponseCode>(statusCode);
            if (HttpResponseCode::UNAUTHORIZED == responseCode)
            {
                isUnauthorized = true;

                DWORD supportedSchemes;
                DWORD firstScheme;
                DWORD target;

                success = WinHttpQueryAuthSchemes(requestHandle, &supportedSchemes, &firstScheme, &target);
                if (success)
                {
                    DWORD scheme = 0;
					// [PGODBC-1463] Check and use WINHTTP_AUTH_SCHEME_NEGOTIATE if supported by the server
					if (supportedSchemes & WINHTTP_AUTH_SCHEME_NEGOTIATE)
					{
						scheme = WINHTTP_AUTH_SCHEME_NEGOTIATE;
					}
					else
                    if (supportedSchemes & WINHTTP_AUTH_SCHEME_NTLM)
                    {
                        scheme = WINHTTP_AUTH_SCHEME_NTLM;
                    }
                    else
                    {
                        isDone = true;
						continue;
                    }

					DWORD flags = WINHTTP_AUTOLOGON_SECURITY_LEVEL_LOW;

					success = WinHttpSetOption(requestHandle, WINHTTP_OPTION_AUTOLOGON_POLICY, &flags, sizeof(flags))
						&& WinHttpSetCredentials(requestHandle, target, scheme, NULL, NULL, NULL);
                }
            }
            else
            {
                isUnauthorized = false;
                isDone = true;
            }
        }

        if (!success)
        {
            isDone = true;
        }
    }

    if (!success)
    {
        IAMTHROWGEN2("IAMHttpRequestError", in_host, IAMUtils::GetLastErrorText());
    }

    if (isUnauthorized)
    {
        IAMTHROW1(DIAG_INVALID_AUTH_SPEC, "IAMHttpUnauthorizedError", in_host);
    }

    if (requestHandle)
    {
        WinHttpCloseHandle(requestHandle);
    }
    if (connectHandle)
    {
        WinHttpCloseHandle(connectHandle);
    }
    if (sessionHandle)
    {
        WinHttpCloseHandle(sessionHandle);
    }

    return httpContentBody;
}

#endif
