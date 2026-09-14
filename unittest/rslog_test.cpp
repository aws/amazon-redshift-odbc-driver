
#include "common.h"
#include <rslog.h>
#include <atomic>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <regex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <process.h>  // _getpid, which rslog.h maps getpid onto
#else
#include <sys/stat.h>
#include <unistd.h>  // getpid
#endif

int findInFile(const std::string& filename, const std::string& target) {
    int cnt = 0;
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Failed to open the file." << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find(target.c_str()) != std::string::npos) {
            ++cnt;
        }
    }

    file.close();
    return cnt;
}

// Remove a single directory, portably.
static void rmdirPortable(const std::string& d) {
#ifdef _WIN32
    _rmdir(d.c_str());
#else
    rmdir(d.c_str());
#endif
}

// Remove the log file, then rmdir `dir` and its parents bottom-up.
static void removeDirChain(const std::string& dir) {
    std::string d = dir;
    while (!d.empty()) {
        rmdirPortable(d);
        const size_t slash = d.find_last_of("/\\");
        if (slash == std::string::npos) {
            break;
        }
        d = d.substr(0, slash);
    }
}

// RAII: a unique per-process LogPath tree (`shape` under the root), cleaned on
// entry and scope exit.
struct ScopedLogDir {
    std::string dir;
    std::string file;
    explicit ScopedLogDir(const std::string& tag, const std::string& shape = "logs") {
        const std::string root =
            "rslog_logpath_" + tag + "_" + std::to_string(getpid());
        dir = shape.empty() ? root : root + "/" + shape;
        file = dir + "/redshift_odbc.log";
        clean();
    }
    ~ScopedLogDir() { clean(); }
    void clean() {
        std::remove(file.c_str());
        removeDirChain(dir);
    }
};

// RAII: a regular file where a dir is expected, so mkdir fails ENOTDIR even
// as root; `logFile` is an uncreatable path under it.
struct ScopedBlockerFile {
    std::string path;
    std::string logFile;
    explicit ScopedBlockerFile(const std::string& tag) {
        path = "rslog_logpath_" + tag + "_" + std::to_string(getpid());
        logFile = path + "/logs/redshift_odbc.log";
        std::remove(path.c_str());
        std::ofstream(path) << "x";  // a regular file, not a directory
    }
    ~ScopedBlockerFile() { std::remove(path.c_str()); }
};

// Initialize logging to `traceFile` at TRACE, emit one marker line, shut down.
// Returns whether the marker landed in `traceFile`.
static bool writeMarkerToLog(const std::string& traceFile,
                             const std::string& marker) {
    RS_LOG_VARS rsLogVars;
    rsLogVars.iTraceLevel = 6;
    snprintf(rsLogVars.szTraceFile, sizeof(rsLogVars.szTraceFile), "%s",
             traceFile.c_str());
    rsLogVars.isInitialized = 1;
    initializeLoggingWithGlobalLogVars(&rsLogVars);
    RS_LOG_INFO("RSLOGTEST", "%s", marker.c_str());
    shutdownLogging();
    // findInFile() returns 1 when the file is absent, so check existence first.
    std::ifstream check(traceFile);
    if (!check.good()) {
        return false;
    }
    check.close();
    return findInFile(traceFile, marker) >= 1;
}

// Run `fn` with std::cerr redirected, and return what it captured.
template <typename Fn>
static std::string captureCerr(Fn&& fn) {
    std::ostringstream cap;
    std::streambuf* old = std::cerr.rdbuf(cap.rdbuf());
    fn();
    std::cerr.rdbuf(old);
    return cap.str();
}

#ifdef __linux__
// Threads currently in this process, or -1 if unavailable.
static int threadCount() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("Threads:", 0) == 0) {
            return std::atoi(line.c_str() + 8);
        }
    }
    return -1;
}
#endif

// Malformed paths the driver must never write to: the filename joined to
// "/tmp" with no separator (a drive-root path on Windows).
static bool tempFallbackArtifactExists() {
    return std::ifstream("/tmpredshift_odbc.log").good() ||
           std::ifstream("tmpredshift_odbc.log").good();
}

