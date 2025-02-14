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

#[=======================================================================[.rst:
Findtirpc
-----------

Finds tirpc.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``tirpc::tirpc``
  Main tirpc library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``tirpc_FOUND``
  true if tirpc headers and library were found
``tirpc_LIBRARY``
  tirpc library to be linked
``tirpc_INCLUDE_DIR``
  the directory containing tirpc headers

#]=======================================================================]

find_path(
  tirpc_INCLUDE_DIR
  NAMES "rpc/rpc.h"
  PATH_SUFFIXES "tirpc"
)
find_library(tirpc_LIBRARY NAMES "tirpc")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  tirpc REQUIRED_VARS tirpc_INCLUDE_DIR tirpc_LIBRARY
)
mark_as_advanced(tirpc_INCLUDE_DIR tirpc_LIBRARY)

if(tirpc_FOUND AND NOT TARGET tirpc::tirpc)
  add_library(tirpc::tirpc SHARED IMPORTED)
  set_target_properties(
    tirpc::tirpc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${tirpc_INCLUDE_DIR}"
                            IMPORTED_LOCATION "${tirpc_LIBRARY}"
  )
endif()
