function(configure_file_pgclient)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config.h.win32
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config.h)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config_os.h.win32
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config_os.h)
endfunction()

function(include_directories_libpq)
  include_directories(${PGCLIENT_SRC_DIR}/include)
  include_directories(${PGCLIENT_SRC_DIR}/include/port/win32)
  include_directories(${PGCLIENT_SRC_DIR}/include/port/win32_msvc)
  include_directories(${PGCLIENT_SRC_DIR}/port)
  include_directories(${CMAKE_INCLUDE_PATH}) # kfw
endfunction()

function(include_directories_libpqport)
  include_directories(${PGCLIENT_SRC_DIR}/include)
  include_directories(${PGCLIENT_SRC_DIR}/include/port/win32)
  include_directories(${PGCLIENT_SRC_DIR}/include/port/win32_msvc)
endfunction()

function(add_libpqport_sources)
  list(
    APPEND
    LIBPQ_PORT_SOURCE_NAMES
    chklocale.c
    inet_net_ntop.c
    noblock.c
    pgsleep.c
    pgstrcasecmp.c
    thread.c)
  foreach(FILE ${LIBPQ_PORT_SOURCE_NAMES})
    list(APPEND LIBPQ_PORT_SOURCES "${PGCLIENT_SRC_DIR}/port/${FILE}")
  endforeach()
endfunction()

function(target_compile_options_pq)
  target_compile_options(
    pq
    PRIVATE /nologo
            /W3
            /WX-
            /diagnostics:column
            /EHsc
            /GS
            /Zc:wchar_t
            /Zc:forScope
            /Zc:inline
            /wd4018
            /wd4244
            /wd4273
            /wd4102
            /wd4090
            /wd4267
            /FC
            /errorReport:queue
            /MP)

  if(RS_BUILD_TYPE STREQUAL "Release")
    target_compile_options(pq PRIVATE /GF /fp:fast)
  endif()

  if(RS_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(pq PRIVATE /Zi /Gm- /fp:precise)
  endif()
endfunction()

function(target_compile_definitions_pq)
  target_compile_definitions(
    pq
    PRIVATE WIN32
            _WINDOWS
            __WINDOWS__
            __WIN32__
            EXEC_BACKEND
            WIN32_STACK_RLIMIT=4194304
            _CRT_SECURE_NO_DEPRECATE
            _CRT_NONSTDC_NO_DEPRECATE
            FRONTEND
            UNSAFE_STAT_OK
            _WIN64
            USE_SSL
            ENABLE_GSS
            ENABLE_SSPI
            _MBCS)
endfunction()

function(target_compile_options_pgport)
  target_compile_options(
    pqport
    PRIVATE /nologo
            /W3
            /WX-
            /diagnostics:column
            /EHsc
            /GS
            /Zc:wchar_t
            /Zc:forScope
            /Zc:inline
            /wd4018
            /wd4244
            /wd4273
            /wd4102
            /wd4090
            /wd4267
            /FC
            /errorReport:queue
            /MP)

  if(RS_BUILD_TYPE STREQUAL "Release")
    target_compile_options(pqport PRIVATE /GF /fp:fast)
  endif()

  if(RS_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(pqport PRIVATE /Zi /Gm- /fp:precise)
  endif()
endfunction()

function(target_link_libraries_rsodbc TARGET_NAME)
  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC ${RS_STATIC_LIBS}
           shell32
           odbccp32
           libcrypto
           libssl
           ws2_32
           comdlg32
           wldap32
           Ncrypt
           User32
           UserEnv
           bcrypt
           RpcRT4
           winhttp
           WinInet
           Version
           legacy_stdio_definitions
           wsock32
           secur32
           crypt32
           shlwapi
           kernel32
           user32
           gdi32
           winspool
           comdlg32
           advapi32
           shell32
           ole32
           oleaut32
           uuid
           odbc32
           odbccp32
           pq
           pqport
    # rslog
  )
  target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
  target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_SYSTEM_LIBRARY_PATH})
endfunction()

