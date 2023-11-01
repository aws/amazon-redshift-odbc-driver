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

# The following routine set environment variable for compilation
# Those variables in exports.sh link include paths to dependnecies
# build64.sh has the same routines
RS_ROOT_DIR="../../.." #Technically RS_ROOT_DIR is same as ROOT_DIR unless folders change
pushd ${RS_ROOT_DIR}
source ${ROOT_DIR}/exports_basic.sh
if test -f "${ROOT_DIR}/exports.sh"; then
    source ${ROOT_DIR}/exports.sh
fi
popd

# Build logger
pushd ${RS_ROOT_DIR}/src/logging
make clean
checkExitCode $?
make
checkExitCode $?
popd

# Build libpq & libpgport
pushd ${RS_ROOT_DIR}/src/pgclient
./build64.sh
checkExitCode $?
popd

# Build ODBC Driver Shared Object

pushd ${RS_ROOT_DIR}/src/odbc/rsodbc
./build64.sh $odbc_version $svn_rev
checkExitCode $?
popd

# Build ODBC Driver Samples
pushd ./samples
./build64.sh
checkExitCode $?
popd

# Build rsodbcsql
pushd ./rsodbcsql
./build64.sh
checkExitCode $?
popd


# Now make the installer using RPM label provided
./create64bit-rpm.sh ${odbc_version} ${svn_rev}
checkExitCode $?

echo Please get RPM package under "./rpm" directory.

exit $?
