Summary: Amazon Redshift ODBC Driver for x64 Linux
Name: %{rpmname}
Version: %{version}
Release: %{release}
License: Redshift EULA
Group: Redshift-ODBC-Driver
Vendor: Amazon Web Services, Inc.
AutoReqProv: no

%description
Amazon Redshift ODBC Driver for x64 Linux platforms.

%setup 

%prep

%build

%install
pwd
rm -rf "$RPM_BUILD_ROOT"
mkdir -p "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64"
mkdir "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/samples"
mkdir "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/samples/connect"
cp -f "%{_tmppath}/redshiftodbcx64/librsodbc64.so" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/amazon.redshiftodbc.ini" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/root.crt" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/odbc.csh" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/odbc.ini" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/odbcinst.ini" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/odbc.sh" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/rsodbcsql" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/"
cp -f "%{_tmppath}/redshiftodbcx64/samples/connect/connect" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/samples/connect/"
cp -f "%{_tmppath}/redshiftodbcx64/samples/connect/connect.c" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/samples/connect/"
cp -f "%{_tmppath}/redshiftodbcx64/samples/connect/connect.mak" "$RPM_BUILD_ROOT/opt/amazon/redshiftodbcx64/samples/connect/"


%files
/opt/amazon/redshiftodbcx64/librsodbc64.so
/opt/amazon/redshiftodbcx64/amazon.redshiftodbc.ini
/opt/amazon/redshiftodbcx64/root.crt
/opt/amazon/redshiftodbcx64/odbc.csh
/opt/amazon/redshiftodbcx64/odbc.ini
/opt/amazon/redshiftodbcx64/odbcinst.ini
/opt/amazon/redshiftodbcx64/odbc.sh
/opt/amazon/redshiftodbcx64/rsodbcsql
/opt/amazon/redshiftodbcx64/samples/connect/connect
/opt/amazon/redshiftodbcx64/samples/connect/connect.c
/opt/amazon/redshiftodbcx64/samples/connect/connect.mak

%clean
rm -rf "$RPM_BUILD_ROOT"

%postun

