
function(configure_directory srcDir destDir)
    #message(STATUS "Configuring directory ${destDir}")
    make_directory(${destDir})

    file(GLOB templateFiles RELATIVE ${srcDir} "${srcDir}/*")
    foreach(templateFile ${templateFiles})
        set(srcTemplatePath ${srcDir}/${templateFile})
        if(NOT IS_DIRECTORY ${srcTemplatePath})
            #message(STATUS "Configuring file ${templateFile}")
            configure_file(
                    ${srcTemplatePath}
                    ${destDir}/${templateFile}
                    @ONLY)
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
    set(${Name} "${Value}"  PARENT_SCOPE)
    #message(STATUS "${Name} ${Value}")
    endforeach()
endfunction(import_ini)

MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

function(include_and_link_odbc TARGET_NAME)
    if (ODBC_DIR)
        target_include_directories(${TARGET_NAME} PRIVATE ${ODBC_DIR}/include)
        target_link_directories(${TARGET_NAME} PRIVATE ${ODBC_DIR}/lib)
    else()
        target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
        target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
    endif()
endfunction()

function(include_and_link_openssl TARGET_NAME)
    if (OPENSSL_DIR)
        target_include_directories(${TARGET_NAME} PRIVATE ${OPENSSL_DIR}/include)
        target_link_directories(${TARGET_NAME} PRIVATE ${OPENSSL_DIR}/lib)
    else()
        target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_INCLUDE_PATH})
        target_link_directories(${TARGET_NAME} PRIVATE ${CMAKE_LIBRARY_PATH})
    endif()
endfunction()