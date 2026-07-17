#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2025 Bareos GmbH & Co. KG
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(DISTARCH $ENV{VSCMD_ARG_TGT_ARCH})
  set(PLATFORM Windows-${DISTARCH})

  # DISTVER is only used in GetOsInfoString() on Non-Windows O However, we set
  # it to DISTARCH to have at least a valid value
  set(DISTVER ${DISTARCH})

  set(Host ${CMAKE_SYSTEM})
  set(Distribution ${PLATFORM})
else() # NOT Windows
  if(NOT DEFINED DISTINFO)
    execute_process(
      COMMAND ${CMAKE_CURRENT_LIST_DIR}/distname.sh
      OUTPUT_VARIABLE DISTINFOTXT
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(DISTINFO
        ${DISTINFOTXT}
        CACHE INTERNAL "Distribution info from distname.sh"
    )
    mark_as_advanced(DISTINFO)
  endif()
  list(LENGTH DISTINFO DISTINFO_LENGTH)
  list(GET DISTINFO 0 PLATFORM)
  list(GET DISTINFO 1 DISTVER)
  set(Distribution ${LSB_RELEASE_ID_SHORT})
  set(DISTVER ${DISTVER})
  set(Host "${CMAKE_SYSTEM} ${LSB_RELEASE_DESCRIPTION}")
endif()
set(BAREOS_PLATFORM ${PLATFORM})

if(NOT DEFINED DIST_VERSION_ID)
  cmake_host_system_information(RESULT DIST_VERSION_ID QUERY DISTRIB_VERSION_ID)
  set(DIST_VERSION_ID
      "${DIST_VERSION_ID}"
      CACHE INTERNAL "DISTRIB_VERSION_ID from cmake host system"
  )
  mark_as_advanced(DIST_VERSION_ID)
endif()
