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

IF(${HAVE_WIN32})
  set(QT_QMAKE_EXECUTABLE /usr/x86_64-w64-mingw32/bin/qmake)
  set(QT_MOC_EXECUTABLE /usr/x86_64-w64-mingw32/bin/moc)
  set(QT_RCC_EXECUTABLE /usr/x86_64-w64-mingw32/bin/rcc)
  set(QT_UIC_EXECUTABLE /usr/x86_64-w64-mingw32/bin/uic)
  add_definitions(-DNO_OLDNAMES)
ENDIF()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


find_package(Qt4 REQUIRED)

IF(${HAVE_WIN32})
   include_directories(
      ${CMAKE_SOURCE_DIR}/src/win32/include
      ${CMAKE_SOURCE_DIR}/src/win32/lib
      ../include ..
      )
ELSE()
   include_directories(. .. ../include ../lib)
ENDIF()


set(SOURCES
   tray-monitor.cpp authenticate.cpp tray_conf.cpp tray-monitor.cpp
   traymenu.cpp systemtrayicon.cpp mainwindow.cpp monitoritem.cpp
   monitoritemthread.cpp
   )

add_executable(bareos-tray-monitor ${SOURCES} main.qrc)
target_link_libraries(bareos-tray-monitor Qt4::QtGui bareos bareoscfg)

INSTALL(TARGETS bareos-tray-monitor DESTINATION "${bindir}")

install(CODE "set(SRC_DIR \"${CMAKE_SOURCE_DIR}\")")
INSTALL(SCRIPT ${CMAKE_SOURCE_DIR}/cmake/install-tray-monitor-configfiles.cmake)

INSTALL(FILES bareos-tray-monitor.desktop DESTINATION ${datarootdir}/applications/)
INSTALL(FILES bareos-tray-monitor.desktop DESTINATION ${sysconfdir}/xdg/autostart/ )
INSTALL(FILES bareos-tray-monitor.xpm DESTINATION ${datarootdir}/pixmaps/)
