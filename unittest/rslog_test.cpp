
#include "common.h"
#include <rslog.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>

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