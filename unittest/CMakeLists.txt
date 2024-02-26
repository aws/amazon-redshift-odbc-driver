# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Add all test files in the current directory to a variable
file(GLOB TEST_SOURCES *.cpp)
configure_file(${CMAKE_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt COPYONLY)

# Add an executable for all the tests
add_executable(all_unit_tests ${TEST_SOURCES})

# Link against GTest and your_library
target_link_libraries(all_unit_tests ${GTEST_LIBRARIES} pthread rsodbc64-test)
target_include_directories(all_unit_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc)
target_include_directories(all_unit_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/pgclient/src/interfaces/libpq)
target_compile_definitions_rsodbc(all_unit_tests)

gtest_discover_tests(all_unit_tests)


