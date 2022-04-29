#!/bin/sh
#./build64.sh n.n.n n

function checkExitCode {
  retVal=$1;

  if [ "$retVal" != 0 ]; then
    exit $retVal;
  fi
}

odbc_version=$1
svn_rev=$2
odbc_driver_full_version="${odbc_version}.${svn_rev}"

echo $odbc_version
echo $svn_rev
echo $odbc_driver_full_version

echo Setting REDSHIFT_DRIVER_VERSION to $odbc_driver_full_version in rsodbc_setup/version.h
sed -i "s/#define REDSHIFT_DRIVER_VERSION.*/#define REDSHIFT_DRIVER_VERSION         \"${odbc_driver_full_version}\"/g" rsodbc_setup/version.h

echo Setting Driver version in rsversion.h
major=$(echo $odbc_version | cut -d '.' -f1)
minor=$(echo $odbc_version | cut -d '.' -f2)
patch=$(echo $odbc_version | cut -d '.' -f3)

padded_full_version=$(printf "%02d.%02d.%04d\n" "$major" "$minor" "$patch")
sed -i "s/#define ODBC_DRIVER_VERSION.*/#define ODBC_DRIVER_VERSION \"$padded_full_version\"/g" rsversion.h

sed -i "s/#define FILEVER.*/#define FILEVER        $major,$minor,$patch,$svn_rev/g" rsversion.h
sed -i "s/#define PRODUCTVER.*/#define PRODUCTVER     $major,$minor,$patch,$svn_rev/g" rsversion.h
sed -i "s/#define STRFILEVER.*/#define STRFILEVER     \"$major, $minor, $patch, $svn_rev\\\0\"/g" rsversion.h 
sed -i "s/#define STRPRODUCTVER.*/#define STRPRODUCTVER  \"$major, $minor, $patch, $svn_rev\"/g" rsversion.h

echo Building 64 bit Linux Amazon Redshift ODBC Driver

cd Release
make clean
checkExitCode $?
make
checkExitCode $?

echo Done building 64 bit Linux Amazon Redshift ODBC Driver

cd ..

exit $?