TEST(RSLOG_TEST_SUITE, ascii_37) {
    const std::string filename = "rslog_test.log";
    std::remove(filename.c_str());
    RS_LOG_VARS rsLogVars;
    rsLogVars.iTraceLevel = 6;
    sprintf(rsLogVars.szTraceFile, filename.c_str());
    rsLogVars.isInitialized = 1;
    initializeLoggingWithGlobalLogVars(&rsLogVars);
    std::string str="%";
    RS_LOG_DEBUG("CAT", "%s", "123");
    RS_LOG_DEBUG("CAT", "%s", "%");
    RS_LOG_DEBUG("CAT", "%s", str.c_str());
    shutdownLogging();
    ASSERT_EQ(2, findInFile(filename, "%"));
}

// Test for processLogLine function by examining the log file output
TEST(RSLOG_TEST_SUITE, ProcessLogLine) {
    // Setup - create a unique log file for this test
    const std::string filename = "rslog_processline_test_" + std::to_string(getpid()) + ".log";
    std::remove(filename.c_str());
    
    // Initialize logging with our test file
    RS_LOG_VARS rsLogVars;
    rsLogVars.iTraceLevel = 6; // Set to TRACE level to capture all logs
    sprintf(rsLogVars.szTraceFile, "%s", filename.c_str());
    rsLogVars.isInitialized = 1;
    initializeLoggingWithGlobalLogVars(&rsLogVars);
    
    // Create a unique test message that we can search for
    const std::string uniqueTestMessage = "UNIQUE_TEST_MESSAGE_FOR_PROCESSLOGLINE_TEST";
    
    // Use the RS_LOG_DEBUG macro which will call processLogLine internally
    // (PID is only included in DEBUG level and above)
    RS_LOG_DEBUG("TEST_TAG", "%s", uniqueTestMessage.c_str());
    
    // Shutdown logging to ensure the file is flushed and closed
    shutdownLogging();
    
    // Now read the log file and verify the output
    std::ifstream logFile(filename);
    ASSERT_TRUE(logFile.is_open()) << "Failed to open log file: " << filename;
    
    std::string logContent;
    std::string line;
    while (std::getline(logFile, line)) {
        logContent += line + "\\n";
    }
    logFile.close();
    
    // Verify the log contains our unique message
    ASSERT_NE(logContent.find(uniqueTestMessage), std::string::npos) 
        << "Log file does not contain our test message";
    
    // Verify the log contains the tag
    ASSERT_NE(logContent.find("TEST_TAG"), std::string::npos) 
        << "Log file does not contain the tag";
    
    // Verify the log contains the filename (rslog_test.cpp)
    ASSERT_NE(logContent.find("rslog_test.cpp"), std::string::npos) 
        << "Log file does not contain the filename";
    
    // Verify the log contains PID information with correct PID
    const int actualPid = getpid();
    std::string pidStr = "[pid:" + std::to_string(actualPid) + "]";
    ASSERT_NE(logContent.find(pidStr), std::string::npos) 
        << "Log file does not contain the correct PID: " << pidStr;
    
    // Use regex to verify the format of the log line
    // Expected format: [LEVEL] timestamp [TEST_TAG:rslog_test.cpp:line] [threadid] [pid:123] message
    std::regex formatRegex("\\[DEBUG\\].*\\[TEST_TAG:rslog_test\\.cpp:\\d+\\] \\[(0x)?[0-9a-f]+\\] \\[pid:\\d+\\]");
    ASSERT_TRUE(std::regex_search(logContent, formatRegex)) 
        << "Log format does not match expected pattern. Actual content: " << logContent;

    // Clean up the test log file
    std::remove(filename.c_str());
}

// a missing multi-level LogPath dir is created and the log written there.
TEST(RSLOG_TEST_SUITE, missing_parent_dir_is_created_and_log_written) {
    ScopedLogDir log("missing", "nested/logs");
    ASSERT_TRUE(writeMarkerToLog(log.file, "MARKER_DIR_CREATION_MARKER"))
        << log.file;
}

#ifdef _WIN32
// A LogPath may use either separator on Windows; these cover backslash
// and mixed, the shapes above are all forward-slash.
TEST(RSLOG_TEST_SUITE, windows_separator_forms_are_created) {
    const std::string root =
        "rslog_logpath_winsep_" + std::to_string(getpid());
    const std::string shapes[] = {"a\\b\\logs", "c/d\\logs", "e\\f/logs"};
    for (const std::string &shape : shapes) {
        const std::string dir = root + "\\" + shape;
        const std::string file = dir + "\\redshift_odbc.log";
        std::remove(file.c_str());
        removeDirChain(dir);

        EXPECT_TRUE(writeMarkerToLog(file, "MARKER_WINSEP"))
            << "no log written for separator shape '" << shape << "': " << file;

        std::remove(file.c_str());
        removeDirChain(dir);
    }
}
#endif

