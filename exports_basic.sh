#!/bin/sh

# Detect the operating system
OS=$(uname -s)

# Set common variables
export RS_ROOT_DIR=$(pwd)
export ENABLE_TESTING=1

# OS-specific settings
if [ "$OS" = "Linux" ]; then
    # Linux-specific settings
    : # no-op
elif [ "$OS" = "Darwin" ]; then
    # macOS-specific settings
    export RS_MULTI_DEPS_DIRS=/path/to/odbc_deps_folder/install
    export RS_OPENSSL_DIR=/path/to/odbc_deps_folder/odbc_deps_folder/install/openssl
    export RS_ODBC_DIR="'$(brew --prefix unixodbc);$(brew --prefix libtool)'"
else
    echo "Unsupported operating system: $OS"
    exit 1
fi