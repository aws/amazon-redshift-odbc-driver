# Azure OAuth2 Quick Start Guide

## TL;DR - הפתרון המהיר

אם התחברות Azure OAuth2 נתקעת בסביבה ארגונית עם פרוקסי:

**הוסף פרמטר אחד ל-connection string:**

```
idp_use_https_proxy=1
```

זהו! זה הפתרון.

---

## Connection String Example

```
Driver={Amazon Redshift (x64)};
Server=your-cluster.redshift.amazonaws.com;
Database=your_database;
plugin_name=BrowserAzureADOAuth2;
idp_tenant=your-tenant-id;
client_id=your-client-id;
scope=openid profile;
https_proxy_host=your-proxy-host;
https_proxy_port=8080;
idp_use_https_proxy=1
```

## Why?

הדפדפן נפתח ✅
ההתחברות ב-Azure עובדת ✅
ה-redirect ל-localhost עובד ✅
**בקשת ה-token ל-Azure נחסמת ללא פרוקסי** ❌

הפרמטר `idp_use_https_proxy=1` מאפשר לבקשת ה-token להשתמש בפרוקסי הארגוני.

## How to Test

1. התקן את הדרייבר:
   - `AmazonRedshiftODBC64-Fork-v2.1.13.0-AzureOAuth.msi`

2. הגדר ODBC DSN עם הפרמטרים לעיל

3. נסה להתחבר

4. בדוק logs ב:
   ```
   %TEMP%\Amazon Redshift ODBC Driver\logs\
   ```

5. חפש בלוגים:
   - ✅ "Status code: 200" = הצלחה!
   - ❌ "Status code: -1" = בדוק הגדרות פרוקסי

## Full Documentation

- English: [AZURE_OAUTH_SOLUTION.md](AZURE_OAUTH_SOLUTION.md)
- עברית: [AZURE_OAUTH_SOLUTION_HE.md](AZURE_OAUTH_SOLUTION_HE.md)

## Need Help?

Check logs for status code:
- **-1**: Proxy configuration issue → Add `idp_use_https_proxy=1`
- **400/401**: Azure authentication issue → Check client_id, tenant, scope
- **200**: Success! 🎉
