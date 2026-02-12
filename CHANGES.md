# Amazon Redshift ODBC Driver - מסמך שינויים מפורט

## סקירה כללית
מסמך זה מפרט את **כל השינויים** שבוצעו ב-Amazon Redshift ODBC Driver כדי לתקן בעיות Azure AD OAuth2 authentication.

---

## 📝 רשימת קבצים ששונו

| # | קובץ | סוג שינוי | חשיבות |
|---|------|-----------|---------|
| 1 | `IAMBrowserAzureOAuth2CredentialsProvider.cpp` | Core Fix | ⭐⭐⭐ קריטי |
| 2 | `rsodbcm_x64.wxs` | Installer Fix | ⭐⭐⭐ קריטי |
| 3 | `build64.bat` | Build Script | ⭐⭐ חשוב |
| 4 | `CMakeLists.txt` | Build Config | ⭐⭐ חשוב |
| 5 | `cmake/Common.cmake` | CMake Helper | ⭐ בינוני |
| 6 | `cmake/Windows.cmake` | Windows Config | ⭐ בינוני |
| 7 | `src/odbc/rsodbc/CMakeLists.txt` | Linking Config | ⭐⭐ חשוב |
| 8 | `src/odbc/rsodbc/samples/connect/CMakeLists.txt` | Sample Config | ⭐ נמוך |
| 9 | `.github/workflows/build-windows-driver.yml` | CI/CD | ⭐⭐⭐ קריטי |
| 10 | `exports_basic.bat` | Environment | ⭐ נמוך |

---

## 🔍 שינויים מפורטים לפי קובץ

### 1. ⭐⭐⭐ IAMBrowserAzureOAuth2CredentialsProvider.cpp
**מיקום**: `src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp`

**מטרה**: תיקון הבעיה העיקרית - הוספה אוטומטית של `openid` ל-scope

#### שינוי A: RequestAuthorizationCode() - שורות 215-224
```cpp
// לפני:
const rs_string uri = idpHostUrl + "/" +
    m_argsMap[IAM_KEY_IDP_TENANT] +
    "/oauth2/authorize?client_id=" +
    m_argsMap[IAM_KEY_CLIENT_ID] +
    "...&scope=" + m_argsMap[IAM_KEY_SCOPE] +  // ❌ ישיר ללא בדיקה
    "...";

// אחרי:
rs_string scopeParam = m_argsMap[IAM_KEY_SCOPE];
rs_string scope;
if (scopeParam.find("openid") == rs_string::npos) {
    scope = "openid%20" + scopeParam;  // ✅ הוסף openid עם URL encoding
    RS_LOG_DEBUG("IAMCRD", "RequestAuthorizationCode: Added 'openid' to scope");
} else {
    scope = scopeParam;
    RS_LOG_DEBUG("IAMCRD", "RequestAuthorizationCode: Scope already contains 'openid'");
}
const rs_string uri = "...&scope=" + scope + "...";
```

**הסבר**:
- בודק אם `openid` כבר קיים ב-scope
- אם לא קיים - מוסיף אותו עם רווח מקודד (`%20`)
- אם קיים - משתמש ב-scope כמו שהוא
- מוסיף logging לdebug

#### שינוי B: RequestAccessToken() - שורות 274-284
```cpp
// לפני:
std::map<rs_string, rs_string> paramMap = {
    { "scope", m_argsMap[IAM_KEY_SCOPE] },  // ❌ ישיר מה-config
    ...
};

// אחרי:
rs_string scopeParam = m_argsMap[IAM_KEY_SCOPE];
rs_string scope;
if (scopeParam.find("openid") == rs_string::npos) {
    scope = "openid " + scopeParam;  // ✅ הוסף openid עם רווח רגיל
    RS_LOG_DEBUG("IAMCRD", "Added 'openid' prefix to scope. Final scope: %s", scope.c_str());
} else {
    scope = scopeParam;
    RS_LOG_DEBUG("IAMCRD", "Scope already contains 'openid': %s", scope.c_str());
}

std::map<rs_string, rs_string> paramMap = {
    { "scope", scope },  // ✅ משתמש ב-scope המתוקן
    ...
};
```

**הסבר**:
- אותה לוגיקה כמו ב-RequestAuthorizationCode
- רווח רגיל (לא מקודד) כי זה POST body, לא URL
- Logging מפורט עם ה-scope הסופי

