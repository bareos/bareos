# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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
FindxxHash
-----------

Find xxHash headers and libraries.
`
IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``xxHash::xxhash``
  xxHash library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``XXHASH_FOUND``
True if xxHash found.
``XXHASH_INCLUDE_DIRS``
  Where to find xxhash.h.
  ``XXHASH_LIBRARIES``
List of libraries when using xxHash.
``XXHASH_VERSION_STRING``
The version of xxHash found.
``HAVE_XXHASH``
1 if xxHash found.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_XXHASH QUIET libxxhash)

find_path(
  XXHASH_INCLUDE_DIR
  NAMES xxhash.h
  HINTS ${PC_XXHASH_INCLUDEDIR} ${PC_XXHASH_INCLUDE_DIRS}
)

find_library(
  XXHASH_LIBRARY
  NAMES xxhash libxxhash
  HINTS ${PC_XXHASH_LIBDIR} ${PC_XXHASH_LIBRARY_DIRS}
)

if(PC_XXHASH_VERSION)
  set(XXHASH_VERSION_STRING ${PC_XXHASH_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  xxHash
  REQUIRED_VARS XXHASH_LIBRARY XXHASH_INCLUDE_DIR
  VERSION_VAR XXHASH_VERSION_STRING
)

if(XXHASH_FOUND)
  set(XXHASH_LIBRARIES ${XXHASH_LIBRARY})
  set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIR})
  set(HAVE_XXHASH 1)
endif()

mark_as_advanced(XXHASH_INCLUDE_DIR XXHASH_LIBRARY)

if(XXHASH_FOUND AND NOT TARGET xxHash::xxhash)
  add_library(xxHash::xxhash UNKNOWN IMPORTED)
  set_target_properties(
    xxHash::xxhash PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                              "${XXHASH_INCLUDE_DIRS}"
  )
  set_target_properties(
    xxHash::xxhash PROPERTIES INTERFACE_COMPILE_OPTIONS "${XXHASH_DEFINITIONS}"
  )
  set_property(
    TARGET xxHash::xxhash
    APPEND
    PROPERTY IMPORTED_LOCATION "${XXHASH_LIBRARY}"
  )
endif()
