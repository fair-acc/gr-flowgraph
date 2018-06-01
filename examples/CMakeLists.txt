# Copyright 2016 Free Software Foundation, Inc.
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

add_compile_options(-std=c++11 -O1 -fno-omit-frame-pointer -g)

include_directories(
    ${GR_ANALOG_INCLUDE_DIRS}
    ${GNURADIO_RUNTIME_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

link_directories(
    ${GR_ANALOG_LIBRARY_DIRS}
    ${GNURADIO_RUNTIME_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIRS}
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/example.grc
     ${CMAKE_CURRENT_SOURCE_DIR}/example_big.grc
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(factory_example factory_example.cc)

target_link_libraries(factory_example
	${GNURADIO_RUNTIME_LIBRARIES}
	${GNURADIO_ANALOG_LIBRARIES}
	${GNURADIO_BLOCKS_LIBRARIES}
    ${Boost_LIBRARIES}
	${DIGITIZERS_LIBRARIES}
	gnuradio-flowgraph
	#asan
)

INSTALL(TARGETS
  factory_example
  DESTINATION ${GR_FLOWGRAPH_EXAMPLES_DIR}
  COMPONENT "factory_example"
)


