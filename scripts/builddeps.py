"""
ODBC Dependency Build Script

This script automates the process of building and installing dependencies for an ODBC driver project. 
It manages the download, compilation, and installation of various libraries required for the driver, 
including OpenSSL, cURL, AWS SDK for C++, and others.

Key features:
1. Cross-platform support (Windows, macOS, Linux)
2. Automatic dependency resolution and build order determination
3. Git integration for source code management
4. Customizable build commands for each dependency
5. Error handling and reporting
6. Flexible execution modes: full build, source retrieval only, or build only

The script performs the following main tasks:
1. Parse command-line arguments
2. Define repository information and build commands
3. Detect the current platform and set appropriate build parameters
4. Topologically sort dependencies to determine the build order
5. Clone or update source code repositories
6. Build and install each dependency in the correct order
7. Report on the build process and installation directories

Usage:
    python builddeps.py [OPTIONS]

Options:
    --parent-src-dir PATH         Path to ODBC dependencies directory (default: ./src/tp)
    --install-dir PATH       Path to install dependencies (default: <parent-src-dir>/install)
    --build-dir PATH         Path to parent build directories (default: <parent-src-dir>/<src>/build) For now used only in cmake
    --build-type {Release,Debug}  Build type (Windows only, default: Release)
    --get-only               Only retrieve sources and commit changes, skip building
    --get-all                Ignore platform settings when retrieving sources.
    --build-only             Only build existing sources, skip retrieval
    
    If no options are specified, the script will perform a full process: 
    retrieve sources, commit changes, and build dependencies.

Note: 
    - This script requires Git and CMake to be installed and available in the system PATH.
    - We use CMake 3.20 as the minimum version. Please check the ODBC's root CMakeLists.txt
      for the latest minimum CMake version requirement.
    - The --build-type option is only applicable for Windows.
    - When using --get-only, the script will not perform any builds, only source retrieval and git commits.
    - When using --build-only, the script assumes sources are already present and will only perform builds.
    - --build-only and --build-type can be used together to specify the build type when only building.

Examples:
    python builddeps.py                          # Full process: get sources, commit, and build
    python builddeps.py --get-only               # Only retrieve sources and commit changes
    python builddeps.py --build-only             # Only build existing sources
    python builddeps.py --build-type Debug       # Full process with Debug build (Windows only)
    python builddeps.py --build-only --build-type Debug  # Only build existing sources with Debug build (Windows only)
"""



# Repository information
repo_info = {
    "krb5": {
        "url": "https://github.com/krb5/krb5",
        "branch": "master",
        "platform" : "unix",
    },
    "openssl": {
        "url": "https://github.com/openssl/openssl.git",
        "branch": "OpenSSL_1_1_1-stable",
        "dependencies": [],
        "win_dependencies": [],
    },
    "awssdkcpp": {
        "url": "https://github.com/aws/aws-sdk-cpp.git",
        "branch": "main",
        "dependencies": ["curl", "openssl"],
        "win_dependencies": ["openssl"],
        "recurse_submodules": True,
    },
    "cares": {
        "url": "https://github.com/c-ares/c-ares.git",
        "branch": "main",
        "dependencies": [],
        "win_dependencies": [],
    },
    "googletest": {
        "url": "https://github.com/google/googletest.git",
        "branch": "main",
    },
    "nghttp2": {
        "url": "https://github.com/nghttp2/nghttp2.git",
        "branch": "master",
        "dependencies": ["openssl"],
        "platform" : "unix",
    },
    "curl": {
        "url": "https://github.com/curl/curl.git",
        "branch": "master",
        "dependencies": ["openssl", "zlib", "cares", "nghttp2", "krb5"],
        "platform" : "unix",
    },
    "zlib": {
        "url": "https://github.com/madler/zlib.git",
        "branch": "master",
        "platform" : "unix",
    },
    "python": {
        "url": "https://www.python.org/ftp/python/3.13.0/python-3.13.0-embed-amd64.zip",
        "source_type": "compressed",
        "dependencies": [],
        "win_dependencies": [],
        "platform" : "windows",
    },
    "perl": {
        "url": "https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_53822_64bit/strawberry-perl-5.38.2.2-64bit-portable.zip",
        "source_type": "compressed",
        "dependencies": [],
        "win_dependencies": [],
        "platform" : "windows",
    },
    "nasm": {
        "url": "https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip",
        "source_type": "compressed",
        "dependencies": [],
        "win_dependencies": [],
        "platform" : "windows",
    },
}

import os
import subprocess
import pathlib, sys
import platform
import re
import argparse
from enum import Enum
from datetime import datetime, timezone
from collections import namedtuple
import shutil
import stat
import zipfile
import time
import sys
from pathlib import Path
import shlex

default_cmake = "cmake"
cmake_command = os.getenv("CMAKE", default_cmake)

class GlobalConfiguration:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(GlobalConfiguration, cls).__new__(cls)
            # Initialize default values
            cls._instance.build_dir_postfix_cfg = 'build'
            cls._instance.parent_src_dir_cfg = ""
            cls._instance.install_dir_cfg = ""
            cls._instance.build_type_cfg = ""
            cls._instance.build_dir_cfg = ""
            cls._instance.get_all_platforms_sources_cfg = False
            cls._instance.extract_zip_sources_cfg = False
        return cls._instance

# Create a global configuration object
g_config = GlobalConfiguration()


# List of the repositories this script supports.
# The config can come from anywhere else. For now, config is repo_info
class Libs(Enum):
    OPENSSL = "openssl"
    AWS_SDK_CPP = "awssdkcpp"
    CARES = "cares"
    GOOGLETEST = "googletest"
    CURL = "curl"
    NGHTTP2 = "nghttp2"
    ZLIB = "zlib"
    KRB5 = "krb5"
    PYTHON = "python"
    PERL = "perl"
    NASM = "nasm"

    @staticmethod
    def get_element_by_values(values):
        elements = []
        if values is None:
            return elements
        for value in values:
            length = len(elements)
            for member in Libs:
                if member.value == value:
                    elements.append(member)
                    break  # Stop once the name is found
            if length == len(elements):
                print(f"WARNING: Searching for invalid dependency {value}.", flush=True)
        return elements


