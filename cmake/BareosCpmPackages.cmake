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

CPMFindPackage(
  NAME CLI11
  VERSION 2.4.2
  GITHUB_REPOSITORY CLIUtils/CLI11
  EXCLUDE_FROM_ALL YES
)

CPMFindPackage(
  NAME CPMLicenses.cmake 
  GITHUB_REPOSITORY cpm-cmake/CPMLicenses.cmake
  VERSION 0.0.5
)

CPMFindPackage(
  NAME fmt
  VERSION 6.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
  EXCLUDE_FROM_ALL YES
)

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(x86_64|amd64|AMD64)$")
  set(XXHASH_ENABLE_DISPATCH ON
      CACHE INTERNAL ""
  )
else()
  set(XXHASH_ENABLE_DISPATCH OFF
      CACHE INTERNAL ""
  )
endif()
CPMFindPackage(
  NAME xxHash
  VERSION 0.8.2
  GITHUB_REPOSITORY Cyan4973/xxHash
  EXCLUDE_FROM_ALL YES
  OPTIONS "XXHASH_BUILD_ENABLE_INLINE_API OFF" "XXHASH_BUILD_XXHSUM ON" "XXASH_ENABLE_DISPATCH ${XXHASH_ENABLE_DISPATCH}"
  SOURCE_SUBDIR cmake_unofficial
)
set(xxHash::xxhash ALIAS xxhash)
target_link_libraries(xxhash PRIVATE xxHash::xxhash)


# **IMPORTANT** keep this as last item!
cpm_licenses_create_disclaimer_target(
  write-licenses "${CMAKE_CURRENT_BINARY_DIR}/LICENSES_third_party.txt" "${CPM_PACKAGES}"
)

