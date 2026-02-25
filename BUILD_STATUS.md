# Amazon Redshift ODBC Driver - Build Status & Changes Log

## מצב נוכחי (2026-02-02)

### ✅ Build מצליח
- **Branch**: `fix-azure-oauth-scope`
- **GitHub Actions**: הצלחה
- **Latest Build**: [Run #21601686394](https://github.com/ORELASH/amazon-redshift-odbc-driver/actions/runs/21601686394)
- **Build Duration**: 21m 53s
- **MSI Output**: `AmazonRedshiftODBC64-2.1.12.0.msi` (5MB)

### תיקונים שבוצעו

#### 1. Azure OAuth2 - הוספה אוטומטית של `openid` scope
**קובץ**: `src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp`

**שינויים**:
- שורות 215-224: הוספת `openid` ב-`RequestAuthorizationCode()`
- שורות 275-284: הוספת `openid` ב-`RequestAccessToken()`
- שורות 305-310: תמיכה ב-`client_secret` parameter (תיקון ל-Issue #16)

**תכלית**:
- תאימות עם JDBC driver behavior
- פתרון לבעיות authentication עם Azure AD
- משתמש לא צריך להוסיף `openid` ידנית יותר

#### 2. WiX Installer - תיקון נתיבי OpenSSL
**קובץ**: `src/odbc/rsodbc/install/rsodbcm_x64.wxs`

**הבעיה המקורית**:
```xml
<File Id="libcrypto_1_1_x64.dll" Source="$(var.DependenciesDir)/openssl/Release/bin/libcrypto-1_1-x64.dll" />
<File Id="libssl_1_1_x64.dll" Source="$(var.DependenciesDir)/openssl/Release/bin/libssl-1_1-x64.dll" />
```

**התיקון**:
```xml
<File Id="libcrypto_3_x64.dll" Source="$(var.DependenciesDir)/bin/libcrypto-3-x64.dll" />
<File Id="libssl_3_x64.dll" Source="$(var.DependenciesDir)/bin/libssl-3-x64.dll" />
```

**סיבה**:
- vcpkg מתקין OpenSSL 3.x (לא 1.1.x)
- הנתיבים ב-vcpkg שונים מבנייה ידנית
- שמות הקבצים שונים בין גרסאות

**שגיאה שתוקנה**:
```
error LGHT0103: The system cannot find the file '.../openssl/Release/bin/libcrypto-1_1-x64.dll'
```

#### 3. CMake & Build Configuration Updates
**קבצים**:
- `CMakeLists.txt`
- `build64.bat`
- `cmake/Common.cmake`
- `cmake/Windows.cmake`

**שינויים**:
- תמיכה ב-vcpkg paths
- תיקוני linking ל-ODBC libraries
- עדכון AWS SDK features (sso-oidc, redshift-serverless)
- מעבר מ-static ל-dynamic MSVC runtime

#### 4. GitHub Actions Workflow
**קובץ**: `.github/workflows/build-windows-driver.yml`

**תכונות**:
- Build אוטומטי על Windows Server 2022
- vcpkg integration עם cache
- Retry logic להתקנת dependencies
- Artifacts upload (MSI + logs)

## היסטוריית Builds

### Build #21601686394 (2026-02-02 18:07) ✅
- **Commit**: `eea8c48` - "Fix WiX installer: update OpenSSL DLL paths for vcpkg"
- **Status**: SUCCESS
- **Duration**: 21m 53s
- **Output**: MSI installer created successfully

### Builds קודמים (כולם נכשלו)
- #21597500841 - Failed (OpenSSL DLL path issues)
- #21595654298 - Failed (OpenSSL DLL path issues)
- #21594859450 - Failed (OpenSSL DLL path issues)
- ...31 builds נכשלו בסך הכל לפני התיקון

**סיבת הכשלונות**: WiX installer לא מצא את קבצי OpenSSL DLLs

## Dependencies

### vcpkg Packages
```
- openssl (3.x)
- aws-sdk-cpp[core,redshift,redshift-serverless,sts,sso-oidc,identity-management]
- c-ares
- gtest
```

### Build Tools
- Visual Studio 2022
- CMake
- NASM (for OpenSSL)
- Perl (Strawberry Perl)
- WiX Toolset 3.14

## דרכי Build

### GitHub Actions (אוטומטי)
Push ל-branch `fix-azure-oauth-scope` מפעיל build אוטומטי.

### Local Build (Windows)
```cmd
REM Clone and checkout
git clone https://github.com/ORELASH/amazon-redshift-odbc-driver.git
cd amazon-redshift-odbc-driver
git checkout fix-azure-oauth-scope

REM Install vcpkg dependencies
vcpkg install --triplet x64-windows openssl aws-sdk-cpp[...] c-ares gtest

REM Build
build64.bat --dependencies-install-dir=vcpkg\installed\x64-windows --version=2.1.13.0
```

## Testing

### Manual Testing Required
1. התקן את ה-MSI על Windows
2. פתח ODBC Data Source Administrator (64-bit)
3. צור DSN חדש עם:
   - Auth Type: Identity Provider: Browser Azure AD OAUTH2
   - Scope: `api://YOUR-APP-ID/jdbc_login` (ללא openid!)
   - Client ID: YOUR-CLIENT-ID
   - Client Secret: YOUR-SECRET (אם נדרש)
   - Tenant: YOUR-TENANT-ID
4. בדוק שה-driver מוסיף אוטומטית `openid`
5. בדוק שההתחברות מצליחה

## Known Issues & Warnings

Build מצליח עם warnings:
- `'IDC_CHECK1' : redefinition` (resource file warnings)
- `'IDC_COMBO_KSA' : redefinition` (resource file warnings)
- `'handleFederatedNonIamConnection': not all control paths return a value`
- `Some test files are not available`

אלו warnings קיימים גם ב-upstream repository ולא מונעים את הפעולה התקינה.

## Next Steps

### להשלמת הפרויקט:
1. ✅ Build מצליח ב-GitHub Actions
2. ⏳ בדיקות manual על Windows
3. ⏳ יצירת PR ל-upstream repository
4. ⏳ תיעוד נוסף לפי צורך

### לשיפור:
- [ ] הוסף unit tests ל-Azure OAuth scope logic
- [ ] תיקון warnings בקוד
- [ ] CI/CD pipeline מלא עם automated testing

## Repository Structure

```
amazon-redshift-odbc-driver/
├── .github/workflows/
│   └── build-windows-driver.yml     # GitHub Actions workflow
├── cmake/
│   ├── Common.cmake                 # Common CMake config
│   └── Windows.cmake                # Windows-specific config
├── src/odbc/rsodbc/
│   ├── iam/plugins/
│   │   └── IAMBrowserAzureOAuth2CredentialsProvider.cpp  # Main fix
│   └── install/
│       ├── rsodbcm_x64.wxs         # WiX installer config (fixed)
│       └── Make_x64.bat            # WiX build script
├── build64.bat                      # Windows build script
├── CMakeLists.txt                   # Main CMake config
├── AZURE_OAUTH_FIX_INSTRUCTIONS.md  # Build instructions
├── README_AZURE_FIX.md              # Fix overview
└── BUILD_STATUS.md                  # This file
```

## Commits History

```
eea8c48 - Fix WiX installer: update OpenSSL DLL paths for vcpkg
671c385 - Fix connect.exe linking on Windows: add ODBC libraries
b29ffc2 - Switch from static to dynamic MSVC runtime library
d8168d8 - Add sso-oidc and redshift-serverless to AWS SDK features
28b2c02 - Fix CMake path quoting: remove escaped quotes
8a90e4b - Fix vcpkg paths: add direct include/lib before subdirs
... (31 commits מעל main branch)
```

## Contact & Support

- **Repository**: https://github.com/ORELASH/amazon-redshift-odbc-driver
- **Branch**: fix-azure-oauth-scope
- **Original Issue**: #16 (client_secret support)
- **Upstream**: https://github.com/aws/amazon-redshift-odbc-driver

---

*Last Updated: 2026-02-02*
*Build Status: ✅ SUCCESS*
