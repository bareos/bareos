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

check_c_compiler_flag(-fcanon-prefix-map c_canon)
check_c_compiler_flag(-ffile-prefix-map=.=. c_prefix)
check_cxx_compiler_flag(-fcanon-prefix-map cxx_canon)
check_cxx_compiler_flag(-ffile-prefix-map=.=. cxx_prefix)

if(c_canon
   AND c_prefix
   AND cxx_canon
   AND cxx_prefix
)
  add_compile_options(
    "-fcanon-prefix-map"
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=${CMAKE_SOURCE_DIR}"
  )
endif()