# Build commands used for Mac and Linux systems
unix_build_commands = {
    Libs.CURL: (
        f"autoreconf -i",
        f"./configure {{CPPFLAGS_}} {{LDFLAGS_}} {{LIBS_}} {{EXTRAS_}} \
        --prefix={{install_dir}} --enable-ares={{install_dir_parent}}/cares \
        --with-ssl={{install_dir_parent}}/openssl \
        --with-zlib={{install_dir_parent}}/zlib \
        --with-nghttp2={{install_dir_parent}}/nghttp2 \
        --with-gssapi-includes={{install_dir_parent}}/krb5/include \
        --with-gssapi-libs={{install_dir_parent}}/krb5/lib \
        --without-libidn2 --without-libpsl --disable-ldap --enable-static  --enable-shared=no --disable-shared",
        f"make",
        f"make install",
        f"make clean",
    ),
    Libs.OPENSSL: (
        f"./Configure --prefix={{install_dir}} {{openssl_platform}}",
        f"make",
        f"make install",
        f"make clean",
        f"rm -rf {{install_dir}}/lib/*.dylib {{install_dir}}/lib/*.so",
    ),
    Libs.AWS_SDK_CPP: (
        f"rm -rf {{build_dir}}",
        f"mkdir -p {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} \
          -DCMAKE_VERBOSE_MAKEFILE=TRUE \
          -DBUILD_ONLY='core;sts;redshift;redshift-serverless;sso-oidc' \
          -DCMAKE_INSTALL_PREFIX={{install_dir}} \
          -DBUILD_SHARED_LIBS=OFF -DSIMPLE_INSTALL=ON -DENABLE_TESTING=0 \
          -DENABLE_UNITY_BUILD=ON -DFORCE_SHARED_CRT=OFF -DENABLE_RTTI=ON \
          -DCUSTOM_MEMORY_MANAGEMENT=OFF -USE_OPENSSL=ON -DDISABLE_INTERNAL_IMDSV1_CALLS=ON \
          -DBUILD_DEPS=ON -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INCLUDE_PATH={{install_dir_parent}}/openssl/include \
          -DOPENSSL_ROOT_DIR={{install_dir_parent}}/openssl \
          -DOPENSSL_INCLUDE_DIR={{install_dir_parent}}/openssl/include \
          -DOPENSSL_LIBRARIES='{{install_dir_parent}}/openssl/lib/libssl.a;{{install_dir_parent}}/openssl/lib/libcrypto.a' \
          -Dcrypto_LIBRARY='{{install_dir_parent}}/openssl/lib/libcrypto.a' \
          -DCURL_INCLUDE_DIR={{install_dir_parent}}/curl/include \
          -DCURL_LIBRARY=';{{install_dir_parent}}/curl/lib/libcurl.a;{{install_dir_parent}}/care/lib/libcares.a;{{install_dir_parent}}/krb5/lib/libcom_err.a;{{install_dir_parent}}/krb5/lib/libgssapi_krb5.a;{{install_dir_parent}}/krb5/lib/libgssrpc.a;{{install_dir_parent}}/krb5/lib/libk5crypto.a;{{install_dir_parent}}/krb5/lib/libkadm5clnt.a;{{install_dir_parent}}/krb5/lib/libkadm5srv.a;{{install_dir_parent}}/krb5/lib/libkdb5.a;{{install_dir_parent}}/krb5/lib/libkrad.a;{{install_dir_parent}}/krb5/lib/libkrb5.a;{{install_dir_parent}}/krb5/lib/libkrb5support.a;{{install_dir_parent}}/krb5/lib/libverto.a;{{install_dir_parent}}/nghttp2/lib/libnghttp2.a'",
        f"make -C {{build_dir}} -j 5",
        f"make -C {{build_dir}} install",
        f"rm -rf {{build_dir}}",
    ),
    Libs.CARES: (
        f"mkdir -p {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{install_dir}} -DCMAKE_INSTALL_LIBDIR=lib \
        -DCARES_THREADS=ON -DCARES_STATIC=ON  -DCARES_SHARED=OFF \
        -DCARES_STATIC_PIC=ON",
        f"make -C {{build_dir}} -j5",
        f"make -C {{build_dir}} install",
        f"rm -rf {{build_dir}}",
    ),
    Libs.NGHTTP2: (
        f"mkdir -p {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
        -DOPENSSL_ROOT_DIR={{install_dir_parent}}/openssl -DCMAKE_POSITION_INDEPENDENT_CODE=True \
        -DCMAKE_INSTALL_PREFIX={{install_dir}} -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_LIBDIR=lib\
        -DENABLE_LIB_ONLY=ON -DENABLE_THREADS=OFF -DENABLE_SHARED_LIB=OFF",
        f"make -C {{build_dir}} -j5",
        f"make -C {{build_dir}} install",
        f"rm -rf {{build_dir}}",
    ),
    Libs.ZLIB: (
        "mkdir -p {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{install_dir}} \
        -DCMAKE_POSITION_INDEPENDENT_CODE=True -DBUILD_SHARED_LIBS=OFF",
        f"make -C {{build_dir}} -j5",
        f"make -C {{build_dir}} install",
        f"rm -rf {{build_dir}}",
        f"rm -rf {{install_dir}}/lib/*.dylib {{install_dir}}/lib/*.so",
    ),
    Libs.GOOGLETEST: (
        f"mkdir -p {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} -DCMAKE_INSTALL_PREFIX={{install_dir}} -DBUILD_SHARED_LIBS=OFF",
        f"make -C {{build_dir}} -j5",
        f"make -C {{build_dir}} install",
        f"rm -rf {{build_dir}}",
    ),
    Libs.KRB5: (
        f"autoreconf -i",
        f"./configure CFLAGS=-fPIC {{options}}",
        f"make",
        f"make install",
        f"./configure CFLAGS=-fPIC {{options}} --enable-static --disable-shared \
        --with-tcl=no --without-system-verto \
        --enable-dns-for-realm --with-crypto-impl=builtin",
        f"make",
        f"make install",
        f"rm -rf {{install_dir}}/lib/*.dylib",
        f"ln -sf {{install_dir}}/lib/libgssapi_krb5.a {{install_dir}}/lib/libgssapi.a",
    ),
    Libs.PYTHON: (
    # f"unzip -o python-3.13.0-embed-amd64.zip -d {{install_dir}}",
    ),
}

