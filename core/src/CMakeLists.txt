#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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

if(NOT client-only)
  add_subdirectory(tools)
  add_subdirectory(cats)
  if(NOT HAVE_WIN32 AND ndmp)
    add_subdirectory(ndmp)
  endif()
  add_subdirectory(dird)
  add_subdirectory(stored)
endif()

if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  message(status "Skipping unit tests as testing on installed files is requested")
elseif(NOT GTEST_FOUND)
  message(status "Skipping unit tests as gtest was not found")
else()
  add_subdirectory(tests)
endif()

add_subdirectory(console)
if(${HAVE_LMDB})
  add_subdirectory(lmdb)
endif()
add_subdirectory(lib)
add_subdirectory(findlib)
add_subdirectory(filed)
add_subdirectory(plugins)
add_subdirectory(fastlz)

if(${traymonitor})
  add_subdirectory(qt-tray-monitor)
endif()
