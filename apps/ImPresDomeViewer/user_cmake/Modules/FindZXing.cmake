# - try to find the ZXing SDK - currently designed for the version on GitHub (https://github.com/glassechidna/zxing-cpp).
#
#  Variables:
#  ZXING_INCLUDE_DIRECTORY
#  ZXING_FOUND
#  ZXING_LIBRARY
#
# Requires these CMake modules:
#  FindPackageHandleStandardArgs (known included with CMake >=2.6.2)

if(DEFINED ENV{ZXING_ROOT_DIR})
set(ZXING_ROOT_DIR
  "$ENV{ZXING_ROOT_DIR}"
  CACHE
  PATH
  "Directory to search for ZXing SDK")
else()
set(ZXING_ROOT_DIR
  "${ZXING_ROOT_DIR}"
  CACHE
  PATH
  "Directory to search for ZXing SDK")
endif()

set(_root_dirs)
if(ZXING_ROOT_DIR)
  set(_root_dirs "${ZXING_ROOT_DIR}")
  set(_libdir "${ZXING_ROOT_DIR}/lib")
endif()

find_path(ZXING_INCLUDE_DIR
  NAMES
  zxing/ZXing.h
  HINTS
  "${_libdir}"
  "${_libdir}/.."
  "${_libdir}/../.."
  PATHS
  ${_root_dirs}
  PATH_SUFFIXES
  include)

FIND_LIBRARY(ZXING_LIBRARY_TEMP
  NAMES libzxing
  HINTS
  PATH_SUFFIXES ${_libpath}
  PATHS ${ZXING_ROOT_DIR}
)

IF (ZXING_LIBRARY_TEMP)
  # Set the final string here so the GUI reflects the final state.
  SET(ZXING_LIBRARY ${ZXING_LIBRARY_TEMP} CACHE STRING "Where the ZXing Library can be found")
  # Set the temp variable to INTERNAL so it is not seen in the CMake GUI
  SET(ZXING_LIBRARY_TEMP "${ZXING_LIBRARY_TEMP}" CACHE INTERNAL "")
ENDIF(ZXING_LIBRARY_TEMP)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZXing
  DEFAULT_MSG
  ZXING_INCLUDE_DIR ZXING_LIBRARY)

if(ZXING_FOUND)
  list(APPEND ZXING_INCLUDE_DIRECTORY ${ZXING_INCLUDE_DIR})
  mark_as_advanced(ZXING_ROOT_DIR)
endif()

mark_as_advanced(ZXING_INCLUDE_DIR)