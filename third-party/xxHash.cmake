#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

add_library(xxhash STATIC)
set_property(TARGET xxhash PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(xxhash PRIVATE xxHash/xxhash.c)
target_compile_options(xxhash PRIVATE "-O3")
if(XXHASH_ENABLE_DISPATCH)
  target_sources(xxhash PRIVATE xxHash/xxh_x86dispatch.c)
endif()
target_include_directories(xxhash INTERFACE xxHash)
add_library(xxHash::xxhash ALIAS xxhash)
