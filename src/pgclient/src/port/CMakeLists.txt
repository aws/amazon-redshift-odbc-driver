# Bring files from various sources libpq subfolders here and build them.

list(APPEND SOURCES 
        strlcat.c strlcpy.c chklocale.c getpeereid.c dirmod.c exec.c
        inet_net_ntop.c noblock.c path.c pgcheckdir.c pgmkdirp.c
        pgsleep.c pgstrcasecmp.c qsort.c qsort_arg.c sprompt.c thread.c)
        
if (WIN32)
        list(APPEND SOURCES  snprintf.c thread.c getaddrinfo.c win32setlocale.c inet_aton.c)
endif()

# Create the library target
add_library(pqport STATIC ${SOURCES})
# Set include directories for the library
target_include_directories(pqport PUBLIC ${PGCLIENT_INCLUDE_DIR})

# Also set include directories for a specific header file
get_filename_component(PG_CONFIG_PATHS_DIR ${PG_CONFIG_PATHS_H_FILE} DIRECTORY)
target_include_directories(pqport PRIVATE ${PG_CONFIG_PATHS_DIR})
target_sources(pqport PRIVATE ${PG_CONFIG_PATHS_H_FILE})

include_directories_libpqport()
target_compile_options_pgport()
target_compile_definitions_pgport()
