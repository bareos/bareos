#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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


function (bareos_install_required_dlls required_dlls_file target_sbin_directory)
  cmake_policy(SET CMP0007 NEW)
  message(STATUS "install_required_dlls(): ${required_dlls_file}")
  file(READ ${required_dlls_file} REQUIRED_DLLS)
  string(REGEX REPLACE "[\r\n ]" ";" REQUIRED_DLLS ${REQUIRED_DLLS})
  list(REMOVE_DUPLICATES REQUIRED_DLLS)
  list(SORT REQUIRED_DLLS)
  #message(STATUS "NO DUPS: ${REQUIRED_DLLS}")
  #list(REMOVE_ITEM REQUIRED_DLLS "")
  #message(STATUS "NO EMPTY ELEMENT: ${REQUIRED_DLLS}")
  foreach (library ${REQUIRED_DLLS})
    message(STATUS "INSTALL${library} -> ${target_sbin_directory}")
    file(INSTALL "${library}" DESTINATION "${target_sbin_directory}")
  endforeach()
  #message(FATAL_ERROR "${REQUIRED_DLLS}")
endfunction()
