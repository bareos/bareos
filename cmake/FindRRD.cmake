#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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
FindRRD
-------

Find RRDtool headers and libraries.

IMPORTED Targets
^^^^^^^^^^^^^^^^

``RRD::RRD``
  RRDtool library.

Result variables
^^^^^^^^^^^^^^^^

``RRD_FOUND``
  True if RRDtool found.
``RRD_INCLUDE_DIRS``
  Where to find rrd.h.
``RRD_LIBRARIES``
  List of libraries when using RRDtool.
``RRD_VERSION_STRING``
  The version of RRDtool found.
``HAVE_RRD``
  1 if RRDtool found.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_RRD QUIET librrd)

find_path(
  RRD_INCLUDE_DIR
  NAMES rrd.h
  HINTS ${PC_RRD_INCLUDEDIR} ${PC_RRD_INCLUDE_DIRS}
)

find_library(
  RRD_LIBRARY
  NAMES rrd librrd
  HINTS ${PC_RRD_LIBDIR} ${PC_RRD_LIBRARY_DIRS}
)

if(PC_RRD_VERSION)
  set(RRD_VERSION_STRING ${PC_RRD_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  RRD
  REQUIRED_VARS RRD_LIBRARY RRD_INCLUDE_DIR
  VERSION_VAR RRD_VERSION_STRING
)

if(RRD_FOUND)
  set(RRD_LIBRARIES ${RRD_LIBRARY})
  set(RRD_INCLUDE_DIRS ${RRD_INCLUDE_DIR})
  set(HAVE_RRD 1)
endif()

mark_as_advanced(RRD_INCLUDE_DIR RRD_LIBRARY)

if(RRD_FOUND AND NOT TARGET RRD::RRD)
  add_library(RRD::RRD SHARED IMPORTED)
  set_target_properties(
    RRD::RRD PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${RRD_INCLUDE_DIRS}"
  )
  set_property(
    TARGET RRD::RRD
    APPEND
    PROPERTY IMPORTED_LOCATION "${RRD_LIBRARY}"
  )
endif()
