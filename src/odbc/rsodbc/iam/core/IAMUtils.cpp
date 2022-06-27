
#include "IAMUtils.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#endif // WIN32

#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include <sstream>

using namespace Redshift::IamSupport;

extern "C" {
  size_t wchar_to_utf8(WCHAR *wszStr,size_t cchLen,char *szStr,size_t cbLen);
  size_t utf8_to_wchar(char *szStr,size_t cbLen,WCHAR *wszStr,size_t cchLen);
  void *rs_calloc(size_t NumOfElements, size_t SizeOfElements);
  void *rs_free(void * block);
  char *getDriverPath();
}

namespace
{
// Internal variables and methods ==================================================================

}

// Public Static ===================================================================================
////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<rs_string> IAMUtils::TokenizeSetting(
    const rs_wstring& in_setting,
    const rs_wstring& in_delimiter)
{
    std::vector<rs_string> res;
    rs_wstring wtemp;
    rs_string temp;

    std::wstringstream wss(in_setting);
    while(std::getline(wss, wtemp, in_delimiter.at(0))) {
        temp = convertToUTF8(wtemp);
        res.push_back(temp);
    }

    return res;
}

std::vector<rs_string> IAMUtils::TokenizeSetting(
    const rs_string& in_setting,
    const rs_string& in_delimiter)
{
    std::vector<rs_string> res;
    rs_string temp;

    std::stringstream wss(in_setting);
    while(std::getline(wss, temp, in_delimiter.at(0)))
        res.push_back(temp);

    return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMUtils::ConvertWStringToBool(const rs_wstring& in_str)
{
    return in_str == L"true"
            || in_str == L"TRUE"
            || in_str == L"1";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IAMUtils::ConvertStringToBool(const rs_string& in_str)
{
    return ConvertWStringToBool(convertStringToWstring(in_str));
}

rs_wstring IAMUtils::GetDirectoryPath()
{
  char *driverPath = getDriverPath();
  rs_wstring rc;
  if (driverPath != NULL && *driverPath != '\0')
  {
    rs_string str = driverPath;
    rc =  convertFromUTF8(str);
  }
  else
    rc =  L"";

  if (driverPath != NULL)
	  free(driverPath);

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_wstring IAMUtils::GetDefaultCaFile()
{
    return GetDirectoryPath() + L"/" + IAM_SSLROOTCERT_NAME;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMUtils::ThrowConnectionExceptionWithInfo(
    const rs_wstring& in_errorMsg,
    const rs_string& in_messageKey)
{
    // Invoke this method for various connection failure exceptions
    std::vector<rs_wstring> msgParams;
    msgParams.push_back(in_errorMsg);
    throw RsErrorException(atoi(SQLSTATE_COMM_LINK_FAILURE), IAM_ERROR, in_messageKey, msgParams);
}

void IAMUtils::ThrowConnectionExceptionWithInfo(
    const rs_string& in_errorMsg,
    const rs_string& in_messageKey)
{
    // Invoke this method for various connection failure exceptions
    std::vector<rs_string> msgParams;
    msgParams.push_back(in_errorMsg);
    throw RsErrorException(atoi(SQLSTATE_COMM_LINK_FAILURE), IAM_ERROR, in_messageKey, msgParams);
}


std::string IAMUtils::trim(std::string& str)
{
    str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
    str.erase(str.find_last_not_of(' ')+1);         //surfixing spaces
    return str;
}

std::wstring IAMUtils::trim(std::wstring& wstr)
{
    wstr.erase(0, wstr.find_first_not_of(' '));       //prefixing spaces
    wstr.erase(wstr.find_last_not_of(' ')+1);         //surfixing spaces
    return wstr;
}

rs_string IAMUtils::ReplaceAll(
    rs_string& io_string,
    const char* in_toReplace,
    char in_toInsert)
{
    return ReplaceAll(io_string, in_toReplace, (!in_toReplace) ? 0 : strlen(in_toReplace), in_toInsert);
}

rs_string IAMUtils::ReplaceAll(
    rs_string& io_string,
    const char* in_toReplace,
    size_t in_toReplaceLength,
    char in_toInsert)
{
    size_t startIndex = 0;
    while (rs_string::npos != (startIndex = io_string.find(in_toReplace, startIndex, in_toReplaceLength)))
    {
        io_string.replace(startIndex, in_toReplaceLength, 1 , in_toInsert);
        ++startIndex;
    }

    return io_string;
}

rs_wstring IAMUtils::ReplaceAll(
    rs_wstring& io_string,
    const wchar_t* in_toReplace,
    wchar_t in_toInsert)
{
    return ReplaceAll(io_string, in_toReplace, (!in_toReplace) ? 0 : wcslen(in_toReplace), in_toInsert);
}

rs_wstring IAMUtils::ReplaceAll(
    rs_wstring& io_string,
    const wchar_t* in_toReplace,
    size_t in_toReplaceLength,
    wchar_t in_toInsert)
{
    size_t startIndex = 0;
    while (rs_string::npos != (startIndex = io_string.find(in_toReplace, startIndex, in_toReplaceLength)))
    {
        io_string.replace(startIndex, in_toReplaceLength, 1 , in_toInsert);
        ++startIndex;
    }

    return io_string;
}

rs_string  IAMUtils::convertToUTF8(rs_wstring wstr) {
//  if(wstr == NULL) return NULL;
  if(wstr.empty()) return "";

  std::string str( wstr.begin(), wstr.end() );
  return str;

/*
  size_t cbLen = wstr.length() * sizeof(WCHAR);
  char *szStr = (char *)rs_calloc(cbLen + 1, sizeof(char));


  size_t len =  wchar_to_utf8((WCHAR *)wstr.c_str(), wstr.length(), szStr, cbLen);

  rs_string str = rs_string(szStr);

  rs_free(szStr);

  return str;
*/
}

rs_wstring  IAMUtils::convertFromUTF8(rs_string str) {
//  if(str == NULL) return NULL;
  if(str.empty()) return L"";

  std::wstring wstr( str.begin(), str.end() );
  return wstr;

/*
  size_t cbLen = str.length();
  WCHAR *wszStr = (WCHAR *)rs_calloc(cbLen + 1, sizeof(WCHAR));


  size_t len =  utf8_to_wchar((char *)str.c_str(), cbLen, wszStr, cbLen * sizeof(WCHAR));

  rs_wstring wstr((wchar_t*)wszStr);

  rs_free(wszStr);

  return wstr;
*/
}

rs_wstring IAMUtils::toLower(rs_wstring  wstr) {
//  if(wstr == NULL) return NULL;
  if(wstr.empty()) return L"";

  rs_wstring lwstr = L"";

  for(wchar_t wc : wstr) {
    lwstr += tolower(wc);
  }

  return lwstr;
}


bool IAMUtils::isEmpty(rs_wstring  wstr) {
  return (wstr.empty()); // wstr == NULL ||
}

bool IAMUtils::isEqual(std::wstring s1, std::wstring  s2, bool caseSensitive)
 {
#ifndef _WIN32
    if (!caseSensitive)
      return wcscasecmp(s1.c_str(), s2.c_str()) == 0;
    else
      return wcscmp(s1.c_str(), s2.c_str()) == 0;
#else
    if (!caseSensitive)
      return wcsicmp(s1.c_str(), s2.c_str()) == 0;
    else
      return wcscmp(s1.c_str(), s2.c_str()) == 0;
#endif
 }

rs_wstring IAMUtils::convertStringToWstring(rs_string  str) {
  return (!str.empty()) ? rs_wstring(str.begin(),str.end()) : L"";
}

rs_wstring IAMUtils::convertCharStringToWstring(char *str) {
  rs_string temp = str;
  return  convertStringToWstring(temp);
}

#ifdef WIN32
//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::wstring IAMUtils::GetLastErrorText()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::wstring(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return IAMUtils::convertStringToWstring(message);
}
#endif // WIN32

