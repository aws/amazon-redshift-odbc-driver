@rem version information must by applied to version files. Let's find them.
@echo off
echo "PACKAGING ..."
echo "#define ODBC_DRIVER_VERSION_FULL" > temp1.txt
rem Set the path to the rsversion.h file
set RSVERSION_FILE=src\odbc\rsodbc\rsversion.h
rem Find the line with the ODBC_DRIVER_VERSION_FULL definition
findstr /r /c:"#define ODBC_DRIVER_VERSION_FULL" %RSVERSION_FILE% > temp.txt
rem Parse the version information from the line
for /f "tokens=3" %%a in (temp.txt) do (
    set ODBC_VERSION_FULL=%%a
)
rem Remove the double quotes from the version string
set ODBC_VERSION_FULL=%ODBC_VERSION_FULL:"=%
echo %ODBC_VERSION_FULL%
rem Split the version string into its components
for /f "tokens=1-4 delims=." %%a in ("%ODBC_VERSION_FULL%") do (
    set ODBC_VERSION_MAJOR=%%a
    set ODBC_VERSION_MINOR=%%b
    set ODBC_VERSION_PATCH=%%c
    set ODBC_VERSION_SVN=%%d
)
rem Display the extracted version information
echo ODBC_VERSION_MAJOR=%ODBC_VERSION_MAJOR%
echo ODBC_VERSION_MINOR=%ODBC_VERSION_MINOR%
echo ODBC_VERSION_PATCH=%ODBC_VERSION_PATCH%
echo ODBC_VERSION_SVN=%ODBC_VERSION_SVN%
rem Clean up the temporary file
del temp.txt
rem Make the installer
pushd src\odbc\rsodbc\install
echo %cd%
set THISCOMMAND=Call Make_x64
call Make_x64 %ODBC_VERSION_MAJOR%.%ODBC_VERSION_MINOR%.%ODBC_VERSION_PATCH% %ODBC_VERSION_SVN%
if errorlevel 1 goto baderrorlevel

set THISCOMMAND=Call ModifyMsi.exe
call ModifyMsi.exe AmazonRedshiftODBC64-%VERSION%.msi 
if errorlevel 1 goto baderrorlevel

set WIN_ODBC_BUILD_MSI=%CD%\AmazonRedshiftODBC64-%VERSION%.msi
echo Please get installer MSI in %WIN_ODBC_BUILD_MSI%

certutil -hashfile "%WIN_ODBC_BUILD_MSI%" SHA384
certutil -hashfile "%WIN_ODBC_BUILD_MSI%" SHA256

popd || exit /b %ERRORLEVEL%
echo %WIN_ODBC_BUILD_MSI% > odbcmsi.tmp
:baderrorlevel
if errorlevel 1 (
  echo Failure during "%THISCOMMAND%" step: Exit code is %errorlevel%
  exit /b %errorlevel%
)