#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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

IF(NOT client-only)
   IF(NOT HAVE_WIN32)

      MESSAGE( STATUS "UNITDIR: " ${UNITDIR} )
      MESSAGE( STATUS "DISTNAME: " ${DISTNAME} )

      set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
      if (${DISTNAME} STREQUAL ubuntu)
         add_subdirectory(debian)
      elseif (${DISTNAME} STREQUAL hpux)
         MESSAGE( STATUS "DISTNAME: " ${DISTNAME} )
      else()
         add_subdirectory(${DISTNAME})
      endif()

      if (${systemd})
         add_subdirectory(systemd)
      endif()

   ENDIF()
ENDIF()
