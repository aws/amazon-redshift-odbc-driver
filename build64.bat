@echo off
setlocal enabledelayedexpansion

rem Check for help argument
if "%1"=="/?" goto :show_help
if "%1"=="-h" goto :show_help
if "%1"=="--help" goto :show_help

set startTime=%time%

echo Create a symlink to overcome CMake error FTK1011
set "LINK_PKG_PATH=C:\ProgramData\BTW\workplace\RSODBCDriver"
:: Ensure parent directory exists
mkdir "C:\ProgramData\BTW\workplace"
:: Check if the path already exists and delete if necessary
IF EXIST "!LINK_PKG_PATH!" (
    echo Deleting the directory or junction: !LINK_PKG_PATH!
    rmdir /s /q "!LINK_PKG_PATH!"
) else (
    echo Confirmed that directory or junction does not exist: !LINK_PKG_PATH!
)
:: Create the directory junction
echo Creating the directory junction: !LINK_PKG_PATH! and %CD%
mklink /j "!LINK_PKG_PATH!" "%CD%"
dir "!LINK_PKG_PATH!"

echo Create a clean public directory for artifacts
set "RS_ARTIFACTS_DIR=!LINK_PKG_PATH!\public"
mkdir !RS_ARTIFACTS_DIR!
rmdir /s/q !RS_ARTIFACTS_DIR!\*

rem Initialize variables (preserve pre-set environment variables)
set WIN_ODBC_BUILD_MSI=""
set "CMAKE_ARGS_ODBC_VERSION="
if not defined RS_BUILD_DIR set RS_BUILD_DIR=
if not defined INSTALL_DIR set INSTALL_DIR=
set RS_ROOT_DIR=!LINK_PKG_PATH!
if not defined ENABLE_TESTING set ENABLE_TESTING=
if not defined RS_OPENSSL_DIR set RS_OPENSSL_DIR=
if not defined RS_MULTI_DEPS_DIRS set RS_MULTI_DEPS_DIRS=
if not defined RS_DEPS_DIRS set RS_DEPS_DIRS=
if not defined RS_ODBC_DIR set RS_ODBC_DIR=
set "RS_VERSION="
set "RS_BUILD_TYPE=Release"
if not defined PYTHON_CMD set PYTHON_CMD=
if not defined PERL_CMD set PERL_CMD=

echo "LINK_PKG_PATH====== dir !LINK_PKG_PATH! ========"
@REM dir !LINK_PKG_PATH!

