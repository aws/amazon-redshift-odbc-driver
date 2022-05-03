#!/bin/sh
#./clean64.sh 

echo Cleaning 64 bit Linux 9.1.2 libpq build

cd src/interfaces/libpq
make clean
cd ../../port
make clean
cd ../..

echo Done cleaning 64 bit Linux 9.1.2 libpgport build

