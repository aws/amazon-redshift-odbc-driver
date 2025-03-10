## Redshift ODBC Driver

The Amazon ODBC Driver for Redshift database connectivity through the standard ODBC application program interfaces (APIs). The Driver provides access to Redshift from any C/C++ application.

The driver has many Redshift specific features such as,

* IAM authentication
* IDP authentication
* Redshift specific datatypes support
* External schema support as part of SQLTables() and SQLColumns() ODBC API

Amazon Redshift provides 64-bit ODBC drivers for Linux, and Windows operating systems. 

## Build Driver
### Prerequisites
* https://git-lfs.com/ (for correctly cloning this repository)
* Visual Stuido 2022 Community Edition (For Windows)
* gcc (For Linux)
* cmake >= 3.20 (For Linux)
* Dependencies: To see a list of Linux dependencies, please look into src/odbc/rsodbc/CMakeLists.txt (windows: Dependencies are already included).

### Build Artifacts
On Windows system run:
```
build64.bat n.n.n n 
e.g. build64.bat 2.0.1 0

```
It builds **rsodbc.dll** file under **src\odbc\rsodbc\x64\Release** directory. 


export DEPENDENCY_DIR=
Ensure proper dependencies are provided on Unix systems by configuring the dependency variable: Set the `DEPENDENCY_DIR` variable in the `exports_basic.sh`` file. For further details, consult the `BUILD.CMAKE.md` file.
Then run:
```
build64.sh n.n.n n
e.g. build64.sh 2.0.1 0
```

It builds **librsodbc64.so** file under **src/odbc/rsodbc/Release** directory. 

### Installation and Configuration of Driver

Driver Name: Amazon Redshift ODBC Driver (x64)

Default Installation Directory:
* C:\Program Files\Amazon Redshift ODBC Driver x64\ (For Windows)
* /opt/amazon/redshiftodbcx64/ (For Linux)

See [Amazon Redshift ODBC Driver Installation and Configuration Guide](https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install.html) for more information.

Here are download links for the latest release:
* https://s3.amazonaws.com/redshift-downloads/drivers/odbc/2.1.7.0/AmazonRedshiftODBC64-2.1.7.0.msi (For Windows)
* https://s3.amazonaws.com/redshift-downloads/drivers/odbc/2.1.7.0/AmazonRedshiftODBC-64-bit-2.1.7.0.x86_64.rpm (For Linux)

⚠️ ODBC Driver version 2.1.5 has been recalled. ODBC Driver version 2.1.7 is recommended for use instead.

## Report Bugs

See [CONTRIBUTING](CONTRIBUTING.md#Reporting-Bugs/Feature-Requests) for more information.

## Contributing Code Development

See [CONTRIBUTING](CONTRIBUTING.md#Contributing-via-Pull-Requests) for more information.

## Changelog Generation
An entry in the changelog is generated upon release using `gitchangelog <https://github.com/vaab/gitchangelog>`.
Please use the configuration file, ``.gitchangelog.rc`` when generating the changelog.
	 
## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This project is licensed under the Apache-2.0 License.

