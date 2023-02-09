@echo off
echo Building 64 bit Windows 9.1.2 libpq

echo Copy windows specific file(s)
set THISCOMMAND=Copy pg_config.h.win32
copy .\src\include\pg_config.h.win32 .\src\include\pg_config.h
if errorlevel 1 goto baderrorlevel

set THISCOMMAND=Copy pg_config.os.h.win32
copy .\src\include\pg_config_os.h.win32 .\src\include\pg_config_os.h
if errorlevel 1 goto baderrorlevel

echo Setting environment variables
rem call vcvars64
set THISCOMMAND=Call vcvarsall
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
rem call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
if errorlevel 1 goto baderrorlevel

set THISCOMMAND=devenv
devenv /Rebuild "Release|x64" pgclient.sln
if errorlevel 1 goto baderrorlevel

echo Done building 64 bit Windows 9.1.2 libpq

:baderrorlevel
if errorlevel 1 (
  echo Failure during "%THISCOMMAND%" step: Exit code is %errorlevel%
  exit /b %errorlevel%
)