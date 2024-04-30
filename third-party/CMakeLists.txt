#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2024 Bareos GmbH & Co. KG
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

option(USE_SYSTEM_CLI11 "Use the CLI11 lib of the operating system" OFF)
if(USE_SYSTEM_CLI11)
  find_package(CLI11 "2.2.0" CONFIG REQUIRED)
  set_target_properties(CLI11::CLI11 PROPERTIES IMPORTED_GLOBAL TRUE)
  message(STATUS "Using system CLI11 ${CLI11_VERSION}")
else()
  add_subdirectory(CLI11 EXCLUDE_FROM_ALL)
endif()

option(USE_SYSTEM_XXHASH "Use the xxHash lib of the operating system" OFF)
if(USE_SYSTEM_XXHASH)
  find_package(xxHash REQUIRED)
  set_target_properties(xxHash::xxhash PROPERTIES IMPORTED_GLOBAL TRUE)
  set(XXHASH_ENABLE_DISPATCH
      OFF
      CACHE INTERNAL ""
  )
  message(STATUS "Using system xxHash ${XXHASH_VERSION}")
else()
  include(./xxHash.cmake)
endif()
