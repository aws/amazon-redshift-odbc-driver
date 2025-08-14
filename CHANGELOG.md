Changelog
=========

v2.1.9 (2025-08-14)
---------------------
1. Fixed row-wise parameter binding in SQLBindParameter by implementing correct length indicator pointer and stride calculations.

v2.1.8 (2025-07-01)
---------------------
1. Added support to change the connection to read-write using mode SQL_ATTR_ACCESS_MODE after the connection was established.
2. Fixed the return data type for Date/Time columns in SQLColumns API to match the configured ODBC version set via SQL_ATTR_ODBC_VERSION.
3. Fixed incorrect streaming cursor state handling after SQLTables API catalog list retrieval. 
4. Fixed an issue where the driver incorrectly treated NULL parameters as empty strings in SQLTables when using SQL_ALL_CATALOGS, which caused it to return a catalog list instead of the expected table list.
5. Removed legacy code containing unused libpq quoting functions.
6. Resolved build issues when compiling the Windows driver from GitHub source code.
7. Added support for all Glue Data type in the SQLColumns metadata API, enabling accurate column type information retrieval from AWS Glue catalogs.
8. Enhanced the ODBC driver's data retrieval functionality by implementing proper offset tracking and buffer management through SQLGetData API to correctly handle large data sets retrieved in chunks, ensuring sequential data access rather than repeated retrieval of initial data segments.
9. Fixed a memory leak issue that occurred when initializing result set fields in SQLTables and SQLColumns API calls.

v2.1.7 (2025-03-06)
---------------------
1. Fixed an issue with batch insertion parameters where SQL_ATTR_PARAMSET_SIZE updates were not being properly applied between SQLPrepare and SQLExecute calls.
2. Updated to latest OpenSSL 1.1.1.
3. Fixed SQLTables metadata API to properly include external schemas when listing all schemas in the current database.
4. Corrected SQLTables metadata API result ordering to properly sort by catalog name.
5. Added support for serverless cluster authentication via GetClusterCredentials API - connect to workgroups by prefixing “redshift-serverless” to ClusterId field (server field optional).
6. Added support for network load balancer (NLB) hostnames in server name fields.

v2.1.6 (2024-12-23)
---------------------
1. Introduced token caching support for Browser IdC Auth plugin.
2. Updated the logic for retrieving database metadata through the SQLTables() and SQLColumns() API methods.
3. Addressed security issues as detailed in CVE-2024-12746
4. Fixed an issue where data types returned for the BUFFER_LENGTH, DECIMAL_DIGITS, NUM_PREC_RADIX, and NULLABLE columns= in the SQLColumns ODBC API call result set for external schema did not conform to the ODBC specification.
5. Increased buffer size of MAX_IDEN_LEN to support connection parameter inputs up to 1024 characters.
6. Improved build system, removed prebuilt dependencies and introduced an automated script to build driver dependencies from source.

v2.1.5 (2024-11-21)
---------------------
1. This driver version has been recalled. ODBC Driver version 2.1.4 is recommended for use instead.

v2.1.4 (2024-11-12)
---------------------
1. Increased the session token buffer size from 1024 bytes to 2048 bytes to accommodate larger V2 tokens during IAM authentication processes.
2. Improved regex processing of user provided URIs in all IAM authentication workflows.
3. Improved String Manipulation Functions: stristr (Case-Insensitive String Search), strcasewhole (Whole Word Case-Insensitive Comparison), strcasestr (Custom Case-Insensitive Substring Search for windows), and findSQLClause (SQL Clause Extraction).
4. Fixed Unicode Handling in "getParamVal" for Prepared Query Processing.
5. Updated the version of aws-sdk-cpp to 1.11.336 (windows).
6. Implemented mandatory default functionality for aws-sdk-cpp's WinSyncHttpClient::DoQueryDataAvailable and WinSyncHttpClient::GetActualHttpVersionUsed methods, ensuring compliance with the AWS SDK.
7. Upgraded the c-ares library dependency for Windows to version 1.29.0
8. Upgraded the googletest library dependency for Windows to version 1.14.0
9. Updated the cmake_minimum_required version from 3.12 to 3.20
10. Migrated the Windows build process to utilize the CMake build system. To build and package for Windows, execute the build64.bat script located in the project's root directory.
11. Added support for building ODBC2.x Windows driver in both Release and Debug modes, ensuring that the required dependencies are available in their respective Debug and Release subfolders, enabling more comprehensive testing and debugging scenarios.
12. Enhanced the build scripts by removing unused build and test files, and implemented a consistent naming convention by prefixing internal CMake variables with 'RS_' for better organization and maintainability.
13. Removed the use of the -MMD, -MP, and -MF compiler options in the CMake build configuration for Linux, resolving compatibility issues with C++17 std::string_view.
14. Fixed an issue where the data types returned for the BUFFER_LENGTH, DECIMAL_DIGITS, NUM_PREC_RADIX, and NULLABLE columns in the result set of the SQLColumns ODBC API call were not conforming to the ODBC specification.
15. Updated the SQL data type representations in the rscatalog component for 'date', 'time', 'timetz', 'time with time zone', 'timestamp without time zone', 'timestamptz', and 'timestamp' to use the non-concise data type instead of the concise data type.
16. Added support to connect to Redshift via managed Virtual Private Cloud (VPC) endpoints. 

