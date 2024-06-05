# Locate GTest
# Attempt to find GTest using find_package

# Look for libgtest.a in CMAKE_LIBRARY_PATH
find_library(GTEST_LIBRARY
    NAMES gtest
    PATHS ${CMAKE_LIBRARY_PATH}
)

# Check if libgtest.a is found
if (GTEST_LIBRARY)
    message(STATUS "Found GTest library: ${GTEST_LIBRARY}")
    # Set variables expected by GTest
    set(GTEST_LIBRARIES ${GTEST_LIBRARY})
    # Set the include directories where GTest headers are located
    get_filename_component(GTEST_INCLUDE_DIR "${GTEST_LIBRARY}" DIRECTORY)
    set(GTEST_INCLUDE_DIRS "${GTEST_INCLUDE_DIR}/../include")
    message(STATUS "Found GTest include folder: ${GTEST_INCLUDE_DIRS}")
else()
    message(STATUS "GTest library not found, using find_package!")
    # Search the standard way and fail the standard way
    find_package(GTest REQUIRED)
endif()

include_directories(${GTEST_INCLUDE_DIRS})

# Add all test files in the current directory to a variable
file(GLOB TEST_SOURCES *.cpp)
configure_file(${CMAKE_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt COPYONLY)

# Add an executable for all the tests
add_executable(all_unit_tests ${TEST_SOURCES})

# Link against GTest and your_library
target_link_libraries(all_unit_tests ${GTEST_LIBRARIES} pthread rsodbc64-test)
target_include_directories(all_unit_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/logging)
target_include_directories(all_unit_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc)
target_include_directories(all_unit_tests PUBLIC ${CMAKE_SOURCE_DIR}/src/pgclient/src/interfaces/libpq)
target_compile_definitions_rsodbc(all_unit_tests)

add_test(NAME all_unit_tests COMMAND all_unit_tests)


