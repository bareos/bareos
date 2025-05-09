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
FindPAM
-----------

Finds PAM.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``PAM::pam``
  Main PAM library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``PAM_FOUND``
  true if PAM headers and library were found
``PAM_LIBRARY``
  PAM library to be linked
``PAM_INCLUDE_DIR``
  the directory containing PAM headers

#]=======================================================================]

find_path(PAM_INCLUDE_DIR NAMES "security/pam_appl.h" "pam/pam_appl.h")
find_library(PAM_LIBRARY NAMES "pam")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PAM REQUIRED_VARS PAM_INCLUDE_DIR PAM_LIBRARY)
mark_as_advanced(PAM_INCLUDE_DIR PAM_LIBRARY)

if(PAM_FOUND)
  if(NOT TARGET PAM::pam)
    add_library(PAM::pam SHARED IMPORTED)
    set_target_properties(
      PAM::pam PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${PAM_INCLUDE_DIR}"
    )
    set_target_properties(
      PAM::pam PROPERTIES IMPORTED_LOCATION "${PAM_LIBRARY}"
    )
  endif()
endif()
