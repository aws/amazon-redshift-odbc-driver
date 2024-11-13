#define ODBC_DRIVER_VERSION "2,0,1"
#define FILEVER             2,0,1
#define PRODUCTVER          2,0,1
#define STRFILEVER         "2, 0, 1, \0"
#define STRPRODUCTVER      "2, 0, 1, "
#define ODBC_DRIVER_VERSION_FULL "2.0.1.0"

const char *rsodbcVersion();
const char *rsodbcDependencyVersion(const char *package);
void logAllRsodbcVersions();