#ifndef _WIN32
// the driver sets no mode of its own, so created dirs follow the umask.
// Windows has no POSIX mode bits, so this is POSIX-only.
TEST(RSLOG_TEST_SUITE, created_dir_mode_honors_umask) {
    // A restrictive umask: a hard-coded mode would show 0755, not 0700.
    const mode_t saved = umask(077);
    struct Restore {
        mode_t m;
        ~Restore() { umask(m); }
    } restore{saved};

    ScopedLogDir log("mode", "nested/logs");
    ASSERT_TRUE(writeMarkerToLog(log.file, "MARKER_MODE")) << log.file;

    // Every level, not just the leaf.
    const std::string dir = log.file.substr(0, log.file.find_last_of('/'));
    int checked = 0;
    for (std::string d = dir;;) {
        struct stat st {};
        ASSERT_EQ(0, stat(d.c_str(), &st)) << d;
        EXPECT_EQ(0700u, st.st_mode & 07777u)
            << "created log dir level is wider than umask allows; got "
            << std::oct << (st.st_mode & 07777u) << " for " << d;
        ++checked;
        const size_t slash = d.find_last_of('/');
        if (slash == std::string::npos) {
            break;
        }
        d = d.substr(0, slash);
    }
    EXPECT_EQ(3, checked) << "expected root/nested/logs to all be checked";
}
#endif

// an uncreatable LogPath disables logging with no log and no /tmp fallback.
TEST(RSLOG_TEST_SUITE, uncreatable_path_disables_logging_and_no_fallback) {
    ScopedBlockerFile blocker("blocker");
    std::remove("/tmpredshift_odbc.log");
    std::remove("tmpredshift_odbc.log");

    EXPECT_FALSE(writeMarkerToLog(blocker.logFile, "MARKER_SHOULD_NOT_LAND"))
        << "a log line landed under an uncreatable path";
    EXPECT_FALSE(std::ifstream(blocker.logFile).good())
        << "log created under an uncreatable path: " << blocker.logFile;
    EXPECT_FALSE(tempFallbackArtifactExists())
        << "the removed /tmp fallback artifact reappeared";
}

// the failure path is loud (a stderr diagnostic), not a silent redirect.
TEST(RSLOG_TEST_SUITE, uncreatable_path_emits_stderr_diagnostic) {
    ScopedBlockerFile blocker("diag");

    const std::string err =
        captureCerr([&] { writeMarkerToLog(blocker.logFile, "MARKER_DIAG"); });

#ifndef _WIN32
    // POSIX-only: std::cerr capture cannot cross the driver's CRT on Windows.
    EXPECT_NE(err.find("Failed to open log file"), std::string::npos) << err;
    EXPECT_NE(err.find("Logging is disabled"), std::string::npos) << err;
#else
    (void)err;
#endif
}

// an existing LogPath dir is reused and appended to, not truncated.
TEST(RSLOG_TEST_SUITE, existing_dir_is_reused_and_appended) {
    ScopedLogDir log("exist", "nested/logs");

    ASSERT_TRUE(writeMarkerToLog(log.file, "MARKER_APPEND"))
        << "no log written on the first init: " << log.file;
    ASSERT_TRUE(writeMarkerToLog(log.file, "MARKER_APPEND"))
        << "no log written on the second init: " << log.file;
    EXPECT_GE(findInFile(log.file, "MARKER_APPEND"), 2)
        << "log was truncated instead of appended: " << log.file;
}

// the recursive create handles LogPath shapes from two levels deep to six.
class LogPathShapeTest : public ::testing::TestWithParam<std::string> {};

TEST_P(LogPathShapeTest, dir_is_created_and_written) {
    ScopedLogDir log("shape", GetParam());
    EXPECT_TRUE(writeMarkerToLog(log.file, "MARKER_SHAPE")) << log.file;
}

INSTANTIATE_TEST_SUITE_P(
    LogPathShapes, LogPathShapeTest,
    ::testing::Values(std::string("single"),            // 2 levels under the temp root
                      std::string("nested/logs"),       // 3 levels
                      std::string("deep/a/b/c/logs"),   // 6 levels
                      std::string("trail/logs/")));     // trailing separator

