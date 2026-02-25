Changelog
=========

v2.1.13.1 - Community Edition BL1 (2026-02-24)
---------------------
**Redshift ODBC Community Edition**
**Based on AWS Amazon Redshift ODBC v2.1.13 + Community Enhancements**

### Community Enhancements (Build Level 1):

**Critical Fixes:**
1. Fixed handling of UNSPECIFIEDOID (OID 0) and unknown data types by mapping them to LONGVARCHAR for better compatibility
   - Resolves "Unknown SQL type" errors with certain Redshift functions
   - Improves compatibility with functions like pg_get_session_roles() that return RECORD type

2. Enhanced Azure OAuth2 authentication with immediate error detection
   - User canceling Azure AD login now shows immediate, clear error message
   - Closing browser during authentication shows immediate error instead of timeout
   - Validates authorization code is received before proceeding

3. Fixed browser disconnection detection to fail fast instead of waiting for timeout
   - Detects browser disconnection early (max 3 consecutive empty requests)
   - Significantly reduces wait time when user cancels authentication
   - Adds comprehensive logging for debugging OAuth flow issues

**Build System Improvements:**
4. Added GitHub Actions automated CI/CD for MSI building on Windows
   - Reproducible builds without local Windows machine
   - Automated MSI creation on every commit
   - SHA256 checksum generation

5. Improved build system with vcpkg dependency management
   - Consistent, reproducible builds
   - Automated dependency installation
   - Version-locked dependencies for stability

6. Updated Azure OAuth2 implementation to use v2.0 endpoints consistently
   - Fixes authentication freeze issues
   - Proper PKCE support
   - client_secret support for confidential clients

### AWS Official Base (v2.1.13):
1. Improved error handling and SQL state reporting across SQLGetData, SQLPutData, SQLExtendedFetch, and SQLSetCursorName APIs
2. Fixed SQLGetTypeInfo to dynamically return column names based on the application's ODBC version
3. Added SQL_DESC_CONCISE_TYPE synchronization to comply with ODBC specification
4. Corrected default values for ARD, APD, and IPD descriptors as per ODBC specification
5. Added proper error messages when accessing non-readable or non-writable descriptor fields
6. Added length indicators for non-string data types to comply with ODBC specification
7. Enhanced escape clause handling by addressing gaps in existing implementation and adding support for previously missing functions
8. Improved logging in IAMJwtPluginCredentialsProvider
9. Fixed IdC Browser authentication plugin to respect configured HTTPS proxy settings
10. Prioritized configured region over DNS lookup for CNAME connections
11. Fixed SQLGetData to return correct octet length for numeric types
12. Fixed macOS build by converting std::string to C-string for snprintf compatibility

v2.1.12 (2025-12-18)
---------------------
1. Added apple macOS support with ARM64 and x86_64 architecture compatibility
2. Added support for both iODBC and unixODBC driver managers on apple macOS
3. Added UTF-32 encoding support on macOS complementing the existing UTF-16 support
4. Extended SQLSetEnvAttr and SQLSetConnectAttr to support SQL_ATTR_APP_UNICODE_TYPE, SQL_ATTR_APP_WCHAR_TYPE, and SQL_ATTR_DRIVER_UNICODE_TYPE attributes
5. Enhanced all ODBC wide character APIs with improved Unicode conversions, comprehensive input and buffer validations, robust error handling, and ODBC specification-compliant return codes (SQL_SUCCESS, SQL_SUCCESS_WITH_INFO)

v2.1.11 (2025-11-20)
---------------------
1. Added the idp_partition parameter which allows users to authenticate against Azure Active Directory across different Microsoft cloud environments (e.g., Global, US Gov, China).
2. Enhanced SQLTables and SQLColumns metadata APIs to support uppercase column names in server responses.
3. Added warning messages when DEBUG or TRACE log levels are enabled.
4. Removed unsupported PostgreSQL replication features.
5. Added native ARM64 support for RPM-based Linux distributions.
6. Enhanced database metadata retrieval logic in SQLTables(), SQLColumns(), SQLPrimaryKeys(), SQLForeignKeys(), SQLSpecialColumns(), SQLColumnPrivileges(), SQLTablePrivileges(), SQLProcedures(), and SQLProcedureColumns() API methods to enable data sharing capabilities while maintaining ODBC specification compliance.
7. Fixed IDC authentication redirect URL for China regions.

v2.1.10 (2025-10-14)
---------------------
1. Standardized logging output to conform with ODBC logger format specifications.
2. Updated SQLGetInfo to reflect existing String and Date/Time function support.
3. Removed unsupported client/stdin Copy and Unload feature implementation that was no longer maintained or supported.
4. Fixed SQLGetConnectAttr API to correctly return StringLengthPtr for NULL ValuePtr in string attribute scenarios.
5. Fixed buffer length validation for integer attributes in SQLGetConnectAttr.
6. Improvements for logging.

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
