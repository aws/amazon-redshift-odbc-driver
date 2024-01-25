# Bring files from various sources libpq subfolders here and build them.
file(GLOB_RECURSE LIBPQ_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
file(GLOB_RECURSE LIBPQ_COMMON_SOURCES_WIN ${CMAKE_CURRENT_SOURCE_DIR}/*win*.c)
list(REMOVE_ITEM LIBPQ_COMMON_SOURCES ${LIBPQ_COMMON_SOURCES_WIN})

# list(APPEND LIBPQ_COMMON_SOURCES fe-auth.c fe-connect.c fe-exec.c fe-misc.c fe-print.c fe-lobj.c
# 	fe-protocol2.c fe-protocol3.c pqexpbuffer.c pqsignal.c fe-secure.c libpq-events.c
# 	ClientSideCursorExecutor.c ClientSideCursorInputStream.c ClientSideCursorLock.c
# 	ClientSideCursorMultiResultLock.c ClientSideCursorOptions.c ClientSideCursorOutputStream.c
# 	ClientSideCursorResult.c ClientSideCursorShutdownHook.c ClientSideCursorThread.c ClientSideCursorTrace.c
# 	file_util.c rsfile.c rslock.c rsmem.c MessageLoopState.c)

# libpq/port files
list (APPEND LIBPQ_PORT_SOURCE_NAMES chklocale.c inet_net_ntop.c noblock.c pgstrcasecmp.c thread.c strlcpy.c getpeereid.c)
foreach(FILE ${LIBPQ_PORT_SOURCE_NAMES})
    list(APPEND LIBPQ_PORT_SOURCES "${PGCLIENT_SRC_DIR}/port/${FILE}")
endforeach()

# backend/libpq files
list (APPEND LIBPQ_BACKEND_FILES ip.c md5.c)
foreach(FILE ${LIBPQ_BACKEND_FILES})
    list(APPEND LIBPQ_BACKEND_SOURCES "${PGCLIENT_SRC_DIR}/backend/libpq/${FILE}")
endforeach()

# backend/utils/mb files
list (APPEND LIBPQ_MB_FILES encnames.c wchar.c)
foreach(FILE ${LIBPQ_MB_FILES})
    list(APPEND LIBPQ_MB_SOURCES "${PGCLIENT_SRC_DIR}/backend/utils/mb/${FILE}")
endforeach()

list(APPEND SOURCES ${LIBPQ_COMMON_SOURCES} ${LIBPQ_PORT_SOURCES} ${LIBPQ_BACKEND_SOURCES} ${LIBPQ_MB_SOURCES})

# Create the library target
add_library(pq STATIC ${SOURCES})

# Set include directories for the library(for openssl)
if(OPENSSL_DIR)
    target_include_directories(pq PRIVATE ${OPENSSL_DIR}/include)
endif()

target_include_directories(pq PRIVATE ${PGCLIENT_INCLUDE_DIR})

# Also set include directories for a specific header file
get_filename_component(PG_CONFIG_PATHS_DIR ${PG_CONFIG_PATHS_H_FILE} DIRECTORY)
target_include_directories(pq PRIVATE ${PG_CONFIG_PATHS_DIR})
target_sources(pq PRIVATE ${PG_CONFIG_PATHS_H_FILE})

target_compile_options(pq PRIVATE
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
    -pthread
    -fpic
)

target_compile_definitions(pq PRIVATE
    -D_REENTRANT
    -D_THREAD_SAFE
    -D_POSIX_PTHREAD_SEMANTICS
    -DLINUX
    -DUSE_SSL
    -DENABLE_GSS
    -DFRONTEND
    -DUNSAFE_STAT_OK
    -D_GNU_SOURCE
    -DSO_MAJOR_VERSION=5
)
# Any extra custom definitions
target_compile_definitions_pq()

# Optionally, set compiler options
# target_compile_options(mylibrary PRIVATE -Wall -Wextra)

# Optionally, set linker options
# target_link_options(mylibrary PRIVATE -lm)

# Optionally, specify additional dependencies
# target_link_libraries(mylibrary PUBLIC otherlibrary)

# Set the output directory for the library
set_target_properties(pq PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)