# BLD002 Debug Build - Test Connection Fix

**Version:** 2.1.13.1-BLD002
**Date:** 2026-02-24
**Type:** DEBUG BUILD with extensive logging

---

## 🎯 Problem Being Investigated

**User Report:**
> "צריך שתבדוק לי מה השתנה במערכת של TEST, למה זה לא חוזר למסך של הDRIVER ונתקע בלי להודיע על הצלחה או כשלון?"

Translation: Test Connection button doesn't return to driver screen and appears frozen without showing success or failure message.

---

## 🔍 Root Cause Analysis

### Issue Identified
The Test Connection functionality in ODBC DSN setup dialog (`setup.c:3228`) was experiencing focus loss after browser-based Azure OAuth authentication:

1. User clicks "Test Connection" (mapped to PSN_HELP notification)
2. Browser opens for Azure AD authentication
3. **Browser takes window focus**
4. User authenticates, browser closes
5. Control returns to `rs_dsn_test_connect()`
6. **Parent dialog window lost focus and may be hidden**
7. `MessageBox()` attempts to display but is not visible/accessible
8. Result: User sees frozen interface

### Technical Details

**Call Flow:**
```
rs_connect_tab() [PSN_HELP notification]
  └─> rs_dsn_test_connect(hwndDlg, rs_dsn_setup_ctxt)
       ├─> SQLDriverConnect(hdbc, NULL, ..., SQL_DRIVER_NOPROMPT)
       │    └─> IAMBrowserAzureOAuth2CredentialsProvider::RequestAuthorizationCode()
       │         ├─> WEBServer::LaunchServer() - starts listener thread
       │         ├─> LaunchBrowser(uri) - opens browser with ShellExecute()
       │         │    └─> [BROWSER TAKES FOCUS]
       │         ├─> srv.Join() - waits for authentication
       │         └─> [Browser closes, returns]
       ├─> [Parent window now unfocused/hidden]
       └─> MessageBox(hwndParent, ...) - NOT VISIBLE!
```

**Key Code Location:**
- File: `src/odbc/rsodbc/rsodbc_setup/setup.c`
- Function: `rs_dsn_test_connect()` (lines 3228-3333)
- Issue: MessageBox called with parent that lost focus

---

## ✅ Changes Implemented in BLD002

### 1. Window Focus Recovery (Primary Fix)

**Location:** `setup.c:3315-3340`

Added comprehensive window focus restoration before showing MessageBox:

```c
// Bring the parent window to foreground before showing MessageBox
// This is necessary because browser authentication may have taken focus
HWND hwndToUse = rs_dsn_setup_ctxt->hwndParent;

if (hwndToUse != NULL) {
    // Try to restore if minimized
    if (IsIconic(hwndToUse)) {
        ShowWindow(hwndToUse, SW_RESTORE);
    }

    SetForegroundWindow(hwndToUse);
    BringWindowToTop(hwndToUse);
} else {
    // Fallback to tab window if parent is NULL
    hwndToUse = hdlg;
}

// Add MB_SETFOREGROUND and MB_TASKMODAL to ensure MessageBox appears on top
dlg_flag |= MB_SETFOREGROUND | MB_TASKMODAL;

MessageBox(hwndToUse, resultmsg, "Connection Test", dlg_flag);
```

**What This Does:**
- **`IsIconic()`** - Checks if window is minimized
- **`ShowWindow(SW_RESTORE)`** - Restores minimized window
- **`SetForegroundWindow()`** - Brings window to front
- **`BringWindowToTop()`** - Ensures window is topmost
- **`MB_SETFOREGROUND`** - Forces MessageBox to foreground
- **`MB_TASKMODAL`** - Makes MessageBox system-modal (blocks other windows in app)

### 2. Extensive Debug Logging

**Markers:** All debug logs tagged with `[BLD002-DEBUG]`

Added logging at critical points:

**Before SQLDriverConnect (line 3289):**
```c
rs_dsn_log(__LINE__, "test_connect: calling SQLDriverConnect, parent_hwnd=%p, tab_hwnd=%p [BLD002-DEBUG]",
    rs_dsn_setup_ctxt->hwndParent, hdlg);
```

**After SQLDriverConnect (line 3295):**
```c
rs_dsn_log(__LINE__, "test_connect: SQLDriverConnect returned rc=%d [BLD002-DEBUG]", rc);
```