#### שינוי C: client_secret support - שורות 305-310
```cpp
// לפני:
std::map<rs_string, rs_string> paramMap = {
    { "grant_type", "authorization_code" },
    { "client_id", m_argsMap[IAM_KEY_CLIENT_ID] },
    // ❌ client_secret חסר
    ...
};

// אחרי:
std::map<rs_string, rs_string> paramMap = {
    { "grant_type", "authorization_code" },
    { "client_id", m_argsMap[IAM_KEY_CLIENT_ID] },
    ...
};

// ✅ הוסף client_secret אם קיים
if (m_argsMap.find(IAM_KEY_CLIENT_SECRET) != m_argsMap.end() &&
    !m_argsMap[IAM_KEY_CLIENT_SECRET].empty()) {
    paramMap["client_secret"] = m_argsMap[IAM_KEY_CLIENT_SECRET];
    RS_LOG_DEBUG("IAMCRD", "client_secret parameter added to token request");
}
```

**הסבר**:
- מוסיף תמיכה ב-confidential clients (עם secret)
- בודק שה-secret קיים ולא ריק
- מוסיף אותו ל-token request רק אם נדרש
- פותר את Issue #16

**השפעה**: ⭐⭐⭐ קריטי
- ללא תיקון זה - אין authentication עם Azure AD
- עם התיקון - עובד אוטומטית כמו JDBC driver

---

### 2. ⭐⭐⭐ rsodbcm_x64.wxs
**מיקום**: `src/odbc/rsodbc/install/rsodbcm_x64.wxs`

**מטרה**: תיקון נתיבי OpenSSL DLLs ב-WiX installer

#### שינוי: שורות 25-26
```xml
<!-- לפני: -->
<File Id="libcrypto_1_1_x64.dll"
      Name="libcrypto-1_1-x64.dll"
      Source="$(var.DependenciesDir)/openssl/Release/bin/libcrypto-1_1-x64.dll" />
<!-- ❌ נתיב לא נכון ל-vcpkg -->
<!-- ❌ שם קובץ של OpenSSL 1.1.x -->

<File Id="libssl_1_1_x64.dll"
      Name="libssl-1_1-x64.dll"
      Source="$(var.DependenciesDir)/openssl/Release/bin/libssl-1_1-x64.dll" />
<!-- ❌ נתיב לא נכון ל-vcpkg -->
<!-- ❌ שם קובץ של OpenSSL 1.1.x -->

<!-- אחרי: -->
<File Id="libcrypto_3_x64.dll"
      Name="libcrypto-3-x64.dll"
      Source="$(var.DependenciesDir)/bin/libcrypto-3-x64.dll" />
<!-- ✅ נתיב נכון ל-vcpkg: bin/ ישירות -->
<!-- ✅ שם קובץ של OpenSSL 3.x -->

<File Id="libssl_3_x64.dll"
      Name="libssl-3-x64.dll"
      Source="$(var.DependenciesDir)/bin/libssl-3-x64.dll" />
<!-- ✅ נתיב נכון ל-vcpkg: bin/ ישירות -->
<!-- ✅ שם קובץ של OpenSSL 3.x -->
```

**הבדלים**:
1. **נתיב**: `openssl/Release/bin/` → `bin/`
   - vcpkg מניח DLLs ישירות ב-`bin/`
   - לא בתוך תת-תיקייה `openssl/Release/`

2. **שם קובץ**: `libcrypto-1_1-x64.dll` → `libcrypto-3-x64.dll`
   - OpenSSL 3.x משתמש בשמות שונים
   - `1_1` = OpenSSL 1.1.x (ישן)
   - `3` = OpenSSL 3.x (חדש)

3. **שם קובץ**: `libssl-1_1-x64.dll` → `libssl-3-x64.dll`
   - אותה סיבה

**שגיאה שתוקנה**:
```
error LGHT0103: The system cannot find the file
'D:\...\vcpkg\installed\x64-windows/openssl/Release/bin/libcrypto-1_1-x64.dll'
```

**השפעה**: ⭐⭐⭐ קריטי
- ללא תיקון זה - MSI installer נכשל
- תיקן 10+ builds נכשלים ברציפות
- עכשיו build מצליח

---

