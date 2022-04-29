#include "IAMExternalCredentialsProvider.h"
#include "IAMUtils.h"

using namespace Redshift::IamSupport;
using namespace Aws::Utils;

namespace
{
// Buffer size for the stdout buffer
static const size_t BUFSIZE =  1024;
// Error message for any executable error
static const rs_wstring EXECUTE_ERROR = L"Failed to execute the external plugin. ";

}

#ifdef _WIN32
// EXECUTE_ERROR,
////////////////////////////////////////////////////////////////////////////////////////////////////
// Global macros
////////////////////////////////////////////////////////////////////////////////////////////////////
#define CHECKERROR(result) \
    if (!(result)) { \
        IAMUtils::ThrowConnectionExceptionWithInfo( \
                 (rs_wstring)(IAMUtils::GetLastErrorText())); \
    }

////////////////////////////////////////////////////////////////////////////////////////////////////
WindowsExecutable::WindowsExecutable(
    const rs_wstring& in_exePath,
    const rs_wstring& in_args) :
    m_exePath(in_exePath),
    m_args(in_args),
    m_stdOutputReadHandle(NULL),
    m_stdOutputWriteHandle(NULL)
{
    ZeroMemory(&m_processInfo, sizeof(PROCESS_INFORMATION));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowsExecutable::SetArgs(const rs_string& in_args)
{
    m_args = IAMUtils::convertStringToWstring(in_args);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_wstring WindowsExecutable::GetArgs() const
{
    return m_args;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowsExecutable::Add(const rs_wstring& in_arg)
{
    if (IAMUtils::isEmpty(m_args))
    {
        m_args = in_arg;
    }
    else
    {
        m_args += (L" " + in_arg);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////
WindowsExecutable::~WindowsExecutable() 
{
    if (NULL != m_processInfo.hThread)
    {
        CloseHandle(m_processInfo.hThread);
    }

    if (NULL != m_processInfo.hProcess)
    {
        CloseHandle(m_processInfo.hProcess);
    }

    if (NULL != m_stdOutputReadHandle)
    {
        CloseHandle(m_stdOutputReadHandle);
    }

    if (NULL != m_stdOutputWriteHandle)
    {
        CloseHandle(m_stdOutputWriteHandle);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string WindowsExecutable::Run()
{
    STARTUPINFO startInfo;
    SECURITY_ATTRIBUTES securityAttrInfo;

    ZeroMemory(&startInfo, sizeof(STARTUPINFO));
    ZeroMemory(&securityAttrInfo, sizeof(SECURITY_ATTRIBUTES));

    bool isSuccess = false;

    // initialize startinfo buffer
    securityAttrInfo.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttrInfo.bInheritHandle = true;
    securityAttrInfo.lpSecurityDescriptor = NULL;

    isSuccess = CreatePipe(&m_stdOutputReadHandle, &m_stdOutputWriteHandle, &securityAttrInfo, 0);
    CHECKERROR(isSuccess);

    isSuccess = SetHandleInformation(m_stdOutputReadHandle, HANDLE_FLAG_INHERIT, 0);
    CHECKERROR(isSuccess);

    startInfo.cb = sizeof(STARTUPINFO);
    startInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startInfo.dwFlags = STARTF_USESTDHANDLES;
    startInfo.wShowWindow = SW_HIDE;
    startInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startInfo.hStdOutput = m_stdOutputWriteHandle;
    startInfo.hStdError = m_stdOutputWriteHandle;

    isSuccess = CreateProcess(
        IAMUtils::convertToUTF8(m_exePath).c_str(), // GetAsPlatformWString()
        const_cast<TCHAR*>(IAMUtils::convertToUTF8(m_args).c_str()), // GetAsPlatformWString
        NULL,
        NULL,
        true,
        0,
        NULL,
        NULL,
        &startInfo,
        &m_processInfo);

    CHECKERROR(isSuccess);

    CloseHandle(m_stdOutputWriteHandle); // Vital: close output write buffer 
    m_stdOutputWriteHandle = NULL;

    DWORD numBytesRead;
    rs_string samlResponse; // the output saml response are saved into this variable
    char outBuffer[BUFSIZE]; // the stdout output buffer

    while (true)
    {
        isSuccess = ReadFile(m_stdOutputReadHandle, outBuffer, BUFSIZE - 1, &numBytesRead, NULL);
        if (!isSuccess || 0 == numBytesRead) {
            break;
        }
        outBuffer[numBytesRead] = '\0';
        samlResponse += outBuffer;
    }

    return samlResponse;
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMExternalCredentialsProvider::IAMExternalCredentialsProvider(
    RsLogger* in_log,
    const IAMConfiguration& in_config,
    const std::map<rs_string, rs_string>& in_argsMap) :
    IAMSamlPluginCredentialsProvider(in_log, in_config, in_argsMap)
{
    RS_LOG(m_log)("IAMExternalCredentialsProvider::IAMExternalCredentialsProvider");

    InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMExternalCredentialsProvider::InitArgumentsMap()
{
    RS_LOG(m_log)("IAMExternalCredentialsProvider::InitArgumentsMap");
    IAMPluginCredentialsProvider::InitArgumentsMap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void IAMExternalCredentialsProvider::ValidateArgumentsMap()
{
    RS_LOG(m_log)("IAMExternalCredentialsProvider::ValidateArgumentsMap");

    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMExternalCredentialsProvider::GetSamlAssertion()
{
    RS_LOG(m_log)("IAMExternalCredentialsProvider::GetSamlAssertion");

    return RunExecutable();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
rs_string IAMExternalCredentialsProvider::RunExecutable()
{
    RS_LOG(m_log)("IAMExternalCredentialsProvider::RunExecutable");
#ifdef _WIN32
    rs_string pluginName = m_argsMap[IAM_KEY_PLUGIN_NAME];
    WindowsExecutable executable(IAMUtils::convertFromUTF8(pluginName));
    
    // Also add all arguments read from AWS Profile to the command line args, with format key=val
    for (const auto& it : m_argsMap)
    {
        rs_string argument = it.first + "=" + it.second;
        executable.Add(IAMUtils::convertFromUTF8(argument));
    }
    // Execute the executable
    return executable.Run();

#else
    return rs_string();
#endif 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IAMExternalCredentialsProvider::~IAMExternalCredentialsProvider()
{
    /* Do nothing */
}