**Window State Diagnostics (lines 3313-3328):**
```c
rs_dsn_log(__LINE__, "test_connect: preparing msgbox [BLD002-DEBUG], result: %s", resultmsg);
rs_dsn_log(__LINE__, "test_connect: parent_hwnd=%p, tab_hwnd=%p [BLD002-DEBUG]", ...);
rs_dsn_log(__LINE__, "test_connect: window minimized, restoring [BLD002-DEBUG]");
rs_dsn_log(__LINE__, "test_connect: SetForegroundWindow=%d, BringWindowToTop=%d [BLD002-DEBUG]", ...);
```

**MessageBox Execution (lines 3337-3342):**
```c
rs_dsn_log(__LINE__, "test_connect: MessageBox flags=0x%X, hwnd=%p [BLD002-DEBUG]", dlg_flag, hwndToUse);
int mbResult = MessageBox(hwndToUse, resultmsg, "Connection Test", dlg_flag);
rs_dsn_log(__LINE__, "test_connect: MessageBox returned %d [BLD002-DEBUG]", mbResult);
rs_dsn_log(__LINE__, "test_connect: completed [BLD002-DEBUG]");
```

### 3. Version Update

- Updated `version.txt` from `2.1.13.1` to `2.1.13.1-BLD002`
- All MSI builds will show this version number
- Easy to identify debug builds in the field

---

## 📋 Related Earlier Fixes

### Commit 0030326: Browser Disconnection Detection
**Date:** Earlier in session
**Issue:** User closing browser caused indefinite wait instead of immediate error

**Fix in `IAMBrowserAzureOAuth2CredentialsProvider.cpp:307-315`:**
```cpp
rs_string authCode = srv.GetCode();

// Check if authorization code is empty - user cancelled or closed browser
if (authCode.empty()) {
    IAMUtils::ThrowConnectionExceptionWithInfo(
        "Authentication failed. The authorization code was not received. "
        "This may occur if you cancelled the login, closed the browser window, "
        "or the OAuth flow did not complete successfully. Please try again.");
}
```

This fix ensures that browser cancellation is detected immediately instead of waiting for timeout.

### Commit c820b1a: DSN Attribute Count
**Issue:** UI positioning conflicts
**Fix:** Updated `DD_DSN_ATTR_COUNT` from 105 to 106

---

## 🔧 Testing Instructions

### Enable Logging

**Windows Registry:**
```
HKEY_LOCAL_MACHINE\SOFTWARE\Amazon\Amazon Redshift ODBC Driver (x64)
LogLevel = 4 (DWORD)
LogPath = C:\temp\odbc_log.txt (String)
```

Or via DSN setup:
- Open ODBC Data Source Administrator
- Configure → Advanced tab
- Set "Log Level" = 4
- Set "Log Path"

### Test Scenarios

#### Test 1: Normal Successful Connection
1. Open ODBC DSN setup
2. Configure Azure OAuth authentication
3. Click "Test Connection"
4. Complete browser login
5. **Expected:** MessageBox appears with "Connection successful!"
6. **Verify in log:**
   - `[BLD002-DEBUG]` markers present
   - `SetForegroundWindow=1` (success)
   - `MessageBox returned` value (1=OK, 2=Cancel)

#### Test 2: Failed Connection
1. Use invalid credentials
2. Click "Test Connection"
3. **Expected:** MessageBox with error details
4. **Verify:** Error message is visible

#### Test 3: Browser Cancellation
1. Click "Test Connection"
2. Close browser window immediately
3. **Expected:** Quick error message (not timeout)
4. **Verify in log:** "authorization code was not received" error

#### Test 4: Minimized Window
1. Start Test Connection
2. Minimize DSN setup dialog during auth
3. Complete browser login
4. **Expected:** Dialog restores and MessageBox appears
5. **Verify in log:** "window minimized, restoring [BLD002-DEBUG]"

---

## 📊 Log Analysis Guide

### Successful Test Connection Flow

