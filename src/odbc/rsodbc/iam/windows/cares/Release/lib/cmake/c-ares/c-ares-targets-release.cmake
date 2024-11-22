#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "c-ares::cares" for configuration "Release"
set_property(TARGET c-ares::cares APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(c-ares::cares PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/cares.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/cares.dll"
  )

list(APPEND _cmake_import_check_targets c-ares::cares )
list(APPEND _cmake_import_check_files_for_c-ares::cares "${_IMPORT_PREFIX}/lib/cares.lib" "${_IMPORT_PREFIX}/bin/cares.dll" )

# Import target "c-ares::cares_static" for configuration "Release"
set_property(TARGET c-ares::cares_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(c-ares::cares_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/cares_static.lib"
  )

list(APPEND _cmake_import_check_targets c-ares::cares_static )
list(APPEND _cmake_import_check_files_for_c-ares::cares_static "${_IMPORT_PREFIX}/lib/cares_static.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)