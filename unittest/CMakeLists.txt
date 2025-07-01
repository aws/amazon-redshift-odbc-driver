set(EXECUTABLE_NAME "all_unit_tests")
set(TEST_NAME "test_${EXECUTABLE_NAME}")
# Locate GTest
find_gtest()

# Add all test files in the current directory to a variable
file(GLOB TEST_SOURCES *.cpp)
configure_file(${CMAKE_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt COPYONLY)

# Add an executable for all the tests
add_executable(${EXECUTABLE_NAME} ${TEST_SOURCES})

if (WIN32)
    target_link_libraries(${EXECUTABLE_NAME} ${GTEST_LIBRARIES} ${CMAKE_FIND_LIBRARY_PREFIXES}rsodbc64-test)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:rsodbc64-test>  # Adjust this if necessary
            ${CMAKE_CURRENT_BINARY_DIR}
    )
    else()
    target_link_libraries(${EXECUTABLE_NAME} ${GTEST_LIBRARIES} rsodbc64-test)
    target_link_libraries(${EXECUTABLE_NAME} pthread)
endif()

target_link_directories(${EXECUTABLE_NAME} PRIVATE "${CMAKE_INSTALL_PREFIX}/lib")
target_link_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/logging)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/interfaces/libpq)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/include)
target_compile_definitions_rsodbc(${EXECUTABLE_NAME})

add_test(NAME ${TEST_NAME} COMMAND "${EXECUTABLE_NAME}")

post_build_copy_dependency_files(${EXECUTABLE_NAME} ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/tests/unittest")
install(TARGETS rsodbc64-test DESTINATION "${CMAKE_INSTALL_PREFIX}/tests/unittest")