# Build commands for windows systems
windows_build_commands = {
    Libs.OPENSSL: (
        f"perl Configure {{openssl_platform}} --prefix={{install_dir}} --openssldir={os.path.join('{install_dir}', 'ssl')} ",
        "nmake",
        "nmake install",
        "nmake clean",
    ),
    Libs.AWS_SDK_CPP: (
        f"if exist {{build_dir}} rmdir /s /q {{build_dir}}",
        f"if not exist {{build_dir}} mkdir {{build_dir}}",
        f"""{{cmake_command}} -B {{build_dir}} \
        -A x64 -DCMAKE_BUILD_TYPE=Release  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
        -DBUILD_SHARED_LIBS=0 -DFORCE_SHARED_CRT=0 -DBUILD_ONLY='core;redshift;redshift-serverless;sts;sso-oidc'\
        -DENABLE_TESTING=OFF -DCMAKE_INSTALL_PREFIX={{install_dir}}""",
        f"{{cmake_command}} --build {{build_dir}} --config Release --parallel 5",
        f"{{cmake_command}} --install {{build_dir}} --config Release",
        f"if exist {{build_dir}} rmdir /s /q {{build_dir}}",
    ),
    Libs.CARES: (
        f"if exist {{build_dir}} rmdir /s /q {{build_dir}}",
        f"if not exist {{build_dir}} mkdir {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} -DCMAKE_BUILD_TYPE=Release  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
        -DCMAKE_INSTALL_PREFIX={{install_dir}} -DCMAKE_INSTALL_LIBDIR=lib \
        -DCARES_THREADS=ON -DCARES_STATIC=ON  -DCARES_SHARED=OFF \
        -DCARES_STATIC_PIC=ON",
        f"{{cmake_command}} --build {{build_dir}} --config Release --parallel 5",
        f"{{cmake_command}} --install {{build_dir}} --config Release",
        f"if exist {{build_dir}} rmdir /s /q {{build_dir}}",
    ),
    Libs.GOOGLETEST: (
        f"if exist {{build_dir}} rmdir /s /q {{build_dir}}",
        f"if not exist {{build_dir}} mkdir {{build_dir}}",
        f"{{cmake_command}} -B {{build_dir}} -DCMAKE_INSTALL_PREFIX={{install_dir}} -DBUILD_SHARED_LIBS=OFF  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
        f"{{cmake_command}} --build {{build_dir}} --config Release --parallel 5",
        f"{{cmake_command}} --install {{build_dir}} --config Release",
        f"if exist {{build_dir}} rmdir /s /q {{build_dir}}",
    ),
    Libs.PYTHON: (
    # f"unzip -o python-3.13.0-embed-amd64.zip -d {{install_dir}}",
    ),
}


"""
Get the installation directory for a given repository.

Args:
    repo_name (str): Name of the repository.

Returns:
    str: Path to the installation directory.
"""

def get_install_dir(repository):
    install_dir = os.path.join(g_config.install_dir_cfg, repository.value)
    install_dir = (
        os.path.join(install_dir, g_config.build_type_cfg) if platform.system() == "Windows" else install_dir
    )
    return install_dir


def get_build_dir(repository):
    if g_config.build_dir_cfg is not None:
        return os.path.join(g_config.build_dir_cfg, repository.value, g_config.build_dir_postfix_cfg)
    return os.path.join(g_config.parent_src_dir_cfg, repository.value, g_config.build_dir_postfix_cfg)


class UnsupportedPlatformError(Exception):
    """Raised when the detected platform is not supported."""
    pass
"""
Determine the host argument based on the current platform.

Returns:
    str: Host argument string.

Raises:
    UnsupportedPlatformError: If the platform is unsupported.
"""


def get_host_argument():
    # Detect platform
    system = platform.system()
    machine = platform.machine()

    if system == "Darwin":
        if machine == "arm64":
            return "arm64-apple-darwin"
        elif machine == "x86_64":
            return "x86_64-apple-darwin"
        else:
            # Unsupported macOS platform
            raise UnsupportedPlatformError(f"Unsupported macOS platform: {machine}")
    else:
        # Unsupported platform
        raise UnsupportedPlatformError(f"Unsupported platform: {system}")


"""
Detect the OpenSSL platform based on the current system.

Returns:
    str: OpenSSL platform string.

Raises:
    UnsupportedPlatformError: If the platform is unsupported.
"""


def detect_openssl_platform():
    # Detect platform
    system = platform.system()
    machine = platform.machine()

    if system == "Linux":
        if machine == "x86_64":
            return "linux-x86_64"
        elif machine.startswith("arm"):
            # Check if it's 32-bit or 64-bit ARM
            if machine.endswith("64"):
                return "linux-aarch64"
            else:
                return "linux-armv4"
        else:
            # Unsupported Linux platform
            raise UnsupportedPlatformError(f"Unsupported Linux platform: {machine}")
    elif system == "Darwin":
        if machine == "arm64":
            return "darwin64-arm64-cc"
        elif machine == "x86_64":
            return "darwin64-x86_64-cc"
        else:
            # Unsupported macOS platform
            raise UnsupportedPlatformError(f"Unsupported macOS platform: {machine}")
    elif system == "Windows":
        return "VC-WIN64A"
    else:
        # Unsupported platform
        raise UnsupportedPlatformError(f"Unsupported platform: {system}")


openssl_platform = detect_openssl_platform()


"""
Get the list of repository names based on the current platform.

Returns:
    list: List of repository names.
"""


