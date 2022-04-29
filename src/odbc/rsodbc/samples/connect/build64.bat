@echo off
echo Building 64 bit Windows Amazon Redshift ODBC Driver Connect Sample


echo Setting environment variables
call vcvars64

devenv /Rebuild "Release|x64" connect.sln

echo Done building 64 bit Windows Amazon Redshift ODBC Driver Connect Sample


