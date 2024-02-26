rem @echo off
rem Take two arguments as version number. e.g. 2.0.0 1
echo Building and Packaging 64 bit Windows Amazon Redshift ODBC Driver

@echo off
setlocal enabledelayedexpansion

rem Check if version string is provided, else read from version.txt
if "%~1" == "" (
    rem Read version string from version.txt
    if not exist ..\..\..\version.txt (
        echo Error: Version string not provided and version.txt not found.
        echo Please provide the version string as an argument or create a version.txt file containing the version string.
        exit /b 1
    ) else (
        set /p VERSION_STRING=<..\..\..\version.txt
        echo Using version string from version.txt: !VERSION_STRING!
    )
) else (
    set "VERSION_STRING=%*"
)

echo VERSION_STRING is !VERSION_STRING!

rem Parse version string
for /f "tokens=1-3,4 delims=. " %%a in ("!VERSION_STRING!") do (
    set ODBC_VERSION_MAJOR=%%a
    set ODBC_VERSION_MINOR=%%b
    set ODBC_VERSION_PATCH=%%c
    set ODBC_VERSION_SVN=%%d
)

@REM Easy to just copy the template and make substitutions
copy rsversion.h.in rsversion.h
powershell -Command "(gc rsversion.h) -replace '@ODBC_VERSION_MAJOR@', !ODBC_VERSION_MAJOR!  | Out-File -encoding ASCII rsversion.h"
powershell -Command "(gc rsversion.h) -replace '@ODBC_VERSION_MINOR@', !ODBC_VERSION_MINOR!  | Out-File -encoding ASCII rsversion.h"
powershell -Command "(gc rsversion.h) -replace '@ODBC_VERSION_PATCH@', !ODBC_VERSION_PATCH!  | Out-File -encoding ASCII rsversion.h"
powershell -Command "(gc rsversion.h) -replace '@ODBC_VERSION_SVN@', !ODBC_VERSION_SVN!  | Out-File -encoding ASCII rsversion.h"

set odbc_version=!ODBC_VERSION_MAJOR!.!ODBC_VERSION_MINOR!.!ODBC_VERSION_PATCH!
set svn_rev=!ODBC_VERSION_SVN!
set odbc_driver_full_version=%odbc_version%.%svn_rev%

@REM More custom(legacy) changes to version.h and rsversion.h
echo Setting REDSHIFT_DRIVER_VERSION to %odbc_driver_full_version% in rsodbc_setup/version.h
cd rsodbc_setup
@PowerShell "(GC .\version.h)|%%{$_ -Replace '#define REDSHIFT_DRIVER_VERSION.*', '#define REDSHIFT_DRIVER_VERSION         \"%odbc_driver_full_version%\"'} |SC .\version.h"
cd ..

echo Setting Driver version in rsversion.h
set comma_delim_version=%odbc_version:.=,%,%svn_rev%
set comma_delim_w_space_version=%odbc_version:.=, %, %svn_rev%

setlocal EnableDelayedExpansion
set idx=0
set space_padded_version=

for %%i in (%odbc_version:.=,%) do (
  set /A idx +=1

  if [!idx!] == [3] (
    set "formattedValue=%%i"
    set formattedValue=!formattedValue:~-4!
  ) else (
    set "formattedValue=%%i"
    set formattedValue=!formattedValue:~-2!
  )
  
  if [!idx!] == [1] (
    set space_padded_version=!space_padded_version!!formattedValue!
  ) else (
    set space_padded_version=!space_padded_version!.!formattedValue!
  )
)
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define ODBC_DRIVER_VERSION .*', '#define ODBC_DRIVER_VERSION \"!space_padded_version!\"'} |SC .\rsversion.h"
endlocal

@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define FILEVER.*', '#define FILEVER        %comma_delim_version%'} |SC .\rsversion.h"
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define PRODUCTVER.*', '#define PRODUCTVER     %comma_delim_version%'} |SC .\rsversion.h"
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define STRFILEVER.*', '#define STRFILEVER     \"%comma_delim_w_space_version%\0\"'} |SC .\rsversion.h"
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define STRPRODUCTVER.*', '#define STRPRODUCTVER  \"%comma_delim_w_space_version%\"'} |SC .\rsversion.h"

rem Build libpq
cd ..\..\pgclient
set THISCOMMAND=Build libpq
call build64.bat
if errorlevel 1 goto baderrorlevel

rem Build ODBC Setup DLL
cd ..\odbc\rsodbc\rsodbc_setup
set THISCOMMAND=Build ODBC Setup DLL
call build64.bat
if errorlevel 1 goto baderrorlevel

rem Build ODBC Driver DLL
cd ..
set THISCOMMAND=Build ODBC Driver DLL
call build64.bat !ODBC_VERSION_MAJOR!.!ODBC_VERSION_MINOR!.!ODBC_VERSION_PATCH! !ODBC_VERSION_SVN!
if errorlevel 1 goto baderrorlevel

rem Build ODBC Samples
cd samples
set THISCOMMAND=Build ODBC Samples
call build64.bat
if errorlevel 1 goto baderrorlevel
cd ..

rem Build rsodbcsql
cd rsodbcsql
call build64.bat
set THISCOMMAND=Build rsodbcsql
if errorlevel 1 goto baderrorlevel
cd ..

rem Now make the installer using version provided
cd install
del *64.wixobj
del *64.msm
del *64.wixpdb
set THISCOMMAND=Call Make_x64
call Make_x64 !ODBC_VERSION_MAJOR!.!ODBC_VERSION_MINOR!.!ODBC_VERSION_PATCH! !ODBC_VERSION_SVN!
if errorlevel 1 goto baderrorlevel

set THISCOMMAND=Call ModifyMsi.exe
call ModifyMsi.exe AmazonRedshiftODBC64-%VERSION%.msi 
if errorlevel 1 goto baderrorlevel


cd ..
echo Please get installer MSI in %CD%\install\AmazonRedshiftODBC64-%VERSION%.msi 

:baderrorlevel
if errorlevel 1 (
  echo Failure during "%THISCOMMAND%" step: Exit code is %errorlevel%
  exit /b %errorlevel%
)
