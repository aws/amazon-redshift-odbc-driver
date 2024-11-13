@echo off
setlocal enabledelayedexpansion
rem ./build64.bat [n.n.n n] [Debug/Release]
rem CMAKE=^"path-to\Cmake\bin\cmake^" ENABLE_TESTING=1 ./build64.bat [n.n.n n]

set argC=0
for %%x in (%*) do Set /A argC+=1
echo "Number of args is %argC%"

set startTime=%time%

set WIN_ODBC_BUILD_MSI=""
set "CMAKE_ARGS_ODBC_VERSION="
set RS_BUILD_DIR=
set INSTALL_DIR=
set RS_ROOT_DIR=
set ENABLE_TESTING=
set CMAKE=
set RS_OPENSSL_DIR=
set RS_MULTI_DEPS_DIRS=
set RS_DEPS_DIRS=
set RS_ODBC_DIR=

echo Building 64 bit Windows Redshift ODBC Driver
rem Process package version

if "%RS_ROOT_DIR%" == "" set "RS_ROOT_DIR=%CD%"
set BUILD_TYPE=Release
if "%1" == "" if "%2" == "" goto :noargs
set "odbc_version=%1"
set "svn_rev=%2"
if not "%2" == "" set "CMAKE_ARGS_ODBC_VERSION=-DODBC_VERSION=%odbc_version%.%svn_rev%"
if not "%3" == "" set "BUILD_TYPE=%3"

goto :noargs
echo Error: Either don't supply any version argument, or supply like the usage: ./build64.bat [n.n.n n]
exit /b 1

:noargs
echo "-BUILD_TYPE=%BUILD_TYPE%"
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
if "%RS_BUILD_DIR%" == "" set "RS_BUILD_DIR=%CD%\cmake-build\%BUILD_TYPE%"
if "%INSTALL_DIR%" == "" set "INSTALL_DIR=%CD%\cmake-build\install"
if "%CMAKE%" == "" set "CMAKE=cmake"

if not exist "%RS_BUILD_DIR%" mkdir "%RS_BUILD_DIR%" || exit /b %ERRORLEVEL%

set "cmake_command=%CMAKE% -B %RS_BUILD_DIR% -S %RS_ROOT_DIR% -DRS_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% %CMAKE_ARGS_ODBC_VERSION%"

if "%BUILD_TYPE%"=="Debug" (
    set "cmake_command=!cmake_command! -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug "
) else (
    set "cmake_command=!cmake_command! -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded "
)

if defined RS_OPENSSL_DIR (
    set "cmake_command=!cmake_command! -DRS_OPENSSL_DIR=%RS_OPENSSL_DIR%"
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
    set "cmake_command=!cmake_command! -DRS_MULTI_DEPS_DIRS=%RS_MULTI_DEPS_DIRS%"
)

if defined RS_ODBC_DIR (
    set "cmake_command=!cmake_command! -DRS_ODBC_DIR=%RS_ODBC_DIR%"
)

if defined ENABLE_TESTING (
    set "cmake_command=!cmake_command! -DENABLE_TESTING=%ENABLE_TESTING%"
)

echo %cmake_command%
call %cmake_command%
if %ERRORLEVEL% neq 0 (
    echo Error occurred during CMake! Error code: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo CMake completed successfully.
)

@REM "%CMAKE%" --build  %RS_BUILD_DIR% --config %BUILD_TYPE%    
msbuild %RS_BUILD_DIR%\ALL_BUILD.vcxproj /p:Configuration=%BUILD_TYPE% /t:build /m:5
if %ERRORLEVEL% neq 0 (
    echo Error occurred during Build! Error code: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo Build completed successfully.
)
@REM "%CMAKE%" --install  %RS_BUILD_DIR% --config %BUILD_TYPE%
msbuild %RS_BUILD_DIR%\INSTALL.vcxproj /p:Configuration=%BUILD_TYPE% /m  /verbosity:normal
if %ERRORLEVEL% neq 0 (
    echo Error occurred during Install! Error code: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
) else (
    echo Install completed successfully.
)

:GOT_INSTALLER
call package64.bat

echo Start Time: %startTime%
echo Finish Time: %time%
exit /b %ERRORLEVEL%