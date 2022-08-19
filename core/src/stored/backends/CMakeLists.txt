#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2022 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

list(APPEND CMAKE_INSTALL_RPATH ${backenddir})

if(${HAVE_GLUSTERFS})
  add_library(bareossd-gfapi MODULE gfapi_device.cc)
  install(TARGETS bareossd-gfapi DESTINATION ${backenddir})
  target_include_directories(bareossd-gfapi PUBLIC ${GFAPI_INCLUDE_DIRS})
  target_link_libraries(bareossd-gfapi ${GFAPI_LIBRARIES})
endif()

if(TARGET droplet)
  add_library(bareossd-chunked SHARED ordered_cbuf.cc chunked_device.cc)
  target_link_libraries(bareossd-chunked droplet)
  install(TARGETS bareossd-chunked DESTINATION ${backenddir})

  add_library(bareossd-droplet MODULE droplet_device.cc)
  target_link_libraries(bareossd-droplet bareossd-chunked droplet)
  install(TARGETS bareossd-droplet DESTINATION ${backenddir})
endif()

add_library(bareossd-fifo MODULE unix_fifo_device.cc)
install(TARGETS bareossd-fifo DESTINATION ${backenddir})
if(HAVE_DARWIN_OS)
  target_link_libraries(bareossd-fifo bareos bareossd)
endif()

add_library(bareossd-gentape SHARED generic_tape_device.cc)
install(TARGETS bareossd-gentape DESTINATION ${backenddir})
if(HAVE_DARWIN_OS)
  target_link_libraries(bareossd-gentape bareos bareossd)
endif()

add_library(bareossd-tape MODULE unix_tape_device.cc)
install(TARGETS bareossd-tape DESTINATION ${backenddir})

target_link_libraries(bareossd-tape bareossd-gentape)
