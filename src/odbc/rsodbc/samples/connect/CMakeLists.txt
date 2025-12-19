set(EXECUTABLE_NAME "connect")
file(GLOB SOURCES connect.c)
set_source_files_properties(${C_SOURCES} PROPERTIES LANGUAGE CXX )

add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
target_link_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
include_and_link_odbc(${EXECUTABLE_NAME})
target_link_libraries(${EXECUTABLE_NAME} ${test_odbc_manager_lib_list})
if (NOT WIN32)
    target_link_libraries(${EXECUTABLE_NAME} c)

    target_compile_options(${EXECUTABLE_NAME} PRIVATE 
    -DODBCVER=0x0350 -DODBC64 -DLINUX -g)
else()
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE 
    ODBCVER=0x0350 ODBC64 WIN32 )
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}")

if ( WIN32 )
    message(STATUS "Targeting ${CMAKE_CURRENT_BINARY_DIR}")
    set_output_directory(${EXECUTABLE_NAME})                    
endif (  )