// a pre-existing dir (not driver-created) is used as-is, perms untouched.
TEST(RSLOG_TEST_SUITE, existing_dir_not_created_by_driver_is_used) {
    ScopedLogDir log("preexist", "");  // dir == root

#ifdef _WIN32
    ASSERT_EQ(0, _mkdir(log.dir.c_str()))  // dir the driver must reuse
        << "test setup could not create " << log.dir;
#else
    ASSERT_EQ(0, mkdir(log.dir.c_str(), 0750))  // dir the driver must reuse
        << "test setup could not create " << log.dir;
    // Not a umask-derived mode, so a change by the driver is detectable.
    ASSERT_EQ(0, chmod(log.dir.c_str(), 0750))  // pin perms regardless of umask
        << "test setup could not set 0750 on " << log.dir;
#endif

    EXPECT_TRUE(writeMarkerToLog(log.file, "MARKER_PREEXIST"))
        << "no log written into the pre-existing dir: " << log.file;

#ifndef _WIN32
    struct stat st{};
    ASSERT_EQ(0, stat(log.dir.c_str(), &st))
        << "pre-existing log dir disappeared: " << log.dir;
    EXPECT_EQ(0750u, (st.st_mode & 0777u))
        << "driver changed the mode of a directory it did not create";
#endif
}

// with logging OFF, no log dir or file is created.
TEST(RSLOG_TEST_SUITE, logging_off_creates_nothing) {
    ScopedLogDir log("off", "nested/logs");

    RS_LOG_VARS v;
    v.iTraceLevel = LOG_LEVEL_OFF;
    snprintf(v.szTraceFile, sizeof(v.szTraceFile), "%s", log.file.c_str());
    v.isInitialized = 1;
    initializeLoggingWithGlobalLogVars(&v);
    shutdownLogging();

    EXPECT_FALSE(std::ifstream(log.file).good())
        << "log file created even though logging is OFF";
#ifndef _WIN32
    struct stat st{};
    EXPECT_NE(0, stat(log.dir.c_str(), &st))
        << "log dir created even though logging is OFF";
#endif
}

#ifdef __linux__
// Logging OFF starts no writer thread however often it is re-initialized.
TEST(RSLOG_TEST_SUITE, logging_off_starts_no_writer_thread) {
    ScopedLogDir log("offthread", "logs");
    shutdownLogging();  // known state: no logger, no writer thread

    RS_LOG_VARS v;
    v.iTraceLevel = LOG_LEVEL_OFF;
    snprintf(v.szTraceFile, sizeof(v.szTraceFile), "%s", log.file.c_str());
    v.isInitialized = 1;

    const int before = threadCount();
    ASSERT_GT(before, 0) << "could not read the thread count";
    for (int i = 0; i < 5; ++i) {
        initializeLoggingWithGlobalLogVars(&v);
    }
    const int offThreads = threadCount();

    v.iTraceLevel = 6;
    initializeLoggingWithGlobalLogVars(&v);
    const int onThreads = threadCount();
    shutdownLogging();

    EXPECT_EQ(before, offThreads)
        << "logging OFF changed the thread count (" << before << " -> "
        << offThreads << ")";
    EXPECT_GT(onThreads, offThreads)
        << "enabled logging did not start its writer thread";
}
#endif

// Logging on with no path disables logging with a diagnostic.
TEST(RSLOG_TEST_SUITE, enabled_with_empty_path_disables_logging_with_diagnostic) {
    shutdownLogging();  // no incumbent logger: an existing one is kept by design

    RS_LOG_VARS v;
    v.iTraceLevel = 6;
    v.szTraceFile[0] = '\0';
    v.isInitialized = 1;
    const std::string err =
        captureCerr([&v] { initializeLoggingWithGlobalLogVars(&v); });
    const int level = getRsLoglevel();
    shutdownLogging();

    EXPECT_EQ((int)LOG_LEVEL_OFF, level)
        << "logging stayed enabled with no LogPath configured";
#ifndef _WIN32
    // The driver DLL has its own iostreams, so std::cerr is not capturable.
    EXPECT_NE(err.find("no LogPath is configured"), std::string::npos) << err;
#else
    (void)err;
#endif
}

