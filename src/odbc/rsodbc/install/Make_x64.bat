@echo off

REM WiX 6 Build Script for Amazon Redshift ODBC Driver

if NOT "%1"=="" SET VERSION=%1.%2
if NOT "%1"=="" GOTO GOT_VERSION

REM The full version number of the build in XX.XX.XXXX format
SET VERSION="2.0.0.0"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

echo Version specified: %VERSION%
del AmazonRedshiftODBC64-%VERSION%.msi 2>nul

set PROJECT_DIR=../../../..

echo.
echo Building Amazon Redshift x64 ODBC merge module with WiX 6
wix build -arch x64 -d ProjectDir=%PROJECT_DIR% -d DependenciesDir=%DEPENDENCIES_INSTALL_DIR% -d BuildType=%RS_BUILD_TYPE% -d VERSION=%VERSION% -out rsodbc_x64.msm rsodbcm_x64.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER
echo Merge module built successfully

echo.
echo Building Amazon Redshift x64 ODBC installer with WiX 6
if defined WIX_UI_EXT_PATH (
    echo Using UI extension from a location: %WIX_UI_EXT_PATH%
    wix build -arch x64 -ext "%WIX_UI_EXT_PATH%" -d ProjectDir=%PROJECT_DIR% -d VERSION=%VERSION% -out AmazonRedshiftODBC64-%VERSION%.msi rsodbc_x64.wxs
) else (
    echo Using UI extension by name
    wix build -arch x64 -ext WixToolset.UI.wixext -d ProjectDir=%PROJECT_DIR% -d VERSION=%VERSION% -out AmazonRedshiftODBC64-%VERSION%.msi rsodbc_x64.wxs
)
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Done!
GOTO EXIT

:ERR_HANDLER
echo.
echo Aborting build!
GOTO EXIT

:EXIT
