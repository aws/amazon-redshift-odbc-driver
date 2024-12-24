#!/bin/sh
#./build64.sh [n.n.n n]
#CMAKE=/path-to/Cmake/bin/cmake ENABLE_TESTING=1 ./build64.sh [n.n.n n]

echo Building 64 bit Linux Redshift ODBC Driver

#Process package version
CMAKE_ARGS_ODBC_VERSION=
if [[ $# -eq 0 ]] || [[ $# -eq 2 ]]; then
    odbc_version=$1
    svn_rev=$2
    if [[ $# -eq 2 ]]; then
      CMAKE_ARGS_ODBC_VERSION="-DODBC_VERSION=$odbc_version.$svn_rev"
    fi
else
  echo "Error: Either don't supply any version argument, or supply like the usage: ./build64.sh [n.n.n n]"
  exit 1
fi

RS_ROOT_DIR=${RS_ROOT_DIR:=$PWD}

source ./exports_basic.sh
#In case there is an additional setting script
if test -f "./exports.sh"; then
    source ./exports.sh
fi

#cmake options
ENABLE_TESTING=${ENABLE_TESTING:=0}
#build and install options
RS_BUILD_DIR=${RS_BUILD_DIR:="$PWD/cmake-build"}
INSTALL_DIR=${INSTALL_DIR:=${RS_BUILD_DIR}/install}
#Incase using a custom cmake installation
CMAKE=${CMAKE:='cmake'}

mkdir -p ${RS_BUILD_DIR}
cmake_command="${CMAKE} -S ${RS_ROOT_DIR} -B ${RS_BUILD_DIR} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${CMAKE_ARGS_ODBC_VERSION}"
if [ -n "${RS_DEPS_DIRS}" ]
then
    cmake_command="${cmake_command} -DRS_DEPS_DIRS=${RS_DEPS_DIRS}"
fi
if [ -n "${RS_MULTI_DEPS_DIRS}" ]
then
    cmake_command="${cmake_command} -DRS_MULTI_DEPS_DIRS=${RS_MULTI_DEPS_DIRS}"
fi
if [ -n "${RS_OPENSSL_DIR}" ]
then
    cmake_command="${cmake_command} -DRS_OPENSSL_DIR=${RS_OPENSSL_DIR}"
fi
if [ -n "${RS_ODBC_DIR}" ]
then
    cmake_command="${cmake_command} -DRS_ODBC_DIR=${RS_ODBC_DIR}"
fi
if [ -n "${ENABLE_TESTING}" ]
then
    cmake_command="${cmake_command} -DENABLE_TESTING=${ENABLE_TESTING}"
fi
echo cmake command is ${cmake_command}
# exit 0
eval ${cmake_command}
make -C ${RS_BUILD_DIR} -j 5
make -C ${RS_BUILD_DIR} install

exit $?
