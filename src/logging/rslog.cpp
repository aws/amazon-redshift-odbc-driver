#include <atomic>
#include <cstdarg>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <typeinfo>

#include <cerrno>
#include <cstring>
#include <string>

#include "rslog.h"

#include <aws/core/Aws.h>
#include <aws/core/platform/FileSystem.h>  // CreateDirectoryIfNotExists
#include <aws/core/utils/FileSystemUtils.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/logging/LogSystemInterface.h>
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

// Creates `dir` and any missing parents, one level at a time. Both separators
// count on Windows, where a LogPath may use either.
static void CreateLogDirTree(const Aws::String &dir) {
    for (size_t end = 1; end <= dir.size(); ++end) {
        if (end != dir.size() && dir[end] != '/'
#ifdef _WIN32
            && dir[end] != '\\'
#endif
        ) {
            continue;
        }
        const Aws::String sub = dir.substr(0, end);
#ifdef _WIN32
        // Skip a bare drive root such as "C:".
        if (sub.size() == 2 && sub[1] == ':') {
            continue;
        }
#endif
        // Mode follows the host umask.
        (void)Aws::FileSystem::CreateDirectoryIfNotExists(sub.c_str());
    }
}

// Thread-safe strerror: std::strerror returns a pointer into a static buffer
// shared by the whole process, so each branch below formats into its own.
static std::string safeStrerror(int err) {
    char buf[256] = {0};
#ifdef _WIN32
    return strerror_s(buf, sizeof(buf), err) == 0 ? std::string(buf)
                                                  : std::string("unknown error");
#elif defined(_GNU_SOURCE)
    // GNU strerror_r returns the text, which may not be `buf`.
    const char *msg = strerror_r(err, buf, sizeof(buf));
    return msg ? std::string(msg) : std::string("unknown error");
#else
    // XSI strerror_r returns 0 on success and fills `buf`.
    return strerror_r(err, buf, sizeof(buf)) == 0 ? std::string(buf)
                                                  : std::string("unknown error");
#endif
}

// No-op log system for level OFF and for an unusable path. DefaultLogSystem
// would start a writer thread for lines it then discards.
class NullLogSystem : public AwsLogging::LogSystemInterface {
  public:
    AwsLogging::LogLevel GetLogLevel() const override {
        return AwsLogging::LogLevel::Off;
    }
    void Log(AwsLogging::LogLevel, const char *, const char *, ...) override {}
    void vaLog(AwsLogging::LogLevel, const char *, const char *,
               va_list) override {}
    void LogStream(AwsLogging::LogLevel, const char *,
                   const Aws::OStringStream &) override {}
    void Flush() override {}
};

// Logger system manager. Its scope is local to ODBC log lines only.
struct RsLogManager {
    typedef std::shared_ptr<AwsLogging::LogSystemInterface> LogInterfacePtr;

  private:
    LogInterfacePtr logSystem = nullptr;
    // Guards logSystem. Held only for a pointer copy or swap, never file I/O.
    std::mutex logMtx;
    // Lock-free level for the hot gate, so RS_LOG_* takes no lock per row.
    std::atomic<int> cachedLevel{(int)LOG_LEVEL_OFF};

