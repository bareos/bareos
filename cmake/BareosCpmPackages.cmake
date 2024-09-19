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

# Keep module alphabetically ordered
CPMAddPackage(
  NAME CLI11
  VERSION 2.4.2
  GITHUB_REPOSITORY CLIUtils/CLI11
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
  NAME fmt
  VERSION 11.0.2
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 11.0.2
  EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
  NAME xxHash
  VERSION 0.8.2
  GITHUB_REPOSITORY Cyan4973/xxHash
  EXCLUDE_FROM_ALL YES
  DOWNLOAD_ONLY True
)
if(xxHash_ADDED)
  include("cmake/xxHash.cmake")
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
