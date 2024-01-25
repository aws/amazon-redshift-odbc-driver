# How to build: refer to BUILD.CMAKE.md

# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.12)
option(ODBC_VERSION "Specify version with format: MAJOR.MINOR.PATCH SVN" "")
option(ENABLE_TESTING "Enable tests" OFF)
option(ENABLE_TEST_BUILD "Enable building test directories, if available." ON)

if (NOT "$ENV{ENABLE_TESTING}" STREQUAL "")
    set(ENABLE_TESTING "$ENV{ENABLE_TESTING}" CACHE INTERNAL "Copied from environment variable")
endif()


message(STATUS "ENABLE_TESTING=${ENABLE_TESTING}")
if(ENABLE_TESTING)
    enable_testing()
endif()

# Set the project name and language
project(rsodbc LANGUAGES C CXX)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE ON)

message (STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

include(cmake/Common.cmake)
# Include OS specific cmake files
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND EXISTS "${CMAKE_SOURCE_DIR}/cmake/Darwin.cmake")
    include(cmake/Darwin.cmake)
    set_default_paths()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(cmake/Linux.cmake)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# ODBC Version
if (NOT ODBC_VERSION OR ODBC_VERSION  STREQUAL "")
    message(STATUS "Reading ODBC_VERSION from file...")
    file(STRINGS version.txt ODBC_VERSION_TXT LIMIT_COUNT 1)
    string(REPLACE " " "." ODBC_VERSION ${ODBC_VERSION_TXT})
endif()
message(STATUS "ODBC_VERSION: ${ODBC_VERSION}")
string(REPLACE "." ";" ODBC_VERSION ${ODBC_VERSION})
list(GET ODBC_VERSION 0 ODBC_VERSION_MAJOR)
list(GET ODBC_VERSION 1 ODBC_VERSION_MINOR)
list(GET ODBC_VERSION 2 ODBC_VERSION_PATCH)
list(GET ODBC_VERSION 3 ODBC_VERSION_SVN)

# Process DEPS_DIRS. These directories each are assumed to have inlude&lib folders.
# For more information refer to BUILD.CMAKE.md
if (DEPS_DIRS)
    FOREACH(DEP_DIR ${DEPS_DIRS})
        IF(IS_DIRECTORY ${DEP_DIR}/include)
            list(APPEND CMAKE_INCLUDE_PATH ${DEP_DIR}/include)
        ENDIF()
        IF(IS_DIRECTORY ${DEP_DIR}/lib)
            list(APPEND CMAKE_LIBRARY_PATH ${DEP_DIR}/lib)
        ENDIF()
        IF(IS_DIRECTORY ${DEP_DIR}/lib64)
            list(APPEND CMAKE_LIBRARY_PATH ${DEP_DIR}/lib64)
        ENDIF()
    ENDFOREACH()
endif()

# Process MULTI_DEPS_DIRS. This is assumed to be one or more parent folder(s) which
# contain subfolders of all dependencies each of which have their own inlude&lib.
# For more information refer to BUILD.CMAKE.md
if (MULTI_DEPS_DIRS)
    message(STATUS MULTI_DEPS_DIRS=${MULTI_DEPS_DIRS})
    FOREACH(child ${MULTI_DEPS_DIRS})
        SUBDIRLIST(SUB_DIRS ${child})
        message(STATUS "sub-dirs of ${child}=${SUB_DIRS}")
        foreach(DEP_DIR ${SUB_DIRS})
            if(IS_DIRECTORY ${child}/${DEP_DIR}/include)
                list(INSERT CMAKE_INCLUDE_PATH 0 ${MULTI_DEPS_DIRS}/${DEP_DIR}/include)
            endif()
            if(IS_DIRECTORY ${child}/${DEP_DIR}/lib)
                list(INSERT CMAKE_LIBRARY_PATH 0 ${MULTI_DEPS_DIRS}/${DEP_DIR}/lib)
            endif()
            if(IS_DIRECTORY ${child}/${DEP_DIR}/lib64)
                list(INSERT CMAKE_LIBRARY_PATH 0 ${MULTI_DEPS_DIRS}/${DEP_DIR}/lib64)
            endif()
        endforeach()
    ENDFOREACH()
else()
    message(STATUS "NO MULTI_DEPS_DIRS")
endif()

# Process ODBC_DIR and OPENSSL_DIR. They are assigned optional variables in order to 
# manage path search order in systems that usually have their own versions of these
# libraries preinstalled.
if (ODBC_DIR)
    if(IS_DIRECTORY ${ODBC_DIR}/include)
        list(INSERT CMAKE_INCLUDE_PATH 0 ${ODBC_DIR}/include)
    endif()
    if(IS_DIRECTORY ${ODBC_DIR}/lib)
        list(INSERT CMAKE_LIBRARY_PATH 0 ${ODBC_DIR}/lib)
    endif()
endif()

if (OPENSSL_DIR)
    if(IS_DIRECTORY ${OPENSSL_DIR}/include)
        list(INSERT CMAKE_INCLUDE_PATH 0 ${OPENSSL_DIR}/include)
    endif()
    if(IS_DIRECTORY ${OPENSSL_DIR}/lib)
        list(INSERT CMAKE_LIBRARY_PATH 0 ${OPENSSL_DIR}/lib)
    endif()
endif()

list(REMOVE_DUPLICATES CMAKE_INCLUDE_PATH)
list(REMOVE_DUPLICATES CMAKE_LIBRARY_PATH)
message("CMAKE_INCLUDE_PATH=${CMAKE_INCLUDE_PATH}")
message("CMAKE_LIBRARY_PATH=${CMAKE_LIBRARY_PATH}")

#Some patforms require supporting libraries(like iconv) beside just odbc
set (odbc_target_libraries "")
get_odbc_target_libraries(odbc_manager_lib_list)
message(STATUS "odbc_manager_lib_list = ${odbc_manager_lib_list}")


# variables needed in various subfolders including pgclient and rsodbc
set(PGCLIENT_SRC_DIR ${CMAKE_SOURCE_DIR}/src/pgclient/src)
set(PGCLIENT_SRC_INCLUDE_DIR ${PGCLIENT_SRC_DIR}/include)
set(PGCLIENT_SRC_IFACE_LIBPQ_DIR ${CMAKE_SOURCE_DIR} ${PGCLIENT_SRC_DIR}/interfaces/libpq)
set(PGCLIENT_SRC_PORT_DIR ${CMAKE_SOURCE_DIR} ${PGCLIENT_SRC_DIR}/port)

include_directories(src/logging)

add_subdirectory(src/logging)
add_subdirectory(src/pgclient/src)
add_subdirectory(src/odbc/rsodbc)
if(ENABLE_TEST_BUILD AND EXISTS "${CMAKE_SOURCE_DIR}/cmake/Test.cmake")
    include(${CMAKE_SOURCE_DIR}/cmake/Test.cmake)
    add_test_subdirectories()
elseif (ENABLE_TESTING)
    message(WARNING "Test files are not available.")
endif()