  protected:
    RS_LOG_VARS *rsLogVars = nullptr;
    // Override filename and stream creation of the Logger.
    // The original logic we are overriding is in AWS SDK.
    std::shared_ptr<Aws::OFStream>
    MakeDefaultLogFile(const Aws::String &filename) {

        // Directory part of <LogPath>/redshift_odbc.log; empty if none.
        // Backslash counts only on Windows; on POSIX it is a filename byte.
        auto parentDirOf = [](const Aws::String &path) -> Aws::String {
            auto const pos{path.find_last_of(
#ifdef _WIN32
                "/\\"
#else
                "/"
#endif
                )};
            return (pos == Aws::String::npos) ? Aws::String{}
                                              : path.substr(0, pos);
        };

        // Create the LogPath dir; the open below reports any real failure.
        const Aws::String parentDir{parentDirOf(filename)};
        if (!parentDir.empty()) {
            CreateLogDirTree(parentDir);
        }

        // Clear errno so the diagnostic reports the open, not the mkdir.
        errno = 0;
        auto res = Aws::MakeShared<Aws::OFStream>(
            "DefaultLogSystem", filename.c_str(),
            Aws::OFStream::out | Aws::OFStream::app);
        if (res && res->good()) {
            return res;
        }
        // ofstream need not set errno, so only append strerror when it did.
        const int openErrno = errno;
        std::cerr << "Failed to open log file '" << filename << "'";
        if (openErrno != 0) {
            std::cerr << " (" << safeStrerror(openErrno) << ")";
        }
        std::cerr << ". Logging is disabled." << std::endl;
        return res;
    }
    // OFF yields a no-op logger; nullptr if there is no usable path.
    LogInterfacePtr initLogInterface(int iTraceLevel, const char *szTraceFile) {
        if (iTraceLevel <= LOG_LEVEL_OFF) {
            return Aws::MakeShared<NullLogSystem>("ODBC2LOG");
        }
        const Aws::String file(szTraceFile ? szTraceFile : "");
        if (file.empty()) {
            // Name the real problem rather than failing to open "".
            std::cerr << "Logging is enabled but no LogPath is configured. "
                         "Logging is disabled."
                      << std::endl;
            return nullptr;
        }
        auto stream = MakeDefaultLogFile(file);
        if (!(stream && stream->good())) {
            return nullptr;
        }
        return Aws::MakeShared<AwsLogging::DefaultLogSystem>(
            "ODBC2LOG", (AwsLogging::LogLevel)(iTraceLevel), stream);
    }

  public:
    RsLogManager() { rsLogVars = getGlobalLogVars(); }

    // (Re)initialize the logger: build outside the lock, swap under it, drop
    // the old one after unlocking (its dtor joins a writer thread).
    virtual void initializeAWSLogging(RS_LOG_VARS *rsLogVars_ = nullptr) {
        rsLogVars_ = rsLogVars_ ? rsLogVars_ : rsLogVars;
        if (!rsLogVars_) {
            return;
        }
        LogInterfacePtr next =
            initLogInterface(rsLogVars_->iTraceLevel, rsLogVars_->szTraceFile);
        LogInterfacePtr old;
        {
            std::lock_guard<std::mutex> lk(logMtx);
            if (next) {
                old = std::move(logSystem);
                logSystem = next;  // swap only on success
            } else if (!logSystem) {
                // Keep it non-null: callers get a logger that discards.
                logSystem = Aws::MakeShared<NullLogSystem>("ODBC2LOG");
            }
            cachedLevel.store(logSystem ? (int)logSystem->GetLogLevel()
                                        : (int)LOG_LEVEL_OFF,
                              std::memory_order_relaxed);
        }
    }

    virtual void ShutdownAWSLogging() {
        LogInterfacePtr old;
        {
            std::lock_guard<std::mutex> lk(logMtx);
            old = std::move(logSystem);
            logSystem.reset();
            cachedLevel.store((int)LOG_LEVEL_OFF, std::memory_order_relaxed);
        }
    }
    // Lock-free current level for the hot gate (RS_LOG_*/IS_TRACE_*).
    int GetCachedLevel() const {
        return cachedLevel.load(std::memory_order_relaxed);
    }
    // Shared owner, so a concurrent swap cannot free it mid-write.
    LogInterfacePtr GetLogSystemShared() {
        std::lock_guard<std::mutex> lk(logMtx);
        return logSystem;
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
};

typedef RsLogManager LogManagerType;
static LogManagerType rsLogManager;

/*
Initialize the logging system. Logs can be wrrtten to the file after successful
completion of this command
*/
void initializeAWSLogging() {
    rsLogManager.initializeAWSLogging();
    auto logSystem = rsLogManager.GetLogSystemShared();  // hold a shared owner
    if (logSystem == nullptr) {
        // Logging is disabled; there is no logger to inspect.
        return;
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

// Shared owner: hold across a log write so a re-init cannot free it.
std::shared_ptr<AwsLogging::LogSystemInterface> GetLogSystemShared() {
    return rsLogManager.GetLogSystemShared();
}

// Lock-free current level for the hot log-level gate.
int GetCachedLevel() { return rsLogManager.GetCachedLevel(); }

void processLogLine(AwsLogging::LogLevel level, const char *filename,
                    const int line, const char *func, const char *tag1,
                    const char *msg) {
    auto logSystem = GetLogSystemShared();
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
    if (internal::GetCachedLevel() < (int)(LEVEL))                             \
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
    auto logSystem = internal::GetLogSystemShared();
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
    return internal::GetCachedLevel();
}
