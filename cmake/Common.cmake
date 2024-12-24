function(set_output_directory EXECUTABLE_NAME)
  
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)

  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG  ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE  ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)

  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG  ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
  set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE  ${CMAKE_CURRENT_BINARY_DIR}/$<$<CONFIG:Debug>:>)
endfunction()

function(get_aws_libraries AWS_DEPENDENCIES)
  set(${AWS_DEPENDENCIES}
      aws-cpp-sdk-redshift
      aws-cpp-sdk-redshift-serverless
      aws-cpp-sdk-sts
      aws-cpp-sdk-sso-oidc
      aws-cpp-sdk-core
      aws-cpp-sdk-sso-oidc
      aws-crt-cpp
      aws-c-s3
      aws-c-auth
      aws-c-event-stream
      aws-c-http
      aws-c-mqtt
      aws-c-io
      aws-c-cal
      aws-checksums
      aws-c-compression
      aws-c-common
      aws-c-sdkutils
      PARENT_SCOPE)
endfunction()

function(get_rsodbc_static_libs RS_STATIC_LIBS)
    message(STATUS "INSIDE get_rsodbc_static_libs")
    set(AWS_DEPENDENCIES "")
    get_aws_libraries(AWS_DEPENDENCIES)
    message(STATUS "AWS_DEPENDENCIES = ${AWS_DEPENDENCIES}")
    get_rsodbc_deps(rsodbc_deps)
    message(STATUS "get_rsodbc_deps: ${rsodbc_deps}")

    foreach(PACKAGE ${rsodbc_deps})
        foreach(LIB_DIR ${CMAKE_LIBRARY_PATH})
            set(LIB_FILE_PATH "")
            set(LIB_FILE_PATH_ "${LIB_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}${PACKAGE}${CMAKE_FIND_LIBRARY_SUFFIXES}")
            file(TO_CMAKE_PATH ${LIB_FILE_PATH_} LIB_FILE_PATH)
            # message(STATUS "Searching for ${PACKAGE} in ${LIB_FILE_PATH}")
            if(EXISTS ${LIB_FILE_PATH})
                message("FOUND STATIC LIB ${LIB_FILE_PATH}")
                list(APPEND RS_STATIC_LIBS "${LIB_FILE_PATH}")
                set(${PACKAGE}_FOUND TRUE)
                break()
            endif()
        endforeach()
        if(NOT ${PACKAGE}_FOUND)
            message(WARNING "NOT FOUND STATIC for ${PACKAGE}")
        endif()
    endforeach()

    set(${RS_STATIC_LIBS} ${${RS_STATIC_LIBS}} PARENT_SCOPE)
endfunction()

macro(SUBDIRLIST result curdir)
  file(TO_CMAKE_PATH "${curdir}" curdir)
  # message(STATUS "curdir=${curdir}")
  file(
    GLOB children
    RELATIVE ${curdir}
    ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

function(include_and_link_odbc TARGET_NAME)
  if(RS_ODBC_DIR)
    target_include_directories(${TARGET_NAME} PRIVATE ${RS_ODBC_DIR}/include)
    target_link_directories(${TARGET_NAME} PRIVATE ${RS_ODBC_DIR}/lib)
  else()
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
    target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
  endif()
endfunction()

function(include_and_link_openssl TARGET_NAME)
  if(RS_OPENSSL_DIR)
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
    target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
  endif()
endfunction()

function(find_gtest)
    # Look for libgtest.a gtest.lib in CMAKE_LIBRARY_PATH
    find_library(GTEST_LIBRARY
    NAMES gtest
    PATHS ${CMAKE_LIBRARY_PATH}
    )

    # Check if libgtest.a is found
    if (GTEST_LIBRARY)
        message(STATUS "Found GTest library: ${GTEST_LIBRARY}")
        # Set variables expected by GTest
        set(GTEST_LIBRARIES ${GTEST_LIBRARY} PARENT_SCOPE)
        # Set the include directories where GTest headers are located
        get_filename_component(GTEST_DIR "${GTEST_LIBRARY}" DIRECTORY)
        message(STATUS "get_filename_component ${GTEST_LIBRARY} == > ${GTEST_DIR}")
        link_directories("${GTEST_DIR}")
        include_directories("${GTEST_DIR}/../include" )
    else()
        message(STATUS "GTest library not found, using find_package!")
        # Search the standard way and fail the standard way
        find_package(GTest REQUIRED)
    endif()

    include_directories(${GTEST_INCLUDE_DIRS})
endfunction(find_gtest)

function(post_build_copy_dependency_files EXECUTABLE_NAME DST_DIR)
  # Copy required dlls
  if ( WIN32 )      
      set_output_directory(${EXECUTABLE_NAME})
      file(TO_CMAKE_PATH "${RS_OPENSSL_DIR}/bin/*" OP)

      file(GLOB DEPENDENCY_FILES "${OP}" "${CMAKE_SOURCE_DIR}/src/pgclient/kfw-3-2-2-final/bin/Win64/*")
      # setting *.dll as the above filter didn't work
      set(DLL_FILES "")
      foreach(FILE ${DEPENDENCY_FILES})
        if(FILE MATCHES "\\.dll$")
            list(APPEND DLL_FILES ${FILE})
        endif()
      endforeach()
      message(STATUS " Files to copy ${DLL_FILES} to ${DST_DIR}")

      # Now DLL_FILES contains all the .dll files from both directories
      foreach(DLL_FILE ${DLL_FILES})
          add_custom_command(
              TARGET ${EXECUTABLE_NAME}
              POST_BUILD
              COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_FILE} "${DST_DIR}"
          )
      endforeach()
  endif ()
endfunction()
