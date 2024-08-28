#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2024 Bareos GmbH & Co. KG
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

message("xxHash local configuration included")

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^(x86_64|amd64|AMD64)$")
  set(XXHASH_ENABLE_DISPATCH
      ON
      CACHE INTERNAL ""
  )
else()
  set(XXHASH_ENABLE_DISPATCH
      OFF
      CACHE INTERNAL ""
  )
endif()

if("${CPM_SOURCE_CACHE}" MATCHES "^/.*$")
  set(XXHASH_SRC_DIR
      "${CPM_SOURCE_CACHE}/xxhash/bd2cc5037bfb339335d743b9f59f6d54b3977030"
  )
else()
  set(XXHASH_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/xxhash-src")
endif()

message("XXHASH_SRC_DIR ${XXHASH_SRC_DIR}")

add_library(xxhash STATIC "${XXHASH_SRC_DIR}")
set_property(TARGET xxhash PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(xxhash PRIVATE "${XXHASH_SRC_DIR}/xxhash.c")
target_compile_options(xxhash PRIVATE "-O3")
if(XXHASH_ENABLE_DISPATCH)
  target_sources(xxhash PRIVATE "${XXHASH_SRC_DIR}/xxh_x86dispatch.c")
endif()
target_include_directories(xxhash INTERFACE ${XXHASH_SRC_DIR})
add_library(xxHash::xxhash ALIAS xxhash)