### 3. ⭐⭐ build64.bat
**מיקום**: `build64.bat`

**מטרה**: שיפור build script ל-Windows

#### שינויים עיקריים:
```batch
REM הוספת בדיקות חסרות
if not exist "!MSBUILD_BIN_DIR!\msbuild.exe" (
    echo Error: msbuild.exe not found
    exit /b 1
)

REM תמיכה טובה יותר ב-vcpkg paths
set "VCPKG_INSTALL_DIR=%VCPKG_ROOT%\installed\x64-windows"
set "RS_OPENSSL_DIR=%VCPKG_INSTALL_DIR%"
set "RS_MULTI_DEPS_DIRS=%VCPKG_INSTALL_DIR%"
```

**השפעה**: ⭐⭐ חשוב
- build יותר יציב
- הודעות שגיאה ברורות יותר

---

### 4. ⭐⭐ CMakeLists.txt
**מיקום**: `CMakeLists.txt` (root)

**מטרה**: עדכון AWS SDK features ושיפור path handling

#### שינוי A: AWS SDK features
```cmake
# לפני:
aws-sdk-cpp[core,redshift,sts,identity-management]

# אחרי:
aws-sdk-cpp[core,redshift,redshift-serverless,sts,sso-oidc,identity-management]
```

**הוספה**:
- `redshift-serverless` - תמיכה ב-Redshift Serverless
- `sso-oidc` - נדרש ל-Azure AD SSO

#### שינוי B: Path handling
```cmake
# שיפור חיפוש libraries בvcpkg
list(APPEND CMAKE_PREFIX_PATH "${RS_MULTI_DEPS_DIRS}")
list(APPEND CMAKE_PREFIX_PATH "${RS_MULTI_DEPS_DIRS}/lib")
```

**השפעה**: ⭐⭐ חשוב
- CMake מוצא את vcpkg dependencies
- Build יותר אמין

---

### 5. ⭐⭐ src/odbc/rsodbc/CMakeLists.txt
**מיקום**: `src/odbc/rsodbc/CMakeLists.txt`

**מטרה**: תיקון linking issues

#### שינוי: ODBC libraries
```cmake
# לפני:
target_link_libraries(rsodbc64
    PRIVATE
    # ❌ ODBC libraries חסרות
    ${AWS_SDK_LIBS}
    ...
)

# אחרי:
target_link_libraries(rsodbc64
    PRIVATE
    # ✅ הוסף ODBC libraries
    odbc32
    odbccp32
    ${AWS_SDK_LIBS}
    ...
)
```

**השפעה**: ⭐⭐ חשוב
- תיקון linker errors
- connect.exe נבנה בהצלחה

---

### 6. ⭐⭐ src/odbc/rsodbc/samples/connect/CMakeLists.txt
**מיקום**: `src/odbc/rsodbc/samples/connect/CMakeLists.txt`

**מטרה**: תיקון connect.exe sample

#### שינוי:
```cmake
# הוספת ODBC libraries גם ל-sample
target_link_libraries(connect
    PRIVATE
    odbc32
    odbccp32
)
```

**השפעה**: ⭐ נמוך (sample only)

---

### 7. ⭐ cmake/Common.cmake
**מיקום**: `cmake/Common.cmake`

**מטרה**: שיפור vcpkg path handling

#### שינויים:
```cmake
# שיפור add_safe_include לטפל ב-vcpkg paths
function(add_safe_include target_name include_path)
    if(EXISTS "${include_path}")
        target_include_directories(${target_name} PRIVATE "${include_path}")
        # טיפול גם בתת-תיקיות
        if(EXISTS "${include_path}/include")
            target_include_directories(${target_name} PRIVATE "${include_path}/include")
        endif()
    endif()
endfunction()
```

**השפעה**: ⭐ בינוני

---

### 8. ⭐ cmake/Windows.cmake
**מיקום**: `cmake/Windows.cmake`

**מטרה**: שינוי MSVC runtime

#### שינוי:
```cmake
# לפני:
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# ❌ static runtime

# אחרי:
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
# ✅ dynamic runtime
```

**הסבר**:
- vcpkg משתמש ב-dynamic runtime (/MD)
- צריך התאמה למנוע conflicts

**השפעה**: ⭐ בינוני

---