def get_repositories():
    current_platform = platform.system().lower()
    repository_names = []

    for key, value in repo_info.items():
        # Check the platform in repo_info
        repo_platform = value.get("platform", "all").lower()

        # Include the key if platform is "all" or matches current platform
        if repo_platform == "all":
            repository_names.append(key)
        elif current_platform == "windows" and repo_platform == "windows":
            repository_names.append(key)
        elif current_platform in ["linux", "darwin"] and repo_platform in ["unix", "linux", "macos"]:
            repository_names.append(key)

    return repository_names


"""
Get the dependencies for a given repository.

Args:
    repository (str): Name of the repository.

Returns:
    list: List of dependencies.
"""


def get_dependencies(repository):
    res = []
    subfiled = "win_dependencies" if platform.system() == "Windows" else "dependencies"
    repo = repo_info[repository.value] if repository.value in repo_info else None
    deps = repo.get(subfiled, []) if repo is not None else None
    return Libs.get_element_by_values(deps)


"""
Perform a topological sort on the repository dependencies.

Args:
    repo_info (dict): Dictionary containing repository information.

Returns:
    list: Topologically sorted list of repository names.

Raises:
    ValueError: If a cycle is detected in the dependency graph.
"""


def topological_sort(repo_info):
    from collections import defaultdict, deque

    topo_order = []
    graph = defaultdict(list)
    repository_names = get_repositories()
    # print(f"repository names:{repository_names}", flush=True)
    repositories = Libs.get_element_by_values(repository_names)
    # print(f"repository enums:{repository_names}", flush=True)
    # Sanity check
    if len(repositories) != len(repository_names):
        print(f"ERROR: { len(repository_names) - len(repositories)} dependencies are not valid.", flush=True)
        return topo_order

    in_degree = {repo: 0 for repo in repositories}
    print(f"repositories:{repositories}", flush=True)
    for repository in repositories:
        deps = get_dependencies(repository)
        # print(f"dependencies({repository.value}): {deps}", flush=True)
        for dep in deps:
            graph[dep].append(repository)
            in_degree[repository] += 1

    # Step 3: Perform topological sorting using Kahn's algorithm (BFS)
    zero_in_degree_queue = deque([repo for repo in repositories if in_degree[repo] == 0])
    # print(f"zero_in_degree_queue: {zero_in_degree_queue}", flush=True)

    while zero_in_degree_queue:
        current_repo = zero_in_degree_queue.popleft()
        topo_order.append(current_repo)

        for neighbor in graph[current_repo]:
            in_degree[neighbor] -= 1
            if in_degree[neighbor] == 0:
                zero_in_degree_queue.append(neighbor)

    # Step 4: Check for cycle (optional)
    if len(topo_order) != len(repositories):
        raise ValueError(
            f"Graph has at least one cycle. Cannot perform topological sorting {len(topo_order)}/{len(repositories)}."
        )
    return topo_order


"""
Clone or update the source code for a given repository.

Args:
    repo_name (str): Name of the repository.

Returns:
    str: Path to the repository directory.
    int: 1 if an error occurred, otherwise the repository directory.
"""

def handle_readonly_error(func, path, exc_info):
    """Error handler for shutil.rmtree to handle read-only files."""
    os.chmod(path, stat.S_IWRITE)
    func(path)

def rename_dir(path, new_path, remove_existing_detination=False):
    """
    Rename (move) the given path to <path>.bak.
    If <path>.bak already exists, it will be removed first, handling read-only files gracefully.
    """
    if os.path.exists(path):
        try:
            if os.path.exists(new_path) and remove_existing_detination:
                print(f"Backup already exists: {new_path}", flush=True)
                try:
                    shutil.rmtree(new_path, onerror=handle_readonly_error)  # Remove directory
                    print(f"Removed existing backup: {new_path}", flush=True)
                except NotADirectoryError:
                    os.remove(new_path)  # Remove file
                    print(f"Removed existing file backup: {new_path}", flush=True)
                except PermissionError as e:
                    print(f"Access denied while removing {new_path}: {e}", file=sys.stderr, flush=True)
                    return
            
            # Rename the target to <path>.bak
            os.rename(path, new_path)
            print(f"Renamed {path} to {new_path}", flush=True)
        except PermissionError as e:
            print(f"Access denied while renaming {path}. Retrying...", file=sys.stderr, flush=True)
            time.sleep(1)  # Wait and retry
            try:
                os.rename(path, new_path)
                print(f"Successfully renamed {path} to {new_path} after retry.", flush=True)
            except Exception as retry_e:
                print(f"Failed to rename {path} after retry: {retry_e}", file=sys.stderr, flush=True)
        except Exception as e:
            print(f"Error renaming {path} to {new_path}: {e}", file=sys.stderr, flush=True)

def get_zip(full_url_with_path, copy_to, extract=g_config.extract_zip_sources_cfg):
    # Step 1: Download the zip file using curl
    local_filename = full_url_with_path.split("/")[-1]  # Extract the filename from the URL
    os.makedirs(copy_to, exist_ok=True)
    local_filename_path = os.path.join(copy_to, local_filename)
    try:
        if not os.path.exists(local_filename_path):
            print(f"Downloading from {full_url_with_path} to {local_filename_path}...", flush=True)
            curl_command = ["curl", "-L", "-o", local_filename_path, full_url_with_path]
            print(f"Running command: {''.join(curl_command)}", flush=True)
            subprocess.run(curl_command, check=True)
            print("Download completed.", flush=True)
        else:
            print(f"File already exists: {local_filename_path}. Skipping download.", flush=True)
    except subprocess.CalledProcessError as e:
        print(f"Error during download: {e}", flush=True)
        return

    # Step 2: Extract the zip file
    if not os.path.exists(local_filename_path):
        print(f"Downloaded file not found: {local_filename_path}", flush=True)
        return
    try:
        if extract is True:
            print(f"Extracting {local_filename_path} to {copy_to} ...", flush=True)
            with zipfile.ZipFile(local_filename_path, 'r') as zip_ref:
                zip_ref.extractall(copy_to)
            print("Extraction completed.", flush=True)
    except zipfile.BadZipFile as e:
        print(f"Error: File is not a valid zip file. {e}", flush=True)
        return
    except Exception as e:
        print(f"An error occurred during copy/extraction: {e}", flush=True)
        return

