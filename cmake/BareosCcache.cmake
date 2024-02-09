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

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CCACHE_CMDLINE "${CCACHE_PROGRAM}")
  list(APPEND CCACHE_CMDLINE "base_dir=${CMAKE_SOURCE_DIR}")
  list(APPEND CCACHE_CMDLINE "hash_dir=true")
  list(APPEND CCACHE_CMDLINE "namespace=bareos")
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_CMDLINE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_CMDLINE}")
endif()
