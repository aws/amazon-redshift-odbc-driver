#include <cstdarg>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <typeinfo>
#include "rslog.h"

#include <aws/core/Aws.h>
#include <aws/core/utils/FileSystemUtils.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/memory/stl/AWSString.h>

namespace AwsLogging = Aws::Utils::Logging;
namespace internal {
    
static std::shared_ptr<RS_LOG_VARS> gRslogSettings;
static RS_LOG_VARS *gRslogSettingsPtr = nullptr;

// Cache PID to avoid repeated getpid() calls
static const pid_t cachedPid = getpid();

void resetGlobalLogVars() {
    gRslogSettings.reset();
    gRslogSettingsPtr = nullptr;
}

RS_LOG_VARS *getGlobalLogVars() {
    if (!gRslogSettingsPtr) {
        gRslogSettings = std::make_shared<RS_LOG_VARS>();
        gRslogSettings->iTraceLevel = 0;
        gRslogSettings->szTraceFile[0] = '\0';
        gRslogSettings->isInitialized = 0;
        gRslogSettingsPtr = gRslogSettings.get();
    }
    return gRslogSettingsPtr;
}

// Logger system manager. Its scope is local to ODBC log lines only.
struct RsLogManager {
    typedef std::shared_ptr<AwsLogging::LogSystemInterface> LogInterfacePtr;

  private:
    LogInterfacePtr logSystem = nullptr;

  protected:
    RS_LOG_VARS *rsLogVars = nullptr;
    // Override filename and stream creation of the Logger.
    // The original logic we are overriding is in AWS SDK.
    std::shared_ptr<Aws::OFStream>
    MakeDefaultLogFile(const Aws::String &filename) {

        auto createOFStream =
            [&](const Aws::String &filename) -> std::shared_ptr<Aws::OFStream> {
            auto res = Aws::MakeShared<Aws::OFStream>(
                "DefaultLogSystem", filename.c_str(),
                Aws::OFStream::out | Aws::OFStream::app);
            if (false == (res && res->good())) {
                std::cerr << "Attempt to create file stream from " << filename
                          << " failed." << std::endl;
            }
            return res;
        };

        auto findFileFromPath =
            [&](const Aws::String &path) -> const Aws::String {
            auto res =
                Aws::Utils::PathUtils::GetFileNameFromPathWithExt(filename);
            if (res.empty()) {
                std::cerr << "Attempt to extract file name from path "
                          << filename << " failed." << std::endl;
            }
            return res;
        };

        auto getTempPath = [&]() -> const Aws::String {
            const char *pPath = std::getenv("TMPDIR");
            return std::string((!pPath || *pPath == '\0') ? "/tmp" : pPath);
        };

        // begin
        auto res = createOFStream(filename);

        if (res && res->good()) {
            return res;
        }
        // retry
        auto newFilename = getTempPath() + findFileFromPath(filename);
        return createOFStream(newFilename);
    }
    LogInterfacePtr initLogInterface(int iTraceLevel, const char *szTraceFile) {
        return Aws::MakeShared<AwsLogging::DefaultLogSystem>(
            "ODBC2LOG", (AwsLogging::LogLevel)(iTraceLevel),
            MakeDefaultLogFile(Aws::String(szTraceFile ? szTraceFile : "")));
    }

  public:
    RsLogManager() { rsLogVars = getGlobalLogVars(); }

    virtual void initializeAWSLogging(RS_LOG_VARS *rsLogVars_ = nullptr) {
        rsLogVars_ = rsLogVars_ ? rsLogVars_ : rsLogVars;
        if (!rsLogVars_) {
            return;
        }
        logSystem =
            initLogInterface(rsLogVars_->iTraceLevel, rsLogVars_->szTraceFile);
    }

    virtual void ShutdownAWSLogging() { logSystem.reset(); }
    virtual AwsLogging::LogSystemInterface *GetLogSystem() {
        return logSystem.get();
    }
};

// Use this if you'd like to connect to universal AWSLogsystem
struct AwsLogManager : public RsLogManager {
    virtual void initializeAWSLogging(RS_LOG_VARS *rsLogVars_ = nullptr) {
        rsLogVars_ = rsLogVars_ ? rsLogVars_ : rsLogVars;
        if (!rsLogVars_) {
            return;
        }
        AwsLogging::InitializeAWSLogging(
            initLogInterface(rsLogVars_->iTraceLevel, rsLogVars_->szTraceFile));
    }

