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
        ${RSLOG_SRC_DIR}/*.cpp
        rsodbc.rc
)

list(REMOVE_ITEM SOURCES "${RSODBC_SRC_DIR}/dllmain.c")
set_source_files_properties(${C_SOURCES} PROPERTIES LANGUAGE CXX )

# Helper function
function(basic_settings TARGET_NAME)
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
    target_link_directories(${TARGET_NAME} PRIVATE  ${CMAKE_LIBRARY_PATH})
    target_compile_options_rsodbc(${TARGET_NAME})
    target_compile_definitions_rsodbc(${TARGET_NAME})
    target_link_options_rsodbc(${TARGET_NAME})
endfunction()

# Step-4: Build only Object files
add_library(objects OBJECT ${SOURCES})
basic_settings(objects)

list(APPEND RSODBC_TARGETS objects_static)
foreach(TARGET_NAME_ ${RSODBC_TARGETS})
    set(TARGET_NAME ${TARGET_NAME_})
    add_library(${TARGET_NAME} STATIC $<TARGET_OBJECTS:objects>)
    basic_settings(${TARGET_NAME})
    target_link_libraries_rsodbc(${TARGET_NAME})
    install(TARGETS ${TARGET_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
endforeach()
set(RSODBC_TARGETS "")
# Step-5:Build shared libraris one is the main lib and the other one used for testing

# Building main library and test library (without export list)
list(APPEND RSODBC_TARGETS rsodbc64)
if (ENABLE_TEST_BUILD)
    list(APPEND RSODBC_TARGETS rsodbc64-test)
endif()
foreach(TARGET_NAME_ ${RSODBC_TARGETS})
    set(TARGET_NAME ${TARGET_NAME_})
    add_library(${TARGET_NAME} SHARED ${RSODBC_SRC_DIR}/dllmain.c $<TARGET_OBJECTS:objects>)
    basic_settings(${TARGET_NAME})
    target_link_libraries_rsodbc(${TARGET_NAME})
    export_signatures(${TARGET_NAME})
    if ( WIN32 )
        message(STARTUS "Targeting${CMAKE_CURRENT_BINARY_DIR}")
        set_output_directory(${TARGET_NAME})                    
    endif (  )
    install(TARGETS ${TARGET_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
endforeach()


add_subdirectory(rsodbcsql)
add_subdirectory(samples/connect)

if (WIN32)
    add_subdirectory(rsodbc_setup)
endif()