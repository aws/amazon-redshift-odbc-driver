# Amazon Redshift ODBC Driver - Azure OAuth2 Fix

## מה זה?

Branch זה מכיל תיקון ל-Amazon Redshift ODBC Driver שמוסיף אוטומטית `openid` ל-scope parameter כשמשתמשים ב-Browser Azure AD OAuth2.

## למה זה נחוץ?

ה-JDBC driver של Redshift מוסיף אוטומטית `openid`, אבל ה-ODBC driver לא. זה גורם לשגיאות authentication עם Azure AD.

## מה השתנה?

קובץ אחד: `src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp`

**שינויים:**
- ✅ הוספה אוטומטית של `openid` אם חסר (תואם JDBC)
- ✅ תמיכה ב-`client_secret` (Issue #16)
- ✅ Logging מפורט

## איך לבנות?

### דרך 1: בנייה על Windows (מומלץ)

ראה: [AZURE_OAUTH_FIX_INSTRUCTIONS.md](AZURE_OAUTH_FIX_INSTRUCTIONS.md)

### דרך 2: קבל רק את ה-Patch

```bash
git diff master fix-azure-oauth-scope -- src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp > fix.patch
```

## דוגמת שימוש

**לפני התיקון:**
```
Scope: openid api://991abc78-78ab-4ad8-a123-zf123ab03612p/jdbc_login
```
צריך להוסיף `openid` ידנית ❌

**אחרי התיקון:**
```
Scope: api://991abc78-78ab-4ad8-a123-zf123ab03612p/jdbc_login
```
Driver מוסיף `openid` אוטומטית ✅

## סטטוס

✅ קוד מוכן
✅ הוראות בנייה מוכנות
⚠️ GitHub Actions build עדיין בעבודה (בעיות dependency)

**המלצה**: בנה על Windows עם ההוראות המצורפות.

## קרדיטים

Branch: `fix-azure-oauth-scope`
Based on: AWS amazon-redshift-odbc-driver
Issue: #16 (client_secret support)
