#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2020 Bareos GmbH & Co. KG
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

cmake_minimum_required(VERSION 3.12)

project(bareos NONE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/core/cmake")

add_custom_target(
  uninstall "${CMAKE_COMMAND}" -P
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake"
)

set(default_build_type "Debug")

find_package(Git)
include(BareosVersionFromGit)
include(BareosExtractVersionInfo)
include(BareosGetDistInfo)

set(BUILDNAME
    "${BAREOS_FULL_VERSION}"
    CACHE STRING "site name variable for CDash"
)
set(SITE
    "${CMAKE_SYSTEM_NAME}-${DISTVER}-${CMAKE_HOST_SYSTEM_PROCESSOR}"
    CACHE STRING "build name variable for CDash"
)
# enable "make test"
enable_testing()
set(CMAKE_CTEST_COMMAND ctest -V)
include(CTest)

if(docs-build-json)
  # check for wrong combinations of build flags
  if(docs-only)
    message(
      FATAL_ERROR
        "\n\"docs-build-json\" needs bareos-executables and cannot be combined with \"docs-only\""
    )
  endif()
  if(NOT traymonitor)
    message(
      FATAL_ERROR
        "\n\"docs-build-json\" needs the traymonitor executable: add \"-Dtraymonitor=yes\""
    )
  endif()
endif()

if(docs-only)
  set(BUILD_BAREOS_BINARIES no)
else()
  set(BUILD_BAREOS_BINARIES yes)
endif()

if(BUILD_BAREOS_BINARIES)
  add_subdirectory(core)
  add_subdirectory(webui)
  if(NOT client-only AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_subdirectory(systemtests)
  endif()
endif()

if(docs
   OR docs-only
   OR docs-build-json
)
  add_subdirectory(docs/manuals)
endif()
set(SITE ${PLATFORM}-${DISTVER})
