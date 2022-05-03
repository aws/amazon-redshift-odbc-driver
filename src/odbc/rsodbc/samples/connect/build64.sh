#!/bin/sh
#./build64.sh 

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building 64 bit Linux Amazon Redshift ODBC Driver connect Sample

rm ./connect64
make -f connect.mak clean
make -f connect.mak
checkExitCode $?
mv ./connect ./connect64
checkExitCode $?

echo Done building 64 bit Linux Amazon Redshift ODBC Driver connect Sample

exit $?
