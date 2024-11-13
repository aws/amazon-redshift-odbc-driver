file(GLOB SOURCES rsodbcsql.c)
set_source_files_properties(${SOURCES} PROPERTIES LANGUAGE CXX )

add_executable(rsodbcsql ${SOURCES})

target_link_libraries(rsodbcsql ${odbc_manager_lib_list})

if (RS_ODBC_DIR)
    target_include_directories(rsodbcsql PRIVATE ${RS_ODBC_DIR}/include)
    target_link_directories(rsodbcsql PRIVATE ${RS_ODBC_DIR}/lib)
endif()

target_include_directories(rsodbcsql PRIVATE ${CMAKE_INCLUDE_PATH})
target_link_directories(rsodbcsql PRIVATE ${CMAKE_LIBRARY_PATH})

if (WIN32)
    target_compile_definitions(rsodbcsql PRIVATE WIN32)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin")
    target_compile_options(rsodbcsql PRIVATE 
    -ggdb -gdwarf-3 -g -DLINUX -O2 -Wall -c -fmessage-length=0)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

install(TARGETS rsodbcsql DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
set_target_properties(
    rsodbcsql
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
             ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(
    RSODBCSQL_BINARY
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:rsodbcsql>"
    VERBATIM
    )