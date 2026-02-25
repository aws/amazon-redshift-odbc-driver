# פתרון התחברות Azure OAuth2 - מדריך מלא

## תיאור הבעיה

התחברות Azure OAuth2 עם תוסף BrowserAzureADOAuth2 נכשלה בסביבות ארגוניות עם:
- הדפדפן נפתח בהצלחה
- המשתמש מתחבר ב-Azure
- ההתחברות אושרה (נראה ב-INTRA)
- **הדרייבר נתקע** - השליטה לא חוזרת לאפליקציה

## השורש הבעיה

הבעיה לא הייתה קשורה ל-localhost redirect או לפרוקסי שחוסם את ה-OAuth callback.

**הבעיה האמיתית**: בקשת HTTP POST לשרת ה-token של Azure (`https://login.microsoftonline.com/.../token`) נכשלה כי:
- הסביבה הארגונית דורשת פרוקסי לחיבורי HTTPS חיצוניים
- ברירת מחדל: `idp_use_https_proxy=0` (מנוטרל)
- בקשת ה-token נכשלה בשקט עם status code -1
- הדרייבר המתין ללא הגבלה לתגובה

## הפתרון

### הגדרה נדרשת

הוסף את הפרמטר הבא ל-connection string של ODBC:

```
idp_use_https_proxy=1
```

### דוגמה ל-Connection String מלא

```
Driver={Amazon Redshift (x64)};
Server=your-cluster.redshift.amazonaws.com;
Database=your_database;
UID=;
PWD=;
plugin_name=BrowserAzureADOAuth2;
idp_tenant=your-tenant-id;
client_id=your-client-id;
scope=openid profile;
https_proxy_host=your-proxy-host;
https_proxy_port=8080;
idp_use_https_proxy=1
```

### למה זה עובד

כאשר `idp_use_https_proxy=1` מוגדר:
- תהליך OAuth בדפדפן עובד כרגיל (localhost callback)
- בקשת החלפת ה-token משתמשת בפרוקסי הארגוני
- החיבור HTTPS חיצוני מצליח
- ההתחברות מושלמת בהצלחה

## פרטים טכניים

### תהליך ההתחברות

1. **פתיחת הדפדפן**: פותח את כתובת ההתחברות של Azure
2. **התחברות משתמש**: המשתמש מתחבר דרך הדפדפן
3. **OAuth Callback**: Azure מפנה ל-`http://localhost:PORT/redshift/` עם קוד אישור
4. **שרת מקומי**: WEBServer מקבל POST עם קוד האישור
5. **בקשת Token**: HTTP POST ל-`https://login.microsoftonline.com/.../oauth2/v2.0/token`
   - **שלב זה דורש פרוקסי בסביבות ארגוניות**
6. **תגובת Token**: מקבל access token ו-ID token
7. **חיבור**: משתמש ב-tokens כדי להתחבר ל-Redshift

### לוגים אבחוניים

Build #25 כולל לוגים משופרים:

**WEBServer.cpp**:
- בחירת פורט listening
- קבלת בקשות
- התקדמות פירוק

**Parser.cpp**:
- פירוק בקשות HTTP
- חילוץ headers
- חילוץ קוד אישור

**IAMBrowserAzureOAuth2CredentialsProvider.cpp**:
- פתיחת דפדפן
- בדיקת הגדרות פרוקסי
- status code של בקשת token
- טיפול בתגובה

### דוגמה לניתוח לוגים

**ללא `idp_use_https_proxy=1`** (נכשל):
```
[DEBUG] RequestAccessToken: Proxy check - UsingHTTPSProxy=1, UseProxyIdpAuth=0
[DEBUG] RequestAccessToken: NOT using proxy for IDP auth
[DEBUG] RequestAccessToken: HTTP request completed. Status code: -1
[ERROR] Authentication failed on the Browser server... Response code: -1
```

**עם `idp_use_https_proxy=1`** (עובד):
```
[DEBUG] RequestAccessToken: Proxy check - UsingHTTPSProxy=1, UseProxyIdpAuth=1
[DEBUG] RequestAccessToken: Using HTTPS proxy - Host=campusbc, Port=8080
[DEBUG] RequestAccessToken: HTTP request completed. Status code: 200
[DEBUG] Response: {"token_type":"Bearer","access_token":"...","id_token":"..."}
[DEBUG] SQLDriverConnectW() return SQL_SUCCESS
```

