@echo off
echo Building 64 bit Windows Redshift ODBC Setup DLL


echo Setting environment variables
call vcvars64

devenv /Rebuild "Release|x64" rsodbcsetup.sln

echo Done building 64 bit Windows Redshift ODBC Setup DLL


