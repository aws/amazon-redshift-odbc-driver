#!/bin/sh
#sudo ./create64bit-rpm.sh odbc_version svn_rev arch_name

# This routine packages the 64 bit rpm
odbc_version=$1
svn_rev=$2
arch_name=$3

echo "RS_ROOT_DIR=${RS_ROOT_DIR}"
INSTALL_DIR=${INSTALL_DIR:="${RS_ROOT_DIR}/cmake-build/install/"}

if [ ! -d ./rpm ]
then
	mkdir -p ./rpm
fi


spec_file=rs-${odbc_version}-${svn_rev}-${arch_name}.spec

# Create the stamped 64 bit spec file
echo Creating the 64 bit stamped spec file..
sed "s|Version:.*$|Version: ${odbc_version} |" < odbc_x64.spec > tt
sed "s|Name:.*$|Name: AmazonRedshiftODBC-64-bit |" < tt > tt2
sed "s|Release:.*$|Release: ${svn_rev} |" < tt2 > temp64.spec
cat temp64.spec | tr -d "\r" > $spec_file

if [ $? -ne 0 ]
then
    exit $?
fi
rm temp64.spec
rm tt
rm tt2


# Copy the files to stage for RPM creation
if [ -d "/tmp/redshiftodbcx64" ]
then
   rm -Rf /tmp/redshiftodbcx64
fi

mkdir -p /tmp/redshiftodbcx64
mkdir -p /tmp/redshiftodbcx64/samples/connect

cp ${INSTALL_DIR}/lib/librsodbc64.so /tmp/redshiftodbcx64/
cp ./amazon.redshiftodbc.ini  /tmp/redshiftodbcx64/
cp ./root.crt /tmp/redshiftodbcx64/
cp ./odbc.ini.x64  /tmp/redshiftodbcx64/odbc.ini
cp ./odbcinst.ini.x64 /tmp/redshiftodbcx64/odbcinst.ini
cp ./odbc.csh.x64 /tmp/redshiftodbcx64/odbc.csh
cp ./odbc.sh.x64 /tmp/redshiftodbcx64/odbc.sh

cp ./samples/connect/connect.c /tmp/redshiftodbcx64/samples/connect/
cp ./samples/connect/connect.mak /tmp/redshiftodbcx64/samples/connect/connect.mak
cp ${INSTALL_DIR}/bin/connect64 /tmp/redshiftodbcx64/samples/connect/connect

cp ${INSTALL_DIR}/bin/rsodbcsql /tmp/redshiftodbcx64/

# This is the directory used by RPMBUILD
rm -rf /var/tmp/redshiftodbcx64/
cp -avrf /tmp/redshiftodbcx64/ /var/tmp/redshiftodbcx64/ 

# Set rpm_src for EL5
#rpm_src=/usr/src/rpm/RPMS/x86_64/AmazonRedshiftODBC-64-bit-${odbc_version}-${svn_rev}.x86_64.rpm

# Set rpm_src for EL7
rpm_src=$HOME/rpmbuild/RPMS/${arch_name}/AmazonRedshiftODBC-64-bit-${odbc_version}-${svn_rev}.${arch_name}.rpm
rpm_new_name=AmazonRedshiftODBC-64-bit-${odbc_version}.${svn_rev}.${arch_name}.rpm

# Build the 64 bit rpm 
echo Running the 64 bit rpm build using this spec file: $spec_file
rpmbuild -v --target ${arch_name} -bb $spec_file
if [ $? -ne 0 ]
then
    exit $?
fi

rm -rf /var/tmp/redshiftodbcx64
rm -rf /tmp/redshiftodbcx64
rm ${spec_file}

# Move the 64 bit file to the current working directory
echo Moving the 64 bit rpm to the rpm folder..
rm ./rpm/$rpm_new_name
mv $rpm_src ./rpm/$rpm_new_name
if [ $? -ne 0 ]
then
    exit $?
fi

exit $?
