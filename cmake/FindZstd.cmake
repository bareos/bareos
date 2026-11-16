# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#[=======================================================================[.rst:
FindZstd
--------

Find Zstandard headers and libraries.  The module first accepts the CMake
configuration supplied by vcpkg and then falls back to a regular header and
library search, which is useful for source-built Unix installations.

IMPORTED Targets
^^^^^^^^^^^^^^^^

``Zstd::Zstd``
  Zstandard library.

Result variables
^^^^^^^^^^^^^^^^

``Zstd_FOUND``
  True if Zstandard was found.
``Zstd_INCLUDE_DIR``
  Where to find zstd.h when the fallback search is used.
``Zstd_LIBRARY``
  Zstandard library found by the fallback search.
#]=======================================================================]

find_package(zstd CONFIG QUIET)

if(TARGET zstd::libzstd_shared)
  add_library(Zstd::Zstd INTERFACE IMPORTED)
  set_property(
    TARGET Zstd::Zstd PROPERTY INTERFACE_LINK_LIBRARIES zstd::libzstd_shared
  )
  set(Zstd_FOUND TRUE)
  return()
elseif(TARGET zstd::libzstd)
  add_library(Zstd::Zstd INTERFACE IMPORTED)
  set_property(
    TARGET Zstd::Zstd PROPERTY INTERFACE_LINK_LIBRARIES zstd::libzstd
  )
  set(Zstd_FOUND TRUE)
  return()
endif()

find_path(Zstd_INCLUDE_DIR NAMES zstd.h)
find_library(Zstd_LIBRARY NAMES zstd libzstd)
mark_as_advanced(Zstd_INCLUDE_DIR Zstd_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Zstd REQUIRED_VARS Zstd_LIBRARY Zstd_INCLUDE_DIR
)

if(Zstd_FOUND AND NOT TARGET Zstd::Zstd)
  add_library(Zstd::Zstd UNKNOWN IMPORTED)
  set_target_properties(
    Zstd::Zstd PROPERTIES IMPORTED_LOCATION "${Zstd_LIBRARY}"
                          INTERFACE_INCLUDE_DIRECTORIES "${Zstd_INCLUDE_DIR}"
  )
endif()
