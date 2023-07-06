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

source ./exports_basic.sh
if test -f "./exports.sh"; then
    source ./exports.sh
fi

echo "OPENSSL_INC_DIR=${OPENSSL_INC_DIR}"
echo "OPENSSL_LIB_DIR=${OPENSSL_LIB_DIR}"
echo "AWS_SDK_LIB_DIR=${AWS_SDK_LIB_DIR}"
echo "CURL_LIB_DIR=${CURL_LIB_DIR}"
echo "ENBLE_CNAME=${ENBLE_CNAME}"

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