v2.1.3 (2024-07-31)
---------------------
1. Fixed an inconsistent metadata issue for Redshift type timestamptz in SQLColumns and SQLDescribeCol by changing the return value for Data type column in SQLColumns API from 2014 to SQL_TYPE_TIMESTAMP (93) and in SQLDescribeCol API from SQL_VARCHAR (12) to SQL_TYPE_TIMESTAMP (93).
2. Fixed an issue when invoking SQLBindParameter with the C type SQL_C_DEFAULT by converting SQL_C_DEFAULT to a new C type based on the SQL type. Also, added enhanced logging to provide informational and error messages related to SQL_C_DEFAULT conversions.
3. Added missing group federation checkbox in JWT IAM Auth Plugin ODBC DSN GUI in Windows.
4. Fixed an issue where an incorrect return value was generated when calling SQLGetData with the C type SQL_C_DEFAULT.
5. Added support for a new browser authentication plugin called BrowserIdcAuthPlugin to facilitate single-sign-on integration with AWS IAM Identity Center.

v2.1.2 (2024-06-05)
---------------------
1. Upgraded the deprecated minimum TLS functions in OpenSSL to more versatile TLS methods that can support multiple TLS protocol versions.
2. Updated the default minimum TLS version to the more secure and faster TLS 1.2, changed from the previous default of TLS 1.0. 
3. Added new options in the Windows ODBC DSN GUI to allow setting the minimum TLS version to any supported version.
4. Fixed a bug where the user option to provide the database username in the connection string was not working when connecting to Redshift using a database username and password.
5. Fixed an issue with querying large geometry data type using SQLGetData.
6. Fixed an issue in SQLFetch function where query buffer offset was not being reset properly, causing function execution to hang.
7. Fixed issues in unicode conversion utility methods used internally. Updated calls to these conversion methods in the SQLForeignKeysW, SQLPrimaryKeysW, SQLColumnsW and SQLDriverConnectW APIs.
8. Added missing OID mapping for Redshift Text data type.
9. Added missing invalid SQL type validation in SQLBindParameter API.
10. Fixed bug where databaseMetadataCurrentDbOnly flag was not using its default value when it is not included in DSN in Linux.
11. Updated catalog filter to use LIKE instead of = to allow filter patterns with '%' in catalog name in ODBC metadata APIs like SQLTables.
12. Fixed a bug where datatype of DECIMAL_DIGITS, NUM_PREC_RADIX, and NULLABLE columns in SQLColumns API result set were not following ODBC specification. This can impact behavior on some clients and tools.

