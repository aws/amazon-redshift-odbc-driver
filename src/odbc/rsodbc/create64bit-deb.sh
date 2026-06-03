#!/bin/bash
#
# Create a .deb package for the Redshift ODBC 2.x 64-bit Linux driver.
# This is the Debian equivalent of create64bit-rpm.sh.
#
# Usage: ./create64bit-deb.sh <odbc_version> <build_number> <arch>
#
# Arguments:
#   odbc_version  - Driver version string, e.g. "2.1.14"
#   build_number  - Build/release number, e.g. "1"
#   arch          - Architecture: "x86_64" or "aarch64"
#                   (mapped to Debian's "amd64" or "arm64")
#
# Environment:
#   RS_ROOT_DIR   - Root of the source tree (optional, defaults to cmake-build parent)
#   INSTALL_DIR   - Path to cmake-build/install (optional, auto-detected)
#
# Output:
#   ./deb/AmazonRedshiftODBC-64-bit-<version>.<build>.<deb_arch>.deb
#
# Example:
#   ./create64bit-deb.sh 2.1.14 1 x86_64
#   ./create64bit-deb.sh 2.1.14 1 aarch64

# Ensures the script fails fast on any unexpected issue rather than silently
set -euo pipefail

# Clean up temp directories on exit (success or failure)
cleanup() {
    rm -rf "${DEB_STAGING:-}" "${BUILD_TMPDIR:-}"
}
trap cleanup EXIT

usage() {
    echo "Usage: $0 <odbc_version> <build_number> <arch>"
    exit 1
}

[ $# -eq 3 ] || usage

odbc_version=$1
build_number=$2
arch_name=$3

echo "create64bit-deb.sh: odbc_version=$odbc_version, build_number=$build_number, arch=$arch_name"

# Map Linux arch names to Debian arch names
case "$arch_name" in
    x86_64)  deb_arch="amd64" ;;
    aarch64) deb_arch="arm64" ;;
    *)
        echo "ERROR: Unsupported architecture '$arch_name'. Use 'x86_64' or 'aarch64'."
        exit 1
        ;;
esac

echo "Debian architecture: $deb_arch"
echo "RS_ROOT_DIR=${RS_ROOT_DIR:-not set}"

if [ -z "${INSTALL_DIR:-}" ] && [ -z "${RS_ROOT_DIR:-}" ]; then
    echo "ERROR: Either INSTALL_DIR or RS_ROOT_DIR must be set."
    exit 1
fi
INSTALL_DIR="${INSTALL_DIR:-${RS_ROOT_DIR}/cmake-build/install/}"

# Validate that required artifacts exist
for artifact in librsodbc64.so rsodbcsql amazon.redshiftodbc.ini root.crt \
                odbc.ini odbcinst.ini odbc.sh odbc.csh; do
    if [ ! -f "${INSTALL_DIR}/${artifact}" ]; then
        echo "ERROR: Required artifact not found: ${INSTALL_DIR}/${artifact}"
        exit 1
    fi
done

# Create output directory
mkdir -p ./deb

# Define staging paths
DEB_STAGING="/tmp/deb-staging-$$"
INSTALL_PATH="/opt/amazon/redshiftodbcx64"
DEB_INSTALL_ROOT="${DEB_STAGING}${INSTALL_PATH}"

deb_version="${odbc_version}"
deb_filename="AmazonRedshiftODBC-64-bit-${odbc_version}.${build_number}.${deb_arch}.deb"

# Clean up any previous staging
rm -rf "${DEB_STAGING}"

echo ""
echo "Creating .deb staging directory..."
echo ""

# Create directory structure
mkdir -p "${DEB_STAGING}/DEBIAN"
mkdir -p "${DEB_INSTALL_ROOT}/samples/connect"

# ---- Copy built artifacts ----
cp -f "${INSTALL_DIR}/librsodbc64.so"            "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/rsodbcsql"                  "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/amazon.redshiftodbc.ini"    "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/root.crt"                   "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/odbc.ini"                   "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/odbcinst.ini"               "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/odbc.sh"                    "${DEB_INSTALL_ROOT}/"
cp -f "${INSTALL_DIR}/odbc.csh"                   "${DEB_INSTALL_ROOT}/"

