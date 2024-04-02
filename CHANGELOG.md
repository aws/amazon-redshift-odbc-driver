Changelog
=========
v2.1.1 (2024-04-01)
-------------------

1. **Data Type Fixation:**

    Corrected the data type of the bind offset pointer to SQLLEN from long for the plBindOffsetPtr variable in RS_DESC_HEADER to ensure compatibility

2. **Default Value Adjustment:**

    Modified the default value for SQL_BOOKMARK_PERSISTENCE from SQL_BP_UPDATE | SQL_BP_SCROLL to SQL_BP_DROP

3. **Logging Enhancement:**

    Upgraded logging capabilities and streamlined the logging system to boost speed and transparency.

4. **Unicode Library Expansion:**

    Expanded unicode library capabilities by:
    
      * Introducing wchar16_to_utf8_char() to convert 16-bit wide characters to UTF-8 char array.
      * Improved buffer safety checks in the char_utf8_to_wchar_utf16() function to make it more robust.

5. **Unicode Conversion Improvement:**

    Enhanced unicode conversion within SQLTablesW and SQLPrepareW for improved compatibility and accuracy.

6. **Bug Fixes:**

    Resolved issues causing Power Query container crashes in Microsoft products.
    
      * The catalog filter was updated to use 'LIKE' instead of '+' to allow filter patterns with '%' which impacted metadata ODBC APIs like SQLTables.
      * In case the IAM authentication client cannot deduce the workgroup configuration from the serverless destination address, use the user-provided DSN setting as an alternative.
      * Invoke both cmake.find_library() and cmake.find_package() functions to locate the Gtest library, so that if one method fails to find Gtest, the other can be used as a fallback option.
      * Ensure that CaFile and CaPath connection settings are available to both IAM as well as Idp authentication plugins.

7. **TCP Proxy Enhancement and Fixes:**

    The TCP Proxy implementation in the DSN GUI was improved by fixing issues related to saving and loading proxy settings. Additionally, the connection handling routines in lipbq's library were enhanced to better support connections made through proxies. Specifically, the connect_using_proxy, connectDBStart, PQconnectPoll, and internal_cancel functions were updated to properly handle proxy configurations.


8. **Configuration Update:**

    Updated the registry path that stores Windows DSN log settings for the ODBC driver, from HKEY_CURRENT_USER\SOFTWARE\ODBC\ODBC.INI\ODBC to HKEY_LOCAL_MACHINE\SOFTWARE\Amazon\Amazon Redshift ODBC Driver (x64)\Driver.
Windows users should update the log settings for their DSNs to use this new registry path, if needed.

**Note:** Going forward, this software will use Semantic Versioning (SemVer) for version numbering. SemVer versions consist of three numbers in the MAJOR.MINOR.PATCH format. For compatibility purposes, the build id (4th number) will continue to be included only in the package name.

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


