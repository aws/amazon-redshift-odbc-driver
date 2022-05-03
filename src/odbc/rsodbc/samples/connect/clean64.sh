#!/bin/sh
#./clean64.sh 

echo Cleaning 64 bit Linux Amazon Redshift ODBC Driver connect Sample Build

rm ./connect64
make -f connect.mak clean

echo Done cleaning 64 bit Linux Amazon Redshift ODBC Driver connect Sample Build

exit $?
