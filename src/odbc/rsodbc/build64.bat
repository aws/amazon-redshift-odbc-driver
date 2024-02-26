@echo off
echo Building 64 bit Windows Redshift ODBC Driver

echo Setting environment variables
rem call vcvars64
rem call vcvarsall x86_amd64
set THISCOMMAND=Call vcvarsall
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
rem call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
if errorlevel 1 goto baderrorlevel

set THISCOMMAND=devenv
devenv /Rebuild "Release|x64" rsodbc.sln
if errorlevel 1 goto baderrorlevel

echo Done building 64 bit Windows Redshift ODBC Driver

:baderrorlevel
if errorlevel 1 (
  echo Failure during "%THISCOMMAND%" step: Exit code is %errorlevel%
  exit /b %errorlevel%
)