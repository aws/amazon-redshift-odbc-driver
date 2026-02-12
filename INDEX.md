# Amazon Redshift ODBC Driver - Azure OAuth Fix - מפת התיעוד

**גרסה:** v2.1.12.0-azure-oauth-fix | **תאריך:** 2026-02-02 | **סטטוס:** ✅ Production Ready

---

## 🚀 התחלה מהירה (Start Here)

### אני רוצה להוריד ולהתקין - 5 דקות
👉 **[DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md)**
- הורדת MSI
- הוראות התקנה
- הגדרת DSN
- בדיקת התיקון

### אני רוצה הסבר מהיר על הפרויקט
👉 **[README_HE.md](README_HE.md)** (עברית) או **[README.md](README.md)** (English)
- מה הבעיה
- מה הפתרון
- Quick start
- קישורים חשובים

---

## 📚 לפי נושא

### 🔧 Technical Documentation

#### רוצה להבין מה השתנה בקוד?
👉 **[CHANGES.md](CHANGES.md)** (14KB)
- הסבר line-by-line לכל שינוי
- קוד לפני/אחרי
- 10 קבצים מנותחים
- הסברים מפורטים בעברית

#### רוצה לדעת על ה-Build?
👉 **[BUILD_STATUS.md](BUILD_STATUS.md)** (6.7KB)
- היסטוריית Build מלאה
- 36 commits
- GitHub Actions workflow
- Dependencies ו-Tools
- Troubleshooting

#### רוצה את כל המידע להמשך עבודה?
👉 **[PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md)** (20KB+)
- תיעוד master מקיף
- Timeline מלא
- כל הקישורים
- המשך עבודה אפשרי
- Quick commands reference

### 🐛 Issues & Support

#### רוצה לדעת על באגים ידועים?
👉 **[KNOWN_ISSUES.md](KNOWN_ISSUES.md)** (11KB)
- 19 issues פתוחים ב-upstream
- קטגוריזציה לפי severity
- Workarounds
- המלצות לשימוש

### 📦 Installation & Usage

#### הוראות הורדה והתקנה?
👉 **[DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md)** (7.8KB)
- הורדה מRelease או Artifacts
- התקנה (UI / CLI / Silent)
- אימות התקנה
- בדיקת התיקון
- Troubleshooting

#### דוגמאות קוד?
👉 **[README_HE.md](README_HE.md)** - סעיף "דוגמת שימוש"
- Python (pyodbc)
- PowerShell
- Connection strings

---

## 📖 לפי קהל יעד

### 👤 End User (רוצה פשוט להשתמש)
1. **[README_HE.md](README_HE.md)** - קרא סקירה כללית
2. **[DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md)** - הורד והתקן
3. **[KNOWN_ISSUES.md](KNOWN_ISSUES.md)** - תדע על בעיות אפשריות

### 💻 Developer (רוצה להבין/לתרום)
1. **[README_HE.md](README_HE.md)** או **[README.md](README.md)** - סקירה
2. **[CHANGES.md](CHANGES.md)** - הבן את השינויים
3. **[BUILD_STATUS.md](BUILD_STATUS.md)** - הבן את ה-Build
4. **[PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md)** - כל המידע

### 🔧 DevOps / CI (רוצה לbuild/לdeploy)
1. **[BUILD_STATUS.md](BUILD_STATUS.md)** - Build pipeline
2. **[PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md)** - Commands reference
3. **[.github/workflows/build-windows-driver.yml](.github/workflows/build-windows-driver.yml)** - CI/CD code

### 📝 Manager / Stakeholder (רוצה סיכום)
1. **[README_HE.md](README_HE.md)** - Quick start + Overview
2. **[PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md)** - סעיף "סיכום מהיר"
3. **[KNOWN_ISSUES.md](KNOWN_ISSUES.md)** - סעיף "המלצות לשימוש"

---

## 🎯 לפי משימה

### הורדת MSI
```
Direct Link: https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/download/v2.1.12.0-azure-oauth-fix/AmazonRedshiftODBC64-2.1.12.0.msi

Release Page: https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix
```

