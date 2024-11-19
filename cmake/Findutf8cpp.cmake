# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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
Findutf8cpp
-----------

Find utf8cpp headers and libraries.
`
IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``utf8cpp::utf8cpp``
  utf8cpp library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``UTF8CPP_FOUND``
True if library found.
``UTF8CPP_INCLUDE_DIRS``
Where to find utf8.h.
``HAVE_UTF8CPP``
1 if utf8cpp found.

#]=======================================================================]

find_path(
  UTF8CPP_INCLUDE_DIRS
  NAMES utf8.h
  PATH_SUFFIXES utf8cpp
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(utf8cpp REQUIRED_VARS UTF8CPP_INCLUDE_DIRS)

mark_as_advanced(UTF8CPP_INCLUDE_DIRS)

if(UTF8CPP_FOUND AND NOT TARGET utf8cpp::utf8cpp)
  add_library(utf8cpp::utf8cpp INTERFACE IMPORTED)
  set_target_properties(
    utf8cpp::utf8cpp PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                "${UTF8CPP_INCLUDE_DIRS}"
  )
endif()
