@echo off

REM Values to change include VERSION and SUBLOC, both below.

if NOT "%1"=="" SET VERSION=%1.%2
if NOT "%1"=="" GOTO GOT_VERSION

REM The full version number of the build in XX.XX.XXXX format
SET VERSION="2.0.0.0"

echo.
echo Version not specified - defaulting to %VERSION%
echo.

:GOT_VERSION

del AmazonRedshiftODBC64-%VERSION%.msi

set MSVCCFG=%ProgramFiles%
IF exist %HOMEDRIVE%\"Program Files (x86)" set MSVCCFG=%ProgramFiles(x86)%


:VS2K8
set MERGECRT=%MSVCCFG%/Common Files/Merge Modules/Microsoft_VC90_CRT_x86_x64.msm
set MERGEPOLICY=%MSVCCFG%/Common Files/Merge Modules/policy_9_0_Microsoft_VC90_CRT_x86_x64.msm


echo.
echo Building Amazon Redshift x64 ODBC merge module

candle -nologo -dVERSION=%VERSION% rsodbcm_x64.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -sw -out rsodbc_x64.msm rsodbcm_x64.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
rem echo Building Amazon Redshift x64 ODBC Help merge module

rem candle -nologo -dVERSION=%VERSION% helpm_x64.wxs
rem IF ERRORLEVEL 1 GOTO ERR_HANDLER

rem light -nologo -sw -out rsodbchelp_x64.msm helpm_x64.wixobj
rem IF ERRORLEVEL 1 GOTO ERR_HANDLER
rem echo Help module successfully built.


echo.
echo Building Amazon Redshift x64 ODBC installer database...

candle -nologo -dVERSION=%VERSION% -dMERGECRT="%MERGECRT%" -dMERGEPOLICY="%MERGEPOLICY%" rsodbc_x64.wxs
IF ERRORLEVEL 1 GOTO ERR_HANDLER

light -nologo -sw -ext WixUIExtension -cultures:en-us -out AmazonRedshiftODBC64-%VERSION%.msi rsodbc_x64.wixobj
IF ERRORLEVEL 1 GOTO ERR_HANDLER

echo.
echo Done!
GOTO EXIT

:ERR_HANDLER
echo.
echo Aborting build!
GOTO EXIT

:EXIT