    virtual void ShutdownAWSLogging() { AwsLogging::ShutdownAWSLogging(); }
    virtual AwsLogging::LogSystemInterface *GetLogSystem() {
        return AwsLogging::GetLogSystem();
    }
};

typedef RsLogManager LogManagerType;
static LogManagerType rsLogManager;

/*
Initialize the logging system. Logs can be wrrtten to the file after successful
completion of this command
*/
void initializeAWSLogging() {
    rsLogManager.initializeAWSLogging();
    auto* logSystem = rsLogManager.GetLogSystem();
    if (logSystem == nullptr) {
        throw std::runtime_error("Failed to initialize logging system");
    }

    // Check log level
    auto logLevel = logSystem->GetLogLevel();
    if (logLevel >= AwsLogging::LogLevel::Debug) {
        RS_LOG_WARN("RSLOG",
                    "\n"
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                    "!!!! WARNING "
                    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                    "!!!!!!!\n"
                    "**********************************************************"
                    "**********************************************************"
                    "********************\n"
                    "*                                                         "
                    "                                                          "
                    "                   *\n"
                    "*  Log level (%d) is enabled!                             "
                    "                                                          "
                    "                    *\n"
                    "*  WARNING: Log level Debug (level 5) or higher captures "
                    "ALL data which may include sensitive information.         "
                    "                    *\n"
                    "*                                                         "
                    "                                                          "
                    "                   *\n"
                    "*  IMPORTANT: Before sharing logs with support or any "
                    "third party, ensure all sensitive information is "
                    "redacted/removed from the logs  *\n"
                    "**********************************************************"
                    "**********************************************************"
                    "********************\n",
                    static_cast<int>(logLevel));
    }
}
void initializeLoggingWithGlobalLogVars(RS_LOG_VARS *rsLogVars) {
    rsLogManager.initializeAWSLogging(rsLogVars);
}

/*
Finalize the logging system. No more Logs can be wrrtten to the file after this
step
*/
void ShutdownAWSLogging() { rsLogManager.ShutdownAWSLogging(); }

/*
Returns the AWS Log system interface
*/
AwsLogging::LogSystemInterface *GetLogSystem() {
    return rsLogManager.GetLogSystem();
}

void processLogLine(AwsLogging::LogLevel level, const char *filename,
                    const int line, const char *func, const char *tag1,
                    const char *msg) {
    AwsLogging::LogSystemInterface *logSystem = internal::GetLogSystem();
    if (!logSystem) {
        return; // Log system not initialized, silently return
    }

    std::string filenameStr =
        filename ? Aws::Utils::PathUtils::GetFileNameFromPathWithExt(filename)
                       .c_str()
                 : "<null>";

    const std::string locationTag = "[" + std::string(tag1 ? tag1 : "<null>") +
                                    ":" + filenameStr + ":" +
                                    std::to_string(line) + "]";

    // Only include PID in log lines for DEBUG level and above
    std::string fullMessage;
    if (level >= AwsLogging::LogLevel::Debug) {
        fullMessage = "[pid:" + std::to_string(cachedPid) + "] " + 
            std::string(msg ? msg : "<null>");
    } else {
        fullMessage = std::string(msg ? msg : "<null>");
    }
    
    const Aws::OStringStream messageStream(fullMessage.c_str());
    logSystem->LogStream(level, locationTag.c_str(), messageStream);
    logSystem->Flush();
}
} // namespace internal

#define RS_LOG_MACRO(LEVEL)                                                    \
    if (!internal::GetLogSystem() ||                                           \
        internal::GetLogSystem()->GetLogLevel() < LEVEL)                       \
        return;                                                                \
    va_list args;                                                              \
    va_start(args, fmt);                                                       \
    va_list args_copy;                                                         \
    va_copy(args_copy, args);                                                  \
    const auto size = std::vsnprintf(nullptr, 0, fmt, args_copy) + 1;          \
    va_end(args_copy);                                                         \
    std::string buffer(size, ' ');                                             \
    vsnprintf(&buffer[0], size, fmt, args);                                    \
    buffer.pop_back();                                                         \
    va_end(args);                                                              \
    internal::processLogLine(LEVEL, file, line, func, tag, buffer.c_str());

void RS_LOG_FATAL_(const char *file, const int line, const char *func,
                   const char *tag, const char *fmt, ...) {
    RS_LOG_MACRO(AwsLogging::LogLevel::Fatal);
}

void RS_LOG_ERROR_(const char *file, const int line, const char *func,
                   const char *tag, const char *fmt, ...) {
    RS_LOG_MACRO(AwsLogging::LogLevel::Error);
}

void RS_LOG_WARN_(const char *file, const int line, const char *func,
                  const char *tag, const char *fmt, ...) {
    RS_LOG_MACRO(AwsLogging::LogLevel::Warn);
}

void RS_LOG_INFO_(const char *file, const int line, const char *func,
                  const char *tag, const char *fmt, ...) {
    RS_LOG_MACRO(AwsLogging::LogLevel::Info);
}

void RS_LOG_DEBUG_(const char *file, const int line, const char *func,
                   const char *tag, const char *fmt, ...) {
    RS_LOG_MACRO(AwsLogging::LogLevel::Debug);
}

void RS_LOG_TRACE_(const char *file, const int line, const char *func,
                   const char *tag, const char *fmt, ...) {
    RS_LOG_MACRO(AwsLogging::LogLevel::Trace);
}

void RS_STREAM_LOG_TRACE_(const char *file, const int line, const char *func,
                          const char *tag, const char *s, long long len) {
    // Avoid noisy data
    if (typeid(internal::rsLogManager) == typeid(internal::RsLogManager)) {
        return;
    }
    AwsLogging::LogSystemInterface *logSystem = internal::GetLogSystem();
    if (logSystem && logSystem->GetLogLevel() >= AwsLogging::LogLevel::Trace) {
        Aws::OStringStream logStream;
        if (len < 0) {
            len = strlen(s);
        }
        while (len-- > 0) {
            logStream << *s++;
        }
        logSystem->LogStream(AwsLogging::LogLevel::Trace, tag, logStream);
        logSystem->Flush();
    }
}

RS_LOG_VARS *getGlobalLogVars() {
    return internal::getGlobalLogVars();
}

void initializeLogging() { internal::initializeAWSLogging(); }

void initializeLoggingWithGlobalLogVars(RS_LOG_VARS *rsLogVars) {
    internal::initializeLoggingWithGlobalLogVars(rsLogVars);
}

void shutdownLogging() { internal::ShutdownAWSLogging(); }

// Legacy mapping
int getRsLoglevel() {
    return internal::GetLogSystem()
               ? (int)internal::GetLogSystem()->GetLogLevel()
               : (int)LOG_LEVEL_OFF;
}
