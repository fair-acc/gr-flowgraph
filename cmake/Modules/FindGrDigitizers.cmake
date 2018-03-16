#
# Find the gr-digitizers includes and library
#
# This module defines
# DIGITIZERS_INCLUDE_DIR, where to find tiff.h, etc.
# DIGITIZERS_LIBRARIES, the libraries to link against to use digitizers.
# DIGITIZERS_FOUND, If false, do not try to use gr-digitizers.


INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_DIGITIZERS digitizers)

FIND_PATH(
    DIGITIZERS_INCLUDE_DIRS
    NAMES digitizers/api.h
    HINTS $ENV{DIGITIZERS_DIR}/include
        ${PC_DIGITIZERS_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /home/bel/schwinn/lnx/picoscope/snapshot_cosylab/Digitizers/gr-digitizers/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    DIGITIZERS_LIBRARIES
    NAMES gnuradio-digitizers
    HINTS $ENV{DIGITIZERS_DIR}/lib
        ${PC_DIGITIZERS_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /home/bel/schwinn/lnx/picoscope/snapshot_cosylab/Digitizers/gr-digitizers/build/lib
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DIGITIZERS DEFAULT_MSG DIGITIZERS_LIBRARIES DIGITIZERS_INCLUDE_DIRS)
MARK_AS_ADVANCED(DIGITIZERS_LIBRARIES DIGITIZERS_INCLUDE_DIRS)

