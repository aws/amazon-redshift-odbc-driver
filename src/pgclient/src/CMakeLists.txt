set(PGCLIENT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PGCLIENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PG_CONFIG_PATHS_H_FILE ${CMAKE_CURRENT_SOURCE_DIR}/port/pg_config_paths.h)

configure_file_pgclient()

add_subdirectory(interfaces/libpq)
add_subdirectory(port)