#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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

macro(add_sd_backend backend_name)
  if(HAVE_DYNAMIC_SD_BACKENDS)
    add_library(${backend_name} MODULE)
    install(TARGETS ${backend_name} DESTINATION ${backenddir})
  else()
    add_library(${backend_name} STATIC)
    target_link_libraries(bareossd PRIVATE ${backend_name})
  endif()
  set_property(TARGET ${backend_name} PROPERTY POSITION_INDEPENDENT_CODE ON)
endmacro()

if(${HAVE_GLUSTERFS})
  add_sd_backend(bareossd-gfapi)
  target_sources(bareossd-gfapi PRIVATE gfapi_device.cc)
  target_include_directories(bareossd-gfapi PUBLIC ${GFAPI_INCLUDE_DIRS})
  target_link_libraries(bareossd-gfapi ${GFAPI_LIBRARIES})
endif()

if(TARGET droplet)
  add_sd_backend(bareossd-droplet)
  target_sources(bareossd-droplet PRIVATE droplet_device.cc)
  target_link_libraries(bareossd-droplet PRIVATE droplet)
  target_compile_definitions(
    bareossd-droplet INTERFACE HAVE_BAREOSSD_DROPLET_DEVICE
  )
  if(HAVE_DYNAMIC_SD_BACKENDS)
    add_library(bareossd-chunked SHARED ordered_cbuf.cc chunked_device.cc)
    install(TARGETS bareossd-chunked DESTINATION ${backenddir})
    target_link_libraries(bareossd-droplet PRIVATE bareossd-chunked)
  else()
    target_sources(bareossd-droplet PRIVATE ordered_cbuf.cc chunked_device.cc)
  endif()

endif()

add_sd_backend(bareossd-fifo)
target_sources(bareossd-fifo PRIVATE unix_fifo_device.cc)
if(HAVE_DARWIN_OS)
  target_link_libraries(bareossd-fifo bareos bareossd)
endif()

add_sd_backend(bareossd-tape)
target_sources(bareossd-tape PRIVATE unix_tape_device.cc)

if(HAVE_DYNAMIC_SD_BACKENDS)
  add_library(bareossd-gentape SHARED generic_tape_device.cc)
  install(TARGETS bareossd-gentape DESTINATION ${backenddir})
  target_link_libraries(bareossd-tape bareossd-gentape)
  if(HAVE_DARWIN_OS)
    target_link_libraries(bareossd-gentape bareos bareossd)
  endif()
else()
  target_sources(bareossd-tape PRIVATE generic_tape_device.cc)
endif()

if(NOT HAVE_WIN32)
  add_sd_backend(bareossd-file)
  target_sources(bareossd-file PRIVATE unix_file_device.cc)
endif()
