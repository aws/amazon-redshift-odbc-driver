# Bring files from various sources libpq subfolders here and build them.
file(GLOB_RECURSE LIBPQ_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
file(GLOB_RECURSE LIBPQ_COMMON_SOURCES_WIN ${CMAKE_CURRENT_SOURCE_DIR}/*win*.c)
if (NOT WIN32)
    list(REMOVE_ITEM LIBPQ_COMMON_SOURCES ${LIBPQ_COMMON_SOURCES_WIN})
endif()

# list(APPEND LIBPQ_COMMON_SOURCES fe-auth.c fe-connect.c fe-exec.c fe-misc.c fe-print.c fe-lobj.c
# 	fe-protocol2.c fe-protocol3.c pqexpbuffer.c pqsignal.c fe-secure.c libpq-events.c
# 	ClientSideCursorExecutor.c ClientSideCursorInputStream.c ClientSideCursorLock.c
# 	ClientSideCursorMultiResultLock.c ClientSideCursorOptions.c ClientSideCursorOutputStream.c
# 	ClientSideCursorResult.c ClientSideCursorShutdownHook.c ClientSideCursorThread.c ClientSideCursorTrace.c
# 	file_util.c rsfile.c rslock.c rsmem.c MessageLoopState.c)

# libpq/port files
set(LIBPQ_PORT_SOURCES)
add_libpqport_sources()

# backend/libpq files
list (APPEND LIBPQ_BACKEND_FILES ip.c md5.c zpq_stream.c z_stream.c lz4.c zstd-in.c)
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
if (WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
# Set include directories for the library(for openssl)
if(RS_OPENSSL_DIR)
    add_safe_include(pq ${RS_OPENSSL_DIR})
endif()

target_include_directories(pq PRIVATE ${PGCLIENT_INCLUDE_DIR})
include_directories_libpq()

# Also set include directories for a specific header file
get_filename_component(PG_CONFIG_PATHS_DIR ${PG_CONFIG_PATHS_H_FILE} DIRECTORY)
target_include_directories(pq PRIVATE ${PG_CONFIG_PATHS_DIR})
target_sources(pq PRIVATE ${PG_CONFIG_PATHS_H_FILE})



# Any extra custom definitions
target_compile_options_pq()
target_compile_definitions_pq()

# Optionally, set compiler options
# target_compile_options(mylibrary PRIVATE -Wall -Wextra)

# Optionally, set linker options
# target_link_options(mylibrary PRIVATE -lm)

# Optionally, specify additional dependencies
# target_link_libraries(mylibrary PUBLIC otherlibrary)
