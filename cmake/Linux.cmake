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

function(target_compile_options_rsodbc TARGET_NAME)
    target_compile_options(${TARGET_NAME} PRIVATE
        -O2 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF
    )
endfunction()

function(target_compile_definitions_rsodbc TARGET_NAME)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        -DLINUX -DUSE_SSL -DODBCVER=0x0352 -DBUILD_REAL_64_BIT_MODE
    )
endfunction()

function(target_link_options_rsodbc TARGET_NAME)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endfunction()

function(export_signatures TARGET_NAME)
    target_link_options(${TARGET_NAME} PRIVATE
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.linux.list -Wl,--no-undefined)
endfunction()

function(target_link_libraries_rsodbc TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PUBLIC ${STATIC_LIBS} pq pqport rslog pthread resolv keyutils dl rt)
    target_link_options(${TARGET_NAME} PRIVATE -static-libgcc -static-libstdc++)
    target_link_directories(${TARGET_NAME} PRIVATE  ${CMAKE_LIBRARY_PATH})
    target_link_directories(${TARGET_NAME} PRIVATE  ${CMAKE_SYSTEM_LIBRARY_PATH})
endfunction()

function(get_odbc_target_libraries odbc_manager_lib_list)
    set(${odbc_manager_lib_list} odbc PARENT_SCOPE)
endfunction()

function(set_deps_library_suffix)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" PARENT_SCOPE)
    message("4. CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES}")
endfunction()