# Make the version file
set(TARGET_NAME "rsodbcsetup")
configure_file(version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h @ONLY)
add_library(${TARGET_NAME} SHARED setup.c rsodbcsetup.def rsodbcsetup.rc)

target_compile_definitions(
  ${TARGET_NAME}
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
  ${TARGET_NAME}
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


install(TARGETS ${TARGET_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}")

if ( WIN32 )
    message(STARTUS "Targeting${CMAKE_CURRENT_BINARY_DIR}")
    set_output_directory(${TARGET_NAME})                    
endif (  )