// Threads logging during a re-init must not crash or break logging.
TEST(RSLOG_TEST_SUITE, concurrent_reinit_and_log_is_stable) {
    ScopedLogDir log("concur", "");

    std::atomic<bool> stop{false};
    std::vector<std::thread> writers;
    for (int t = 0; t < 4; ++t) {
        writers.emplace_back([&stop] {
            while (!stop.load(std::memory_order_relaxed)) {
                RS_LOG_INFO("CONCUR", "%s", "info line");
                RS_LOG_ERROR("CONCUR", "%d", 7);
            }
        });
    }

    for (int i = 0; i < 200; ++i) {
        RS_LOG_VARS v;
        v.iTraceLevel = (i % 2) ? 6 : 1;  // vary the level writers race against
        snprintf(v.szTraceFile, sizeof(v.szTraceFile), "%s", log.file.c_str());
        v.isInitialized = 1;
        initializeLoggingWithGlobalLogVars(&v);
    }

    stop.store(true, std::memory_order_relaxed);
    for (auto& th : writers) {
        th.join();
    }
    shutdownLogging();

    // Surviving the churn is not enough: logging must still deliver lines.
    EXPECT_TRUE(writeMarkerToLog(log.file, "MARKER_AFTER_CONCURRENT_REINIT"))
        << "logging stopped working after concurrent re-init: " << log.file;
}

// Regression tests for the format-string fix in the trace logging
// subsystem. Confirms that when user-controlled data is passed as a
// %s argument (not as the fmt), format specifiers like %n, %s, %p,
// %x embedded in the data are written to the log as literal bytes
// and are not interpreted by vsnprintf.

static void setup_log(const std::string& filename) {
    std::remove(filename.c_str());
    RS_LOG_VARS rsLogVars;
    rsLogVars.iTraceLevel = 6;
    sprintf(rsLogVars.szTraceFile, "%s", filename.c_str());
    rsLogVars.isInitialized = 1;
    initializeLoggingWithGlobalLogVars(&rsLogVars);
}

TEST(RSLOG_FORMAT_STRING_SUITE, debug_pct_n_payload_written_literally) {
    const std::string filename = "rslog_fmt_n_" + std::to_string(getpid()) + ".log";
    setup_log(filename);
    const char* payload = "malicious_%n%n%n%n_end";
    RS_LOG_DEBUG("FMTTEST", "%s", payload);
    shutdownLogging();
    ASSERT_GE(findInFile(filename, "malicious_%n%n%n%n_end"), 1);
    std::remove(filename.c_str());
}

TEST(RSLOG_FORMAT_STRING_SUITE, error_pct_s_payload_written_literally) {
    const std::string filename = "rslog_fmt_s_" + std::to_string(getpid()) + ".log";
    setup_log(filename);
    const char* payload = "boom_%s%s%s%s_end";
    RS_LOG_ERROR("FMTTEST", "%s", payload);
    shutdownLogging();
    ASSERT_GE(findInFile(filename, "boom_%s%s%s%s_end"), 1);
    std::remove(filename.c_str());
}

TEST(RSLOG_FORMAT_STRING_SUITE, info_pct_p_payload_written_literally) {
    const std::string filename = "rslog_fmt_p_" + std::to_string(getpid()) + ".log";
    setup_log(filename);
    const char* payload = "leak_%p.%p.%p.%p_end";
    RS_LOG_INFO("FMTTEST", "%s", payload);
    shutdownLogging();
    ASSERT_GE(findInFile(filename, "leak_%p.%p.%p.%p_end"), 1);
    std::remove(filename.c_str());
}

TEST(RSLOG_FORMAT_STRING_SUITE, warn_pct_x_payload_written_literally) {
    const std::string filename = "rslog_fmt_x_" + std::to_string(getpid()) + ".log";
    setup_log(filename);
    const char* payload = "leak_%08x.%08x.%08x_end";
    RS_LOG_WARN("FMTTEST", "%s", payload);
    shutdownLogging();
    ASSERT_GE(findInFile(filename, "leak_%08x.%08x.%08x_end"), 1);
    std::remove(filename.c_str());
}

TEST(RSLOG_FORMAT_STRING_SUITE, user_controlled_string_via_c_str) {
    // Verify that when a runtime std::string containing format
    // specifiers is passed as a %s data argument (rather than as the
    // fmt), the specifiers reach the log file as literal bytes.
    // This mirrors the server-error-echo path, where an error
    // message from the server may contain % characters and is then
    // logged by the driver.
    const std::string filename = "rslog_fmt_cstr_" + std::to_string(getpid()) + ".log";
    setup_log(filename);
    std::string msg = "server error: relation \"%n%n\" does not exist";
    RS_LOG_ERROR("FMTTEST", "%s", msg.c_str());
    shutdownLogging();
    ASSERT_GE(findInFile(filename, "server error: relation \"%n%n\" does not exist"), 1);
    std::remove(filename.c_str());
}