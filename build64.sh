#!/bin/sh
#./build64.sh [n.n.n n]

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building 64 bit Linux Redshift ODBC Driver

odbc_version=$1
svn_rev=$2

# Build libpq & libpgport
cd ./src/pgclient
./build64.sh
checkExitCode $?

# Build ODBC Driver Shared Object
cd ../odbc/rsodbc
./build64.sh $odbc_version $svn_rev
checkExitCode $?

cd ../../..

exit $?
