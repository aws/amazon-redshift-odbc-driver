# Zlib
if(PLATFORM_ANDROID AND ANDROID_BUILD_ZLIB)
    set(BUILD_ZLIB 1)
    message(STATUS "  Building Zlib as part of AWS SDK")
elseif(NOT PLATFORM_WINDOWS AND NOT PLATFORM_CUSTOM)
    #If zlib is required either by openssl and curl in their linking chain, we should find it.
    include(FindZLIB)
    if(NOT ZLIB_FOUND)
        message(FATAL_ERROR "Could not find zlib")
    else()
        message(STATUS "  Zlib library: ${ZLIB_LIBRARIES}")
    endif()
endif()

# Request Compression dependencies
if (ENABLE_ZLIB_REQUEST_COMPRESSION)
    find_package(ZLIB REQUIRED)
endif()


# Encryption control
if(NOT NO_ENCRYPTION)
    if(PLATFORM_WINDOWS)
        set(ENABLE_BCRYPT_ENCRYPTION ON)
    elseif(PLATFORM_LINUX OR PLATFORM_ANDROID)
        set(ENABLE_OPENSSL_ENCRYPTION ON)
    elseif(PLATFORM_APPLE)
        set(ENABLE_COMMONCRYPTO_ENCRYPTION ON)
    endif()
else()
    set(ENABLE_INJECTED_ENCRYPTION ON)
endif()

if(ENABLE_BCRYPT_ENCRYPTION)
    add_definitions(-DENABLE_BCRYPT_ENCRYPTION)
    set(CRYPTO_LIBS Bcrypt)
    set(CRYPTO_LIBS_ABSTRACT_NAME Bcrypt)
    message(STATUS "Encryption: Bcrypt")
elseif(ENABLE_OPENSSL_ENCRYPTION)
    add_definitions(-DENABLE_OPENSSL_ENCRYPTION)
    message(STATUS "Encryption: LibCrypto")

    set(CRYPTO_TARGET_NAME "AWS::crypto")
    if(PLATFORM_ANDROID AND ANDROID_BUILD_OPENSSL)
        set(BUILD_OPENSSL 1)
        set(CRYPTO_TARGET_NAME "crypto")
        set(USE_OPENSSL ON)
        message(STATUS "  Building Openssl as part of AWS SDK")
    else()
        find_package(crypto REQUIRED)
    endif()
    set(CRYPTO_LIBS ${CRYPTO_TARGET_NAME} ${ZLIB_LIBRARIES})
    # ssl depends on libcrypto
    set(CRYPTO_LIBS_ABSTRACT_NAME ${CRYPTO_TARGET_NAME} ssl z)
elseif(ENABLE_COMMONCRYPTO_ENCRYPTION)
    add_definitions(-DENABLE_COMMONCRYPTO_ENCRYPTION)
    message(STATUS "Encryption: CommonCrypto")
elseif(ENABLE_INJECTED_ENCRYPTION)
    message(STATUS "Encryption: None")
    message(STATUS "You will need to inject an encryption implementation before making any http requests!")
endif()

# Http client control
if(NOT NO_HTTP_CLIENT AND NOT USE_CRT_HTTP_CLIENT)
    if(PLATFORM_WINDOWS)
        if(FORCE_CURL)
            set(ENABLE_CURL_CLIENT 1)
        else()
            set(ENABLE_WINDOWS_CLIENT 1)
        endif()
    elseif(PLATFORM_LINUX OR PLATFORM_APPLE OR PLATFORM_ANDROID)
        set(ENABLE_CURL_CLIENT 1)
    endif()

    if(ENABLE_CURL_CLIENT)
        add_definitions(-DENABLE_CURL_CLIENT)
        message(STATUS "Http client: Curl")

        if(PLATFORM_ANDROID AND ANDROID_BUILD_CURL)
            set(BUILD_CURL 1)
            message(STATUS "  Building Curl as part of AWS SDK")
        else()
            include(FindCURL)
            if(NOT CURL_FOUND)
                message(FATAL_ERROR "Could not find curl")
            endif()

            # When built from source using cmake, curl does not include
            # CURL_INCLUDE_DIRS or CURL_INCLUDE_DIRS so we need to use
            # find_package to fix it
            if ("${CURL_INCLUDE_DIRS}" STREQUAL "" AND "${CURL_LIBRARIES}" STREQUAL "")
                message(STATUS "Could not find curl include or library path, falling back to find with config.")
                find_package(CURL)
                set(CURL_LIBRARIES CURL::libcurl)
            else ()
                message(STATUS "  Curl include directory: ${CURL_INCLUDE_DIRS}")
                List(APPEND EXTERNAL_DEPS_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
                set(CLIENT_LIBS ${CURL_LIBRARIES})
            endif ()
            set(CLIENT_LIBS_ABSTRACT_NAME curl)
            message(STATUS "  Curl target link: ${CURL_LIBRARIES}")
        endif()

        if(TEST_CERT_PATH)
            message(STATUS "Setting curl cert path to ${TEST_CERT_PATH}")
            add_definitions(-DTEST_CERT_PATH="\"${TEST_CERT_PATH}\"")
        endif()
    elseif(ENABLE_WINDOWS_CLIENT)
        add_definitions(-DENABLE_WINDOWS_CLIENT)

        if(USE_IXML_HTTP_REQUEST_2)
            add_definitions(-DENABLE_WINDOWS_IXML_HTTP_REQUEST_2_CLIENT)
            set(CLIENT_LIBS msxml6 runtimeobject)
            set(CLIENT_LIBS_ABSTRACT_NAME msxml6 runtimeobject)
            message(STATUS "Http client: IXmlHttpRequest2")
            if(BYPASS_DEFAULT_PROXY)
                add_definitions(-DBYPASS_DEFAULT_PROXY)
                list(APPEND CLIENT_LIBS winhttp)
                list(APPEND CLIENT_LIBS_ABSTRACT_NAME winhttp)
                message(STATUS "Proxy bypass is enabled via WinHttp")
            endif()
        else()
            set(CLIENT_LIBS Wininet winhttp)
            set(CLIENT_LIBS_ABSTRACT_NAME Wininet winhttp)
            message(STATUS "Http client: WinHttp")
        endif()

    else()
        message(FATAL_ERROR "No http client available for target platform and client injection not enabled (-DNO_HTTP_CLIENT=ON)")
    endif()
elseif(USE_CRT_HTTP_CLIENT)
    add_definitions("-DAWS_SDK_USE_CRT_HTTP -DHAVE_H2_CLIENT")
else()
    message(STATUS "You will need to inject an http client implementation before making any http requests!")
endif()

# Open telemetry client
if (BUILD_OPTEL)
    find_package(opentelemetry-cpp CONFIG REQUIRED)
endif ()
if (BUILD_OPTEL_OTLP_BENCHMARKS)
    find_package(Protobuf REQUIRED)
    find_package(CURL REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(opentelemetry-cpp CONFIG REQUIRED)
endif ()

if (EXTERNAL_DEPS_INCLUDE_DIRS)
    List(REMOVE_DUPLICATES EXTERNAL_DEPS_INCLUDE_DIRS)
endif()
