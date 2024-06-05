#include "common.h"
#include <rsversion.h>

#include <fstream>
#include <iostream>
#include <string>

std::string getVersionFromVersionTxt(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file: " << filename << std::endl;
        return "";
    }

    std::string firstLine;
    std::getline(file, firstLine);

    file.close();
    char findChar = ' ';
    char replaceChar = '.';

    size_t pos = 0;
    while ((pos = firstLine.find(findChar, pos)) != std::string::npos) {
        // Replace 1 character at position 'pos'/ with 'replaceChar'
        firstLine.replace(pos, 1, 1, replaceChar);
        pos++; // Move to the next position to search from
    }

    return firstLine;
}

TEST(VERSION_TEST_SUITE, odbc_version) {
    ASSERT_EQ(ODBC_DRIVER_VERSION_FULL, rsodbcVersion())
        << "rsodbcVersion(" << rsodbcVersion()
        << ") Does not return value of ODBC_DRIVER_VERSION_FULL("
        << ODBC_DRIVER_VERSION_FULL << ")";
}

TEST(VERSION_TEST_SUITE, odbc_version_txt) {

    ASSERT_EQ(rsodbcVersion(), getVersionFromVersionTxt("version.txt"))
        << "rsodbcVersion(" << rsodbcVersion()
        << ") Does not return value of version.txt("
        << getVersionFromVersionTxt("version.txt") << ")";
}

TEST(VERSION_TEST_SUITE, odbc_dependency_search_missing) {
    ASSERT_TRUE(rsodbcDependencyVersion("XYZ") == "Dependency 'XYZ' not found")
        << "rsodbcDependencyVersion(): No error on invalid dependency.";
}

TEST(VERSION_TEST_SUITE, odbc_dependency_change_check) {

// Any change to the number or names of dependencies will be detected. And this
// test will enforce a manual check
    std::set<std::string> expectedDependencies = {"aws-sdk-cpp", "openssl", "c-ares"};
#ifdef LINUX
    expectedDependencies.insert({"aws-sdk-cpp", "openssl", "curl", "nghttp2", "zlib"});
#endif
    std::vector<std::string> dependencies;

    int depCnt = getVersionedDependencyNames(dependencies);
    ASSERT_EQ(depCnt, expectedDependencies.size());
    std::set<std::string> dependenciesSet(dependencies.begin(),
                                          dependencies.end());
    ASSERT_EQ(dependenciesSet, expectedDependencies)
        << "Mismatch in the expected contents of the dependencies.";

    // Feed an invalid name to the list of valid names to make sure the
    // follwoing logic catches it
    expectedDependencies.insert("XYZ");
    std::stringstream assertMsg;
    int totalErrors = 0;
    for (auto dependency : expectedDependencies) {
        std::string unexpectedErrorMsg("Dependency '" + dependency + "' not found");
        bool isError = rsodbcDependencyVersion(dependency) == unexpectedErrorMsg;
        if (isError) {
            assertMsg << "Expected Dependency '" << dependency
                      << "' not found: " << unexpectedErrorMsg << std::endl;
        }
        totalErrors += isError;
    }

    ASSERT_EQ(totalErrors, 1) << assertMsg.str(); // 1 is the invalid name
}
