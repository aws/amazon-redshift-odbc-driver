# תיקון Azure OAuth2 - הוספה אוטומטית של 'openid' ל-scope

## מה התיקון עושה?

תיקון זה מוסיף אוטומטית `openid` ל-scope parameter בעת שימוש ב-Browser Azure AD OAuth2, בדיוק כמו שה-JDBC driver עושה.

## השינוי שבוצע

קובץ: `src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp`

התיקון כולל:
1. ✅ הוספה אוטומטית של `openid` אם חסר ב-scope
2. ✅ תמיכה ב-`client_secret` (תיקון ל-Issue #16)
3. ✅ Logging מפורט

---

## הוראות בנייה על Windows

### דרישות מוקדמות

1. Visual Studio 2022 (Enterprise, Professional, או Community)
2. Git for Windows
3. CMake
4. Perl (Strawberry Perl מומלץ)
5. NASM
6. WiX Toolset 3.14

### שלב 1: שכפול הפרויקט

```cmd
git clone https://github.com/ORELASH/amazon-redshift-odbc-driver.git
cd amazon-redshift-odbc-driver
git checkout fix-azure-oauth-scope
```

### שלב 2: הכנת Dependencies

יש לך שתי אופציות:

#### אופציה א: שימוש ב-vcpkg (מומלץ)

```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
vcpkg install openssl:x64-windows aws-sdk-cpp[core,redshift,sts,identity-management]:x64-windows c-ares:x64-windows gtest:x64-windows
cd ..
```

#### אופציה ב: בנייה ידנית של Dependencies

עקוב אחרי ההוראות בקובץ `BUILD.CMAKE.md`

### שלב 3: בנייה

```cmd
REM אם השתמשת ב-vcpkg:
build64.bat --dependencies-install-dir=vcpkg\installed\x64-windows --version=2.1.13.0

REM אם בנית dependencies ידנית:
build64.bat --dependencies-install-dir=C:\path\to\your\deps --version=2.1.13.0
```

### שלב 4: התקנה

ה-MSI installer ימצא ב:
```
src\odbc\rsodbc\install\AmazonRedshiftODBC64_2.1.13.0.msi
```

התקן אותו:
```cmd
msiexec /i src\odbc\rsodbc\install\AmazonRedshiftODBC64_2.1.13.0.msi
```

---

## בדיקה

אחרי ההתקנה, צור DSN חדש:

1. פתח "ODBC Data Source Administrator (64-bit)"
2. לחץ "Add" → בחר "Amazon Redshift ODBC Driver (x64)"
3. הגדר:
   - **Auth Type**: Identity Provider: Browser Azure AD OAUTH2
   - **Scope**: `api://YOUR-APP-ID/jdbc_login` (ללא openid!)
   - **Client ID**: YOUR-CLIENT-ID
   - **Client Secret**: YOUR-SECRET (אם נדרש)
   - **Tenant**: YOUR-TENANT-ID
4. לחץ "Test"

Driver יוסיף אוטומטית `openid` ל-scope!

---

## אם אתה רוצה רק את ה-Patch

אם יש לך כבר סביבת build, אפשר להוריד רק את השינוי:

```cmd
git diff master fix-azure-oauth-scope -- src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp > azure_oauth_fix.patch
git apply azure_oauth_fix.patch
```

---

## פתרון בעיות

### שגיאת "openid missing"
✅ התיקון אמור לפתור את זה אוטומטית

### Client secret לא עובד
✅ התיקון מוסיף תמיכה ב-client_secret

### איך לראות logs?
הפעל logging ב-DSN configuration ובדוק את הלוג - תראה:
```
Added 'openid' prefix to scope. Final scope: openid api://...
```

---

## קרדיטים

תיקון זה מבוסס על ההתנהגות של Amazon Redshift JDBC driver ומתקן את Issue #16.
