
#include "common.h"
#include <rslog.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <regex>

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
    pid_t actualPid = getpid();
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