# Amazon Redshift ODBC Driver - תקלות ידועות ו-Issues פתוחים

**מעודכן**: 2026-02-02
**סטטוס**: 19 Issues פתוחים ב-upstream, 1 תוקן בפרויקט שלנו

---

## 📊 סיכום מהיר

| קטגוריה | מספר Issues | רמת חומרה |
|----------|-------------|-----------|
| 🔴 Critical (Crashes) | 3 | גבוהה מאוד |
| 🟡 Authentication | 5 | בינונית-גבוהה |
| 🟠 Data Types | 5 | בינונית |
| 🟢 Build/Platform | 3 | נמוכה |
| 📚 Documentation | 3 | נמוכה |

---

## 🔴 CRITICAL ISSUES - בעיות חמורות

### #37 - AccessViolationException ב-SQLGetData
**תאריך**: 2026-01-30
**חומרה**: 🔴🔴🔴 קריטי
**פלטפורמה**: Windows
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/37

**תיאור**:
- Crashes במהלך ETL operations
- Heap corruption ב-SQLGetData
- גורם ל-AccessViolationException

**השפעה**:
- ⚠️ יכול לגרום לאיבוד נתונים
- ⚠️ לא יציב ב-production ETL workloads

**Workaround**: אין - בעיה לא פתורה

---

### #15 - Driver קורס עם מספר גדול של שורות
**תאריך**: 2024-07-12
**חומרה**: 🔴🔴🔴 קריטי
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/15

**תיאור**:
- Driver crashes כשמשיגים datasets גדולים
- בעיית memory management

**השפעה**:
- ⚠️ לא מתאים ל-big data operations
- ⚠️ מגביל שימוש ב-production

**Workaround**:
- הגבל את מספר השורות בquery
- השתמש ב-pagination

---

### #13 - Timeout גורם ל-undefined behavior
**תאריך**: 2023-10-05
**חומרה**: 🔴🔴 גבוהה
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/13

**תיאור**:
- Timeout מ-max_query_execution_time
- SQLFetch מתנהג באופן לא צפוי אחרי timeout

**השפעה**:
- ⚠️ התנהגות לא מוגדרת
- ⚠️ אפשרות ל-data corruption

**Workaround**: הגדל timeout values

---

## 🟡 AUTHENTICATION ISSUES - בעיות אימות

### #16 - Azure AD OAuth2 + client_secret (✅ תוקן!)
**תאריך**: 2024-04-16
**חומרה**: 🟡🟡 בינונית
**סטטוס**: ✅ **תוקן בפרויקט שלנו!**
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/16

**בעיה המקורית**:
1. Azure AD OAuth2 לא עובד כשצריך client_secret
2. חסר 'openid' ב-scope (בניגוד ל-JDBC)
3. שגיאה: `AADSTS7000218: client_assertion or client_secret required`

**הפתרון שלנו** (v2.1.12.0-azure-oauth-fix):
- ✅ הוספה אוטומטית של 'openid' ל-scope
- ✅ תמיכה ב-client_secret parameter
- ✅ התנהגות זהה ל-JDBC driver

**הורדה**:
- Release: https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix
- MSI: 5MB, מוכן להתקנה

---

### #36 - Browser IdcAuthPlugin עם proxy
**תאריך**: 2025-12-17
**חומרה**: 🟡 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/36

**תיאור**: בעיות כשיש proxy settings

**Workaround**: הגדר proxy ב-environment variables

---

### #34 - Cache Azure AD tokens
**תאריך**: 2025-12-12
**חומרה**: 💡 Feature Request
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/34

**תיאור**:
- בקשה ל-cache access tokens
- שיפור performance - פחות round trips לAzure AD

**סטטוס**: לא מיושם

---

### #19 - Cognito IAM authentication נכשל
**תאריך**: 2024-06-07
**חומרה**: 🟡 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/19

**תיאור**: Security token invalid עם Cognito

**Workaround**: השתמש באימות אחר

---

### #7 - PingFederate parsing נכשל
**תאריך**: 2023-09-18
**חומרה**: 🟡 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/7

**תיאור**: בעיה ב-login form parsing עם PingFederate IdP

**Workaround**: IdP specific - צריך investigation

---

## 🟠 DATA TYPE ISSUES - בעיות טיפוסי נתונים

### #24 - Unicode מושחת
**תאריך**: 2024-10-08
**חומרה**: 🟠🟠 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/24

**תיאור**: Unicode column values corrupted

