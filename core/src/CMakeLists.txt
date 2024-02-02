#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2024 Bareos GmbH & Co. KG
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
bareos_configure_file(FILES include/config.h.in)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(filed)

if(NOT client-only)
  add_subdirectory(tools)
  add_subdirectory(cats)
  if(NOT HAVE_WIN32 AND ndmp)
    add_subdirectory(ndmp)
  endif()
  if(VIXDISKLIB_FOUND)
    add_subdirectory(vmware)
  endif()
  add_subdirectory(stored)
  add_subdirectory(dird)
endif()

if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  message(
    STATUS "Skipping unit tests as testing on installed files is requested"
  )
elseif(NOT GTest_FOUND)
  message(STATUS "Skipping unit tests as gtest was not found")
else()
  add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(ENABLE_BCONSOLE)
  add_subdirectory(console)
endif()

if(${HAVE_LMDB})
  add_subdirectory(lmdb)
endif()
add_subdirectory(lib)
add_subdirectory(findlib)
add_subdirectory(plugins)
add_subdirectory(fastlz)
if(${traymonitor})
  add_subdirectory(qt-tray-monitor)
endif()
