#!/bin/sh
# This is custom script used to make custom settings to the ODBC-2.x build environment.
# Detect the operating system
OS=$(uname -s)

# Set common variables
export RS_ROOT_DIR=$(pwd)
export ENABLE_TESTING=1

# OS-specific settings
if [ "$OS" = "Linux" ]; then
    # Linux-specific settings
    : # no-op
    # please make sure you are setting RS_MULTI_DEPS_DIRS or RS_DEPS_DIRS
    # please make sure you are setting RS_OPENSSL_DIR 
    # please make sure you are setting RS_ODBC_DIR if required
    # For more information read BUILD.CMAKE.md
    # Examples:
    # export RS_MULTI_DEPS_DIRS=/path/to/odbc_deps_folder/install
    # export RS_OPENSSL_DIR=/path/to/odbc_deps_folder/odbc_deps_folder/install/openssl
elif [ "$OS" = "Darwin" ]; then
    : # no-op
    # macOS-specific settings
    # please make sure you are setting RS_MULTI_DEPS_DIRS or RS_DEPS_DIRS
    # please make sure you are setting RS_OPENSSL_DIR 
    # please make sure you are setting RS_ODBC_DIR if required
    # For more information read BUILD.CMAKE.md
    # Examples:
    # export RS_MULTI_DEPS_DIRS=/path/to/odbc_deps_folder/install
    # export RS_OPENSSL_DIR=/path/to/odbc_deps_folder/odbc_deps_folder/install/openssl
    # export RS_ODBC_DIR="'$(brew --prefix unixodbc);$(brew --prefix libtool)'"
else
    echo "Unsupported operating system: $OS"
    exit 1
fi