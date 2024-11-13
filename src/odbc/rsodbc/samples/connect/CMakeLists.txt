file(GLOB SOURCES connect.c)
set_source_files_properties(${C_SOURCES} PROPERTIES LANGUAGE CXX )

add_executable(connect64 ${SOURCES})

if (RS_ODBC_DIR)
    target_include_directories(connect64 PRIVATE ${RS_ODBC_DIR}/include)
    target_link_directories(connect64 PRIVATE ${RS_ODBC_DIR}/lib)
endif()

target_include_directories(connect64 PRIVATE ${CMAKE_INCLUDE_PATH})
target_link_directories(connect64 PRIVATE ${CMAKE_LIBRARY_PATH})
target_link_libraries(connect64 ${odbc_manager_lib_list})
if (NOT WIN32)
    target_link_libraries(connect64 c)

    target_compile_options(connect64 PRIVATE 
    -DODBCVER=0x0350 -DODBC64 -DLINUX -g)
else()
target_compile_definitions(connect64 PRIVATE 
    ODBCVER=0x0350 ODBC64 WIN32 )
endif()

install(TARGETS connect64 DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")