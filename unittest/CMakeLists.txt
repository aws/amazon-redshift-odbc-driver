set(EXECUTABLE_NAME "all_unit_tests")
set(TEST_NAME "test_${EXECUTABLE_NAME}")
# Locate GTest
find_gtest()

# Add all test files in the current directory to a variable
file(GLOB TEST_SOURCES *.cpp)
configure_file(${CMAKE_SOURCE_DIR}/version.txt ${CMAKE_CURRENT_BINARY_DIR}/version.txt COPYONLY)

# Add an executable for all the tests
add_executable(${EXECUTABLE_NAME} ${TEST_SOURCES})

# Tests find the built-in root.crt next to the module that calls getDriverPath()
# (rsodbc64-test.dll on Windows), so copy it there.
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc/root.crt
        $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>)

# GoogleTest expands each TEST/TEST_F into its own set of COFF sections, and the
# larger suites here (rsutil_test.cpp is several thousand tests) exceed the
# default 65,279-section limit of the object file format, which MSVC reports as
# "error C1128: number of sections exceeded object file format limit".
# /bigobj raises that limit. It only affects the object file encoding, not code
# generation, and is MSVC-only.
if (MSVC)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /bigobj)
endif()

if (WIN32)
    target_link_libraries(${EXECUTABLE_NAME} ${GTEST_LIBRARIES} ${CMAKE_FIND_LIBRARY_PREFIXES}rsodbc64-test ${RS_STATIC_LIBS} crypto ssl)
    # SslModeDsnTest writes registry DSNs (odbccp32 needs legacy_stdio_definitions);
    # the fake TLS endpoint opens loopback sockets (ws2_32).
    target_link_libraries(${EXECUTABLE_NAME} odbccp32 legacy_stdio_definitions ws2_32)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/include/port/win32)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/include/port/win32_msvc)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/port)
    # getDriverPath() returns this DLL's directory, so copy the DLL beside the exe
    # like root.crt above.
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:rsodbc64-test>
            $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
    )
    # ConfigDsnPersistenceTest loads rsodbcsetup.dll from beside the exe, so copy it
    # there. The target exists only on WIN32, hence the guard.
    add_dependencies(${EXECUTABLE_NAME} rsodbcsetup)
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:rsodbcsetup>
            $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
    )
    else()
    target_link_libraries(${EXECUTABLE_NAME} ${GTEST_LIBRARIES} rsodbc64-test pq ${RS_STATIC_LIBS})
    if(NOT APPLE)
        # AWS-LC is statically linked; its transitive system deps must be explicit.
        target_link_libraries(${EXECUTABLE_NAME} pthread dl rt resolv keyutils)
    endif()
endif()

target_link_directories(${EXECUTABLE_NAME} PRIVATE "${CMAKE_INSTALL_PREFIX}")
target_link_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/logging)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc/iam)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc/iam/core)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc/iam/plugins)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc/iam/http)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/interfaces/libpq)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/pgclient/src/include)
target_compile_definitions_rsodbc(${EXECUTABLE_NAME})
include_and_link_odbc(${EXECUTABLE_NAME})
add_test(NAME ${TEST_NAME} COMMAND "${EXECUTABLE_NAME}")

post_build_copy_dependency_files(${EXECUTABLE_NAME} ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/tests/unittest")
install(TARGETS rsodbc64-test DESTINATION "${CMAKE_INSTALL_PREFIX}/tests/unittest")
if (WIN32)
    # ConfigDsnPersistenceTest needs rsodbcsetup.dll beside the installed tests too.
    # The target exists only on WIN32, hence the guard.
    install(TARGETS rsodbcsetup DESTINATION "${CMAKE_INSTALL_PREFIX}/tests/unittest")
endif()
# Installed tests need root.crt beside the binary too.
install(FILES ${CMAKE_SOURCE_DIR}/src/odbc/rsodbc/root.crt
    DESTINATION "${CMAKE_INSTALL_PREFIX}/tests/unittest")
