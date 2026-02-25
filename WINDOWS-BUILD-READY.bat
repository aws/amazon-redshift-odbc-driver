@echo off
REM ================================================================================
REM   Redshift ODBC Driver v2.1.15.0 - Quick Build Script
REM   Build and create MSI installer on Windows
REM ================================================================================

echo.
echo ========================================
echo   Redshift ODBC v2.1.15.0 Builder
echo   Authentication Cancel Fix
echo ========================================
echo.

REM Check if running from correct directory
if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found!
    echo Please run this script from the amazon-redshift-odbc-driver directory
    echo.
    echo Expected location:
    echo   C:\RedshiftODBC\amazon-redshift-odbc-driver\
    echo.
    pause
    exit /b 1
)

REM Check Visual Studio installation
echo Checking for Visual Studio...
set VS_FOUND=0

if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    set VS_FOUND=1
    echo Found: Visual Studio 2022 Community
    goto :vs_found
)

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    set VS_FOUND=1
    echo Found: Visual Studio 2019 Community
    goto :vs_found
)

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat"
    set VS_FOUND=1
    echo Found: Visual Studio 2019 Professional
    goto :vs_found
)

echo ERROR: Visual Studio not found!
echo Please install Visual Studio 2019 or later with C++ workload
echo Download from: https://visualstudio.microsoft.com/downloads/
pause
exit /b 1

:vs_found
echo Visual Studio environment loaded
echo.

REM Check CMake
echo Checking for CMake...
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: CMake not found!
    echo Please install CMake from: https://cmake.org/download/
    echo Make sure to add CMake to PATH during installation
    pause
    exit /b 1
)
cmake --version
echo.

REM Check WiX Toolset (for MSI creation)
echo Checking for WiX Toolset...
where candle.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: WiX Toolset not found!
    echo MSI installer will NOT be created
    echo To create MSI, install WiX from: https://wixtoolset.org/releases/
    echo.
    echo Press any key to continue without MSI creation, or Ctrl+C to abort
    pause
)
echo.

REM Verify version
echo Verifying version...
if exist "version.txt" (
    set /p VERSION=<version.txt
    echo Current version: %VERSION%
    if not "%VERSION%"=="2.1.15.0" (
        echo WARNING: Version is not 2.1.15.0!
        echo Expected: 2.1.15.0
        echo Found: %VERSION%
        echo.
        echo Press any key to continue anyway, or Ctrl+C to abort
        pause
    )
) else (
    echo WARNING: version.txt not found!
    pause
)
echo.

REM Clean previous build
echo Cleaning previous build...
if exist "cmake-build" (
    echo Removing old cmake-build directory...
    rmdir /S /Q cmake-build
)
echo.

REM Run build script
echo Starting build...
echo.
echo ========================================
echo   Running build64.bat
echo ========================================
echo.

call build64.bat

if %errorlevel% neq 0 (
    echo.
    echo ========================================
    echo   BUILD FAILED!
    echo ========================================
    echo.
    echo Check the error messages above
    pause
    exit /b 1
)

echo.
echo ========================================
echo   BUILD SUCCEEDED!
echo ========================================
echo.

REM Check if MSI was created
set MSI_PATH=cmake-build\install\src\odbc\rsodbc\install\AmazonRedshiftODBC64-2.1.15.0.msi

if exist "%MSI_PATH%" (
    echo.
    echo ========================================
    echo   MSI INSTALLER CREATED!
    echo ========================================
    echo.
    echo Location:
    echo   %CD%\%MSI_PATH%
    echo.

    REM Get file size
    for %%A in ("%MSI_PATH%") do set MSI_SIZE=%%~zA
    set /a MSI_SIZE_MB=%MSI_SIZE% / 1048576
    echo Size: %MSI_SIZE_MB% MB
    echo.

    REM Calculate SHA256
    echo Calculating SHA256 checksum...
    certutil -hashfile "%MSI_PATH%" SHA256 | findstr /v ":" | findstr /v "CertUtil" > "%MSI_PATH%.sha256.txt"
    echo Checksum saved to: %MSI_PATH%.sha256.txt
    echo.

    REM Copy to convenient location
    if not exist "C:\RedshiftBuilds" mkdir C:\RedshiftBuilds
    copy /Y "%MSI_PATH%" "C:\RedshiftBuilds\" >nul
    copy /Y "%MSI_PATH%.sha256.txt" "C:\RedshiftBuilds\" >nul
    echo MSI copied to: C:\RedshiftBuilds\
    echo.

    echo ========================================
    echo   READY TO DISTRIBUTE!
    echo ========================================
    echo.
    echo Next steps:
    echo   1. Test the MSI installer
    echo   2. Verify authentication cancellation works
    echo   3. Distribute to users
    echo.
) else (
    echo.
    echo ========================================
    echo   MSI NOT CREATED
    echo ========================================
    echo.
    echo Build succeeded but MSI was not created.
    echo This usually means WiX Toolset is not installed.
    echo.
    echo Driver DLL built successfully at:
    echo   cmake-build\install\bin\rsodbc.dll
    echo.
    echo To create MSI, install WiX Toolset:
    echo   https://wixtoolset.org/releases/
    echo.
)

echo Build log saved to: build.log
echo.
pause
exit /b 0
