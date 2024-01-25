# Bring files from various sources libpq subfolders here and build them.

list(APPEND SOURCES 
        strlcat.c strlcpy.c chklocale.c getpeereid.c dirmod.c exec.c
        inet_net_ntop.c noblock.c path.c pgcheckdir.c pgmkdirp.c
        pgsleep.c pgstrcasecmp.c qsort.c qsort_arg.c sprompt.c thread.c)

# Create the library target
add_library(pqport STATIC ${SOURCES})

# Set include directories for the library
target_include_directories(pqport PUBLIC ${PGCLIENT_INCLUDE_DIR})

# Also set include directories for a specific header file
get_filename_component(PG_CONFIG_PATHS_DIR ${PG_CONFIG_PATHS_H_FILE} DIRECTORY)
target_include_directories(pqport PRIVATE ${PG_CONFIG_PATHS_DIR})
target_sources(pqport PRIVATE ${PG_CONFIG_PATHS_H_FILE})

target_compile_options(pqport PRIVATE
    # -m64
    -O2
    -Wall
    -Wmissing-prototypes
    -Wpointer-arith
    -Wdeclaration-after-statement
    -Wendif-labels
    -Wformat-security
    -fno-strict-aliasing
    -fwrapv
)

target_compile_definitions(pqport PRIVATE
-DLINUX -DUSE_SSL -D_GNU_SOURCE -DFRONTEND
)
target_compile_definitions_pgport()

# Optionally, set compiler options
# target_compile_options(mylibrary PRIVATE -Wall -Wextra)

# Optionally, set linker options
# target_link_options(mylibrary PRIVATE -lm)

# Optionally, specify additional dependencies
# target_link_libraries(mylibrary PUBLIC otherlibrary)

# Set the output directory for the library
set_target_properties(pqport PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)