function(target_compile_options_rsodbc TARGET_NAME)
  target_compile_options(
    ${TARGET_NAME}
    PRIVATE /W3
            /WX-
            /diagnostics:column
            /GL
            /Gm-
            /EHsc
            /GS
            /Gy
            /fp:precise
            /Zc:wchar_t
            /Zc:forScope
            /Zc:inline
            /std:c++17
            /external:W3
            /Gd
            /TP
            /FC
            /errorReport:queue
            /LTCG)

  if(RS_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${TARGET_NAME} PRIVATE /fp:fast)
  endif()

  if(RS_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${TARGET_NAME} PRIVATE /Zi /Gm- /fp:precise /DEBUG)
  endif()
endfunction()

function(target_link_options_rsodbc TARGET_NAME)
  if(RS_BUILD_TYPE STREQUAL "Release")
    target_link_options(${TARGET_NAME} PRIVATE "/NODEFAULTLIB:libcmt")
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/LTCG")
  elseif(RS_BUILD_TYPE STREQUAL "Debug")
    target_link_options(${TARGET_NAME} PRIVATE "/NODEFAULTLIB:libcmtd")
  endif()
endfunction()

function(export_signatures TARGET_NAME)
  if(${TARGET_NAME} STREQUAL "rsodbc64")
    target_link_options(${TARGET_NAME} PRIVATE
                        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/rsodbc.def")
  elseif(${TARGET_NAME} STREQUAL "rsodbc64-test")
    target_link_options(${TARGET_NAME} PRIVATE
                        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/rsodbc_test.def")
  endif()
endfunction()

function(target_compile_definitions_rsodbc TARGET_NAME)
  target_compile_definitions(
    ${TARGET_NAME} PRIVATE WIN32 ODBCVER=0x0352 SQL_NOUNICODEMAP USE_SSL
                           CARES_STATICLIB _MBCS)
endfunction()

function(get_odbc_target_libraries odbc_manager_lib_list)
  set(${odbc_manager_lib_list}
      odbc32
      PARENT_SCOPE)
endfunction()

function(set_export_cmd EXPORT_CMD)
  set(${EXPORT_CMD}
      "SET"
      PARENT_SCOPE)
endfunction()

function(target_compile_definitions_pgport)
  target_compile_definitions(
    pqport
    PRIVATE WIN32
            _WINDOWS
            __WINDOWS__
            __WIN32__
            EXEC_BACKEND
            WIN32_STACK_RLIMIT=4194304
            _CRT_SECURE_NO_DEPRECATE
            _CRT_NONSTDC_NO_DEPRECATE
            FRONTEND
            USE_SSL
            ENABLE_SSPI
            ENABLE_GSS
            _MBCS)
endfunction()

function(set_library_suffixes)
  set(CMAKE_FIND_LIBRARY_SUFFIXES
      ".lib"
      PARENT_SCOPE)
  set(CMAKE_SHARED_LIBRARY_SUFFIX
      ".dll"
      PARENT_SCOPE)
endfunction()

function(set_library_prefixes)
  set(CMAKE_FIND_LIBRARY_PREFIXES
      ""
      PARENT_SCOPE)
  set(CMAKE_SHARED_LIBRARY_PREFIX
      ""
      PARENT_SCOPE)
      message(STATUS "CMAKE_FIND_LIBRARY_PREFIXES=${CMAKE_FIND_LIBRARY_PREFIXES}")
endfunction()

function(get_rsodbc_deps rsodbc_deps)
  message(STATUS "INSIDE get_rsodbc_deps")
  # s2n only in Linux, not in Darwin
  set(${rsodbc_deps}
      gtest gmock cares_static gssapi64 ${AWS_DEPENDENCIES}
      PARENT_SCOPE)

  message(STATUS "INSIDE get_rsodbc_deps=> ${rsodbc_deps}")
endfunction()

function(basic_build_settings)
  # Set compiler flags for Debug mode
  if(RS_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring Debug build")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
  endif()

  # Set compiler flags for Release mode
  if(RS_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring Release build")
    add_definitions(-DNDEBUG)
  endif()
endfunction()
