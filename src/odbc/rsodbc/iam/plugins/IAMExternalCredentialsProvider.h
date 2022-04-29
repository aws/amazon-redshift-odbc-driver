#ifndef _IAMEXTERNALCREDENTIALSPROVIDER_H_
#define _IAMEXTERNALCREDENTIALSPROVIDER_H_

#include "IAMSamlPluginCredentialsProvider.h"

#include <map>

#include "../rs_iam_support.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#endif

namespace Redshift
{
namespace IamSupport
{
    /// @brief Executable plugin interface
    class Executable
    {
    public:
        /// @brief Run the executable
        ///
        /// @return Based64 encoded SAML assertion
        virtual rs_string Run() = 0;

        /// @brief Destructor
        virtual ~Executable() {};
    protected:
        /// @brief Constructor
        Executable() {};
    };

#ifdef _WIN32
    /// @brief Executable plugin implementation for Windows
    class WindowsExecutable : public Executable
    {
    public:
        /// @brief Constructor          Construct windows executable object
        ///
        /// @param in_exePath            The path to the executable file
        /// @param in_args               The arguments passed to the executable, default to empty
        WindowsExecutable(
            const rs_wstring& in_exePath,
            const rs_wstring& in_args = rs_wstring());

        /// @brief Set the arguments of the plugin
        ///
        /// @param in_args               The arguments passed to the executable, default to empty
        void SetArgs(const rs_string& in_args);

        /// @brief Add the full arguments to the executable
        ///
        /// @return      The full arguments list, separately using space
        rs_wstring GetArgs() const;

        /// @brief Run the executable
        ///
        /// @return Base64 encoded SAML assertion
        rs_string Run() override;

        /// @brief Add the argument to the executable
        ///
        /// @param in_arg      The argument to be added
        virtual void Add(const rs_wstring& in_arg);

        /// @brief Destructor
        virtual ~WindowsExecutable();

    private:
        // Path to the executable file
        rs_wstring m_exePath;

        // Arguments passed to the .exe, separately using spaces. (e.g., input1 input2 input3)
        rs_wstring m_args;

        // Process information about the running executable
        PROCESS_INFORMATION m_processInfo;

        // Std output handle for read, used by driver
        HANDLE  m_stdOutputReadHandle;

        // Std output handle for write,used by the executable
        HANDLE  m_stdOutputWriteHandle;
    };
#endif

    /// @brief IAMPluginCredentialsProvider implementation class.
    ///        Retrieves AWSCredentials using External plug-in.
    class IAMExternalCredentialsProvider : public IAMSamlPluginCredentialsProvider
    {
    public:
        /// @brief Constructor        Construct credentials provider using argument map
        ///
        /// @param in_log                The logger. (NOT OWN)
        /// @param in_config             The IAM Connection Configuration
        /// @param in_argsMap            Optional arguments map passed to the credentials provider
        explicit IAMExternalCredentialsProvider(
            RsLogger* in_log,
            const IAMConfiguration& in_config = IAMConfiguration(),
            const std::map<rs_string, rs_string>& in_argsMap
            = std::map<rs_string, rs_string>());

        /// Initialize default values for argument map
        /// 
        /// @exception ErrorException if required arguments do not exist
        void InitArgumentsMap() override;

        /// @brief  Validate if the arguments map contains all required arguments
        ///         that later will be used by plugin. E.g., app_id in Okta
        /// 
        /// @exception ErrorException with the error message indicated,
        ///            If any required arguments are missing.
        void ValidateArgumentsMap() override;

        /// @brief  Get saml assertion from given connection settings
        /// 
        /// @return Saml assertion
        rs_string GetSamlAssertion() override;

        /// @brief  Gets Saml assertion by running an external executable program
        /// 
        /// @return The result from running the executable program
        /// 
        /// @exception ErrorException if failed to run the executable
        rs_string RunExecutable();

        /// @brief Destructor
        ~IAMExternalCredentialsProvider();

    private:
        /// @brief Disabled assignment operator to avoid warning.
        IAMExternalCredentialsProvider & operator=(const IAMExternalCredentialsProvider& in_ExternalProvider);
    };
}
}

#endif
