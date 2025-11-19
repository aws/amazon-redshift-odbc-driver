function(set_default_paths)
  # noop
endfunction()

function(set_export_cmd EXPORT_CMD)
  set(${EXPORT_CMD}
      "export"
      PARENT_SCOPE)
endfunction()

function(configure_file_pgclient)
  # message (STATUS "configure_file_pgclient")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config.h.linux64
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config.h)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config_os.h.linux64
                 ${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config_os.h)
endfunction()

function(include_directories_libpq)
  # noop
endfunction()

function(include_directories_libpqport)
  # noop
endfunction()

# Same as Darwin
function(add_libpqport_sources)
  list(
    APPEND
    LIBPQ_PORT_SOURCE_NAMES
    chklocale.c
    inet_net_ntop.c
    noblock.c
    pgstrcasecmp.c
    thread.c
    strlcpy.c
    getpeereid.c)
  foreach(FILE ${LIBPQ_PORT_SOURCE_NAMES})
    list(APPEND LIBPQ_PORT_SOURCES "${PGCLIENT_SRC_DIR}/port/${FILE}")
  endforeach()
endfunction()

# Same as Darwin
function(target_compile_options_pq)
  target_compile_options(
    pq
    PRIVATE # -m64
            -O2
            -Wall
            -Wmissing-prototypes
            -Wpointer-arith
            -Wdeclaration-after-statement
            -Wendif-labels
            -Wformat-security
            -fno-strict-aliasing
            -fwrapv
            -pthread
            -fpic)
endfunction()

function(target_compile_definitions_pq)
  # same should be in Darwin
  target_compile_definitions(
    pq
    PRIVATE -D_REENTRANT
            -D_THREAD_SAFE
            -D_POSIX_PTHREAD_SEMANTICS
            -DLINUX
            -DUSE_SSL
            -DENABLE_GSS
            -DFRONTEND
            -DUNSAFE_STAT_OK
            -D_GNU_SOURCE
            -DSO_MAJOR_VERSION=5)
endfunction()

function(target_compile_options_pgport)
  target_compile_options(
    pqport
    PRIVATE # -m64
            -O2
            -Wall
            -Wmissing-prototypes
            -Wpointer-arith
            -Wdeclaration-after-statement
            -Wendif-labels
            -Wformat-security
            -fno-strict-aliasing
            -fwrapv
            -fpic)
endfunction()

function(target_compile_definitions_pgport)
  target_compile_definitions(pqport PRIVATE -DLINUX -DUSE_SSL -D_GNU_SOURCE
                                            -DFRONTEND)
endfunction()

function(target_compile_options_rsodbc TARGET_NAME)
  target_compile_options(
    ${TARGET_NAME}
    PRIVATE -O2
            -Wall
            -c
            -fmessage-length=0
            -fPIC
            )
endfunction()

function(target_compile_definitions_rsodbc TARGET_NAME)
  target_compile_definitions(
    ${TARGET_NAME} PRIVATE -DLINUX -DUSE_SSL -DODBCVER=0x0352
                           -DBUILD_REAL_64_BIT_MODE)
endfunction()

function(target_link_options_rsodbc TARGET_NAME)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endfunction()

function(export_signatures TARGET_NAME)
  if(${TARGET_NAME} STREQUAL "rsodbc64")
    target_link_options(
      ${TARGET_NAME} PRIVATE
      -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.linux.list
      -Wl,--no-undefined)
  endif()
endfunction()

function(target_link_libraries_rsodbc TARGET_NAME)
  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC ${RS_STATIC_LIBS}
           pq
           pqport
          #  rslog
           pthread
           resolv
           keyutils
           dl
           rt)
  target_link_options(${TARGET_NAME} PRIVATE -static-libgcc -static-libstdc++)
  target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
  target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_SYSTEM_LIBRARY_PATH})
endfunction()

function(get_odbc_target_libraries odbc_manager_lib_list)
  set(${odbc_manager_lib_list}
      odbc
      PARENT_SCOPE)
endfunction()

function(set_library_suffixes)
  set(CMAKE_FIND_LIBRARY_SUFFIXES
      ".a"
      PARENT_SCOPE)
  set(CMAKE_SHARED_LIBRARY_SUFFIX
      ".so"
      PARENT_SCOPE)
endfunction()

function(set_library_prefixes)
  # set(CMAKE_FIND_LIBRARY_PREFIXES "lib" PARENT_SCOPE)
  # set(CMAKE_SHARED_LIBRARY_PREFIX
  #     ""
  #     PARENT_SCOPE)
endfunction()

function(get_rsodbc_deps rsodbc_deps)
  message("{AWS_DEPENDENCIES} = ${AWS_DEPENDENCIES}")
  # s2n only in Linux, not in Darwin
  set(${rsodbc_deps}
      ssl
      ${AWS_DEPENDENCIES}
      s2n
      curl
      ssl
      crypto
      nghttp2
      cares
      z
      gssapi_krb5
      krb5
      k5crypto
      krb5support
      k5crypto
      com_err
      PARENT_SCOPE)
endfunction()

function(basic_build_settings)
endfunction()
