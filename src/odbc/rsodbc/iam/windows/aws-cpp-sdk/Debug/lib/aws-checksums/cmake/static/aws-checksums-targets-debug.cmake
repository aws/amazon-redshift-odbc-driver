#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "AWS::aws-checksums" for configuration "Debug"
set_property(TARGET AWS::aws-checksums APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(AWS::aws-checksums PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/aws-checksums.lib"
  )

list(APPEND _cmake_import_check_targets AWS::aws-checksums )
list(APPEND _cmake_import_check_files_for_AWS::aws-checksums "${_IMPORT_PREFIX}/lib/aws-checksums.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
