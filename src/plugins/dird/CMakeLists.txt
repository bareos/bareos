#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


IF(${HAVE_WIN32})
   include_directories(
      ${CMAKE_SOURCE_DIR}/src/win32/generic
      ${CMAKE_SOURCE_DIR}/src/win32/dird
      ${CMAKE_SOURCE_DIR}/src/win32/plugins/python/include
      ../../include
      ../../dird
      ../..)
   link_directories(${CMAKE_SOURCE_DIR}/src/win32/plugins/python/lib/64)
   add_definitions(-DMS_WIN64)
ELSE()
   include_directories(../.. . ../../include ../../dird)
ENDIF()

add_library(python-dir MODULE python-dir.cc)
# do not prefix with "lib"
set_target_properties(python-dir PROPERTIES PREFIX "")
INSTALL(TARGETS python-dir DESTINATION ${plugindir})

IF(${HAVE_WIN32})
   target_link_libraries(python-dir python27 bareos)
ENDIF()


set (PYFILES bareos-dir.py.template
             bareos-dir-class-plugin.py
             bareos_dir_consts.py
             BareosDirPluginBaseclass.py
             BareosDirWrapper.py)

INSTALL(FILES ${PYFILES} DESTINATION ${plugindir})