def git_current_branch(repo_dir):
    try:
        # Reset the current branch to match its remote counterpart
        current_branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=repo_dir,
            text=True
        ).strip()
        return current_branch
    except subprocess.CalledProcessError as e:
        print(f"Error during git_current_branch: {e}", flush=True)
    return ""

def get_src(repo_name):
    if repo_name not in repo_info or repo_name is None:
        return None

    repo_url = repo_info[repo_name]["url"]
    source_type = repo_info[repo_name].get("source_type", "git")
    repo_dir = os.path.join(g_config.parent_src_dir_cfg, repo_name)

    try:
        if source_type == "git":
            branch_name = repo_info[repo_name]["branch"]
            recurse_submodules = repo_info[repo_name]["recurse_submodules"] if "recurse_submodules" in repo_info[repo_name] else False
            # bring back the deactivated git functionality
            git_dir = os.path.join(repo_dir, ".git")
            if os.path.exists(git_dir  + ".bak"):
                rename_dir(git_dir  + ".bak", git_dir)
                git_ignore_file = os.path.join(repo_dir, ".gitignore")
                rename_dir(git_ignore_file + ".bak", git_ignore_file)
            else:
                print(f"could not find {git_dir  + '.bak'}", flush=True)
           
            # pull
            if os.path.exists(repo_dir):
                print("Action: PULL", flush=True)
                current_branch = git_current_branch(repo_dir)
                print(f"current branch:{current_branch} : {repo_dir}", flush=True)
                git_commands = [
                    ["git", "reset", "--hard", f"origin/{current_branch}"],
                    ["git", "clean", "-fdx"],
                    ["git", "fetch", "origin", branch_name],
                    ["git", "checkout", branch_name],
                    ["git", "reset", "--hard", f"origin/{branch_name}"],
                    ["git", "clean", "-fdx"],
                ]
                if recurse_submodules:
                    git_commands.append(["git", "submodule", "update", "--init", "--recursive", "--depth=1"])
                for git_command in git_commands:
                    print(f"Running command: {''.join(git_command)} on {repo_dir}", flush=True)
                    subprocess.run(git_command, cwd=repo_dir, check=True)
            else:
                print("Action: CLONE", flush=True)
                git_commands_extra_options = []
                if recurse_submodules:
                    git_commands_extra_options = ["--recurse-submodules", "--shallow-submodules"]
                # clone
                git_commands = [[
                    "git",
                    "clone",
                    repo_url,
                    "--branch",
                    branch_name,
                    "--depth",
                    "1",
                    repo_dir,
                ] + git_commands_extra_options,]
                for git_command in git_commands:
                    print(f"Running command: {''.join(git_command)}", flush=True)
                    subprocess.run(git_command, check=True)
        elif source_type == "compressed":
            get_zip(repo_url, repo_dir)

        return repo_dir
    except Exception as e:
        if source_type == "compressed":
            print(
                f"Error: Failed to download and extract the {repo_name} repository from {repo_url}.",
                file=sys.stderr, flush=True
            )
        else:
            print(
                f"Error: Failed to pull/clone the {repo_name} repository from {repo_url}.",
                file=sys.stderr, flush=True
            )
        print(f"Error: {e}", flush=True)
        return None


"""
Build a dependency using the provided commands.

Args:
    repo_dir (str): Path to the repository directory.
    install_dir (str): Path to the installation directory.
    commands (list): List of build commands.

Returns:
    int: 0 if build successful, 1 if an error occurred.
"""


def build_dep(repo_dir, install_dir, commands):
    try:
        for command in commands:
            leaf_dir = os.path.split(repo_dir.rstrip("/"))[1]
            print(f"====================={leaf_dir} ", flush=True)
            print(f"=====================command: {command}", flush=True)
            if platform.system() == "Windows":
                # avoiding various slash back slash conflicts
                command = ["cmd", "/c"] + shlex.split(command, posix=False)
            else:
                command = shlex.split(command)
            print(f"command after split: {command}", flush=True)
            print(f"Running command: {' '.join(command)} on {repo_dir}", flush=True)
            subprocess.run(command, cwd=repo_dir, check=True)
    except subprocess.CalledProcessError:
        print("Error: Failed to build and/or install.", file=sys.stderr, flush=True)
        return 1
    return 0


"""
Get the appropriate build commands based on the current platform.

Returns:
    dict: Dictionary of build commands for each repository.
"""


def get_build_commands():
    if platform.system() == "Windows":
        return windows_build_commands
    else:
        return unix_build_commands


"""
Get formatted build commands for a specific repository.

Args:
    repo_name (str): Name of the repository.
    **kwargs: Additional keyword arguments for formatting.

Returns:
    list: Formatted build commands.
"""
def get_repo_build_commands(repository, **kwargs):
    def get_formatted_commands(commands, **kwargs):
        def format_command(cmd):
            try:
                return cmd.format(**kwargs)
            except KeyError as e:
                print(f"Warning: Missing key {e} in command: {cmd}", flush=True)
                return cmd

        return [format_command(cmd) for cmd in commands]

    return get_formatted_commands(get_build_commands()[repository], **kwargs)


def build_python(repo_dir):
    return 0

def build_cares(repo_dir):
    repository = Libs.CARES
    build_dir = get_build_dir(repository)
    install_dir = get_install_dir(repository)
    commands = get_repo_build_commands(
        repository, build_dir=build_dir, install_dir=install_dir, cmake_command=cmake_command
    )
    return build_dep(repo_dir, install_dir, commands)


def build_nghttp2(repo_dir):
    repository = Libs.NGHTTP2
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    install_dir_parent = os.path.dirname(install_dir)
    commands = get_repo_build_commands(
        repository,
        install_dir_parent=install_dir_parent, build_dir=build_dir,
        install_dir=install_dir,
        cmake_command=cmake_command,
    )
    return build_dep(repo_dir, install_dir, commands)


def build_zlib(repo_dir):
    repository = Libs.ZLIB
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    commands = get_repo_build_commands(
        repository, build_dir=build_dir, install_dir=install_dir, cmake_command=cmake_command
    )
    return build_dep(repo_dir, install_dir, commands)


