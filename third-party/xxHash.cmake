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

add_library(xxhash STATIC)
set_property(TARGET xxhash PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(xxhash PRIVATE xxHash/xxhash.c)
if(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
   OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "amd64")
   OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64")
)
  target_sources(xxhash PRIVATE xxHash/xxh_x86dispatch.c)
endif()
target_include_directories(xxhash INTERFACE xxHash)
add_library(xxHash::xxhash ALIAS xxhash)
