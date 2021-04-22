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
FindJansson
-----------

Find Jansson headers and libraries.
`
IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``Jansson::Jansson``
  Jansson library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``JANSSON_FOUND``
  True if Jansson found.
``JANSSON_INCLUDE_DIRS``
  Where to find jansson.h.
``JANSSON_LIBRARIES``
  List of libraries when using Jansson.
``JANSSON_VERSION_STRING``
  The version of Jansson found.
``HAVE_JANSSON``
  1 if Jansson found.

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_JANSSON QUIET jansson)

find_path(
  JANSSON_INCLUDE_DIR
  NAMES jansson.h
  HINTS ${PC_JANSSON_INCLUDEDIR} ${PC_JANSSON_INCLUDE_DIRS}
)

find_library(
  JANSSON_LIBRARY
  NAMES jansson libjansson
  HINTS ${PC_JANSSON_LIBDIR} ${PC_JANSSON_LIBRARY_DIRS}
)

if(PC_JANSSON_VERSION)
  set(JANSSON_VERSION_STRING ${PC_JANSSON_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Jansson
  REQUIRED_VARS JANSSON_LIBRARY JANSSON_INCLUDE_DIR
  VERSION_VAR JANSSON_VERSION_STRING
)

if(JANSSON_FOUND)
  set(JANSSON_LIBRARIES ${JANSSON_LIBRARY})
  set(JANSSON_INCLUDE_DIRS ${JANSSON_INCLUDE_DIR})
  set(HAVE_JANSSON 1)
endif()

mark_as_advanced(JANSSON_INCLUDE_DIR JANSSON_LIBRARY)

if(JANSSON_FOUND AND NOT TARGET Jansson::Jansson)
  add_library(Jansson::Jansson UNKNOWN IMPORTED)
  set_target_properties(
    Jansson::Jansson PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                "${JANSSON_INCLUDE_DIRS}"
  )
  set_target_properties(
    Jansson::Jansson PROPERTIES INTERFACE_COMPILE_OPTIONS
                                "${JANSSON_DEFINITIONS}"
  )
  set_property(
    TARGET Jansson::Jansson
    APPEND
    PROPERTY IMPORTED_LOCATION "${JANSSON_LIBRARY}"
  )
endif()
