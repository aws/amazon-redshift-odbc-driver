rem @echo off
rem Take two arguments as version number. e.g. 2.0.0 1
echo Building 64 bit Windows Amazon Redshift ODBC Driver

rem Build libpq
cd .\src\pgclient
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

cd ..\..\..

:baderrorlevel
if errorlevel 1 (
  echo Failure during "%THISCOMMAND%" step: Exit code is %errorlevel%
  exit /b %errorlevel%
)
