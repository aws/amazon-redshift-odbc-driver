set(EXECUTABLE_NAME "rsodbcsql")
file(GLOB SOURCES ${EXECUTABLE_NAME}.c)
set_source_files_properties(${SOURCES} PROPERTIES LANGUAGE CXX )

add_executable(${EXECUTABLE_NAME} ${SOURCES})

target_link_libraries(${EXECUTABLE_NAME} ${odbc_manager_lib_list})

if (RS_ODBC_DIR)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${RS_ODBC_DIR}/include)
    target_link_directories(${EXECUTABLE_NAME} PRIVATE ${RS_ODBC_DIR}/lib)
endif()

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
target_link_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})

if (WIN32)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE WIN32)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE 
    -ggdb -gdwarf-3 -g -DLINUX -O2 -Wall -c -fmessage-length=0)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

add_custom_target(
    ${EXECUTABLE_NAME}_BINARY
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:${EXECUTABLE_NAME}>"
    VERBATIM
    )

if ( WIN32 )
    message(STARTUS "Targeting${CMAKE_CURRENT_BINARY_DIR}")
    set_output_directory(${EXECUTABLE_NAME})                    
endif (  )