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
# Build and register unit test
########################################################################
include(GrTest)

add_compile_options(-std=c++11 -O1 -fno-omit-frame-pointer -g)

include_directories(
	${CMAKE_SOURCE_DIR}/lib
	${CPPUNIT_INCLUDE_DIRS}
)

list(APPEND test_flowgraph_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/test_flowgraph.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/test_parser.cc
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_parser_options.grc
          ${CMAKE_CURRENT_SOURCE_DIR}/test_collapse_variables.grc
          ${CMAKE_CURRENT_SOURCE_DIR}/test_expressions.grc
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test-flowgraph ${test_flowgraph_sources})

target_link_libraries(
    test-flowgraph
  	${GNURADIO_RUNTIME_LIBRARIES}
	${GNURADIO_ANALOG_LIBRARIES}
	${GNURADIO_BLOCKS_LIBRARIES}
    ${Boost_LIBRARIES}
	${DIGITIZERS_LIBRARIES}
    ${CPPUNIT_LIBRARIES}
    gnuradio-flowgraph
)

GR_ADD_TEST(test_flowgraph test-flowgraph)

