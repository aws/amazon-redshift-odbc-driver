@echo off
REM Build script for RedshiftOdbcTester
REM Supports Visual Studio 2010 - 2019

setlocal

echo ========================================
echo  Building Redshift ODBC Tester
echo ========================================
echo.

REM Check for command line argument
set PLATFORM=%1
if "%PLATFORM%"=="" set PLATFORM=x64

echo Platform: %PLATFORM%
echo.

REM Find MSBuild
set MSBUILD=

REM Try VS 2019
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe
    goto :build
)

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe
    goto :build
)

REM Try VS 2017
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe" (
    set MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe
    goto :build
)

REM Try VS 2015
if exist "%ProgramFiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe" (
    set MSBUILD=%ProgramFiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe
    goto :build
)

REM Try VS 2013
if exist "%ProgramFiles(x86)%\MSBuild\12.0\Bin\MSBuild.exe" (
    set MSBUILD=%ProgramFiles(x86)%\MSBuild\12.0\Bin\MSBuild.exe
    goto :build
)

REM Try VS 2010
if exist "%SystemRoot%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe" (
    set MSBUILD=%SystemRoot%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe
    goto :build
)

echo ERROR: MSBuild not found!
echo.
echo Please install one of:
echo  - Visual Studio 2010 or later
echo  - .NET Framework 4.0 SDK or later
echo.
exit /b 1

:build
echo Found MSBuild: %MSBUILD%
echo.

echo Building Release configuration for %PLATFORM%...
echo.

"%MSBUILD%" RedshiftOdbcTester.csproj /p:Configuration=Release /p:Platform=%PLATFORM% /v:minimal

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ========================================
echo  Build Successful!
echo ========================================
echo.
echo Output: bin\%PLATFORM%\Release\RedshiftOdbcTester.exe
echo.
echo To run:
echo   cd bin\%PLATFORM%\Release
echo   RedshiftOdbcTester.exe
echo.

endlocal