## שינויים בקוד

### היסטוריית Builds

- **Build #18-19**: שיחזור כל 14 שיטות ההתחברות (ADFS, AzureAD, Ping, Okta וכו')
- **Build #20**: הוספת לוגים debug ל-WEBServer ו-Parser
- **Build #21-24**: ניסיונות פתרונות proxy bypass (לא נדרשים)
- **Build #25**: פישוט LaunchBrowser, שמירת לוגים אבחוניים

### קבצים עיקריים ששונו

1. **IAMBrowserAzureOAuth2CredentialsProvider.cpp**:
   - פישוט LaunchBrowser (הסרת ניסיונות proxy bypass)
   - שיפור לוגים ב-RequestAccessToken
   - אבחון ברור של הגדרות פרוקסי

2. **WEBServer.cpp**:
   - הוספת לוגים ל-listener thread
   - לוגים לבחירת פורט
   - מעקב אחר קבלת בקשות

3. **Parser.cpp**:
   - לוגים ל-state machine של פירוק HTTP
   - לוגים ל-request line ו-headers
   - לוגים לחילוץ קוד אישור

4. **rs_iam_support.h**:
   - הוספת קבוע `IAM_KEY_PROXY_BYPASS_LIST` (לא בשימוש בפתרון סופי)

## שיטות התחברות נתמכות

Build #25 כולל תמיכה בכל שיטות ההתחברות התואמות AWS:

1. BrowserAzureADOAuth2 (עם OAuth 2.0)
2. BrowserAzureAD (SAML מסורתי)
3. BrowserSAML
4. ADFS
5. AzureAD
6. Ping
7. Okta
8. External
9. JWT
10. JwtIamAuthPlugin
11. IdpTokenAuthPlugin
12. BrowserIdcAuthPlugin
13. Instance Profile
14. Static credentials

## בדיקות

### סביבת בדיקה
- Windows עם פרוקסי ארגוני
- רשת Air-Gap
- דפדפן Chrome מותקן
- Azure AD tenant מוגדר
- אשכול Redshift עם IAM authentication

### תוצאות בדיקה
✅ הדפדפן נפתח בהצלחה
✅ התחברות Azure מושלמת
✅ OAuth callback מתקבל ב-localhost
✅ בקשת token מצליחה דרך פרוקסי
✅ החיבור מתבצע
✅ שאילתות SQL מתבצעות כרגיל

## פתרון בעיות

### אם ההתחברות עדיין נכשלת:

1. **בדוק הגדרות פרוקסי**:
   ```
   https_proxy_host=<your-proxy>
   https_proxy_port=<port>
   idp_use_https_proxy=1
   ```

2. **הפעל לוגים אבחוניים**:
   - בדוק לוגים של הדרייבר ב: `%TEMP%\Amazon Redshift ODBC Driver\logs\`
   - חפש "RequestAccessToken: Status code: XXX"
   - Status code -1 = בעיית פרוקסי
   - Status code 400/401 = בעיית התחברות
   - Status code 200 = הצלחה

3. **אמת App Registration ב-Azure**:
   - Redirect URI: `http://localhost:7890/redshift/` (או הפורט שלך)
   - אישור admin consent להרשאות הנדרשות
   - וודא שלמשתמש יש גישה

4. **בדוק חומת אש**:
   - וודא שתעבורת localhost מותרת
   - פורט 7890 (או listen_port מותאם) לא חסום

## התקנה

1. הורד: `AmazonRedshiftODBC64-Fork-v2.1.13.0-AzureOAuth.msi`
2. הסר גרסה קודמת (אם קיימת)
3. הרץ את מתקין ה-MSI
4. הגדר ODBC DSN עם הפרמטרים הנדרשים
5. בדוק חיבור

## מידע על גרסה

- **גרסת דרייבר**: 2.1.13.0
- **Build**: #25
- **תאריך**: 4 בפברואר 2026
- **Branch**: fix-azure-oauth-scope
- **Repository**: ORELASH/amazon-redshift-odbc-driver

## קרדיטים

נוצר עם Claude Code (https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
