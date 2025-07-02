## Redshift ODBC Driver

The Amazon ODBC Driver for Redshift database connectivity through the standard ODBC application program interfaces (APIs). The Driver provides access to Redshift from any C/C++ application.

The driver has many Redshift specific features such as,

* IAM authentication
* IDP authentication
* Redshift specific datatypes support
* External schema support as part of SQLTables() and SQLColumns() ODBC API

Amazon Redshift provides 64-bit ODBC drivers for Linux, and Windows operating systems. 


## Download Driver
You can download the latest release of Redshift ODBC drivers from AWS Redshift documentation links below:

Windows: https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-win.html

Linux: https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-linux.html

## Build Driver
Amazon Redshift recommends downloading and using the prebuilt driver installer from [AWS Redshift documentation](https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-win.html) for the best experience.

If you choose to build from source, please note that the Redshift ODBC driver does not include pre-built binaries or complementary build script for building driver dependencies.
Building Redshift ODBC driver from source requires pre-built binaries for following dependencies:
* OpenSSL 1.1.1x latest version (requires OpenSSL Premium support)
* AWS SDK for CPP
* C-ares
* GoogleTest
* Curl (Linux only)

### Prerequisites
#### Common:
* https://git-lfs.com/ (for correctly cloning this repository)
* CMake >= 3.20

#### Linux:
* gcc

#### Windows:
* Wix 3.14
* Build Tools for Visual Studio

### Build Steps
For Building on Windows:
1. Ensure Cmake (3.20+), Wix 3.14 and Build Tools for Visual Studio 2022 are installed on your Machine and added to system PATH.
2. Build above mentioned platform specific dependencies and keep their pre-built binaries in a specific directory. This directory path is later required for `dependencies-install-dir` option while building the driver.
3. Cd to cloned `amazon-redshift-odbc-driver` package home and build the driver using following command:
> .\build64.bat --dependencies-install-dir=absolute-path-to-dependencies-installation-directory

Optionally you can also provide the desired driver version number in the build command. It outputs the installer MSI under `amazon-redshift-odbc-driver\src\odbc\rsodbc\install\` directory. You can find the built `rsodbc64.dll` in `cmake-build/install/lib/` directory.

For building on Linux:
1. Build above mentioned platform specific dependencies and keep their pre-built binaries in a specific directory.
2. Export `DEPENDENCY_DIR=absolute-path-to-dependencies-installation-directory` or set the `DEPENDENCY_DIR` variable in the `exports_basic.sh` file. For more details, refer `BUILD.CMAKE.md`. 
3. Cd to cloned `amazon-redshift-odbc-driver` package home and build the driver using following command:
> build64.sh n.n.n n

e.g. build64.sh 2.1.8 0

It builds `librsodbc64.so` file under `src/odbc/rsodbc/Release` directory.

### Installation and Configuration of Driver

Driver Name: Amazon Redshift ODBC Driver (x64)

Default Installation Directory:
* Windows: `C:\Program Files\Amazon Redshift ODBC Driver x64\`
* Linux: `/opt/amazon/redshiftodbcx64/`

See [Amazon Redshift ODBC Driver Installation and Configuration Guide](https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install.html) for more information.

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

