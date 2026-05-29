#!/bin/sh
#sudo ./create64bit-rpm.sh odbc_version svn_rev arch_name

# This routine packages the 64 bit rpm
odbc_version=$1
svn_rev=$2
arch_name=$3
echo "create64bit-rpm.sh: odbc_version=$odbc_version, svn_rev=$svn_rev, arch_name=$arch_name"

echo "RS_ROOT_DIR=${RS_ROOT_DIR}"
INSTALL_DIR=${INSTALL_DIR:="${RS_ROOT_DIR}/cmake-build/install/"}

# Resolve rpmbuild from Brazil's Rpm-4.x package (rpm 4.18.2).
# rpm >= 4.14 is required to produce the PAYLOADDIGEST header tag. Without it, RPM
# verification fails on FIPS-enabled systems (RHEL 8/9, AL2023) where MD5 is disabled
# and PAYLOADDIGEST is the only mechanism to verify payload integrity before extraction.
# No fallback to system rpmbuild (4.11.3) — a missing PAYLOADDIGEST is a silent customer-facing failure.
_BRAZIL_BOOTSTRAP="${BRAZIL_BOOTSTRAP:-brazil-bootstrap}"
if ! command -v "$_BRAZIL_BOOTSTRAP" >/dev/null 2>&1; then
  echo "ERROR: Cannot locate brazil-bootstrap (tried: '$_BRAZIL_BOOTSTRAP')."
  echo "       Ensure BRAZIL_BOOTSTRAP is exported by the calling script (custom-build)"
  echo "       or that brazil-bootstrap is available on PATH."
  exit 1
fi
RPM_TOOL=$("$_BRAZIL_BOOTSTRAP" --package Rpm-4.x)
if [ ! -x "${RPM_TOOL}/bin/rpmbuild" ]; then
  echo "ERROR: Rpm-4.x package resolved to '${RPM_TOOL}' but rpmbuild binary not found."
  echo "       Verify that Rpm-4.x is present in the version set and built for this platform."
  exit 1
fi
RPMBUILD="${RPM_TOOL}/bin/rpmbuild"
export LD_LIBRARY_PATH="${RPM_TOOL}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
echo "Using Brazil rpmbuild: $RPMBUILD"
"$RPMBUILD" --version

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

cp ${INSTALL_DIR}/librsodbc64.so /tmp/redshiftodbcx64/
cp ${INSTALL_DIR}/amazon.redshiftodbc.ini  /tmp/redshiftodbcx64/
cp ${INSTALL_DIR}/root.crt /tmp/redshiftodbcx64/
cp ${INSTALL_DIR}/odbc.ini  /tmp/redshiftodbcx64/odbc.ini
cp ${INSTALL_DIR}/odbcinst.ini /tmp/redshiftodbcx64/odbcinst.ini
cp ${INSTALL_DIR}/odbc.csh /tmp/redshiftodbcx64/odbc.csh
cp ${INSTALL_DIR}/odbc.sh /tmp/redshiftodbcx64/odbc.sh

cp ./samples/connect/connect.c /tmp/redshiftodbcx64/samples/connect/
cp ./samples/connect/connect.mak /tmp/redshiftodbcx64/samples/connect/connect.mak
cp ${INSTALL_DIR}/connect /tmp/redshiftodbcx64/samples/connect/connect

cp ${INSTALL_DIR}/rsodbcsql /tmp/redshiftodbcx64/

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
$RPMBUILD -v --target ${arch_name} -bb $spec_file
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
echo "sha256sum=$(sha256sum "$(pwd)/rpm/$rpm_new_name" | awk '{print $1}')"
echo "sha512sum=$(sha512sum "$(pwd)/rpm/$rpm_new_name" | awk '{print $1}')"
rc_=$?
if [ $rc_ -ne 0 ]
then
    exit $rc_
fi
echo "Find the package in $(pwd)/rpm/$rpm_new_name"
exit $rc_
