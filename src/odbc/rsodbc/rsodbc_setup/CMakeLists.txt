# Make the version file
configure_file(version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h @ONLY)
add_library(rsodbcsetup SHARED setup.c rsodbcsetup.def rsodbcsetup.rc)

target_compile_definitions(
  rsodbcsetup
  PRIVATE NDEBUG
          WIN32
          _WINDOWS
          _USRDLL
          ODBCVER=0x0352
          WIN_MULTITHREAD_SUPPORT
          DYNAMIC_LOAD
          MULTIBYTE
          UNICODE_SUPPORT
          _CRT_SECURE_NO_DEPRECATE)

target_link_libraries(
  rsodbcsetup
  PRIVATE kernel32
          user32
          gdi32
          advapi32
          odbc32
          odbccp32
          wsock32
          ws2_32
          winspool
          comdlg32
          shell32
          ole32
          oleaut32
          uuid
          winmm
          comctl32
          libcrypto
          legacy_stdio_definitions)

include_and_link_openssl(rsodbcsetup)


# Set the output directories for the target
set_target_properties(
  rsodbcsetup
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
             ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")

# Get the expanded value of RUNTIME_OUTPUT_DIRECTORY for rsodbcsetup
get_target_property(RUNTIME_OUTPUT_DIR rsodbcsetup RUNTIME_OUTPUT_DIRECTORY)

# Print the value
message(STATUS "rsodbcsetup RUNTIME_OUTPUT_DIRECTORY: ${RUNTIME_OUTPUT_DIR}")

install(TARGETS rsodbcsetup DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
