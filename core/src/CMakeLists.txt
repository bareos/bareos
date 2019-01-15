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
   add_subdirectory(tools)
   IF(NOT HAVE_WIN32 AND GTEST_FOUND)
      add_subdirectory(tests)
   ENDIF()
   add_subdirectory(cats)
   IF(NOT HAVE_WIN32)
      add_subdirectory(ndmp)
   ENDIF()
   add_subdirectory(dird)
   add_subdirectory(stored)
ENDIF()

add_subdirectory(console)
IF(${HAVE_LMDB})
add_subdirectory(lmdb)
ENDIF()
add_subdirectory(lib)
add_subdirectory(findlib)
add_subdirectory(filed)
add_subdirectory(plugins)

if (${traymonitor})
   add_subdirectory(qt-tray-monitor)
endif()
