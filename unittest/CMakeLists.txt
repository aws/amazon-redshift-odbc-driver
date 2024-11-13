# Locate GTest
find_gtest()

# Add all test files in the current directory to a variable
file(GLOB TEST_SOURCES *.cpp)
configure_file(${CMAKE_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt COPYONLY)

# Add an executable for all the tests
add_executable(all_unit_tests ${TEST_SOURCES})

if (WIN32)
    target_link_libraries(all_unit_tests ${GTEST_LIBRARIES} ${CMAKE_FIND_LIBRARY_PREFIXES}rsodbc64-test)
    add_custom_command(TARGET all_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:rsodbc64-test>  # Adjust this if necessary
            $<TARGET_FILE_DIR:all_unit_tests>
    )
    else()
    target_link_libraries(all_unit_tests ${GTEST_LIBRARIES} rsodbc64-test)
    target_link_libraries(all_unit_tests pthread)
endif()

target_link_directories(all_unit_tests PRIVATE "${CMAKE_INSTALL_PREFIX}/lib")
target_link_directories(all_unit_tests PRIVATE ${CMAKE_LIBRARY_PATH})
target_include_directories(all_unit_tests PRIVATE ${CMAKE_INCLUDE_PATH})
target_include_directories(all_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/logging)
target_include_directories(all_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc)
target_include_directories(all_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/interfaces/libpq)
target_include_directories(all_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/include)
target_compile_definitions_rsodbc(all_unit_tests)

add_test(NAME all_unit_tests COMMAND all_unit_tests)


