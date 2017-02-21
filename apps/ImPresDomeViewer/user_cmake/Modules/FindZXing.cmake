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

FIND_LIBRARY(ZXING_LIBRARY_RELEASE_TEMP
  NAMES libzxing
  HINTS
  PATH_SUFFIXES lib
  PATHS ${ZXING_ROOT_DIR}
)

FIND_LIBRARY(ZXING_LIBRARY_DEBUG_TEMP
  NAMES libzxingd
  HINTS
  PATH_SUFFIXES lib
  PATHS ${ZXING_ROOT_DIR}
)

if(ZXING_LIBRARY_DEBUG_TEMP)
  # Set the final string here so the GUI reflects the final state.
  set(ZXING_LIBRARY_DEBUG ${ZXING_LIBRARY_DEBUG_TEMP} CACHE STRING "Where the ZXing Debug Library can be found")
  # Set the temp variable to INTERNAL so it is not seen in the CMake GUI
  set(ZXING_LIBRARY_DEBUG_TEMP "${ZXING_LIBRARY_DEBUG_TEMP}" CACHE INTERNAL "")
endif(ZXING_LIBRARY_DEBUG_TEMP)

if(ZXING_LIBRARY_RELEASE_TEMP)
  # Set the final string here so the GUI reflects the final state.
  set(ZXING_LIBRARY_RELEASE ${ZXING_LIBRARY_TEMP} CACHE STRING "Where the ZXing Release Library can be found")
  # Set the temp variable to INTERNAL so it is not seen in the CMake GUI
  set(ZXING_LIBRARY_RELEASE_TEMP "${ZXING_LIBRARY_RELEASE_TEMP}" CACHE INTERNAL "")
endif(ZXING_LIBRARY_RELEASE_TEMP)

if(ZXING_LIBRARY_DEBUG_TEMP AND ZXING_LIBRARY_RELEASE_TEMP)
  set(ZXING_LIBRARY debug ${ZXING_LIBRARY_DEBUG_TEMP} optimized ${ZXING_LIBRARY_RELEASE_TEMP})
elseif(ZXING_LIBRARY_RELEASE_TEMP)
  set(ZXING_LIBRARY ${ZXING_LIBRARY_RELEASE_TEMP})
elseif(ZXING_LIBRARY_DEBUG_TEMP)
  set(ZXING_LIBRARY ${ZXING_LIBRARY_DEBUG_TEMP})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZXing
  DEFAULT_MSG
  ZXING_INCLUDE_DIR ZXING_LIBRARY ZXING_LIBRARY_DEBUG)

if(ZXING_FOUND)
  list(APPEND ZXING_INCLUDE_DIRECTORY ${ZXING_INCLUDE_DIR})
  mark_as_advanced(ZXING_ROOT_DIR)
endif()

mark_as_advanced(ZXING_INCLUDE_DIR)