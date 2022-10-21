#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2022 Bareos GmbH & Co. KG
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

cmake_minimum_required(VERSION 3.17)
cmake_policy(VERSION 3.17...3.19)

project(bareos NONE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/core/cmake")

add_custom_target(
  uninstall "${CMAKE_COMMAND}" -P
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake"
)

set(default_build_type "Debug")

include(CMakePrintHelpers)
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

option(BUILD_UNIVERSAL_CLIENT
       "Override build-options to produce a limited, but universal client" OFF
)
if(BUILD_UNIVERSAL_CLIENT)
  if(CMAKE_VERSION VERSION_LESS 3.18.0)
    message(
      FATAL_ERROR "Building the universal client requires at least CMake 3.18"
    )
  endif()
  set(ENABLE_PYTHON
      OFF
      CACHE BOOL "" FORCE
  )
  set(ENABLE_GFAPI_FD
      OFF
      CACHE BOOL "" FORCE
  )
  set(ENABLE_JANSSON
      OFF
      CACHE BOOL "" FORCE
  )
  set(ENABLE_LZO
      OFF
      CACHE BOOL "" FORCE
  )
  set(ENABLE_CAPABILITY
      OFF
      CACHE BOOL "" FORCE
  )
  set(ENABLE_BCONSOLE
      OFF
      CACHE BOOL "" FORCE
  )
  set(ENABLE_STATIC_RUNTIME_LIBS
      ON
      CACHE BOOL "" FORCE
  )
  set(client-only ON)
endif()

option(ENABLE_WEBUI "Build with WEBUI" ON)

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

if(NOT hostname)
  site_name(hostname)
endif()

if(BUILD_BAREOS_BINARIES)
  add_subdirectory(third-party EXCLUDE_FROM_ALL)
  add_subdirectory(core)
  if(ENABLE_WEBUI)
    add_subdirectory(webui)
  endif()
  if(NOT client-only)
    add_subdirectory(systemtests)
  endif()
  add_subdirectory(contrib)
endif()

if(docs
   OR docs-only
   OR docs-build-json
)
  add_subdirectory(docs/manuals)
endif()
set(SITE ${BAREOS_PLATFORM}-${DISTVER})
