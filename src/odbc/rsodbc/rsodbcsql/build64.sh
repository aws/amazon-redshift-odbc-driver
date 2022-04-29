#!/bin/sh
#./build64.sh

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building 64 bit Linux rsodbcsql

cd Release
make  clean
checkExitCode $?
make
checkExitCode $?
cd ..

echo Done building 64 bit Linux rsodbcsql

exit $?