v2.1.1 (2024-04-01)
---------------------
1. Corrected the data type of the bind offset pointer to SQLLEN from long for the plBindOffsetPtr variable in RS_DESC_HEADER to ensure compatibility
2. Modified the default value for SQL_BOOKMARK_PERSISTENCE from SQL_BP_UPDATE | SQL_BP_SCROLL to SQL_BP_DROP
3. Upgraded logging capabilities and streamlined the logging system to boost speed and transparency.
4. Expanded unicode library capabilities by introducing wchar16_to_utf8_char() to convert 16-bit wide characters to UTF-8 char array. 
5. Improved buffer safety checks in unicode function char_utf8_to_wchar_utf16() to make it more robust.
6. Enhanced unicode conversion within SQLTablesW and SQLPrepareW for improved compatibility and accuracy.
7. Resolved issues causing Power Query container crashes in Microsoft products.
8. The catalog filter was updated to use 'LIKE' instead of '+' to allow filter patterns with '%' which impacted metadata ODBC APIs like SQLTables.
9. In case the IAM authentication client cannot deduce the workgroup configuration from the serverless destination address, use the user-provided DSN setting as an alternative.
10. Invoke both cmake.find_library() and cmake.find_package() functions to locate the Gtest library, so that if one method fails to find Gtest, the other can be used as a fallback option.
11. Ensure that CaFile and CaPath connection settings are available to both IAM as well as Idp authentication plugins.
12. The TCP Proxy implementation in the DSN GUI was improved by fixing issues related to saving and loading proxy settings. Additionally, the connection handling routines in lipbq's library were enhanced to better support connections made through proxies. Specifically, the connect_using_proxy, connectDBStart, PQconnectPoll, and internal_cancel functions were updated to properly handle proxy configurations.
14. Updated the registry path that stores Windows DSN log settings for the ODBC driver, from "HKEY_CURRENT_USER\SOFTWARE\ODBC\ODBC.INI\ODBC" to "HKEY_LOCAL_MACHINE\SOFTWARE\Amazon\Amazon Redshift ODBC Driver (x64)\Driver".

**Note:** 
* Going forward, this software will use Semantic Versioning (SemVer) for version numbering. SemVer versions consist of three numbers in the MAJOR.MINOR.PATCH format. For compatibility purposes, the build id (4th number) will continue to be included only in the package name.
* Windows users should update the log settings for their DSNs to use this new registry path, if needed.

v2.1.0.0 (2024-02-28)
------------------------
1. Added support for LZ4 and ZSTD compression over wire protocol communication between Redshift server and the client/driver. Compression is turned off by default and can be set using a new connection parameter compression=off or lz4 or zstd.
2. Added support for unit-testing using Google Test framework.
3. Upgrade unixODBC dependency from v2.3.1 to v2.3.7 (Linux release).
4. Fix handling of null termination during unicode processing in SQLDescribeColW API.

v2.0.1.0 (2024-01-25)
------------------------
1. Added support for Serverless Custom Name. Upgraded windows aws-sdk-cpp to 1.11.218 accordingly. 
2. Added support for interval data types: SQL_INTERVAL_YEAR_TO_MONTH and SQL_INTERVAL_DAY_TO_SECOND. They can be retrieved as text or using the C structures SQL_YEAR_MONTH_STRUCT and SQL_DAY_SECOND_STRUCT.
3. Migrated Linux to cmake build system.
4. Build base libraries statically into librsodbc64.so including libgcc and libstdc++.
5. Fixed CaFile and CaPath connection options and default root certificate name for IAM and non-IAM connections.
6. Fixed support for group_federation connection parameter that allows customers to use getClusterCredentialsWithIAM in provisioned clusters.
7. Refactor and extend Logging system to accept options from connection string.
8. Removed ssooidc dependencies from build system.

v2.0.0.11 (2023-11-21)
------------------------
1. Upgraded openssl (v1.1.1w) and removed redundant libcURL build dependencies for Windows.
2. Removed outdated Linux build dependencies.
3. Added ability to connect to datashare databases for clusters and serverless workgroups running the PREVIEW_2023 track
4. Removed BrowserIdcAuthPlugin.

v2.0.0.10 (2023-11-01)
------------------------
1. Disabled the rudimentary check in SQLColumns, SQLStatistics and SQLSpecialColumns APIs, for equality of catalog name and database name.
2. Improved ODBC Error messages for unsupported metadata features.
3. Allowed overriding DSN config options in connection string.
4. Fixed memory issue in Statement Descriptors and Query Parameter types by using stack memory vs heap allocation.
5. Add new logging module based on aws-sdk-cpp. It has standard logging levels from FATAL to TRACE, plus OFF mode.
6. Upgraded SQLGetStmtOption  and SQLSetStmtOption APIs to be compatible with their new versions in ODBC standard 3: SQLGetStmtAttr, SQLSetStmtAttr
7. Fixed SQLSetConnectAttr to return SQL_ERROR for SQL_ATTR_ANSI_APP as per standard.
8. Minor adjustments in build scripts (windows and linux) and windows’s config page.
9. Note: This version was recalled. The changes will be available in the next version.

