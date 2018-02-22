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

cmake_minimum_required(VERSION 3.0)
# (for AUTOUIC)

IF(HAVE_WIN32)
  add_definitions(-D_STAT_DEFINED=1)
ENDIF()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


find_package(Qt4 REQUIRED)

IF(HAVE_WIN32)
   include_directories(
      ${CMAKE_SOURCE_DIR}/src/win32/include
      ${CMAKE_SOURCE_DIR}/src/win32/lib
      ../include
      ..
      ../lib
      )
ELSE()
   include_directories(. .. ../include ../lib)
ENDIF()


set(SOURCES
   tray-monitor.cc authenticate.cc tray_conf.cc tray-monitor.cc
   traymenu.cc systemtrayicon.cc mainwindow.cc monitoritem.cc
   monitoritemthread.cc
   )

IF(HAVE_WIN32)
   LIST(APPEND SOURCES ../win32/qt-tray-monitor/traymon.rc)
ENDIF()

set(TRAYMON_LIBRARIES bareos-tray-monitor Qt4::QtGui bareos bareoscfg)

IF(HAVE_WIN32)
   LIST(APPEND TRAYMON_LIBRARIES
      bareosstatic
      )
ENDIF()

IF(HAVE_WIN32)
   add_executable(bareos-tray-monitor WIN32 ${SOURCES} main.qrc)
ELSE()
   add_executable(bareos-tray-monitor ${SOURCES} main.qrc)
ENDIF()

target_link_libraries(${TRAYMON_LIBRARIES})

INSTALL(TARGETS bareos-tray-monitor DESTINATION "${bindir}")

install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")

INSTALL(SCRIPT ${CMAKE_SOURCE_DIR}/cmake/install-tray-monitor-configfiles.cmake)

INSTALL(FILES bareos-tray-monitor.desktop DESTINATION ${datarootdir}/applications/)
INSTALL(FILES bareos-tray-monitor.desktop DESTINATION ${sysconfdir}/xdg/autostart/ )
INSTALL(FILES bareos-tray-monitor.xpm DESTINATION ${datarootdir}/pixmaps/)
