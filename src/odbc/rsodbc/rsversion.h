#include <vector>
#include<string>

#define ODBC_DRIVER_VERSION "2,1,11"
#define FILEVER             2,1,11
#define PRODUCTVER          2,1,11
#define STRFILEVER         "2, 1, 11, \0"
#define STRPRODUCTVER      "2, 1, 11, "
#define ODBC_DRIVER_VERSION_FULL "2.1.11.0"

// Return ODBC version as string
std::string rsodbcVersion();

// Return dependency version as string, given dependency name
std::string rsodbcDependencyVersion(const std::string &dependency);

// Inserts the list of available dependencies whose version is controlled.
// Returns the number of items inserted
int getVersionedDependencyNames(std::vector<std::string> &dependencies);

// Write ODBC and dependency versions into ODBC log file.
void logAllRsodbcVersions();
