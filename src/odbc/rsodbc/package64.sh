#!/bin/sh
#./package64.sh [n.n.n n]

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building and Packaging 64 bit Linux Redshift ODBC Driver

# The following routine set environment variable for compilation
# Those variables in exports.sh link include paths to brazil dependnecies
# build64.sh has the same routines
RS_ROOT_DIR="../../.."
if [[ $# -eq 2 ]]; then
    odbc_version=$1
    svn_rev=$2
else
   VERSION=$(cat ${RS_ROOT_DIR}/version.txt)
   read -r odbc_version svn_rev <<< "${VERSION[0]}"
   echo "version from version.txt: ${odbc_version} ${svn_rev}"
fi

pushd ${RS_ROOT_DIR}
./build64.sh  ${odbc_version} ${svn_rev}
popd


# Now make the installer using RPM label provided
RS_ROOT_DIR=${RS_ROOT_DIR} ./create64bit-rpm.sh ${odbc_version} ${svn_rev} ${ARCH}
checkExitCode $?

echo Please get RPM package under "./rpm" directory.

exit $?