# Build the KRB5 library
# Note: building shared libs is necessary for the static libs to link properly
def build_krb5(repo_dir):
    repository = Libs.KRB5
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    options = f"--prefix={install_dir} --with-tcl=no --without-system-verto \
                --enable-dns-for-realm --with-crypto-impl=builtin"
    commands = commands = get_repo_build_commands(
        repository, build_dir=build_dir, install_dir=install_dir, cmake_command=cmake_command, options=options
    )
    repo_dir = os.path.join(repo_dir, "src")
    return build_dep(repo_dir, install_dir, commands)


# Build the OpenSSL library
def build_openssl(repo_dir):
    repository = Libs.OPENSSL
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    commands = get_repo_build_commands(
        repository, build_dir=build_dir, install_dir=install_dir, openssl_platform=openssl_platform
    )
    return build_dep(repo_dir, install_dir, commands)


# Build the Curl library with all the provided dependencies
def build_curl(repo_dir):
    repository = Libs.CURL
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    install_dir_parent = os.path.dirname(install_dir)
    EXTRAS_ = ""
    LDFLAGS_ = ""
    LIBS_ = ""
    CPPFLAGS_ = ""
    include_flags = f"-I{install_dir_parent}/nghttp2/include -I{install_dir_parent}/openssl/include \
                    -I{install_dir_parent}/cares/include -I{install_dir_parent}/zlib/include \
                    -I{install_dir_parent}/krb5/include"
    CPPFLAGS_ = include_flags
    if platform.system() == "Darwin":
        DEPLOYMENT_TARGET = 10.8
        sysroot = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"  # $(xcrun -sdk macosx --show-sdk-path)
        CPPFLAGS_ = (
            CPPFLAGS_
            + f" -arch {platform.machine()} -isysroot {sysroot} -mmacosx-version-min={DEPLOYMENT_TARGET} "
        )
        EXTRAS_ = f" CC=clang --host={get_host_argument()} --with-secure-transport "
        LIBS_ = " -lresolv -lkrb5 -lk5crypto -lkrb5support -lcom_err "
        LDFLAGS_ += " -framework Kerberos "
    elif platform.system() == "Linux":
        LIBS_ = f"{install_dir_parent}/krb5/lib/libkrb5.a  {install_dir_parent}/krb5/lib/libgssapi_krb5.a \
            {install_dir_parent}/krb5/lib/libk5crypto.a \
            {install_dir_parent}/krb5/lib/libkrb5support.a {install_dir_parent}/krb5/lib/libcom_err.a \
            {install_dir_parent}/krb5/lib/libgssrpc.a \
            {install_dir_parent}/krb5/lib/libkadm5clnt.a {install_dir_parent}/krb5/lib/libkadm5srv.a \
            {install_dir_parent}/krb5/lib/libkdb5.a {install_dir_parent}/krb5/lib/libkrad.a \
            {install_dir_parent}/krb5/lib/libverto.a {install_dir_parent}/nghttp2/lib/libnghttp2.a \
            {install_dir_parent}/openssl/lib/libssl.a {install_dir_parent}/openssl/lib/libcrypto.a \
            {install_dir_parent}/zlib/lib/libz.a -lresolv -lkeyutils -ldl -pthread"
    if len(CPPFLAGS_) > 0:
        CPPFLAGS_ = f"CPPFLAGS='{CPPFLAGS_}'"
    if len(LDFLAGS_) > 0:
        LDFLAGS_ = f"LDFLAGS='{LDFLAGS_}'"
    if len(LIBS_) > 0:
        LIBS_ = f"LIBS='{LIBS_}'"
    commands = get_repo_build_commands(
        repository,
        install_dir=install_dir,
        cmake_command=cmake_command, build_dir=build_dir,
        install_dir_parent=install_dir_parent,
        LIBS_=LIBS_,
        CPPFLAGS_=CPPFLAGS_,
        LDFLAGS_=LDFLAGS_,
        EXTRAS_=EXTRAS_,
    )
    return build_dep(repo_dir, install_dir, commands)


# Build the googletest library
def build_googletest(repo_dir):
    repository = Libs.GOOGLETEST
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    commands = get_repo_build_commands(
        repository, build_dir=build_dir, install_dir=install_dir, cmake_command=cmake_command
    )
    return build_dep(repo_dir, install_dir, commands)


# Build the AWS SDK for C++ alongwith all the dependencies
def build_awssdkcpp(repo_dir):
    repository = Libs.AWS_SDK_CPP
    install_dir = get_install_dir(repository)
    build_dir = get_build_dir(repository)
    install_dir_parent = os.path.dirname(install_dir)
    commands = get_repo_build_commands(
        repository, build_dir=build_dir,
        install_dir=install_dir,
        cmake_command=cmake_command,
        install_dir_parent=install_dir_parent,
    )
    return build_dep(repo_dir, install_dir, commands)


"""
Get the build function for a given repository.

Args:
    repo_name (str): Name of the repository.

Returns:
    function: Build function for the repository, or None if not found.
"""


def get_build_function(repo):
    return globals().get(f"build_{repo}")


"""
Commit changes in the build directory and create a tagged commit.

This function changes to the build directory, stages all changes,
creates a commit with a detailed message, and tags the commit with a timestamp.

Args:
new_repos (list): List of newly cloned repositories.
updated_repos (list): List of updated repositories.

Returns:
None

Raises:
subprocess.CalledProcessError: If any git command fails.
"""

# Used to get ,for example, src/tp from /path/to/src/tp to be used in git add src/tp
def get_path_difference(path1, path2):
    # Convert strings to Path objects
    p1 = Path(path1).resolve()
    p2 = Path(path2).resolve()

    # Ensure p2 is not shorter than p1
    if len(p2.parts) < len(p1.parts):
        p1, p2 = p2, p1

    # Find the common parts
    common = 0
    for x, y in zip(p1.parts, p2.parts):
        if x != y:
            break
        common += 1

    # Join the remaining parts of p2
    difference = Path(*p2.parts[common:])
    print(f"Path difference: {difference}", flush=True)
    return str(difference)

