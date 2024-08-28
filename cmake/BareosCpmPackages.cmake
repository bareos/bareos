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

CPMFindPackage(
  NAME xxHash
  VERSION 0.8.2
  GITHUB_REPOSITORY Cyan4973/xxHash
  EXCLUDE_FROM_ALL YES
  DOWNLOAD_ONLY True
)
if ( NOT licenses-only )
  if (xxHash_ADDED)
    include("cmake/xxHash.cmake")
  endif()
endif()

# **IMPORTANT** keep this as last item!
cpm_licenses_create_disclaimer_target(
  write-licenses "${CMAKE_CURRENT_BINARY_DIR}/LICENSES_third_party.txt" "${CPM_PACKAGES}"
)

