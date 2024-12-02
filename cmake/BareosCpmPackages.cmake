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

CPMAddPackage(
  NAME fmt
  VERSION 6.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
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