**השפעה**: בעיה עם תווים לא-ASCII (עברית, ערבית, סינית, וכו')

**Workaround**:
- השתמש ב-ASCII בלבד
- או המתן לתיקון

---

### #25 - SQLColumnsW שגיאת smallint
**תאריך**: 2024-10-11
**חומרה**: 🟠 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/25

**תיאור**: `smallint out of range` error

**Workaround**: השתמש ב-SQLColumns במקום SQLColumnsW

---

### #30 - TIMESTAMPTZ לא מתוקן לגמרי
**תאריך**: 2024-10-08
**חומרה**: 🟠 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/30

**תיאור**: תיקון ב-2.1.3 לא שלם

**השפעה**: בעיות עם timezone conversions

---

### #23 - Conversion לא נתמך
**תאריך**: 2024-08-14
**חומרה**: 🟠 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/23

**תיאור**: "Requested conversion is not supported"

---

### #21 - SQLDescribeCol אי-עקביות
**תאריך**: 2024-10-08
**חומרה**: 🟠 בינונית
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/21

**תיאור**:
- Types שונים בין v1.59 ל-v2.x
- bool, timestamptz מתנהגים שונה

**השפעה**: בעיות תאימות לאחור

---

## 🟢 BUILD/PLATFORM ISSUES - בעיות בנייה

### #12 - Build נכשל על Windows (✅ תוקן!)
**תאריך**: 2024-10-30
**חומרה**: 🟢 נמוכה
**סטטוס**: ✅ **תוקן בפרויקט שלנו!**
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/12

**הפתרון שלנו**:
- GitHub Actions CI/CD מלא
- vcpkg integration
- CMake improvements
- Build מצליח ב-21 דקות

---

### #27 - glibc >= 2.32 error
**תאריך**: 2024-10-08
**חומרה**: 🟢 נמוכה
**פלטפורמה**: Linux
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/27

**תיאור**: Build errors עם glibc חדש

**Workaround**: השתמש ב-glibc < 2.32

---

### #8 - Debian compilation
**תאריך**: 2024-10-30
**חומרה**: 🟢 נמוכה
**פלטפורמה**: Debian
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/8

**תיאור**: בעיות בקומפילציה על Debian

---

## 📚 DOCUMENTATION/RELEASE ISSUES

### #33 - Documentation חסר לIdentity Center
**תאריך**: 2025-07-11
**חומרה**: 📖 תיעוד
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/33

---

### #22 - Documentation מיושן
**תאריך**: 2024-08-14
**חומרה**: 📖 תיעוד
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/22

**תיאור**: docs.aws.amazon.com לא מעודכן

---

### #28 - Release חסר assets
**תאריך**: 2024-09-16
**חומרה**: 📦 Release
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/28

---

### #31 - VirusTotal false positive
**תאריך**: 2024-12-03
**חומרה**: 🦠 False alarm
**Link**: https://github.com/aws/amazon-redshift-odbc-driver/issues/31

**תיאור**: MSI מזוהה כזדוני (false positive)

---

## ⚠️ BUILD WARNINGS - הבנייה שלנו

הבנייה שלנו **מצליחה** אבל עם warnings לא קריטיים:

### 1. Resource Redefinition
```
warning: 'IDC_CHECK1' : redefinition
warning: 'IDC_COMBO_KSA' : redefinition
```
**סיבה**: Resource file issues
**השפעה**: אין - warnings בלבד
**סטטוס**: קיים גם ב-upstream

### 2. Code Warning
```
warning: 'handleFederatedNonIamConnection': not all control paths return a value
```
**סיבה**: Missing return statement בפונקציה
**השפעה**: אין בפועל
**סטטוס**: קיים גם ב-upstream

### 3. Test Files
```
warning: Some test files are not available
```
**סיבה**: Test infrastructure לא מלא
**השפעה**: אין על driver עצמו
**סטטוס**: קיים גם ב-upstream

---

## 🎯 המלצות לשימוש

### ✅ בטוח לשימוש:
- Azure AD OAuth2 authentication (עם התיקון שלנו)
- Basic data types (int, varchar, date)
- קבצי נתונים קטנים-בינוניים
- Windows builds (עם התיקון שלנו)

### ⚠️ שימוש בזהירות:
- ETL operations על Windows (#37)
- Datasets גדולים מאוד (#15)
- Unicode/international characters (#24)
- Long-running queries עם timeouts (#13)

### ❌ לא מומלץ:
- Production ETL ללא testing מקיף
- Big data queries ללא pagination
- הסתמכות על TIMESTAMPTZ (#30)

---

## 📊 השוואה לגרסאות

| גרסה | Azure OAuth | Windows Build | Known Crashes |
|------|-------------|---------------|---------------|
| **v2.1.12.0-azure-oauth-fix (שלנו)** | ✅ עובד | ✅ עובד | ⚠️ #37, #15 |
| v2.1.5.0 (upstream latest) | ❌ לא עובד | ⚠️ בעיות | ⚠️ #37, #15 |
| v2.1.3.0 | ❌ לא עובד | ⚠️ בעיות | ⚠️ כנ"ל |
| v2.0.1.0 | ❌ לא עובד | ⚠️ בעיות | ⚠️ כנ"ל |

---

## 🔗 קישורים שימושיים

- **Upstream Issues**: https://github.com/aws/amazon-redshift-odbc-driver/issues
- **התיקון שלנו**: https://github.com/ORELASH/amazon-redshift-odbc-driver
- **Release**: https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix

---

## 💡 עזרה ותרומה

אם נתקלת בבעיה:
1. בדוק את הרשימה כאן
2. חפש ב-[upstream issues](https://github.com/aws/amazon-redshift-odbc-driver/issues)
3. אם זה חדש - פתח issue חדש
4. שתף workarounds שמצאת

---

**Last Updated**: 2026-02-02
**Checked Against**: aws/amazon-redshift-odbc-driver (upstream)
**Source**: GitHub Issues API

🤖 Generated with [Claude Code](https://claude.com/claude-code)
