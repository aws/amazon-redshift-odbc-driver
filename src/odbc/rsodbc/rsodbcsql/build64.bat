@echo off
echo Building 64 bit Windows rsodbcsql

echo Setting environment variables
rem call vcvars64

devenv /Rebuild "Release|x64" rsodbcsql.sln

echo Done building 64 bit Windows rsodbcsql


