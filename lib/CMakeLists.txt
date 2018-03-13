# Copyright 2011,2012,2016 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

include_directories(
    ${Boost_INCLUDE_DIR}
    ${ROOT_INCLUDE_DIRS}
    ${GR_INCLUDE_DIR}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${ROOT_LIBRARY_DIRS}
)

list(APPEND flowgraph_sources
    exprtk_impl.cc
    flowgraph_impl.cc)

set(flowgraph_sources "${flowgraph_sources}" PARENT_SCOPE)
if(NOT flowgraph_sources)
	MESSAGE(STATUS "No C++ sources... skipping lib/")
	return()
endif(NOT flowgraph_sources)

add_library(gnuradio-flowgraph SHARED ${flowgraph_sources})
target_link_libraries(gnuradio-flowgraph 
	${Boost_LIBRARIES} 
	${GNURADIO_ALL_LIBRARIES} 

	# ROOT's cmake does not work correctly on some platforms, if you want
	# to link against libs that are really needed uncomment the following two lines:
	#/usr/lib64/root/libMathCore.so
	#/usr/lib64/root/libGraf.so
	${ROOT_LIBRARIES}
	)
set_target_properties(gnuradio-flowgraph PROPERTIES DEFINE_SYMBOL "gnuradio_flowgraph_EXPORTS")

if(APPLE)
    set_target_properties(gnuradio-flowgraph PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)


if(ENABLE_STATIC_LIBS)
  add_library(gnuradio-flowgraph_static STATIC ${flowgraph_sources})

  add_dependencies(gnuradio-flowgraph_static
    flowgraph_generated_includes)

  if(NOT WIN32)
    set_target_properties(gnuradio-flowgraph_static
      PROPERTIES OUTPUT_NAME gnuradio-flowgraph)
  endif(NOT WIN32)

  install(TARGETS gnuradio-flowgraph_static
    ARCHIVE DESTINATION lib${LIB_SUFFIX} COMPONENT "flowgraph_devel"   # .lib file
    )
endif(ENABLE_STATIC_LIBS)


########################################################################
# Install built library files
########################################################################
include(GrMiscUtils)
GR_LIBRARY_FOO(gnuradio-flowgraph RUNTIME_COMPONENT "flowgraph_runtime" DEVEL_COMPONENT "flowgraph_devel")

########################################################################
# Build and register unit test
########################################################################
#include(GrTest)

#include_directories(${CPPUNIT_INCLUDE_DIRS})
#list(APPEND test_flowgraph_sources
#    ${CMAKE_CURRENT_SOURCE_DIR}/test_flowgraph.cc
#    ${CMAKE_CURRENT_SOURCE_DIR}/qa_flowgraph.cc
#)

#add_executable(test-flowgraph ${test_flowgraph_sources})

#target_link_libraries(
#  test-flowgraph
#  ${GNURADIO_RUNTIME_LIBRARIES}
#  ${Boost_LIBRARIES}
#  ${CPPUNIT_LIBRARIES}
#  gnuradio-flowgraph
#)

#GR_ADD_TEST(test_flowgraph test-flowgraph)

########################################################################
# Print summary
########################################################################
message(STATUS "Using install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Building for version: ${VERSION} / ${LIBVER}")

