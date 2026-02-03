#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2025 Bareos GmbH & Co. KG
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
include(BareosConfigureFile)
bareos_configure_file(GLOB_RECURSE "*.d/*" COPY)

list(APPEND CMAKE_INSTALL_RPATH ${backenddir})

macro(add_sd_backend backend_name)
  if(HAVE_DYNAMIC_SD_BACKENDS)
    add_library(${backend_name} MODULE)
    install(TARGETS ${backend_name} DESTINATION ${backenddir})
  else()
    add_library(${backend_name} OBJECT)
    target_link_libraries(bareossd PRIVATE ${backend_name})
  endif()
endmacro()

add_library(chunked-device STATIC)
target_sources(chunked-device PRIVATE ordered_cbuf.cc chunked_device.cc)

add_library(backend-utils STATIC)
target_sources(backend-utils PRIVATE util.cc)
target_link_libraries(backend-utils PUBLIC Bareos::Lib fmt::fmt)

add_sd_backend(bareossd-dplcompat)
target_sources(bareossd-dplcompat PRIVATE dplcompat_device.cc crud_storage.cc)
target_link_libraries(
  bareossd-dplcompat PRIVATE Microsoft.GSL::GSL fmt::fmt tl::expected
                             chunked-device backend-utils
)
if(HAVE_WIN32)
  target_link_libraries(bareossd-dplcompat PRIVATE shlwapi)
endif()

add_sd_backend(bareossd-file)
add_sd_backend(bareossd-fifo)
add_sd_backend(bareossd-tape)
target_sources(bareossd-tape PRIVATE generic_tape_device.cc)
if(HAVE_WIN32)
  target_sources(bareossd-file PRIVATE win32_file_device.cc)
  target_sources(bareossd-fifo PRIVATE win32_fifo_device.cc)
  target_sources(bareossd-tape PRIVATE win32_tape_device.cc)
  target_link_libraries(bareossd-file PRIVATE Bareos::Lib)
  if(MSVC)
    target_link_libraries(bareossd-tape PRIVATE ${THREADS_THREADS})
    target_link_libraries(bareossd-fifo PRIVATE ${THREADS_THREADS})
  endif()
else()
  target_sources(bareossd-file PRIVATE unix_file_device.cc)
  target_sources(bareossd-fifo PRIVATE unix_fifo_device.cc)
  target_sources(bareossd-tape PRIVATE unix_tape_device.cc)
  add_sd_backend(bareossd-dedupable)
  target_sources(
    bareossd-dedupable PRIVATE dedupable_device.cc dedupable/device_options.cc
                               dedupable/volume.cc
  )
  target_link_libraries(
    bareossd-dedupable PRIVATE $<$<NOT:$<PLATFORM_ID:FreeBSD>>:stdc++fs>
                               backend-utils
  )
endif()

if(HAVE_DARWIN_OS)
  target_link_libraries(bareossd-fifo Bareos::Lib Bareos::LibSD)
  target_link_libraries(bareossd-gentape Bareos::Lib Bareos::LibSD)
endif()

if(HAVE_DYNAMIC_SD_BACKENDS)
  add_library(bareossd-autochanger_test MODULE)
  target_sources(
    bareossd-autochanger_test PRIVATE autochanger_test_device.cc
                                      unix_file_device.cc
  )
endif()

include(DebugEdit)
