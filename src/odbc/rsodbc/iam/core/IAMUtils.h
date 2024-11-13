#ifndef _IAMUTILS_H_
#define _IAMUTILS_H_

#include <vector>

#include "../rs_iam_support.h"

#include <ares.h>
#include <ares_dns.h>
#if defined LINUX
#include <netdb.h>
#endif

#if !defined(_WIN32)
#include <curl/curl.h>
#else
#include <winhttp.h>
#endif
namespace Redshift
{
namespace IamSupport
{
    /// @brief Class containing some of the utility functions 
    class IAMUtils
    {
        enum URLPart{
            SCHEME,
            HOST,
            PATH,
            EXTRAINFO
        };

        // Public Static ==============================================================================
    public:
        /// @brief Tokenize the input connection setting using the delimiter
        ///
        /// @param in_setting       The connection setting to be tokenized
        /// @param in_delimiter     The delimiter used to tokenize the connection setting
        ///
        /// @return A vector that contains all tokenized string from the connection setting in UTF-8 format
        static std::vector<rs_string> TokenizeSetting(
            const rs_wstring& in_setting,
            const rs_wstring& in_delimiter);

        static std::vector<rs_string> TokenizeSetting(
            const rs_string& in_setting,
            const rs_string& in_delimiter);

        /// @brief Convert the string to boolean, currently we consider
        ///                     "true" (case-insensitive ) and "1" as true
        ///
        /// @param in_str       The string to be converted 
        /// 
        /// @return True if the string can be converted to true, else false
        static bool ConvertWStringToBool(const rs_wstring& in_str);

        /// @brief Convert the string to boolean, currently we consider
        ///                     "true" (case-insensitive ) and "1" as true
        ///
        /// @param in_str       The string to be converted 
        /// 
        /// @return True if the string can be converted to true, else false
        static bool ConvertStringToBool(const rs_string& in_str);

        static rs_wstring GetDirectoryPath();

        /// @brief Gets the default ca certificate file path
        ///
        /// @return the default ca certificate file path
        static rs_wstring GetDefaultCaFile();

        /// @brief Gets the default ca certificate file path given directory
        ///
        /// @param in_str       The string to be converted 
        ///
        /// @return the default ca certificate file path
        static rs_wstring GetDefaultCaFile(const rs_string& in_str);

        /// @brief Throw a PGOConnectError exception with error message
        ///
        /// @param in_errorMsg               Error message 
        /// @param in_messageKey             Key of the error message
        static void ThrowConnectionExceptionWithInfo(
            const rs_wstring& in_errorMsg,
            const rs_string& in_messageKey = "IAMConnectionError");

        static void ThrowConnectionExceptionWithInfo(
            const rs_string& in_errorMsg,
            const rs_string& in_messageKey = "IAMConnectionError");
        
        /// @brief Base64url encodes the input
        ///
        /// @param input                     Value we want to base64url encode
        /// @param in_messageKey             Length of the value we want to encode
        static std::string base64urlEncode(const unsigned char* input, size_t length);

        static std::string trim(std::string& str);

        static std::wstring trim(std::wstring& str);

        /// @brief trimming functions
        /// @param s 
        /// @return trimmed rs_string
        static rs_string ltrim(const rs_string &s);
        static rs_string rtrim(const rs_string &s);
        static rs_string rs_trim(const rs_string &s);

        static rs_string ReplaceAll(
            rs_string& io_string,
            const char* in_toReplace,
            size_t in_toReplaceLength,
            char in_toInsert);

       static rs_string ReplaceAll(
            rs_string& io_string,
            const char* in_toReplace,
            char in_toInsert);

       static rs_wstring ReplaceAll(
           rs_wstring& io_string,
           const wchar_t* in_toReplace,
           size_t in_toReplaceLength,
           wchar_t in_toInsert);

      static rs_wstring ReplaceAll(
           rs_wstring& io_string,
           const wchar_t* in_toReplace,
           wchar_t in_toInsert);

       static rs_string convertToUTF8(
             rs_wstring  wstr);

       static rs_wstring  convertFromUTF8(rs_string str);

       static rs_wstring toLower(
             rs_wstring  wstr);

       static bool isEmpty(
             rs_wstring  wstr);

       static bool isEqual(std::wstring  s1, std::wstring  s2, bool caseInsenstive);
       static rs_wstring convertStringToWstring(rs_string  str);
       static rs_wstring convertCharStringToWstring(char *str);

        /// @brief Get the Fully Qualified Domain Name (FQDN) from cname endpoint;
        ///
        /// @param cnameEndpoint 
        rs_string GetAwsRegionFromCname(const std::string& cnameEndpoint);
        rs_string m_awsRegion;
        /// @brief Destructor.

        static void AresCallBack(void* arg, int status, int timeouts, struct hostent* hostent_result);

#ifdef _WIN32
        static std::wstring GetLastErrorText();

        static void WinValidatePart(URL_COMPONENTS &url_components, enum URLPart url_part);

        static void WinValidateUrl(const rs_string& in_url);
#endif // WIN32

#if !defined(_WIN32)
        static void CurlValidatePart(CURL *handle, CURLU *url, CURLUPart part_name);

        static void CurlValidateUrl(const rs_string &in_url);
#endif

        static void ValidateURL(const rs_string & in_url);

    };
}
}

#endif

