# Step-1: Make the version file
configure_file(rsversion.h.in ${CMAKE_CURRENT_SOURCE_DIR}/rsversion.h @ONLY)

# Step-2: Collect the files into sources
set(RSODBC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

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

list(REMOVE_ITEM ${SOURCES} ${RSODBC_SRC_DIR}/dllmain.c)
set_source_files_properties(${C_SOURCES} PROPERTIES LANGUAGE CXX )

# Step-3: Handle dependencies.
# look for static libs: .a or .lib
set_deps_library_suffix()
set (PACKAGES 
    ssl aws-cpp-sdk-redshift aws-cpp-sdk-redshift-serverless aws-cpp-sdk-sso-oidc
    aws-cpp-sdk-sts aws-cpp-sdk-core aws-crt-cpp aws-c-s3 aws-c-auth aws-c-event-stream
    aws-c-http aws-c-mqtt aws-c-io aws-c-cal aws-checksums aws-c-compression
    aws-c-common s2n aws-c-sdkutils curl ssl crypto nghttp2 cares z
    krb5 gssapi_krb5 k5crypto krb5support com_err
)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(REMOVE_ITEM PACKAGES s2n)
endif()

foreach (PACKAGE ${PACKAGES})
    foreach(LIB_DIR ${CMAKE_LIBRARY_PATH})
        set(LIB_FILE_PATH "${LIB_DIR}/lib${PACKAGE}${CMAKE_FIND_LIBRARY_SUFFIXES}")
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

# Helper function
function(basic_settings TARGET_NAME)
    if(OPENSSL_DIR)
        target_include_directories(${TARGET_NAME} PRIVATE ${OPENSSL_DIR}/include)
    endif()

    target_include_directories(${TARGET_NAME} PRIVATE 
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
    target_compile_options_rsodbc(${TARGET_NAME})
    target_compile_definitions_rsodbc(${TARGET_NAME})
    target_link_options_rsodbc(${TARGET_NAME})
endfunction()

# Step-4: Build only Object files
add_library(objects OBJECT ${SOURCES})
basic_settings(objects)


# Step-5:Build shared libraris one is the main lib and the other one used for testing
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
# Building main library and test library (without export list)
set(RSODBC_TARGETS rsodbc64)
if (ENABLE_TEST_BUILD)
    list(APPEND RSODBC_TARGETS rsodbc64-test)
endif()
foreach(TARGET_NAME ${RSODBC_TARGETS})
    add_library(${TARGET_NAME} SHARED ${RSODBC_SRC_DIR}/dllmain.c $<TARGET_OBJECTS:objects>)
    basic_settings(${TARGET_NAME})
    target_link_libraries_rsodbc(${TARGET_NAME})
    if (TARGET_NAME STREQUAL "rsodbc64") #skip test library
        export_signatures(${TARGET_NAME})
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

    install(TARGETS ${TARGET_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
endforeach()

add_subdirectory(rsodbcsql)
add_subdirectory(samples/connect)