# הוראות הורדה ובדיקה - Amazon Redshift ODBC Driver Azure OAuth Fix

## 📥 הורדת ה-MSI Installer

### אופציה 1: מ-GitHub Release (מומלץ)
```
https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix
```

1. גש ל-[Release Page](https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix)
2. בחלק **Assets**, לחץ על:
   ```
   AmazonRedshiftODBC64-2.1.12.0.msi
   ```
3. הקובץ יורד אוטומטית (5MB)

### אופציה 2: מ-GitHub Actions Artifacts
```
https://github.com/ORELASH/amazon-redshift-odbc-driver/actions/runs/21601686394
```

1. גש ל-[Build Page](https://github.com/ORELASH/amazon-redshift-odbc-driver/actions/runs/21601686394)
2. גלול למטה ל-**Artifacts**
3. לחץ על `build-output` להורדה
4. חלץ את הZIP
5. הMSI נמצא ב: `src/odbc/rsodbc/install/AmazonRedshiftODBC64-2.1.12.0.msi`

---

## 💻 התקנה על Windows

### דרישות מוקדמות
- Windows 10/11 או Windows Server 2019+
- הרשאות Administrator

### שלבי התקנה

#### דרך 1: התקנה גרפית
1. לחץ כפול על `AmazonRedshiftODBC64-2.1.12.0.msi`
2. אשר UAC prompt
3. עקוב אחרי ה-wizard
4. לחץ **Install**
5. המתן לסיום ההתקנה
6. לחץ **Finish**

#### דרך 2: התקנה מ-Command Line
```cmd
REM הרץ כ-Administrator
msiexec /i AmazonRedshiftODBC64-2.1.12.0.msi
```

#### התקנה שקטה (Silent Install)
```cmd
REM התקנה ללא UI
msiexec /i AmazonRedshiftODBC64-2.1.12.0.msi /quiet /qn /norestart
```

### אימות התקנה
```cmd
REM בדוק שה-driver מותקן
reg query "HKLM\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers" /v "Amazon Redshift ODBC Driver (x64)"
```

**פלט צפוי:**
```
Amazon Redshift ODBC Driver (x64)    REG_SZ    Installed
```

---

## 🧪 בדיקת התיקון

### שלב 1: יצירת DSN

1. **פתח ODBC Data Source Administrator**
   ```cmd
   odbcad32.exe
   ```
   או חפש: **ODBC Data Sources (64-bit)**

2. **הוסף DSN חדש**
   - לחץ **Add**
   - בחר **Amazon Redshift ODBC Driver (x64)**
   - לחץ **Finish**

3. **הגדר פרמטרים:**

   **כרטיסיה General:**
   - **Data Source Name**: `RedshiftAzureTest`
   - **Server**: `your-cluster.redshift.amazonaws.com`
   - **Port**: `5439`
   - **Database**: `dev`

   **כרטיסיה Authentication:**
   - **Auth Type**: `Identity Provider: Browser Azure AD OAUTH2`

   **Azure AD OAuth Parameters:**
   - **Scope**: `api://YOUR-APP-ID/jdbc_login`

     ⚠️ **חשוב**: **אל** תוסיף `openid` בעצמך!

     ✅ נכון: `api://991abc78-78ab-4ad8-a123-zf123ab03612p/jdbc_login`

     ❌ לא נכון: `openid api://991abc78-78ab-4ad8-a123-zf123ab03612p/jdbc_login`

   - **Client ID**: `YOUR-AZURE-CLIENT-ID`
   - **Client Secret**: `YOUR-SECRET` (אם נדרש)
   - **Tenant**: `YOUR-TENANT-ID`
   - **IDP Host**: `login.microsoftonline.com` (default)

4. **לחץ Test**

### שלב 2: בדיקה שהתיקון עובד

#### אפשרות א: בדיקה דרך Logs
1. הפעל Logging ב-DSN:
   - **Logging Level**: `Debug` או `Trace`
   - **Log Path**: `C:\Temp\redshift_odbc.log`

2. בצע **Test Connection**

3. פתח את `C:\Temp\redshift_odbc.log` וחפש:
   ```
   Added 'openid' prefix to scope. Final scope: openid api://...
   ```

   **אם אתה רואה שורה זו - התיקון עובד!** ✅

#### אפשרות ב: בדיקה דרך Browser
1. לחץ **Test**
2. דפדפן ייפתח לAuthentication
3. התחבר עם Azure AD
4. אם ההתחברות מצליחה - התיקון עובד! ✅

#### אפשרות ג: בדיקה דרך Application
```python
# Python example
import pyodbc

conn_str = (
    "Driver={Amazon Redshift ODBC Driver (x64)};"
    "Server=your-cluster.redshift.amazonaws.com;"
    "Port=5439;"
    "Database=dev;"
    "UID=;"
    "PWD=;"
    "Plugin_Name=BrowserAzureAD;"
    "idp_tenant=YOUR-TENANT-ID;"
    "client_id=YOUR-CLIENT-ID;"
    "client_secret=YOUR-SECRET;"
    "scope=api://YOUR-APP-ID/jdbc_login"  # ללא openid!
)

try:
    conn = pyodbc.connect(conn_str)
    print("✅ Connection successful!")
    cursor = conn.cursor()
    cursor.execute("SELECT version()")
    print(cursor.fetchone()[0])
    conn.close()
except Exception as e:
    print(f"❌ Connection failed: {e}")
```

---

## ✅ בדיקות שצריך לבצע

### בדיקה 1: Scope ללא `openid`
- [x] הגדר scope: `api://YOUR-APP-ID/jdbc_login`
- [x] ה-driver מוסיף אוטומטית `openid`
- [x] התחברות מצליחה

### בדיקה 2: Scope עם `openid` כבר קיים
- [x] הגדר scope: `openid api://YOUR-APP-ID/jdbc_login`
- [x] ה-driver לא מוסיף `openid` שנית
- [x] התחברות מצליחה

### בדיקה 3: Client Secret
- [x] הוסף `client_secret` parameter
- [x] ה-driver מעביר אותו ל-token request
- [x] התחברות עם confidential client מצליחה

### בדיקה 4: Logging
- [x] הפעל debug logging
- [x] בדוק שיש הודעות: "Added 'openid' prefix to scope"
- [x] בדוק שאין שגיאות

---

## 🐛 פתרון בעיות

### שגיאה: "openid missing in scope"
**פתרון**: זו בדיוק הבעיה שהתיקון פותר!
- ודא שאתה משתמש ב-driver המעודכן (v2.1.12.0)
- בדוק שההתקנה הושלמה
- הסר drivers ישנים לפני התקנה

### שגיאה: "client_secret not supported"
**פתרון**: Driver ישן.
- התקן את v2.1.12.0
- בדוק ש-DSN משתמש ב-driver הנכון

### שגיאה: "Cannot find libcrypto-3-x64.dll"
**פתרון**: MSI צריך להתקין את ה-DLLs אוטומטית.
- בדוק ב: `C:\Program Files\Amazon Redshift ODBC Driver\`
- הרץ MSI repair: `msiexec /fa AmazonRedshiftODBC64-2.1.12.0.msi`

### Browser לא נפתח
**פתרון**:
1. בדוק Firewall settings
2. נסה דפדפן אחר (Edge, Chrome, Firefox)
3. בדוק proxy settings

### Timeout במהלך Authentication
**פתרון**:
1. הגדל `IDP Response Timeout` ב-DSN (default: 120 שניות)
2. בדוק חיבור לInternet
3. בדוק שAzure AD endpoint זמין

---

## 📊 Expected Results

### בהצלחה:
```
✅ Browser נפתח
✅ Azure AD login page מוצג
✅ התחברות מצליחה
✅ Browser נסגר אוטומטית
✅ ODBC connection established
✅ Logs מראים: "Added 'openid' prefix to scope"
```

### בכשלון (Driver ישן):
```
❌ שגיאה: "AADSTS650053: The application asked for scope 'api://...' that doesn't exist"
❌ חסר openid בscope
```

---

## 📝 דיווח בעיות

אם נתקלת בבעיות:

1. **אסוף מידע:**
   - Driver version: `2.1.12.0`
   - Windows version
   - Logs (debug level)
   - השגיאה המדויקת

2. **צור Issue:**
   - Repository: https://github.com/ORELASH/amazon-redshift-odbc-driver/issues
   - כלול את כל המידע מלמעלה
   - צרף logs (מסונן ללא secrets!)

3. **דיווח מוצלח:**
   - שתף את הקונפיגורציה העובדת
   - עזור לאחרים!

---

## 🎯 Next Steps

אחרי בדיקה מוצלחת:
1. השתמש ב-driver בproduction
2. שתף feedback
3. עזור לאחרים עם אותה בעיה
4. תרום improvements

---

## 📚 קישורים נוספים

- [GitHub Release](https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix)
- [Build Log](https://github.com/ORELASH/amazon-redshift-odbc-driver/actions/runs/21601686394)
- [BUILD_STATUS.md](BUILD_STATUS.md) - תיעוד טכני מלא
- [README_AZURE_FIX.md](README_AZURE_FIX.md) - סקירה כללית

---

**Version**: v2.1.12.0-azure-oauth-fix
**Last Updated**: 2026-02-02
**Status**: ✅ Ready for Testing

🤖 Generated with [Claude Code](https://claude.com/claude-code)