v2.0.0.9 (2023-09-11)
==================

1. Added Identity Center authentication support with new plugins.
2. Fixed IAM Authentication to support longer temporary passwords from 129 to 32767 characters.
3. Added automatic region detection for Custom Cluster Naming (CNAME) feature.
4. Make uniform MetaData queries to return 'TABLE' as table_type for 'EXTERNAL TABLE's too.

v2.0.0.8 (2023-08-15)
---------------------
- Fixed regular expressions used for parsing HTML Tag value responses received form for Ping IdP.[Ruei-Yang Huang]
- Added a new JWT IAM authentication plugin (JwtIamAuthPlugin) to replace BasicJwtCredentialsProvider which has been repurposed to support the new nativeIdP authentication.[Naveen Kumar]
- Revert a memory issue fix in libpq’s MesageLoopState that was resulting in crashes since v2.0.0.6.[Vahid Saber Hamishagi]
- Fixed enablement settings for Custom-Cluster Naming (CNAME) for Windows platform which was disabling CNAME permanently.[Janak Khadka]



v2.0.0.7 (2023-07-06)
---------------------
- Upgrade windows aws-sdk-cpp to 1.11.111. [Janak Khadka]
- Enable CNAME for windows. [Janak Khadka]
- Fix windows log settings.  [Vahid Saber Hamishagi]
- FAdd new pg type: NAMEARRAYOID.  [Vahid Saber Hamishagi]
- Fix NULLABLE and IS_NULLABLE definition in Metadata.  [Vahid Saber Hamishagi]
- Fix SQL_C_CHAR&SQL_C_WCHAR -> SQL_VARCHAR conversion.  [Vahid Saber Hamishagi]

v2.0.0.6 (2023-05-23)
---------------------
- Fix memory leak detected in API tests and conformance tests. [Ruei-Yang Huang]
- Add test to verify the correctness of the 'character_octect_length' column retrieved from 'SQLColumns' function [Janak Khadka]
- Support query column result larger than 2048 characters.  [Vahid Saber Hamishagi]


v2.0.0.5 (2023-03-28)
---------------------
- Minor codestyle changes. [Vahid Saber Hamishagi]
- Fix Driver insert NULL into geometry column instead of returning error. [Janak Khadka]
- Fix large column size issue in SQL_C_WCHAR->SQL_VARCHAR conversion.  [Vahid Saber Hamishagi]
- Fix user AutoCreate feature in IAM. [Janak Khadka]


v2.0.0.3 (2023-02-10)
---------------------
- Unicode Conversion Fix. [Vahid Saber Hamishagi]
- Improve parsing Connection String containing special characters. [Vahid Saber Hamishagi]
- IAM imporovements. [Vahid Saber Hamishagi]
- Fixes in plugin "Identity Provider: PingFederate". [Vahid Saber Hamishagi]
- Replace auto_ptr with unique_ptr in IAM. [Vahid Saber Hamishagi]
- ARN Fix. [Vahid Saber Hamishagi]
- Upgrade Linux build to C++17. [Vahid Saber Hamishagi]
- Upgrade windows build. [Vahid Saber Hamishagi]
- Upgrade to ssl 1.1.1. [Vahid Saber Hamishagi]
- Upgrade to aws-cpp-sdk v1.9.289. [Vahid Saber Hamishagi]
- Tracking large files(*.lib files) [Vahid Saber Hamishagi]


v2.0.0.1 (2022-07-18)
---------------------
- Fixes from security review. [ilesh Garish]
- Update README.md. [iggarish]
- Create CODEOWNERS. [iggarish]
- Update THIRD_PARTY_LICENSES. [iggarish]
- Sync with latest source code. [ilesh Garish]
- Update CONTRIBUTING.md. [iggarish]
- Create checkstyle.xml. [iggarish]
- Create ISSUE_TEMPLATE.md. [iggarish]
- Create THIRD_PARTY_LICENSES. [iggarish]
- Create PULL_REQUEST_TEMPLATE.md. [iggarish]
- Create CHANGELOG.md. [iggarish]
- Added initial content in README file. [ilesh Garish]
- Added link of open issues and close issues. [ilesh Garish]
- Initial version. [Ilesh Garish]
- Initial commit. [Amazon GitHub Automation]


