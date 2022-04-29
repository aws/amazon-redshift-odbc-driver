#!/bin/sh
#./package64.sh [n.n.n n]

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building and Packaging 64 bit Linux Redshift ODBC Driver

# This routine packages the 64 bit rpm
odbc_version=$1
svn_rev=$2

# Build libpq & libpgport
cd ../../pgclient
./build64.sh
checkExitCode $?

# Build ODBC Driver Shared Object
cd ../odbc/rsodbc
./build64.sh $odbc_version $svn_rev
checkExitCode $?

# Build ODBC Driver Samples
cd ./samples
./build64.sh
checkExitCode $?
cd ..

# Build rsodbcsql
cd ./rsodbcsql
./build64.sh
checkExitCode $?
cd ..


# Now make the installer using RPM label provided
./create64bit-rpm.sh ${odbc_version} ${svn_rev}
checkExitCode $?

echo Please get RPM package under "./rpm" directory.

exit $?
