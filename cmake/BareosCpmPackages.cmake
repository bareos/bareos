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

# Set VERSION to the the minimum required version. Point GIT_TAG to the "best"
# version we'd bundle. Keep module alphabetically ordered.
CPMAddPackage(
  NAME CLI11
  VERSION 2.1.2
  GITHUB_REPOSITORY CLIUtils/CLI11
  GIT_TAG v2.4.2
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
  NAME fmt
  VERSION 6.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 11.0.2
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
  NAME utf8cpp
  VERSION 2.3.4
  GITHUB_REPOSITORY nemtrif/utfcpp
  GIT_TAG "v4.0.4"
  EXCLUDE_FROM_ALL YES
)

# some version of utf8cpp come with cmake files that only define the target
# utf8cpp.
if(TARGET utf8cpp)
  if(NOT TARGET utf8cpp::utf8cpp)
    add_library(utf8cpp::utf8cpp ALIAS utf8cpp)
  endif()
endif()

CPMAddPackage(
  NAME xxHash
  VERSION 0.8.0
  GITHUB_REPOSITORY Cyan4973/xxHash
  GIT_TAG v0.8.2
  EXCLUDE_FROM_ALL YES
  SOURCE_SUBDIR cmake_unofficial
  OPTIONS "XXHASH_BUILD_XXHSUM OFF" "DISPATCH ON"
)
if(xxHash_ADDED)
  if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(x86_64|amd64|AMD64)$")
    set(XXHASH_ENABLE_DISPATCH 1)
  endif()
endif()

# Dump package information from CPM into a YAML file
file(WRITE "${CMAKE_BINARY_DIR}/cpm-packages.yaml"
     "# List of packages provided by CPM\n" "---\n"
)
foreach(package ${CPM_PACKAGES})
  file(
    APPEND "${CMAKE_BINARY_DIR}/cpm-packages.yaml"
    "${package}:\n" "    source_dir: ${CPM_PACKAGE_${package}_SOURCE_DIR}\n"
    "    binary_dir: ${CPM_PACKAGE_${package}_BINARY_DIR}\n"
    "    version: ${CPM_PACKAGE_${package}_VERSION}\n"
  )
endforeach()
