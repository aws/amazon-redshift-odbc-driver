# Redshift ODBC Connection Tester

Simple command-line tool to test Amazon Redshift ODBC connections, with support for OAuth 2.0 authentication flows.

## Why This Tool?

The Windows ODBC Administrator (`odbcad32.exe`) freezes during OAuth authentication because it doesn't handle long-running browser-based authentication flows properly. This tool provides a better testing experience:

- ✅ No UI freezing
- ✅ Clear progress messages
- ✅ Supports OAuth 2.0 browser flows
- ✅ Detailed error messages
- ✅ Connection timing information

## Requirements

- .NET Framework 4.0 or higher
- Windows (32-bit or 64-bit)
- Amazon Redshift ODBC driver installed

## Building

### Visual Studio 2010 or later:

1. Open `RedshiftOdbcTester.csproj` in Visual Studio
2. Select Build Configuration:
   - **x64** for 64-bit ODBC drivers (recommended)
   - **x86** for 32-bit ODBC drivers
3. Build → Build Solution (Ctrl+Shift+B)
4. Find output in `bin\x64\Release\` or `bin\x86\Release\`

### Command Line (MSBuild):

**For 64-bit:**
```cmd
"%ProgramFiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe" RedshiftOdbcTester.csproj /p:Configuration=Release /p:Platform=x64
```

**For 32-bit:**
```cmd
"%ProgramFiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe" RedshiftOdbcTester.csproj /p:Configuration=Release /p:Platform=x86
```

## Usage

### Interactive Mode (Easiest):

```cmd
RedshiftOdbcTester.exe
```

Then enter your DSN name when prompted.

### Command Line with DSN:

```cmd
RedshiftOdbcTester.exe MyRedshiftDSN
```

or

```cmd
RedshiftOdbcTester.exe DSN=MyRedshiftDSN
```

### Command Line with Full Connection String:

```cmd
RedshiftOdbcTester.exe "Driver={Amazon Redshift (x64)};Server=cluster.redshift.amazonaws.com;Database=mydb;plugin_name=BrowserAzureADOAuth2;idp_tenant=xxx;client_id=yyy;idp_use_https_proxy=1"
```

## Example Output

### Successful Connection:

```
========================================
  Redshift ODBC Connection Tester
  Version 1.0 - .NET 4.0
========================================

Connection String: DSN=AmazonRedshiftOAuth

[15:30:45] Creating connection...
[15:30:45] Opening connection...
                (Browser may open for authentication)

*** CONNECTION SUCCESSFUL ***
Time: 18.45 seconds

Connection Information:
  Server: my-cluster.redshift.amazonaws.com
  Database: production
  Driver: Amazon Redshift (x64)
  Server Version: 1.0.12345

[15:31:03] Running test query...

Query Results:
  User:      user@company.com
  Database:  production
  Timestamp: 2026-02-04 15:31:03
  Version:   PostgreSQL 8.0.2 on i686-pc-linux-gnu, compiled by GCC...

*** ALL TESTS PASSED ***
```

### Failed Connection:

```
========================================
  Redshift ODBC Connection Tester
  Version 1.0 - .NET 4.0
========================================

Connection String: DSN=InvalidDSN

[15:30:45] Creating connection...
[15:30:45] Opening connection...
                (Browser may open for authentication)

*** CONNECTION FAILED ***

ODBC Error:
  Message: ERROR [HY000] Authentication failed
  Source: rsodbc30

Detailed Errors:
  [1] SQLState: HY000
      Message: Authentication failed on the Browser server
      Native Error: 0

Troubleshooting:
  1. Check ODBC driver is installed
  2. Verify DSN configuration in odbcad32.exe
  3. Check connection parameters (server, database, auth)
  4. Review logs in: %TEMP%\Amazon Redshift ODBC Driver\logs\
```

## OAuth 2.0 Flow

When testing connections with OAuth (e.g., BrowserAzureADOAuth2):

1. Tool starts connection
2. Browser opens automatically
3. User logs in to Azure/IdP
4. User closes browser when instructed
5. Tool completes connection and shows results

The tool remains responsive throughout this process, unlike odbcad32.exe which freezes.

## Exit Codes

- `0` - Success
- `1` - Failure (connection or query error)

Useful for automation and scripting.

## Troubleshooting

### "Could not load file or assembly System.Data"

Install .NET Framework 4.0 or higher.

### "Data source name not found"

Check DSN exists in ODBC Data Sources:
- 64-bit drivers: `C:\Windows\System32\odbcad32.exe`
- 32-bit drivers: `C:\Windows\SysWOW64\odbcad32.exe`

### OAuth browser doesn't open

Check firewall settings and that the driver supports browser-based authentication.

### Connection hangs

If using OAuth in a corporate environment with proxy, ensure:
```
idp_use_https_proxy=1
https_proxy_host=your-proxy
https_proxy_port=8080
```

are set in your DSN or connection string.

## License

This tool is provided as-is for testing Amazon Redshift ODBC connections.

## Related

- [QUICKSTART.md](../../QUICKSTART.md) - Quick setup guide
- [AZURE_OAUTH_SOLUTION.md](../../AZURE_OAUTH_SOLUTION.md) - Azure OAuth troubleshooting
- Driver logs: `%TEMP%\Amazon Redshift ODBC Driver\logs\`
