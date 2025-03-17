#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

option(COMMON_OUTPUT_DIRECTORIES
       "Output all targets into common output directories" OFF
)

if(COMMON_OUTPUT_DIRECTORIES)
  if(CMAKE_CONFIGURATION_TYPES)
    message(
      FATAL_ERROR
        "Cannot configure single output directory for multi-config build (yet)."
    )
  else()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(PLUGIN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
  endif()
endif()
