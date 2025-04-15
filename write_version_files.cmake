# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2025 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

cmake_minimum_required(VERSION 3.17...3.31)

set(CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_LIST_DIR}/cmake" "${CMAKE_CURRENT_LIST_DIR}/core/cmake"
    "${CMAKE_CURRENT_LIST_DIR}/webui/cmake"
)

find_package(Git)
include(BareosVersionFromGit)

if(GIT_DESCRIBE_VERSION)
  if(NOT DEFINED VERSION_STRING)
    set(VERSION_STRING "${GIT_DESCRIBE_VERSION}")
  endif()
  set(VERSION_TIMESTAMP "${GIT_COMMIT_TIMESTAMP}")
else()
  message(
    FATAL_ERROR
      "VERSION_STRING not set, BareosVersion.cmake not found and no version data from git available.\n"
      "For more information why this happened and how to fix it, please see "
      "https://docs.bareos.org/DeveloperGuide/AutomaticVersionGeneration.html#troubleshooting"
  )
endif()

set(version_file_header "# autogenerated by write_version_files.cmake")
set(version_file_contents
    "${version_file_header}\nset(VERSION_STRING    \"${VERSION_STRING}\")\nset(VERSION_TIMESTAMP \"${VERSION_TIMESTAMP}\")\n"
)

message(
  STATUS
    "Configuring source tree for version ${VERSION_STRING} with timestamp ${VERSION_TIMESTAMP}"
)
set(version_file_name "${CMAKE_CURRENT_LIST_DIR}/cmake/BareosVersion.cmake")
message(STATUS "Writing ${version_file_name}")
file(WRITE ${version_file_name} "${version_file_contents}")
