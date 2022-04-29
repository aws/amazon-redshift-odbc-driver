rem @echo off
rem Take two arguments as version number. e.g. 2.0.0 1
echo Building and Packaging 64 bit Windows Amazon Redshift ODBC Driver

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
call build64.bat %1 %2
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
call Make_x64 %1 %2
if errorlevel 1 goto baderrorlevel

set THISCOMMAND=Call ModifyMsi.exe
call ModifyMsi.exe AmazonRedshiftODBC64-%VERSION%.msi 
if errorlevel 1 goto baderrorlevel


cd ..
echo Please get installer MSI under "install" directory.

:baderrorlevel
if errorlevel 1 (
  echo Failure during "%THISCOMMAND%" step: Exit code is %errorlevel%
  exit /b %errorlevel%
)
