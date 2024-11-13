#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "aws-cpp-sdk-sso-oidc" for configuration "Debug"
set_property(TARGET aws-cpp-sdk-sso-oidc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(aws-cpp-sdk-sso-oidc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/aws-cpp-sdk-sso-oidc.lib"
  )

list(APPEND _cmake_import_check_targets aws-cpp-sdk-sso-oidc )
list(APPEND _cmake_import_check_files_for_aws-cpp-sdk-sso-oidc "${_IMPORT_PREFIX}/lib/aws-cpp-sdk-sso-oidc.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
