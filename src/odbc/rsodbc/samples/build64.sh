#!/bin/sh
#./build64.sh 

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building 64 bit Linux Amazon Redshift Analytic Platform ODBC Driver Samples

cd connect
./build64.sh
checkExitCode $?
cd ..

echo Done building 64 bit Linux Amazon Redshift Analytic Platform ODBC Driver Samples

exit $?
