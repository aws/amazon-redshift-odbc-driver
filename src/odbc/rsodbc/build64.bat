@echo off
echo Building 64 bit Windows Redshift ODBC Driver

set odbc_version=%1
set svn_rev=%2
set odbc_driver_full_version=%odbc_version%.%svn_rev%

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
    set "formattedValue=0000%%i"
    set formattedValue=!formattedValue:~-4!
  ) else (
    set "formattedValue=00%%i"
    set formattedValue=!formattedValue:~-2!
  )
  
  if [!idx!] == [1] (
    set space_padded_version=!space_padded_version!!formattedValue!
  ) else (
    set space_padded_version=!space_padded_version!.!formattedValue!
  )
)
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define ODBC_DRIVER_VERSION.*', '#define ODBC_DRIVER_VERSION \"!space_padded_version!\"'} |SC .\rsversion.h"
endlocal

@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define FILEVER.*', '#define FILEVER        %comma_delim_version%'} |SC .\rsversion.h"
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define PRODUCTVER.*', '#define PRODUCTVER     %comma_delim_version%'} |SC .\rsversion.h"
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define STRFILEVER.*', '#define STRFILEVER     \"%comma_delim_w_space_version%\0\"'} |SC .\rsversion.h"
@PowerShell "(GC .\rsversion.h)|%%{$_ -Replace '#define STRPRODUCTVER.*', '#define STRPRODUCTVER  \"%comma_delim_w_space_version%\"'} |SC .\rsversion.h"

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