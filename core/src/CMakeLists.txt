#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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

add_subdirectory(filed)

if(NOT client-only)
  # droplet does not build on solaris because of sys/cdefs.h: No such file or
  # directory
  if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/droplet/CMakeLists.txt)
      message(
        FATAL_ERROR
        "Please checkout the libdroplet submodule via \n \"git submodule update --init --recursive\""
      )
    endif()
    add_subdirectory(droplet)
    if(TARGET droplet)
      set_target_properties(
        droplet PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                           SOVERSION "${BAREOS_VERSION_MAJOR}"
                           PREFIX "libbareos"
      )
    endif()
  endif()
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
    status "Skipping unit tests as testing on installed files is requested"
  )
elseif(NOT GTEST_FOUND)
  message(status "Skipping unit tests as gtest was not found")
else()
  add_subdirectory(tests)
endif()

add_subdirectory(console)
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