### התקנה
```cmd
msiexec /i AmazonRedshiftODBC64-2.1.12.0.msi
```
📖 **הוראות מפורטות:** [DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md#התקנה-על-windows)

### הגדרת DSN
📖 **[DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md#שלב-1-יצירת-dsn)**

### Build מקוד מصדר
📖 **[BUILD_STATUS.md](BUILD_STATUS.md#build-instructions)** או **[PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md#build-מקוד-המصדר-אופציונלי)**

### פתיחת Issue
📖 **[README_HE.md](README_HE.md#תמיכה)** - הוראות דיווח בעיות

### תרומה לפרויקט
📖 **[README_HE.md](README_HE.md#תרומה-לפרויקט)**

---

## 📊 סטטיסטיקות תיעוד

| קובץ | גודל | שפה | קהל יעד |
|------|------|------|---------|
| **README.md** | 4.5KB+ | English | כולם - overview |
| **README_HE.md** | 12KB+ | עברית | כולם - overview מפורט |
| **BUILD_STATUS.md** | 6.7KB | עברית | Developers, DevOps |
| **README_AZURE_FIX.md** | 2.0KB | עברית | כולם - quick summary |
| **DOWNLOAD_AND_TEST.md** | 7.8KB | עברית | End users, Testers |
| **CHANGES.md** | 14KB | עברית | Developers |
| **KNOWN_ISSUES.md** | 11KB | עברית | כולם |
| **PROJECT_COMPLETE_SUMMARY.md** | 20KB+ | עברית | כולם - master reference |
| **INDEX.md** | (זה) | עברית | כולם - navigation |

**סה"כ:** 78KB+ תיעוד מקיף

---

## 🔗 קישורים חיצוניים

### GitHub
- **Repository:** https://github.com/ORELASH/amazon-redshift-odbc-driver
- **Branch:** `fix-azure-oauth-scope`
- **Releases:** https://github.com/ORELASH/amazon-redshift-odbc-driver/releases
- **Actions:** https://github.com/ORELASH/amazon-redshift-odbc-driver/actions
- **Build #21601686394:** https://github.com/ORELASH/amazon-redshift-odbc-driver/actions/runs/21601686394

### Upstream (AWS)
- **Repository:** https://github.com/aws/amazon-redshift-odbc-driver
- **Issue #16:** https://github.com/aws/amazon-redshift-odbc-driver/issues/16
- **All Issues:** https://github.com/aws/amazon-redshift-odbc-driver/issues

### AWS Documentation
- **Windows:** https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-win.html
- **Linux:** https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-linux.html
- **macOS:** https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-mac.html

---

## 🗂️ מבנה קבצים

```
amazon-redshift-odbc-driver/
├── 📄 README.md                        # Main README (English) with banner
├── 📄 README_HE.md                     # Complete Hebrew README
├── 📄 README_AZURE_FIX.md              # Quick summary of the fix
├── 📄 INDEX.md                         # This file - navigation guide
│
├── 📘 DOWNLOAD_AND_TEST.md             # Installation & testing guide
├── 📘 BUILD_STATUS.md                  # Build documentation
├── 📘 CHANGES.md                       # Detailed code changes
├── 📘 KNOWN_ISSUES.md                  # Known bugs catalog
├── 📘 PROJECT_COMPLETE_SUMMARY.md      # Master reference document
│
├── 🔧 .github/
│   └── workflows/
│       └── build-windows-driver.yml    # CI/CD pipeline
│
├── 💻 src/
│   └── odbc/
│       └── rsodbc/
│           ├── iam/plugins/
│           │   └── IAMBrowserAzureOAuth2CredentialsProvider.cpp  # Main fix
│           └── install/
│               └── rsodbcm_x64.wxs     # WiX installer fix
│
└── 📦 Release Assets
    └── AmazonRedshiftODBC64-2.1.12.0.msi (5MB)
```

---

## 🎓 מונחים

| מונח | הסבר |
|------|------|
| **ODBC** | Open Database Connectivity - standard API להתחברות לדאטהבייסים |
| **OAuth2** | פרוטוקול הרשאה - authentication framework |
| **OpenID** | שכבת identity על OAuth2 - צריך scope 'openid' |
| **Scope** | הרשאות שמבקשים ב-OAuth2 |
| **client_secret** | Secret להוכחת זהות של confidential clients |
| **MSI** | Microsoft Installer - פורמט התקנה ל-Windows |
| **WiX** | Windows Installer XML - כלי ליצירת MSI |
| **vcpkg** | C++ package manager של Microsoft |
| **DSN** | Data Source Name - הגדרת חיבור ODBC |
| **upstream** | Repository המקורי (AWS) |
| **fork** | עותק של repository (שלנו) |

---

## ✅ Checklist לשימוש מהיר

### אני רוצה להשתמש ב-Driver:
- [ ] קראתי [README_HE.md](README_HE.md)
- [ ] הורדתי MSI מ-[Release](https://github.com/ORELASH/amazon-redshift-odbc-driver/releases/tag/v2.1.12.0-azure-oauth-fix)
- [ ] התקנתי לפי [DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md)
- [ ] הגדרתי DSN
- [ ] בדקתי שזה עובד
- [ ] קראתי [KNOWN_ISSUES.md](KNOWN_ISSUES.md)

### אני רוצה להבין את הקוד:
- [ ] קראתי [README_HE.md](README_HE.md)
- [ ] קראתי [CHANGES.md](CHANGES.md)
- [ ] בדקתי את [BUILD_STATUS.md](BUILD_STATUS.md)
- [ ] הבנתי את השינויים

### אני רוצה לתרום:
- [ ] קראתי את כל התיעוד
- [ ] clone את הrepository
- [ ] בדקתי [PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md) - "המשך עבודה אפשרי"
- [ ] יצרתי branch חדש
- [ ] עשיתי שינויים
- [ ] פתחתי PR

---

## 🔍 חיפוש מהיר

**אם אתה מחפש...**

- **"איך מורידים?"** → [DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md#הורדת-ה-msi-installer)
- **"איך מתקינים?"** → [DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md#התקנה-על-windows)
- **"מה התיקון?"** → [README_HE.md](README_HE.md#מה-היתה-הבעיה)
- **"מה השתנה בקוד?"** → [CHANGES.md](CHANGES.md)
- **"איך בונים?"** → [BUILD_STATUS.md](BUILD_STATUS.md)
- **"באגים ידועים?"** → [KNOWN_ISSUES.md](KNOWN_ISSUES.md)
- **"כל המידע?"** → [PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md)
- **"דוגמת קוד?"** → [README_HE.md](README_HE.md#דוגמת-שימוש)
- **"Troubleshooting?"** → [DOWNLOAD_AND_TEST.md](DOWNLOAD_AND_TEST.md#פתרון-בעיות)
- **"GitHub Actions?"** → [BUILD_STATUS.md](BUILD_STATUS.md#github-actions-workflow)
- **"Commands?"** → [PROJECT_COMPLETE_SUMMARY.md](PROJECT_COMPLETE_SUMMARY.md#quick-commands-reference)

---

## 📞 עזרה

### קראתי הכל ועדיין יש שאלה?

1. חפש ב-[Issues](https://github.com/ORELASH/amazon-redshift-odbc-driver/issues)
2. פתח [Issue חדש](https://github.com/ORELASH/amazon-redshift-odbc-driver/issues/new)
3. הקפד לכלול:
   - איזה תיעוד קראת
   - מה ניסית
   - מה השגיאה
   - OS ו-version

---

## 🎯 Next Steps

לאחר שקראת את התיעוד המתאים:

1. **End User:** הורד → התקן → בדוק → השתמש
2. **Developer:** הבן → Clone → Build → תרום
3. **DevOps:** הבן pipeline → Deploy → Monitor

---

**תיעוד זה עודכן לאחרונה:** 2026-02-02
**גרסה:** v2.1.12.0-azure-oauth-fix
**סטטוס:** ✅ Complete

---

**🤖 Generated with [Claude Code](https://claude.com/claude-code)**

---

**Navigation:**
[🏠 Home](README_HE.md) | [📥 Download](DOWNLOAD_AND_TEST.md) | [🔧 Build](BUILD_STATUS.md) | [📝 Changes](CHANGES.md) | [🐛 Issues](KNOWN_ISSUES.md) | [📚 Complete](PROJECT_COMPLETE_SUMMARY.md)
