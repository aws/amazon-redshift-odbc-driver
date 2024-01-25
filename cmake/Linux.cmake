function(set_default_paths)
    #noop
endfunction()

function(configure_file_pgclient)
    #message (STATUS "configure_file_pgclient")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config.h.linux64 ${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config.h)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config_os.h.linux64 ${CMAKE_CURRENT_SOURCE_DIR}/include/pg_config_os.h)
endfunction()

function(target_compile_definitions_pq)
    #noop
endfunction()

function(target_compile_definitions_pgport)
    #noop
endfunction()

function(target_compile_options_rsodbc)
    target_compile_options(rsodbc64 PRIVATE
        -O2 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF
    )
endfunction()

function(target_compile_definitions_rsodbc)
    target_compile_definitions(rsodbc64 PRIVATE
        -DLINUX -DUSE_SSL -DODBCVER=0x0352 -DBUILD_REAL_64_BIT_MODE
    )
endfunction()

function(target_link_options_rsodbc)
    target_link_options(rsodbc64 PRIVATE
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.list -Wl,--no-undefined)
endfunction()

function(target_link_libraries_rsodbc)
    target_link_libraries(rsodbc64 PUBLIC ${STATIC_LIBS} pq pqport rslog pthread resolv keyutils dl rt)
    target_link_options(rsodbc64 PRIVATE -static-libgcc -static-libstdc++)
    target_link_directories(rsodbc64 PRIVATE  ${CMAKE_SYSTEM_LIBRARY_PATH})
endfunction()

function(get_odbc_target_libraries odbc_manager_lib_list)
    set(${odbc_manager_lib_list} odbc PARENT_SCOPE)
endfunction()