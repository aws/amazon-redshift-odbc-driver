#include <cstdarg>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <rslog.h>

#include <aws/core/Aws.h>
#include <aws/core/utils/FileSystemUtils.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/memory/stl/AWSString.h>

namespace AwsLogging = Aws::Utils::Logging;

RS_LOG_VARS *getGlobalLogVars() {
    static RS_LOG_VARS gRslogSettings;
    return &gRslogSettings;
}

// Override filename and stream creation of the Logger.
// The original logic we are overriding is in AWS SDK.
std::shared_ptr<Aws::OFStream> MakeDefaultLogFile(const Aws::String &filename) {

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

    auto findFileFromPath = [&](const Aws::String &path) -> const Aws::String {
        auto res = Aws::Utils::PathUtils::GetFileNameFromPathWithExt(filename);
        if (res.empty()) {
            std::cerr << "Attempt to extract file name from path " << filename
                      << " failed." << std::endl;
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

void initializeAWSLogging() {
    AwsLogging::InitializeAWSLogging(
        Aws::MakeShared<AwsLogging::DefaultLogSystem>(
            "ODBC2LOG", (AwsLogging::LogLevel)(getGlobalLogVars()->iTraceLevel),
            MakeDefaultLogFile(Aws::String(getGlobalLogVars()->szTraceFile
                                               ? getGlobalLogVars()->szTraceFile
                                               : ""))));
}

void initTrace() {
    // By this time, we assume respective settings are initialized
    // Initialize the AWS SDK for C++
    initializeAWSLogging();
}

void uninitTrace() { AwsLogging::ShutdownAWSLogging(); }

// Legacy mapping
int getRsLoglevel() { return (int)AwsLogging::GetLogSystem()->GetLogLevel(); }
 
void processLogLine(AwsLogging::LogLevel level, const char *filename,
                    const int line, const char *func, const char *tag1,
                    const char *msg) {
    static const std::string open = "[", close = "]", colon = ":";
    const std::string tag2 =
        (open + Aws::Utils::PathUtils::GetFileNameFromPathWithExt(filename) +
         colon + std::to_string(line) + close);
    const char *tag = tag2.c_str();
        switch (level) {
    case AwsLogging::LogLevel::Fatal:
                AWS_LOG_FATAL(tag, msg);
        break;
    case AwsLogging::LogLevel::Error:
                AWS_LOG_ERROR(tag, msg);
        break;
    case AwsLogging::LogLevel::Warn:
                AWS_LOG_WARN(tag, msg);
        break;
    case AwsLogging::LogLevel::Info:
                AWS_LOG_INFO(tag, msg);
        break;
    case AwsLogging::LogLevel::Debug:
                AWS_LOG_DEBUG(tag, msg);
        break;
    case AwsLogging::LogLevel::Trace:
                AWS_LOG_TRACE(tag, msg);
        break;
    default:
        break;
    }
}

#define RS_LOG_MACRO(LEVEL)                                                    \
    if ((AwsLogging::LogLevel)getRsLoglevel() < LEVEL)                         \
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
    processLogLine(LEVEL, file, line, func, tag, buffer.c_str());

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
    AwsLogging::LogSystemInterface *logSystem = AwsLogging::GetLogSystem();
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
