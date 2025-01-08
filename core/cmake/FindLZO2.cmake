# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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
FindLZO2
-----------

Find LZO2 headers and libraries.
`
IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``LZO2::LZO2``
  LZO2 library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``LZO2_FOUND``
  True if LZO2 found.
``LZO2_INCLUDE_DIRS``
  Where to find lzo/lzo1.h.
``LZO2_LIBRARIES``
  List of libraries when using LZO2.
``LZO2_VERSION_STRING``
  The version of LZO2 found.
``HAVE_LZO2``
  1 if LZO2 found.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LZO2 QUIET lzo2)

find_path(
  LZO2_INCLUDE_DIR
  NAMES lzo/lzo1x.h
  HINTS ${PC_LZO2_INCLUDEDIR} ${PC_LZO2_INCLUDE_DIRS}
)

find_library(
  LZO2_LIBRARY
  NAMES lzo2 liblzo2
  HINTS ${PC_LZO2_LIBDIR} ${PC_LZO2_LIBRARY_DIRS}
)

if(PC_LZO2_VERSION)
  set(LZO2_VERSION_STRING ${PC_LZO2_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LZO2
  REQUIRED_VARS LZO2_LIBRARY LZO2_INCLUDE_DIR
  VERSION_VAR LZO2_VERSION_STRING
)
mark_as_advanced(LZO2_INCLUDE_DIR LZO2_LIBRARY)

if(LZO2_FOUND AND NOT TARGET LZO2::LZO2)
  add_library(LZO2::LZO2 UNKNOWN IMPORTED)
  set_target_properties(
    LZO2::LZO2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                "${LZO2_INCLUDE_DIRS}"
  )
  set_target_properties(
    LZO2::LZO2 PROPERTIES INTERFACE_COMPILE_OPTIONS
                                "${LZO2_DEFINITIONS}"
  )
  set_property(
    TARGET LZO2::LZO2
    APPEND
    PROPERTY IMPORTED_LOCATION "${LZO2_LIBRARY}"
  )
endif()
