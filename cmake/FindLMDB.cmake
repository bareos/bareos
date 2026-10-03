#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026 Bareos GmbH & Co. KG
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
#
# Try to find the LMDB include and library
# LMDB_FOUND - system has LMDB
# LMDB_INCLUDE_DIR - include directory for LMDB
# LMDB_LIBRARY - library for LMDB
# LMDB_VERSION - version of LMDB

include(FindPackageHandleStandardArgs)

find_path(
  LMDB_INCLUDE_DIR
  NAMES lmdb.h
)

find_library(
  LMDB_LIBRARY
  NAMES lmdb
)

if(LMDB_INCLUDE_DIR)
  foreach(component MAJOR MINOR PATCH)
    file(STRINGS "${LMDB_INCLUDE_DIR}/lmdb.h"
      LMDB_VERSION_${component}_LINE
      REGEX "^#define[ \t]+MDB_VERSION_${component}[ \t]+[0-9]+")
    string(REGEX MATCH "[0-9]+"
      LMDB_VERSION_${component}
      "${LMDB_VERSION_${component}_LINE}")
  endforeach()
  set(LMDB_VERSION
    "${LMDB_VERSION_MAJOR}.${LMDB_VERSION_MINOR}.${LMDB_VERSION_PATCH}")
endif()

find_package_handle_standard_args(
  LMDB
  REQUIRED_VARS LMDB_LIBRARY LMDB_INCLUDE_DIR
  VERSION_VAR LMDB_VERSION
)

if(LMDB_FOUND)
  set(LMDB_INCLUDE_DIRS "${LMDB_INCLUDE_DIR}")
  set(LMDB_LIBRARIES "${LMDB_LIBRARY}")

  if(NOT TARGET LMDB::LMDB)
    add_library(LMDB::LMDB UNKNOWN IMPORTED)
    set_target_properties(
      LMDB::LMDB PROPERTIES
        IMPORTED_LOCATION "${LMDB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LMDB_INCLUDE_DIR}"
    )
  endif()
endif()

mark_as_advanced(
  LMDB_INCLUDE_DIR
  LMDB_LIBRARY
)
