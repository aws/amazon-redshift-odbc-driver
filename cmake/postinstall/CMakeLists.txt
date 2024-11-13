if (WIN32)
    install(CODE "execute_process(COMMAND cmd /c ${CMAKE_SOURCE_DIR}/package64.bat  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})")
endif()