### 9. ⭐⭐⭐ .github/workflows/build-windows-driver.yml
**מיקום**: `.github/workflows/build-windows-driver.yml`

**מטרה**: CI/CD pipeline מלא

#### תכונות:
```yaml
# קובץ חדש לגמרי!

jobs:
  build-windows:
    runs-on: windows-2022

    steps:
    - Setup vcpkg with caching
    - Install dependencies: OpenSSL, AWS SDK, c-ares, gtest
    - Build with retry logic
    - Upload MSI artifact
    - Upload logs on failure
```

**מה זה עושה**:
1. **Setup environment** - Windows 2022 + tools
2. **Install vcpkg** - package manager
3. **Cache dependencies** - מהירות build
4. **Build driver** - CMake + MSBuild
5. **Create MSI** - WiX packaging
6. **Upload artifacts** - MSI + logs

**Retry logic**:
```powershell
$maxRetries = 3
while (-not $success -and $retryCount -lt $maxRetries) {
    vcpkg install ...
}
```

**השפעה**: ⭐⭐⭐ קריטי
- Build אוטומטי על כל push
- CI/CD pipeline מלא
- Artifacts זמינים להורדה

---

### 10. ⭐ exports_basic.bat
**מיקום**: `exports_basic.bat`

**מטרה**: מניעת override של משתנים

#### שינוי:
```batch
REM לפני:
set VERSION=2.1.12.0

REM אחרי:
if not defined VERSION (
    set VERSION=2.1.12.0
)
```

**הסבר**: אפשר לoverride VERSION מבחוץ

**השפעה**: ⭐ נמוך

---

## 📊 סיכום השפעות

| קטגוריה | קבצים | חשיבות | השפעה |
|----------|-------|---------|-------|
| **Core OAuth Fix** | 1 | ⭐⭐⭐ | ללא זה - אין Azure AD auth |
| **Installer Fix** | 1 | ⭐⭐⭐ | ללא זה - אין MSI |
| **Build System** | 5 | ⭐⭐ | יציבות build |
| **CI/CD** | 1 | ⭐⭐⭐ | automation |
| **Minor** | 2 | ⭐ | נוחות |

---

## 🔄 Workflow

כך השינויים עובדים ביחד:

```
1. Developer pushes code
   ↓
2. GitHub Actions triggered (.github/workflows/...)
   ↓
3. vcpkg installs dependencies
   ↓
4. CMake configures (CMakeLists.txt + cmake/*.cmake)
   ↓
5. MSBuild compiles (rsodbc64.dll)
   ↓
6. WiX creates MSI (rsodbcm_x64.wxs)
   ✅ Uses correct OpenSSL 3.x DLLs
   ↓
7. MSI uploaded as artifact
   ↓
8. User downloads and installs
   ↓
9. ODBC driver loaded
   ↓
10. Azure AD auth triggered
    ↓
11. IAMBrowserAzureOAuth2CredentialsProvider.cpp
    ✅ Adds 'openid' automatically
    ✅ Sends client_secret if provided
    ↓
12. Authentication succeeds!
```

---

## ✅ בדיקה שהכל עובד

### קובץ 1: Azure OAuth Code
```bash
grep -n "openid" src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp
```
צריך למצוא את הלוגיקה בשורות 218, 278

### קובץ 2: WiX Installer
```bash
grep -n "libcrypto-3-x64" src/odbc/rsodbc/install/rsodbcm_x64.wxs
```
צריך למצוא את ה-DLL החדש

### Build Success
```bash
gh run view 21601686394 --repo orelash/amazon-redshift-odbc-driver
```
צריך לראות: ✓ SUCCESS

---

## 📚 קבצי תיעוד

כל התיעוד מפורט ב:
- `BUILD_STATUS.md` - סטטוס ה build והיסטוריה
- `README_AZURE_FIX.md` - סקירה כללית
- `AZURE_OAUTH_FIX_INSTRUCTIONS.md` - הוראות בנייה
- `DOWNLOAD_AND_TEST.md` - הוראות הורדה ובדיקה
- `CHANGES.md` - **המסמך הזה**

---

**גרסה**: v2.1.12.0-azure-oauth-fix
**תאריך**: 2026-02-02
**Build**: [#21601686394](https://github.com/ORELASH/amazon-redshift-odbc-driver/actions/runs/21601686394)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
