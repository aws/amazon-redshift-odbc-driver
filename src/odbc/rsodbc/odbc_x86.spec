Summary: Redshift Analytic Platform ODBC Driver for x86 Linux
Name: %{rpmname}
Version: %{version}
Release: %{release}
License: Redshift EULA
Group: Redshift-ODBC-Driver
Vendor: Amazon.com Inc
AutoReqProv: no

%description
Redshift Analytic Platform ODBC Driver for x86 Linux platforms.

%setup 

%prep

%build

%files
%defattr(-,root,root)
/opt/rsodbc32/*

%clean
sudo rm -Rf /opt/rsodbc32

%postun

%changelog
* Wed Apr 5 2012 I Garish <ilesh.garish@uSeR.com>
- Created
