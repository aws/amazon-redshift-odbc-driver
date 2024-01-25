# Make the version file
configure_file(rsversion.h.in ${CMAKE_CURRENT_SOURCE_DIR}/rsversion.h @ONLY)
# Bring files from various sources libpq subfolders here and build them.
set(RSODBC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#Relative paths for unclusion
set(IAM_DIR ${RSODBC_SRC_DIR}/iam)
set(IAM_CORE_DIR ${RSODBC_SRC_DIR}/iam/core)
set(IAM_PLUGINS_DIR ${RSODBC_SRC_DIR}/iam/plugins)
set(IAM_HTTP_DIR ${RSODBC_SRC_DIR}/iam/http)
file(GLOB C_SOURCES ${RSODBC_SRC_DIR}/*.c)
file(GLOB SOURCES 
        ${RSODBC_SRC_DIR}/*.c
        ${RSODBC_SRC_DIR}/*.cpp
        ${IAM_DIR}/*.cpp
        ${IAM_CORE_DIR}/*.cpp
        ${IAM_HTTP_DIR}/*.cpp
        ${IAM_PLUGINS_DIR}/*.cpp
)
set_source_files_properties(${C_SOURCES} PROPERTIES LANGUAGE CXX )
# Create the library target
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
add_library(rsodbc64 SHARED ${SOURCES})

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

set (PACKAGES 
    ssl aws-cpp-sdk-redshift aws-cpp-sdk-redshift-serverless aws-cpp-sdk-sts 
    aws-cpp-sdk-core aws-crt-cpp aws-c-s3 aws-c-auth aws-c-event-stream
    aws-c-http aws-c-mqtt aws-c-io aws-c-cal aws-checksums aws-c-compression
    aws-c-common s2n aws-c-sdkutils curl ssl crypto nghttp2 cares z
    krb5 gssapi_krb5 k5crypto krb5support com_err
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(REMOVE_ITEM PACKAGES s2n)
endif()

#only static libs
foreach (PACKAGE ${PACKAGES})
    foreach(LIB_DIR ${CMAKE_LIBRARY_PATH})
        set(LIB_FILE_PATH ${LIB_DIR}/lib${PACKAGE}${CMAKE_FIND_LIBRARY_SUFFIXES})
        if (EXISTS ${LIB_FILE_PATH})
            message("STATIC LIB lib${PACKAGE}${CMAKE_FIND_LIBRARY_SUFFIXES}")
            list(APPEND STATIC_LIBS "${LIB_FILE_PATH}")
            set(${PACKAGE}_FOUND TRUE)
            break()
        endif()
    endforeach()
    if (NOT ${PACKAGE}_FOUND)
        message("NOT FOUND STATIC LIB lib${PACKAGE}${CMAKE_FIND_LIBRARY_SUFFIXES}")
    endif()
endforeach()

# Set include directories for the library

if(OPENSSL_DIR)
    target_include_directories(rsodbc64 PRIVATE ${OPENSSL_DIR}/include)
endif()

target_include_directories(rsodbc64 PRIVATE 
    ${RSLOG_SRC_DIR}
    ${RSODBC_SRC_DIR} 
    ${IAM_DIR}  
    ${IAM_CORE_DIR} 
    ${IAM_PLUGINS_DIR} 
    ${IAM_HTTP_DIR} 
    ${PGCLIENT_SRC_INCLUDE_DIR} 
    ${PGCLIENT_SRC_IFACE_LIBPQ_DIR}
    ${CMAKE_INCLUDE_PATH}
)

target_compile_options_rsodbc()
target_compile_definitions_rsodbc()
target_link_options_rsodbc()
target_link_libraries_rsodbc()

# Set the output directory for the library
set_target_properties(rsodbc64 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(rsodbcsql)
add_subdirectory(samples/connect)

install(TARGETS rsodbc64 DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")