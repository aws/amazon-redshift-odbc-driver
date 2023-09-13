Changelog
=========
v2.0.0.9 (2023-09-11)
==================

1. Added Identity Center authentication support with new plugins.
2. Fixed IAM Authentication to support longer temporary passwords from 129 to 32767 characters.
3. Added automatic region detection for Custom Cluster Naming (CNAME) feature.


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


