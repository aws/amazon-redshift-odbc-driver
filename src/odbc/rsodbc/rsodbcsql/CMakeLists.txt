file(GLOB SOURCES rsodbcsql.c)
set_source_files_properties(${C_SOURCES} PROPERTIES LANGUAGE CXX )

add_executable(rsodbcsql ${SOURCES})

target_link_libraries(rsodbcsql ${odbc_manager_lib_list})

if (ODBC_DIR)
    target_include_directories(rsodbcsql PRIVATE ${ODBC_DIR}/include)
    target_link_directories(rsodbcsql PRIVATE ${ODBC_DIR}/lib)
endif()

target_include_directories(rsodbcsql PRIVATE ${CMAKE_INCLUDE_PATH})
target_link_directories(rsodbcsql PRIVATE ${CMAKE_LIBRARY_PATH})

target_compile_options(rsodbcsql PRIVATE 
-ggdb -gdwarf-3 -g -DLINUX -O2 -Wall -c -fmessage-length=0 -MMD -MP)

install(TARGETS rsodbcsql DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")