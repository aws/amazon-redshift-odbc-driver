include(CMakeFindDependencyMacro)
# Auto resolve BUILD_SHARED_LIBS for dependencies, could be overrided by CRT_BUILD_SHARED_LIBS
set(BUILD_SHARED_LIBS_PREV ${BUILD_SHARED_LIBS})
if (DEFINED CRT_BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${CRT_BUILD_SHARED_LIBS})
else()
    if (DEFINED AWSSDK_CRT_INSTALL_AS_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${AWSSDK_CRT_INSTALL_AS_SHARED_LIBS})
    else()
        set(BUILD_SHARED_LIBS ${AWSSDK_INSTALL_AS_SHARED_LIBS})
    endif()
endif()
find_dependency(aws-crt-cpp)
if (AWSSDK_CRYPTO_IN_SOURCE_BUILD)
    find_dependency(crypto)
    find_dependency(ssl)
endif()
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_PREV})
include("${CMAKE_CURRENT_LIST_DIR}/aws-cpp-sdk-core-targets.cmake")
