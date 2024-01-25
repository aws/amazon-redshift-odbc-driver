#!/bin/sh
#./build64.sh [n.n.n n]
#CMAKE=/path-to/Cmake/bin/cmake ENABLE_TESTING=1 ./build64.sh [n.n.n n]

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

echo Building 64 bit Linux Redshift ODBC Driver

#Process package version
cmake_arg_odbc_version=
if [[ $# -eq 0 ]] || [[ $# -eq 2 ]]; then
    odbc_version=$1
    svn_rev=$2
    if [[ $# -eq 2 ]]; then
      cmake_arg_odbc_version="-DODBC_VERSION=$odbc_version.$svn_rev"
    fi
else
  echo "Error: Either don't supply any version argument, or supply like the usage: ./build64.sh [n.n.n n]"
  exit 1
fi


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
RS_ROOT_DIR=${RS_ROOT_DIR:=$PWD}
#Incase using a custom cmake installation
CMAKE=${CMAKE:='cmake'}

mkdir -p ${RS_BUILD_DIR}
pushd ${RS_BUILD_DIR}
${CMAKE} ${RS_ROOT_DIR} -DDEPS_DIRS=${DEPENDENCY_DIR} -DOPENSSL_DIR=${DEPENDENCY_DIR} \
                        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${cmake_arg_odbc_version} \
                        -DENABLE_TESTING=${ENABLE_TESTING}
make -j 5
make install
popd

exit $?
