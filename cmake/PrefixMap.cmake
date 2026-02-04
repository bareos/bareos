#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2025 Bareos GmbH & Co. KG
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

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

set(BAREOS_PREFIX_MAP "${CMAKE_SOURCE_DIR}=/usr/src/bareos")

check_c_compiler_flag(
  -fdebug-prefix-map=${BAREOS_PREFIX_MAP} C_SUPPORTS_fdebug_prefix_map
)
check_cxx_compiler_flag(
  -fdebug-prefix-map=${BAREOS_PREFIX_MAP} CXX_SUPPORTS_fdebug_prefix_map
)
if(C_SUPPORTS_fdebug_prefix_map AND CXX_SUPPORTS_fdebug_prefix_map)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdebug-prefix-map=${BAREOS_PREFIX_MAP}")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fdebug-prefix-map=${BAREOS_PREFIX_MAP}"
  )
  set(CCACHE_MAY_HASHDIR ON)
endif()

check_c_compiler_flag(
  -fmacro-prefix-map=${BAREOS_PREFIX_MAP} C_SUPPORTS_fmacro_prefix_map
)
check_cxx_compiler_flag(
  -fmacro-prefix-map=${BAREOS_PREFIX_MAP} CXX_SUPPORTS_fmacro_prefix_map
)
if(C_SUPPORTS_fmacro_prefix_map AND CXX_SUPPORTS_fmacro_prefix_map)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmacro-prefix-map=${BAREOS_PREFIX_MAP}")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fmacro-prefix-map=${BAREOS_PREFIX_MAP}"
  )
endif()