def create_commit_message(new_repos, updated_repos, repo_details):
    # Get current date and time with timezone
    now = datetime.now(timezone.utc)
    formatted_time = now.strftime("%Y-%m-%d %H:%M:%S %Z")
    commit_message = f"Update dependencies: {formatted_time}\n"
    if new_repos:
        commit_message += "\nNewly added:\n" + "\n".join(f"- {repo}" for repo in new_repos)
    if updated_repos:
        commit_message += "\nUpdated:\n" + "\n".join(f"- {repo}" for repo in updated_repos)
    if repo_details:
        commit_message += "\nRepository details:\n" + "\n".join(repo_details)
    return commit_message
def create_compressed_file():
    parent_src_dir_cfg_name = g_config.parent_src_dir_cfg.split('/')[-1]
    zip_filename = f"{parent_src_dir_cfg_name}.zip"
    compressed_dir = os.path.join(os.path.dirname(g_config.parent_src_dir_cfg), "compressed")
    zip_filepath = os.path.join(compressed_dir, zip_filename)

    # Create the 'compressed' directory if it doesn't exist
    if not os.path.exists(compressed_dir):
        os.makedirs(compressed_dir)
    with zipfile.ZipFile(zip_filepath, 'w', zipfile.ZIP_DEFLATED) as zip_file:
        for root, dirs, files in os.walk(g_config.parent_src_dir_cfg):
            if 'install' in dirs:
                dirs.remove('install')                    
            if 'compressed' in dirs:
                dirs.remove('compressed')
            for file in files:
                file_path = os.path.join(root, file)
                zip_file.write(file_path, os.path.relpath(file_path, g_config.parent_src_dir_cfg))

    print(f"Zip file created: {zip_filepath}", flush=True)
    return zip_filepath

def git_commit_src_dir(new_repos, updated_repos):
    if not os.path.isdir(".git"):
        print(f"CWD ({os.getcwd()}) is not a Git directory.", flush=True)
        return
    src_dir = os.getcwd()
    try:
        # Change to the parent_src_dir_cfg directory to be safe
        os.chdir(g_config.parent_src_dir_cfg)

        # Collect repo details (name, branch, and commit hash)
        repo_details = []
        for subfolder in os.listdir(g_config.parent_src_dir_cfg):
            subfolder_path = os.path.join(g_config.parent_src_dir_cfg, subfolder)
            git_dir = os.path.join(subfolder_path, ".git")
            if os.path.isdir(subfolder_path) and os.path.isdir(git_dir):
                try:
                    # Get the latest commit hash (short) and branch name
                    commit_hash = subprocess.check_output(
                        ["git", "-C", subfolder_path, "rev-parse", "--short", "HEAD"],
                        text=True
                    ).strip()
                    branch_name = subprocess.check_output(
                        ["git", "-C", subfolder_path, "rev-parse", "--abbrev-ref", "HEAD"],
                        text=True
                    ).strip()
                    repo_details.append(f"{subfolder}: {branch_name} @ {commit_hash}")

                    # Remove the .git directory
                    print(f"Removing .git directory: {git_dir}", flush=True)
                    rename_dir(git_dir, git_dir + ".bak", remove_existing_detination=True)
                    git_ignore_file = os.path.join(subfolder_path, ".gitignore")
                    rename_dir(git_ignore_file, git_ignore_file + ".bak", remove_existing_detination=True)
                except subprocess.CalledProcessError as e:
                    print(f"Error retrieving details for {subfolder}: {e}", file=sys.stderr, flush=True)
                except Exception as e:
                    print(f"Error renaming {git_dir}: {e}", file=sys.stderr, flush=True)
        #done with procesing commit message, now get back to root
        os.chdir(src_dir)
        # Add all changes
        print("Adding chenges(sources) ...", flush=True)
        # get the folder we are adding to git, for example src/tp, we need to get src/tp from /path/to/src/tp
        path_difference = get_path_difference(src_dir, g_config.parent_src_dir_cfg)
        git_add_command = ["git", "add", "-A", f"{path_difference}", "--", f":!{path_difference}/install"]
        print(f"Running command: {''.join(git_add_command)}", flush=True)
        subprocess.run(git_add_command, check=True)
        # Additionally Create and commit a the zip file of whole directory tree and add it
        print("Adding chenges(zipped sources) ...", flush=True)
        zip_filepath = create_compressed_file()
        # Create the Git add command to stage the zip file, excluding the 'install' directory
        git_add_command = ["git", "add", "-A", f"{zip_filepath}"]
        print(f"Running command: {''.join(git_add_command)}", flush=True)
        subprocess.run(git_add_command, check=True)
        status_command = ["git", "status", "--ignored"]
        print(f"Running command: {''.join(status_command)}", flush=True)
        subprocess.run(status_command, check=True)
        print("Committing chenges ...", flush=True)
        # Prepare commit message
        commit_message = create_commit_message(new_repos, updated_repos, repo_details)
        # Commit changes
        commit_command = ["git", "commit", "-m", commit_message]
        print(f"Running command: {''.join(commit_command)}", flush=True)
        subprocess.run(commit_command, check=True)

        print("Changes committed successfully.", flush=True)
    except subprocess.CalledProcessError as e:
        print(f"Error committing changes: {e}", file=sys.stderr, flush=True)
    finally:
        # Change back to the original directory if you haven't already
        print(f"Change directory back to {src_dir}", flush=True)
        os.chdir(src_dir)



"""
Retrieve source code for all repositories in the given order.

This function iterates through the sorted list of repositories,
retrieving the source code for each. It keeps track of which
repositories are newly cloned and which are updated.

Args:
sorted_repos (list): A topologically sorted list of repositories.

Returns:
tuple: A tuple containing:
    - list of source directories
    - error code (0 for success, 1 for failure)
    - error message (empty string if successful)
    - list of newly cloned repositories
    - list of updated repositories
"""
# Define the named tuple
SourceResult = namedtuple('SourceResult', ['src_dirs', 'status', 'message', 'new_repos', 'updated_repos'])
def get_all_sources(repositories):    
    src_dirs = []
    new_repos = []
    updated_repos = []
    for repo in repositories:
        src_dir = get_src(repo.value)
        if src_dir is not None:
            src_dirs.append(src_dir)
            if os.path.exists(src_dir):
                updated_repos.append(repo.value)
            else:
                new_repos.append(repo.value)
        else:
            return SourceResult(src_dirs, 1, f"Failed to get source code for {repo.value}", new_repos, updated_repos)
    
    return SourceResult(src_dirs, 0, "", new_repos, updated_repos)


