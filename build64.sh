#!/bin/sh

# ------------------------------------------------------------------------------
# Build script for 64-bit Linux/macOS Redshift ODBC Driver
# This script configures, builds, installs, and optionally packages the driver
# ------------------------------------------------------------------------------

set -e  # Exit on error

RS_ROOT_DIR=${RS_ROOT_DIR:=$PWD}        # Root directory of the project, default is current directory
OS=$(uname -s)                          # Detect operating system
ARCH=$(uname -m)                        # Detect machine architecture
echo "Building 64 bit $OS $ARCH Redshift ODBC Driver"

# Source basic environment configuration
source ./exports_basic.sh

# Source optional custom environment configuration if it exists
if test -f "./exports.sh"; then
    source ./exports.sh
fi

# Find CMake executable - some systems use cmake3 instead of cmake
CMAKE=$(command -v cmake3 || command -v cmake || { echo "cmake not found, yet!" >&2; })

# Verify CMAKE is available
if [[ -z "$CMAKE" ]]; then
    echo "CMAKE not found! Exiting ..."
    exit 1
else
    echo "cmake: $CMAKE"
    $CMAKE --version
fi

# CMake build options
ENABLE_TESTING=${ENABLE_TESTING:=0}     # Control whether tests are enabled, default is disabled
echo "ENABLE_TESTING: $ENABLE_TESTING" 

# Set build and install directories
RS_BUILD_DIR=${RS_BUILD_DIR:="$PWD/cmake-build"}   # Build directory, default is ./cmake-build
echo "RS_BUILD_DIR:$RS_BUILD_DIR"
INSTALL_DIR=${INSTALL_DIR:=${RS_BUILD_DIR}/install}  # Installation directory
echo "INSTALL_DIR $INSTALL_DIR"
PUBLIC_DIR=$PWD/public                  # Public artifacts directory
echo "PUBLIC_DIR: $PUBLIC_DIR"

# Initialize optional parameters
RS_VERSION=""                           # ODBC driver version
RS_BUILD_TYPE=""                        # Build type (Release, Debug, etc.)
BUILD_DEPENDENCIES="no"                 # Whether to build dependencies
DEPENDENCIES_SRC_DIR=""                 # Dependencies source directory
DEPENDENCIES_BUILD_DIR=""               # Dependencies build directory
DEPENDENCIES_INSTALL_DIR=""             # Dependencies installation directory
CREATE_RSODBC_PACKAGE="no"              # Whether to create package after build

# Parse command-line arguments
echo "scanning the args ... "
for arg in "$@"; do
  case $arg in
    --version=*)
      RS_VERSION="${arg#*=}"
      ;;
    --build-type=*)
      RS_BUILD_TYPE="${arg#*=}"
      ;;
    --dependencies-install-dir=*)
      DEPENDENCIES_INSTALL_DIR="${arg#*=}"
      ;;
    --public-dir=*)
      PUBLIC_DIR="${arg#*=}"
      ;;
    --create-package=*)
      CREATE_RSODBC_PACKAGE="${arg#*=}"
      ;;
    --help)
      # Display help information
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --version=<version>              Set the ODBC version"
      echo "  --build-type=<type>              Set the build type (Release, Debug, etc.)"
      echo "  --dependencies-install-dir=<dir> Set the dependencies install directory"
      echo "  --create-package=yes/no          Create package after build."
      echo "  --public-dir=<dir>               Output directory for artifacts (default: public)"
      echo "Example:"
      echo "  $0 --version=1.2.3.4 --build-type=Release --dependencies-install-dir=/path/to/deps/install"
      exit 0
      ;;
  esac
done
echo "scanning the args Done "

# Create build directory
mkdir -p ${RS_BUILD_DIR}

# Construct CMake command with basic options
cmake_command="${CMAKE} -S ${RS_ROOT_DIR} -B ${RS_BUILD_DIR} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"

# Add platform-specific options
if [ "$OS" = "Darwin" ]; then
    # For macOS, build universal binary (arm64 and x86_64)
    cmake_command="${cmake_command} -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.7"
fi

# Add dependency locations if specified
if [ -n "${RS_DEPS_DIRS}" ]; then
    cmake_command="${cmake_command} -DRS_DEPS_DIRS=${RS_DEPS_DIRS}"
fi
if [ -n "${RS_MULTI_DEPS_DIRS}" ]; then
    cmake_command="${cmake_command} -DRS_MULTI_DEPS_DIRS=${RS_MULTI_DEPS_DIRS}"
fi
if [ -n "${RS_OPENSSL_DIR}" ]; then
    cmake_command="${cmake_command} -DRS_OPENSSL_DIR=${RS_OPENSSL_DIR}"
fi
if [ -n "${RS_ODBC_DIR}" ]; then
    cmake_command="${cmake_command} -DRS_ODBC_DIR=${RS_ODBC_DIR}"
fi

# Add testing option if specified
if [ -n "${ENABLE_TESTING}" ]; then
    cmake_command="${cmake_command} -DENABLE_TESTING=${ENABLE_TESTING}"
fi

# Add version if specified, otherwise CMake will read from version.txt
if [ -n "${RS_VERSION}" ]; then
    cmake_command="${cmake_command} -DODBC_VERSION=${RS_VERSION}"
    # Else cmake will get it from version.txt
fi

# Add build type if specified
if [ -n "${RS_BUILD_TYPE}" ]; then
    cmake_command="${cmake_command} -DRS_BUILD_TYPE=${RS_BUILD_TYPE} -DCMAKE_BUILD_TYPE=${RS_BUILD_TYPE}"
fi

# Display the final CMake command
echo cmake command is ${cmake_command}

# Run CMake to configure the build
eval ${cmake_command}

# Build and install the driver
make -C ${RS_BUILD_DIR} -j 10
make -C ${RS_BUILD_DIR} install

mkdir -p $PUBLIC_DIR

# Create package if requested
if [ "${CREATE_RSODBC_PACKAGE:-}" = "yes" ]; then
    if [ -z "$RS_VERSION" ]; then
        # Read version and number from the file if not specified via command line
        read version number < version.txt
        RS_VERSION="${version}.${number}"
    fi
    version="${RS_VERSION%.*}"  # removes the last .part
    number="${RS_VERSION##*.}" # gets the last part
    if [ "$OS" = "Linux" ]; then
        # Create RPM package for Linux
        pushd $RS_ROOT_DIR/src/odbc/rsodbc 
        ./create64bit-rpm.sh ${version} ${number} ${ARCH}
        popd
    elif [ "$OS" = "Darwin" ]; then
        # Create PKG package for macOS
        # Create macOS package
        $RS_ROOT_DIR/scripts/mac/create_pkg.sh --version=${RS_VERSION} --source=$RS_ROOT_DIR/cmake-build/install --destination=$PUBLIC_DIR --arch=universal
    else
        echo "Unsupported operating system: $OS"
        exit 1
    fi
fi

exit $?