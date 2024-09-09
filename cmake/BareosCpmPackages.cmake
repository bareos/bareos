#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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

if(NOT DEFINED CPM_USE_LOCAL_PACKAGES AND NOT DEFINED
                                          ENV{CPM_USE_LOCAL_PACKAGES}
)
  set(CPM_USE_LOCAL_PACKAGES ON)
endif()
if(FETCHCONTENT_FULLY_DISCONNECTED AND NOT CPM_LOCAL_PACKAGES_ONLY)
  message(
    WARNING
      " Detected FETCHCONTENT_FULLY_DISCONNECTED is set.\n"
      " As this will break fetching, only local packages will be considered!"
  )
  set(CPM_LOCAL_PACKAGES_ONLY ON)
endif()
include(CPM)

option(USE_SYSTEM_CLI11 "Use the CLI11 lib of the operating system" OFF)
if(USE_SYSTEM_CLI11)
  find_package(CLI11 "2.2.0" CONFIG REQUIRED)
  message(STATUS "Using system CLI11 ${CLI11_VERSION}")
else()
  CPMAddPackage(
    NAME CLI11
    VERSION 2.4.2
    GITHUB_REPOSITORY CLIUtils/CLI11
    EXCLUDE_FROM_ALL YES
  )
endif()

option(USE_SYSTEM_FMT "Use the FMT lib of the operating system" OFF)
if(USE_SYSTEM_FMT)
  find_package(FMT "6.2.1" CONFIG REQUIRED)
  message(STATUS "Using system FMT ${FMT_VERSION}")
else()
  CPMAddPackage(
    NAME fmt
    VERSION 6.2.1
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 10.2.1
    EXCLUDE_FROM_ALL YES
  )
endif()

option(USE_SYSTEM_XXHASH "Use the xxHash lib of the operating system" OFF)
if(USE_SYSTEM_XXHASH)
  find_package(xxHash REQUIRED)
  set(XXHASH_ENABLE_DISPATCH
      OFF
      CACHE INTERNAL ""
  )
  message(STATUS "Using system xxHash ${XXHASH_VERSION}")
else()
  CPMAddPackage(
    NAME xxHash
    VERSION 0.8.2
    GITHUB_REPOSITORY Cyan4973/xxHash
    EXCLUDE_FROM_ALL YES
  )
  add_library(xxHash::xxhash ALIAS xxhash)
endif()