# Sample files (source from the source tree, binary from install)
cp -f ./samples/connect/connect.c                 "${DEB_INSTALL_ROOT}/samples/connect/"
cp -f ./samples/connect/connect.mak               "${DEB_INSTALL_ROOT}/samples/connect/"
cp -f "${INSTALL_DIR}/connect"                     "${DEB_INSTALL_ROOT}/samples/connect/"

# ---- Create DEBIAN/control ----
cat > "${DEB_STAGING}/DEBIAN/control" <<EOF
Package: amazonredshiftodbcx64
Version: ${deb_version}-${build_number}
Architecture: ${deb_arch}
Maintainer: Amazon Web Services, Inc.
Description: Amazon Redshift ODBC Driver for 64-bit Linux (${deb_arch})
 Amazon Redshift ODBC Driver for 64-bit Linux platforms.
Section: database
Priority: optional
Depends: libc6, libkeyutils1, unixodbc
Homepage: https://docs.aws.amazon.com/redshift/latest/mgmt/odbc20-install-config-linux.html
EOF

# ---- Build the .deb package ----
echo ""
echo "Building ${deb_arch} Redshift ODBC driver .deb package..."
echo ""

if command -v dpkg-deb &>/dev/null; then
    # Preferred method: use dpkg-deb directly.
    # --root-owner-group sets all file ownership to root:root regardless of build user.
    dpkg-deb --root-owner-group --build "${DEB_STAGING}" "./deb/${deb_filename}"
else
    # Fallback: build the .deb manually using ar + tar.
    # dpkg-deb is not available in the default AL2 (Amazon Linux 2) repos, and the
    # ODBC 2.x Linux pipeline builds on AL2. Rather than requiring
    # a third-party package or custom repo, we construct the .deb archive directly.
    # A .deb file is an 'ar' archive containing three members:
    #   1. debian-binary   — format version string ("2.0\n")
    #   2. control.tar.gz  — package metadata (DEBIAN/control)
    #   3. data.tar.gz     — installed files (everything under opt/)
    echo "dpkg-deb not found, using ar+tar fallback for AL2 compatibility..."

    BUILD_TMPDIR="/tmp/deb-build-$$"
    mkdir -p "${BUILD_TMPDIR}"

    # 1. debian-binary
    echo "2.0" > "${BUILD_TMPDIR}/debian-binary"

    # 2. control.tar.gz: contains the DEBIAN/ metadata files
    tar czf "${BUILD_TMPDIR}/control.tar.gz" \
        --owner=root --group=root \
        -C "${DEB_STAGING}/DEBIAN" .

    # 3. data.tar.gz: contains the actual installed files
    tar czf "${BUILD_TMPDIR}/data.tar.gz" \
        --owner=root --group=root \
        -C "${DEB_STAGING}" --exclude='./DEBIAN' .

    # Assemble the .deb archive using ar
    # The order of members matters: debian-binary must come first.
    ar rc "./deb/${deb_filename}" \
        "${BUILD_TMPDIR}/debian-binary" \
        "${BUILD_TMPDIR}/control.tar.gz" \
        "${BUILD_TMPDIR}/data.tar.gz"

    rm -rf "${BUILD_TMPDIR}"
fi

# Print checksums
echo "sha256sum=$(sha256sum "$(pwd)/deb/${deb_filename}" | awk '{print $1}')"
echo "sha512sum=$(sha512sum "$(pwd)/deb/${deb_filename}" | awk '{print $1}')"

echo ""
echo "Find the package in $(pwd)/deb/${deb_filename}"

# print package info for build log readability
# This is NOT a build success criteria, the DEB is already built at this point using ar/dpkg-deb above
# dpkg-deb --info may not be available on the build fleet (Amazon Linux), so we skip
# package integrity is verified later in the test pipeline via SHA-256 checksum + dpkg -i install
if command -v dpkg-deb &>/dev/null; then
    echo ""
    echo "Package info:"
    dpkg-deb --info "./deb/${deb_filename}"
    echo ""
    echo "Package contents:"
    dpkg-deb --contents "./deb/${deb_filename}"
else
    echo ""
    echo "Skipping dpkg-deb --info/--contents (dpkg-deb not available on this host)"
    echo "Package can be verified on a Debian/Ubuntu host with: dpkg-deb --info ./deb/${deb_filename}"
fi

exit 0
