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
            message(STATUS "Searching for ${PACKAGE} in ${LIB_FILE_PATH}")
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

function(configure_directory srcDir destDir)
  make_directory(${destDir})

  file(
    GLOB templateFiles
    RELATIVE ${srcDir}
    "${srcDir}/*")
  foreach(templateFile ${templateFiles})
    set(srcTemplatePath ${srcDir}/${templateFile})
    if(NOT IS_DIRECTORY ${srcTemplatePath})
      configure_file(${srcTemplatePath} ${destDir}/${templateFile} @ONLY)
    endif(NOT IS_DIRECTORY ${srcTemplatePath})
  endforeach(templateFile)
endfunction(configure_directory)

function(import_ini srcDir)
  file(STRINGS ${srcDir} ConfigContents)
  foreach(NameAndValue ${ConfigContents})
    # Strip leading spaces
    string(REGEX REPLACE "^[ ]+" "" NameAndValue ${NameAndValue})
    # Find variable name
    string(REGEX MATCH "^[^=]+" Name ${NameAndValue})
    # Find the value
    string(REPLACE "${Name}=" "" Value ${NameAndValue})
    # Set the variable
    set(${Name}
        "${Value}"
        PARENT_SCOPE)
    # message(STATUS "${Name} ${Value}")
  endforeach()
endfunction(import_ini)

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