```
test_connect: connecting [BLD002-DEBUG]
test_connect: calling SQLDriverConnect, parent_hwnd=0x000X, tab_hwnd=0x000Y [BLD002-DEBUG]
[... OAuth authentication logs ...]
test_connect: SQLDriverConnect returned rc=0 [BLD002-DEBUG]
test_connect: preparing msgbox [BLD002-DEBUG], result: Connection successful!
test_connect: parent_hwnd=0x000X, tab_hwnd=0x000Y [BLD002-DEBUG]
test_connect: attempting SetForegroundWindow [BLD002-DEBUG]
test_connect: SetForegroundWindow=1, BringWindowToTop=1 [BLD002-DEBUG]
test_connect: MessageBox flags=0x50000, hwnd=0x000X [BLD002-DEBUG]
test_connect: MessageBox returned 1 [BLD002-DEBUG]
test_connect: completed [BLD002-DEBUG]
```

### Key Indicators

| Log Entry | What It Means | Normal Value |
|-----------|---------------|--------------|
| `parent_hwnd=0x000...` | Valid window handle | Non-zero pointer |
| `SetForegroundWindow=1` | Successfully brought to front | 1 = success, 0 = failed |
| `BringWindowToTop=1` | Successfully raised window | 1 = success, 0 = failed |
| `MessageBox flags=0x...` | Dialog flags used | 0x50000 = MB_OK + MB_SETFOREGROUND + MB_TASKMODAL |
| `MessageBox returned 1` | User clicked OK | 1 = OK, 2 = Cancel, 0 = error |

### Troubleshooting

**If MessageBox Still Not Visible:**

1. Check log for `parent_hwnd=0x0` or `parent_hwnd=(nil)`
   - Indicates parent window handle is NULL
   - Should fallback to tab window

2. Check `SetForegroundWindow=0`
   - Failed to bring window to front
   - May indicate system restrictions or inactive app

3. Check `MessageBox returned 0`
   - MessageBox failed to display
   - Check for blocking dialogs or system errors

---

## 🚀 Build Instructions

```bash
cd /home/orel/redshift-odbc-fix/amazon-redshift-odbc-driver

# Verify version
cat version.txt
# Should show: 2.1.13.1-BLD002

# Build (Windows)
cd src/odbc/rsodbc/install
Make_x64.bat

# Output: RedshiftODBC-Community-v2.1.13.1-BLD002.msi
```

---

## 📝 Files Modified

1. **src/odbc/rsodbc/rsodbc_setup/setup.c**
   - Function: `rs_dsn_test_connect()`
   - Lines: 3287-3345 (approx)
   - Changes: Window focus recovery + extensive logging

2. **version.txt**
   - Old: `2.1.13.1`
   - New: `2.1.13.1-BLD002`

---

## ⚠️ Important Notes

### For Production Use
This is a **DEBUG BUILD** with verbose logging. For production:
- Remove `[BLD002-DEBUG]` logging statements
- Keep the window focus fix (lines 3315-3340 logic)
- Keep `MB_SETFOREGROUND | MB_TASKMODAL` flags
- Update version to production number

### Known Limitations
1. **SetForegroundWindow Restrictions**
   - Windows has restrictions on which processes can set foreground
   - May fail if another app is actively being used
   - MessageBox flags help mitigate this

2. **Multiple Monitor Setups**
   - Window may restore on different monitor
   - MessageBox follows parent window placement

3. **Terminal Services / RDP**
   - Focus behavior may differ in remote sessions
   - Browser authentication may have additional challenges

---

## 📚 References

### Related Code Files
- `src/odbc/rsodbc/iam/plugins/IAMBrowserAzureOAuth2CredentialsProvider.cpp` - OAuth flow
- `src/odbc/rsodbc/iam/http/WEBServer.cpp` - Browser callback listener
- `src/odbc/rsodbc/rsodbc_setup/setup.c` - DSN configuration UI

### Windows API Documentation
- [SetForegroundWindow](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
- [BringWindowToTop](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-bringwindowtotop)
- [MessageBox Flags](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-messagebox)

---

## 🎯 Success Criteria

This build is successful if:
1. ✅ Test Connection button always shows result MessageBox
2. ✅ MessageBox appears in foreground (not hidden)
3. ✅ Dialog returns to usable state after test
4. ✅ All `[BLD002-DEBUG]` logs appear in output
5. ✅ Browser cancellation shows immediate error (not timeout)

---

**Build prepared by:** Claude Code
**Issue reporter:** User (Hebrew)
**Debug build marker:** BLD002
**Status:** Ready for testing
