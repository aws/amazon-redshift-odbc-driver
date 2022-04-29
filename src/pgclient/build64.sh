#!/bin/sh
#./build64.sh 

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

# Command-line switch to prevent removing of header files
keepFlag=on
while [ $# -gt 0 ]
do
  case "$1" in
    -k)  keepFlag=on;;
  esac
  shift
done

echo Building 64 bit Linux 9.1.2 libpq

cd src/interfaces/libpq
make clean
checkExitCode $?
cd ../../port
make clean
checkExitCode $?
cd ../..

cp src/include/pg_config.h.linux64 src/include/pg_config.h
checkExitCode $?
cp src/include/pg_config_os.h.linux64 src/include/pg_config_os.h
checkExitCode $?

cd src/interfaces/libpq
make
checkExitCode $?

if [ ! -d ./Release ]
then
	mkdir -p ./Release
fi

mv libpq.a ./Release/
checkExitCode $?

echo Done building 64 bit Linux 9.1.2 libpq

echo Building 64 bit Linux 9.1.2 libpgport

cd ../../port
make
checkExitCode $?

if [ ! -d ./Release ]
then
	mkdir -p ./Release
fi

mv libpgport.a ./Release/
checkExitCode $?

echo Done building 64 bit Linux 9.1.2 libpgport

cd ../..

if [ "$keepFlag" != "on" ];
then
  rm  src/include/pg_config.h
  rm  src/include/pg_config_os.h
  rm  src/port/pg_config_paths.h
fi

exit $?
