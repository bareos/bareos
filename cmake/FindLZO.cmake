# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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
FindLZO
-----------

Find LZO headers and libraries.
`
IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``LZO::LZO``
LZO library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``LZO_FOUND``
True if LZO found.
``LZO_INCLUDE_DIRS``
  Where to find jansson.h.
``LZO_LIBRARIES``
List of libraries when using LZO.
``LZO_VERSION_STRING``
The version of LZO found.
``HAVE_LZO``
1 if LZO found.

#]=======================================================================]

find_path(LZO_INCLUDE_DIR NAMES lzo/lzoconf.h)
mark_as_advanced(LZO_INCLUDE_DIR)

if(WIN32)
  find_library(LZO_IMPLIB NAMES lzo2)

  get_filename_component(LZO_LIBDIR "${LZO_IMPLIB}" DIRECTORY)
  find_library(
    LZO_IMPLIB_DEBUG
    NAMES lzo2
    PATHS ${LZO_LIBDIR} ${LZO_LIBDIR}/..
    PATH_SUFFIXES "debug/lib"
  )

  find_file(
    LZO_LIBRARY
    NAMES lzo2.dll
    PATHS ${LZO_LIBDIR} ${LZO_LIBDIR}/..
    PATH_SUFFIXES "bin"
  )
  find_file(
    LZO_LIBRARY_DEBUG
    NAMES lzo2.dll
    PATHS ${LZO_LIBDIR} ${LZO_LIBDIR}/..
    PATH_SUFFIXES "debug/bin"
  )
  mark_as_advanced(LZO_LIBRARY LZO_IMPLIB LZO_LIBRARY_DEBUG LZO_IMPLIB_DEBUG)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(LZO_LIBRARY "${HOMEBREW_PREFIX}/opt/lzo/lib/liblzo2.a")
  mark_as_advanced(LZO_LIBRARY)
else()
  find_library(LZO_LIBRARY NAMES lzo2)
  mark_as_advanced(LZO_LIBRARY)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZO REQUIRED_VARS LZO_LIBRARY LZO_INCLUDE_DIR)

if(LZO_FOUND AND NOT TARGET LZO::LZO)
  add_library(LZO::LZO SHARED IMPORTED)
  set_target_properties(
    LZO::LZO PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LZO_INCLUDE_DIRS}"
  )
  set_property(
    TARGET LZO::LZO
    APPEND
    PROPERTY IMPORTED_LOCATION "${LZO_LIBRARY}"
  )
  if(LZO_LIBRARY_DEBUG)
    set_property(
      TARGET LZO::LZO
      APPEND
      PROPERTY IMPORTED_LOCATION_DEBUG "${LZO_LIBRARY_DEBUG}"
    )
  endif()
  if(LZO_IMPLIB)
    set_property(
      TARGET LZO::LZO
      APPEND
      PROPERTY IMPORTED_IMPLIB "${LZO_IMPLIB}"
    )
  endif()
  if(LZO_IMPLIB_DEBUG)
    set_property(
      TARGET LZO::LZO
      APPEND
      PROPERTY IMPORTED_IMPLIB_DEBUG "${LZO_IMPLIB_DEBUG}"
    )
  endif()
endif()