rem Parse command line arguments
:parse_args
if "%~1"=="" goto :end_parse_args
if "%~1"=="--version" (
    set "RS_VERSION=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--build-type" (
    set "RS_BUILD_TYPE=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--dependencies-install-dir" (
    set "DEPENDENCIES_INSTALL_DIR=%~2"
    shift
    shift
    goto :parse_args
)
shift
goto :parse_args
:end_parse_args

if not defined DEPENDENCIES_INSTALL_DIR (
    if exist "!LINK_PKG_PATH!\find-dependencies-install-dir.bat" (
        :: Hoping to find DEPENDENCIES_INSTALL_DIR from a helper script
        echo "DEPENDENCIES_INSTALL_DIR does not exist. Finding by Calling !LINK_PKG_PATH!\find-dependencies-install-dir.bat ..."
        call !LINK_PKG_PATH!\find-dependencies-install-dir.bat
    )
    :: Default behavior
    if not defined DEPENDENCIES_INSTALL_DIR (
        echo "DEPENDENCIES_INSTALL_DIR still does not exist. Falling back to default !RS_ARTIFACTS_DIR!.
        set "DEPENDENCIES_INSTALL_DIR=!RS_ARTIFACTS_DIR!"
    )
)


rem Display parsed arguments
echo Version: !RS_VERSION!
echo Build Type: !RS_BUILD_TYPE!
echo Dependencies Install: !DEPENDENCIES_INSTALL_DIR!

rem Set OpenSSL and dependencies directories based on DEPENDENCIES_INSTALL_DIR
if defined DEPENDENCIES_INSTALL_DIR (
    if not defined RS_OPENSSL_DIR (
        if exist "!DEPENDENCIES_INSTALL_DIR!\openssl\Release" (
            set "RS_OPENSSL_DIR=!DEPENDENCIES_INSTALL_DIR!\openssl\Release"
            echo Using OpenSSL from: !RS_OPENSSL_DIR!
        )
    ) else (
        echo RS_OPENSSL_DIR already set to: !RS_OPENSSL_DIR!
    )
    if not defined RS_MULTI_DEPS_DIRS (
        set "RS_MULTI_DEPS_DIRS=!DEPENDENCIES_INSTALL_DIR!"
    )
    echo Using dependencies from: !RS_MULTI_DEPS_DIRS!
)

rem Visual Studio environment settings
set "VS_PATH="
set "cmake_generator=Visual Studio 17 2022"
if "!VS_PATH!"=="" (
    echo checking vs candidates
    set "vs_path_candidates="
    rem List of paths to check
    set "vs_path_candidates=!vs_path_candidates!C:\Program Files\Microsoft Visual Studio\2022\Enterprise;"
    set "vs_path_candidates=!vs_path_candidates!C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools;"
    set "vs_path_candidates=!vs_path_candidates!C:\Program Files (x86)\Microsoft Visual Studio\2022\Community;"
    set "vs_path_candidates=!vs_path_candidates!C:\Program Files\Microsoft Visual Studio\2022\BuildTools;"
    set "vs_path_candidates=!vs_path_candidates!C:\Program Files\Microsoft Visual Studio\2022\Community"
    rem Loop through paths and find the first valid one
    FOR %%p IN ("!vs_path_candidates:;=";"!") do (
        echo checking %%p
        if exist "%%p" (
            echo setting %%p
            set "VS_PATH=%%p"
            echo done setting !VS_PATH!
            goto :vs_path_found
        ) else (
            echo not found %%p
        )
    )
) else (
    goto :vs_path_found
)


if "!VS_PATH!"=="" (
    echo No Visual Studio installation found.
) else (
    echo Visual Studio Path using candiates paths: !VS_PATH!
)
:vs_path_found
set "VS_PATH=!VS_PATH:"=!"
echo calling !VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat
call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64
where nmake
rem Absolute paths to the required build tool directories
set "CMAKE_BIN_DIR=!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
set "MSBUILD_BIN_DIR=!VS_PATH!\MSBuild\Current\Bin\amd64"
set "VS170COMNTOOLS=!VS_PATH!\Common7\Tools"

rem Find the latest Wix, used for creating MSI
set "WIX_HIGHEST_VERSION=0"
set "WIX_HIGHEST_PATH="

rem Search both Program Files (x86) and Program Files
for %%R in ("C:\Program Files (x86)","C:\Program Files") do (
    for /d %%D in ("%%~R\WiX Toolset*") do (
        rem Extract the version number from the folder name
        for /f "tokens=1-2 delims=. " %%A in ("%%~nD") do (
            if /i "%%A"=="WiX" (
                set "WIX_CURRENT_VERSION=%%B"
                rem Compare versions numerically
                if !WIX_CURRENT_VERSION! GTR !WIX_HIGHEST_VERSION! (
                    set "WIX_HIGHEST_VERSION=!WIX_CURRENT_VERSION!"
                    set "WIX_HIGHEST_PATH=%%D"
                )
            )
        )
    )
)

if defined WIX_HIGHEST_PATH (
    echo Highest WiX version path: !WIX_HIGHEST_PATH!
    set "WIX_BIN_DIR=!WIX_HIGHEST_PATH!\bin"
) else (
    echo Error: No WiX Toolset installation found under Program Files.
    exit /b 1
)

rem Ensure that the directories and executables exist
if not exist "!CMAKE_BIN_DIR!\cmake.exe" (
    echo Error: cmake.exe not found in "!CMAKE_BIN_DIR!"
    exit /b 1
)
if not exist "!MSBUILD_BIN_DIR!\msbuild.exe" (
    echo Error: msbuild.exe not found in "!MSBUILD_BIN_DIR!"
    exit /b 1
)
if not exist "!WIX_BIN_DIR!\candle.exe" (
    echo Error: WiX tools not found in "!WIX_BIN_DIR!"
    exit /b 1
)

rem Add build tool directories to the PATH for this script
set "PATH=!CMAKE_BIN_DIR!;!MSBUILD_BIN_DIR!;!WIX_BIN_DIR!;!VS170COMNTOOLS!;!PATH!"

rem Setting few environment variables required for build
SET "SystemDrive=C:"
SET "TMP=C:\Windows\TEMP"
SET "windir=C:\Windows"

rem Process command-line arguments
set argC=0
for %%x in (%*) do Set /A argC+=1
echo "Number of args is %argC%"

echo Building 64 bit Windows Redshift ODBC Driver
rem Process package version

if not "!RS_VERSION!"=="" (
    set "CMAKE_ARGS_ODBC_VERSION=-DODBC_VERSION=!RS_VERSION!"
)

rem Load environment variables from exports files if they exist
if exist ".\exports_basic.bat" (
    call .\exports_basic.bat
    echo Loaded exports_basic.bat
)
if exist ".\exports.bat" (
    call .\exports.bat
    echo Loaded exports.bat
)
echo "batch ENABLE_TESTING=%ENABLE_TESTING%"
rem cmake options
if "%ENABLE_TESTING%" == "" set ENABLE_TESTING=0
rem build and install options
if "%RS_BUILD_DIR%" == "" set "RS_BUILD_DIR=%LINK_PKG_PATH%\cmake-build\%RS_BUILD_TYPE%"
if "%INSTALL_DIR%" == "" set "INSTALL_DIR=%LINK_PKG_PATH%\cmake-build\install"

if not exist "%RS_BUILD_DIR%" mkdir "%RS_BUILD_DIR%" || exit /b %ERRORLEVEL%

@REM Start creating cmake command with various arguments
set "cmake_command=cmake -G "%cmake_generator%" -B "%RS_BUILD_DIR%" -S "%RS_ROOT_DIR%" -DRS_BUILD_TYPE=%RS_BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% %CMAKE_ARGS_ODBC_VERSION%"

if "%RS_BUILD_TYPE%"=="Debug" (
    set "cmake_command=!cmake_command! -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug"
) else (
    set "cmake_command=!cmake_command! -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
)

if defined RS_OPENSSL_DIR (
    set "cmake_command=!cmake_command! -DRS_OPENSSL_DIR=\"!RS_OPENSSL_DIR!\""
) else (
    echo "RS_OPENSSL_DIR not set"
)

set DEPS_DIRS=-DRS_DEPS_DIRS="%RS_ROOT_DIR%\src\pgclient\kfw-3-2-2-final"
if defined RS_DEPS_DIRS (
    set RS_DEPS_DIRS=!DEPS_DIRS!;%RS_DEPS_DIRS%
) else (
    set RS_DEPS_DIRS=%DEPS_DIRS%
)
set "cmake_command=!cmake_command! %RS_DEPS_DIRS%"

if defined RS_MULTI_DEPS_DIRS (
    set "cmake_command=!cmake_command! -DRS_MULTI_DEPS_DIRS=\"!RS_MULTI_DEPS_DIRS!\""
    set "cmake_command=!cmake_command! -DCMAKE_PREFIX_PATH=\"!RS_MULTI_DEPS_DIRS!\""
)

if defined RS_ODBC_DIR (
    set "cmake_command=!cmake_command! -DRS_ODBC_DIR=%RS_ODBC_DIR%"
)

if defined ENABLE_TESTING (
    set "cmake_command=!cmake_command! -DENABLE_TESTING=%ENABLE_TESTING%"
)

echo "RSODBC CMAKE COMMAND: %cmake_command%"
call %cmake_command%
if %ERRORLEVEL% neq 0 (
    echo Error occurred during CMake! Error code: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo CMake completed successfully.
)

@REM "%CMAKE%" --build  %RS_BUILD_DIR% --config %RS_BUILD_TYPE%    
msbuild %RS_BUILD_DIR%\ALL_BUILD.vcxproj /p:Configuration=%RS_BUILD_TYPE% /t:build /m:5
if %ERRORLEVEL% neq 0 (
    echo Error occurred during Build! Error code: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo Build completed successfully.
)
@REM "%CMAKE%" --install  %RS_BUILD_DIR% --config %RS_BUILD_TYPE%
msbuild %RS_BUILD_DIR%\INSTALL.vcxproj /p:Configuration=%RS_BUILD_TYPE% /m  /verbosity:normal
if %ERRORLEVEL% neq 0 (
    echo Error occurred during Install! Error code: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo Install completed successfully.
)

:GOT_INSTALLER
call package64.bat

echo "RS_ARTIFACTS_DIR is !RS_ARTIFACTS_DIR!"
echo "RS_BUILD_DIR is !RS_BUILD_DIR!"
REM Copy artifacts
copy version.txt !RS_ARTIFACTS_DIR!
echo "========== running: copy !WIN_ODBC_BUILD_MSI! !RS_ARTIFACTS_DIR!"
copy !WIN_ODBC_BUILD_MSI! !RS_ARTIFACTS_DIR!
REM Compress and Copy the test artifacts to a public folder
echo "========== running dir !RS_BUILD_DIR!(RS_BUILD_DIR)"
if exist "!RS_BUILD_DIR!" (
    echo "==========!RS_BUILD_DIR! exists"
    dir !RS_BUILD_DIR!
)
set "TEST_ARCHIVER_SCRIPT=!LINK_PKG_PATH!\scripts\tests_archiver.ps1"
echo "========== running set TEST_ARCHIVER_SCRIPT=!LINK_PKG_PATH!\scripts\tests_archiver.ps1"
if exist "%TEST_ARCHIVER_SCRIPT%" (
    echo "========== running powershell !TEST_ARCHIVER_SCRIPT!  -testFolder !RS_BUILD_DIR! -zipFileName tests.zip (RS_ARTIFACTS_DIR)"
    powershell !TEST_ARCHIVER_SCRIPT! -testFolder !RS_BUILD_DIR! -zipFileName tests.zip
    copy tests.zip !RS_ARTIFACTS_DIR!
) else (
    echo File "%TEST_ARCHIVER_SCRIPT%" not found. Skipping execution safely.
)

echo "========== running dir !RS_ARTIFACTS_DIR!(RS_ARTIFACTS_DIR)"
dir !RS_ARTIFACTS_DIR!

echo "========== running where /r !RS_BUILD_DIR! *.exe (RS_BUILD_DIR)"
cmd /C "where /r !RS_BUILD_DIR! *.exe"

tree /F !RS_ARTIFACTS_DIR!


rem Calculate elapsed time
set endTime=%time%
for /F "tokens=1-4 delims=:.," %%a in ("%startTime%") do (
   set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
for /F "tokens=1-4 delims=:.," %%a in ("%endTime%") do (
   set /A "end=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)
set /A elapsed=end-start

rem Calculate hours, minutes, seconds
set /A hh=elapsed/(60*60*100), rest=elapsed%%(60*60*100), mm=rest/(60*100), ss=rest%%(60*100), cc=rest%%100

if %hh% lss 10 set hh=0%hh%
if %mm% lss 10 set mm=0%mm%
if %ss% lss 10 set ss=0%ss%
if %cc% lss 10 set cc=0%cc%

echo Start Time: %startTime%
echo Finish Time: %endTime%
echo Elapsed Time: %hh%:%mm%:%ss%.%cc%

@REM The script end here
exit /b %ERRORLEVEL%

:show_help
echo Usage: %~nx0 [OPTIONS]
echo Build the 64-bit Windows Redshift ODBC Driver
echo.
echo Options:
echo   --version=X.X.X.X               Set the version number (optional)
echo   --build-type=TYPE               Set the build type (default: Release)
echo   --dependencies-install-dir=PATH Path to store and/or use dependency libraries 
echo.
echo   /?, -h, --help            Show this help message
echo.
echo Examples:
echo   %~nx0 --version=1.1.1.1 --build-type=Debug
echo   %~nx0 --dependencies-install-dir=%%CD%%\tp\install
exit /b 0