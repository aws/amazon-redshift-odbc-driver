# Azure OAuth2 Authentication - Complete Solution

## Problem Summary

Azure OAuth2 authentication with BrowserAzureADOAuth2 plugin was failing in corporate environments with:
- Browser opens successfully
- User authenticates in Azure
- Authentication approved (visible in INTRA)
- **Driver freezes** - control never returns to the application

## Root Cause

The issue was NOT related to localhost redirect or proxy blocking the OAuth callback.

**Actual Problem**: The HTTP POST request to Azure's token endpoint (`https://login.microsoftonline.com/.../token`) was failing because:
- Corporate environment requires proxy for external HTTPS connections
- Default setting: `idp_use_https_proxy=0` (disabled)
- Token request failed silently with status code -1
- Driver waited indefinitely for response

## Solution

### Required Configuration

Add the following parameter to your ODBC connection string:

```
idp_use_https_proxy=1
```

### Complete Connection String Example

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

### Why This Works

When `idp_use_https_proxy=1` is set:
- OAuth browser flow works normally (localhost callback)
- Token exchange request uses configured corporate proxy
- External HTTPS connection succeeds
- Authentication completes successfully

## Technical Details

### Authentication Flow

1. **Browser Launch**: Opens Azure login URL
2. **User Authentication**: User logs in via browser
3. **OAuth Callback**: Azure redirects to `http://localhost:PORT/redshift/` with authorization code
4. **Local Server**: WEBServer receives POST with authorization code
5. **Token Request**: HTTP POST to `https://login.microsoftonline.com/.../oauth2/v2.0/token`
   - **This step requires proxy in corporate environments**
6. **Token Response**: Receives access token and ID token
7. **Connection**: Uses tokens to connect to Redshift

### Diagnostic Logging

Build #25 includes enhanced logging:

**WEBServer.cpp**:
- Listen port selection
- Request reception
- Parsing progress

**Parser.cpp**:
- HTTP request parsing
- Header extraction
- Authorization code extraction

**IAMBrowserAzureOAuth2CredentialsProvider.cpp**:
- Browser launch
- Proxy configuration check
- Token request status code
- Response handling

### Log Analysis Example

**Without `idp_use_https_proxy=1`** (fails):
```
[DEBUG] RequestAccessToken: Proxy check - UsingHTTPSProxy=1, UseProxyIdpAuth=0
[DEBUG] RequestAccessToken: NOT using proxy for IDP auth
[DEBUG] RequestAccessToken: HTTP request completed. Status code: -1
[ERROR] Authentication failed on the Browser server... Response code: -1
```

**With `idp_use_https_proxy=1`** (works):
```
[DEBUG] RequestAccessToken: Proxy check - UsingHTTPSProxy=1, UseProxyIdpAuth=1
[DEBUG] RequestAccessToken: Using HTTPS proxy - Host=campusbc, Port=8080
[DEBUG] RequestAccessToken: HTTP request completed. Status code: 200
[DEBUG] Response: {"token_type":"Bearer","access_token":"...","id_token":"..."}
[DEBUG] SQLDriverConnectW() return SQL_SUCCESS
```

## Implementation Changes

### Build History

- **Build #18-19**: Restored all 14 authentication providers (ADFS, AzureAD, Ping, Okta, etc.)
- **Build #20**: Added debug logging to WEBServer and Parser
- **Build #21-24**: Attempted proxy bypass solutions (unnecessary)
- **Build #25**: Simplified LaunchBrowser, kept diagnostic logging

### Key Files Modified

1. **IAMBrowserAzureOAuth2CredentialsProvider.cpp**:
   - Simplified LaunchBrowser (removed proxy bypass attempts)
   - Enhanced RequestAccessToken logging
   - Clear proxy configuration diagnostics

2. **WEBServer.cpp**:
   - Added listener thread logging
   - Port selection logging
   - Request reception tracking

3. **Parser.cpp**:
   - HTTP parsing state machine logging
   - Request line and header logging
   - Authorization code extraction logging

4. **rs_iam_support.h**:
   - Added `IAM_KEY_PROXY_BYPASS_LIST` constant (unused in final solution)

## Supported Authentication Methods

Build #25 includes support for all AWS-compatible authentication methods:

1. BrowserAzureADOAuth2 (with OAuth 2.0)
2. BrowserAzureAD (legacy SAML)
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

## Testing

### Test Environment
- Windows with corporate proxy
- Air-gapped network
- Chrome browser installed
- Azure AD tenant configured
- Redshift cluster with IAM authentication

### Test Results
✅ Browser launches successfully
✅ Azure authentication completes
✅ OAuth callback received on localhost
✅ Token request succeeds via proxy
✅ Connection established
✅ SQL queries execute normally

## Troubleshooting

### If authentication still fails:

1. **Check proxy settings**:
   ```
   https_proxy_host=<your-proxy>
   https_proxy_port=<port>
   idp_use_https_proxy=1
   ```

2. **Enable debug logging**:
   - Check driver logs at: `%TEMP%\Amazon Redshift ODBC Driver\logs\`
   - Look for "RequestAccessToken: Status code: XXX"
   - Status code -1 = proxy issue
   - Status code 400/401 = authentication issue
   - Status code 200 = success

3. **Verify Azure App Registration**:
   - Redirect URI: `http://localhost:7890/redshift/` (or your port)
   - Grant admin consent for required scopes
   - Ensure user has access

4. **Check firewall**:
   - Ensure localhost traffic allowed
   - Port 7890 (or custom listen_port) not blocked

## Installation

1. Download: `AmazonRedshiftODBC64-Fork-v2.1.13.0-AzureOAuth.msi`
2. Uninstall previous version (if exists)
3. Run MSI installer
4. Configure ODBC DSN with required parameters
5. Test connection

## Version Information

- **Driver Version**: 2.1.13.0
- **Build**: #25
- **Date**: February 4, 2026
- **Branch**: fix-azure-oauth-scope
- **Repository**: ORELASH/amazon-redshift-odbc-driver

## Credits

Generated with Claude Code (https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