"""
Build all sources in the given directories.

This function iterates through the source directories and corresponding
repositories, calling the appropriate build function for each.

Args:
src_dirs (list): List of source directories to build.
repositories (list): List of corresponding repository objects.

Returns:
tuple: A tuple containing:
    - failed source directory (empty string if successful)
    - error code (0 for success, 1 for failure)
    - error message
"""
def build_all_sources(repositories):
    for repo in repositories:
        src_dir = os.path.join(g_config.parent_src_dir_cfg, repo.value)
        build_func = get_build_function(repo.value)
        if build_func:
            if build_func(src_dir) != 0:
                return src_dir, 1, f"Failed to build {repo.value}"
        else:
            print(f"No build function found for {repo.value}", flush=True)
    return "", 0, ""


"""
Retrieve all sources, commit changes, and build all sources.

This function orchestrates the entire process of getting sources,
committing changes to the build directory, and building all sources.

Args:
sorted_repos (list): A topologically sorted list of repositories.

Returns:
tuple: A tuple containing:
    - list of source directories (or failed source directory)
    - error code (0 for success, 1 for failure)
    - error message
"""
# Define a new named tuple for get_and_build_all
BuildResult = namedtuple('BuildResult', ['src_dirs', 'status', 'message'])
def get_and_build_all(sorted_repos):
    try:
        # Get all sources
        source_result = get_all_sources(list(Libs) if g_config.get_all_platforms_sources_cfg is True else sorted_repos)
        
        if source_result.status:
            return BuildResult(source_result.src_dirs, source_result.status, source_result.message)

        # Commit changes in parent_src_dir_cfg
        git_commit_src_dir(source_result.new_repos, source_result.updated_repos)
        
        # Build all sources
        failed_src, error, errMsg = build_all_sources(sorted_repos)
        if error:
            return BuildResult(failed_src, error, errMsg)
        
        return BuildResult(source_result.src_dirs, 0, "")
    except ValueError as e:
        print(e, flush=True)
        return BuildResult("", 1, str(e))


"""
Main function to parse arguments and execute the build process.

This function parses command-line arguments, sets up global configuration
variables, and executes the appropriate actions based on the provided options
(get-only, build-only, or full process).

Command-line arguments:
--parent-src-dir: Path to ODBC dependencies directory
--install-dir: Path to install dependencies
--get-only: Only get sources and commit
--get-all:  Get every source regardless of the platform
--build-only: Only build existing sources
--build-type: Build type (Release or Debug, Windows only)

Returns:
None
"""
def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Build dependencies")
    parser.add_argument(
        "--parent-src-dir",
        default=os.path.join(os.getcwd(), "src", "tp"),
        help="Path to ODBC dependencies directory",
    )
    parser.add_argument("--build-dir", default=None, help="Path to ODBC dependencies directory")
    parser.add_argument("--install-dir", default=None, help="Subdirectory prefix to each ODBC dependencu build directory")
    parser.add_argument("--extract-zip", default=False, help="Extract retrieved zip files")
    
    # Add mutually exclusive group for operation mode
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument("--get-only", action="store_true", help="Only get sources and commit")
    parser.add_argument("--get-all", action="store_true", help="Get every source regardless of the platform")
    mode_group.add_argument("--build-only", action="store_true", help="Only build existing sources")
    
    # Only add build-type argument if on Windows
    if platform.system() == "Windows":
        parser.add_argument(
            "--build-type",
            choices=["Release", "Debug"],
            help="Build type (Release or Debug)",
        )

    args = parser.parse_args()

    # Validate arguments
    if args.get_only and hasattr(args, 'build_type') and args.build_type is not None:
        parser.error("--build-type should not be specified with --get-only")

    g_config.parent_src_dir_cfg = args.parent_src_dir
    # Set the default install directory to be the build directory + "/install"
    if args.install_dir is None:
        g_config.install_dir_cfg = os.path.join(g_config.parent_src_dir_cfg, "install")
    else:
        g_config.install_dir_cfg = args.install_dir

    # Set build type, just in case
    g_config.build_type_cfg = args.build_type if hasattr(args, 'build_type') and args.build_type is not None else "Release"
    
    g_config.build_dir_cfg = args.build_dir

    sorted_repos = topological_sort(repo_info)
    print(f"sorted order: {sorted_repos}", flush=True)
    if args.get_all:
        g_config.get_all_platforms_sources_cfg = True
    g_config.extract_zip_sources_cfg = args.extract_zip
    if args.get_only:
        # Only get sources and commit
        source_result = get_all_sources(list(Libs) if g_config.get_all_platforms_sources_cfg is True else sorted_repos)
        if source_result.status == 0:
            git_commit_src_dir(source_result.new_repos, source_result.updated_repos)
            print("Sources retrieved and committed successfully.", flush=True)
        else:
            print(f"An error occurred while getting sources: {source_result.message}", file=sys.stderr, flush=True)
            sys.exit(1)
    elif args.build_only:
        # Only build existing sources
        failed_src, error, errMsg = build_all_sources(sorted_repos)
        if error != 1:
            print("Sources built successfully.", flush=True)
        else:
            print(f"An error occurred while building {failed_src}: {errMsg}", file=sys.stderr, flush=True)
            sys.exit(1)
    else:
        # Get and build all sources
        build_result = get_and_build_all(sorted_repos)
        if build_result.status == 0:
            print("All sources retrieved, committed, and built successfully.", flush=True)
        else:
            print(f"An error occurred: {build_result.message}", file=sys.stderr, flush=True)
            sys.exit(1)

if __name__ == "__main__":
    main()
