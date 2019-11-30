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

cmake_minimum_required(VERSION 3.0)
# (for AUTOUIC)

if(HAVE_WIN32)
  add_definitions(-D_STAT_DEFINED=1)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Core Widgets)
if(Qt5Widgets_FOUND)
  message(STATUS "Found QT5Widgets")
else()
  message(STATUS "QT5Widgets NOT found, checking for Qt4 ...")
  find_package(Qt4)
  if(NOT Qt4_FOUND)
    message(FATAL_ERROR "Both Qt5 and Qt4 not found, cannot build tray-monitor")
  endif()
endif()

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/include ${PROJECT_SOURCE_DIR}/src/win32/lib
  )
endif()

set(SOURCES
    tray-monitor.cc
    authenticate.cc
    tray_conf.cc
    tray-monitor.cc
    traymenu.cc
    systemtrayicon.cc
    mainwindow.cc
    monitoritem.cc
    monitoritemthread.cc
)

if(HAVE_WIN32)
  list(APPEND SOURCES ../win32/qt-tray-monitor/traymon.rc)
endif()

if(Qt4_FOUND)
  set(TRAYMON_LIBRARIES bareos-tray-monitor Qt4::QtGui bareos)
else()
  set(TRAYMON_LIBRARIES bareos-tray-monitor bareos)
endif()

if(HAVE_WIN32)
  add_executable(bareos-tray-monitor WIN32 ${SOURCES} main.qrc)
else()
  add_executable(bareos-tray-monitor ${SOURCES} main.qrc)
endif()

if(Qt5Widgets_FOUND)
  qt5_use_modules(bareos-tray-monitor Widgets)
endif()

target_link_libraries(${TRAYMON_LIBRARIES})

install(TARGETS bareos-tray-monitor DESTINATION "${bindir}")

install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")

install(
  SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-tray-monitor-configfiles.cmake
)

install(FILES bareos-tray-monitor.desktop
        DESTINATION ${datarootdir}/applications/
)
install(FILES bareos-tray-monitor.desktop
        DESTINATION ${sysconfdir}/xdg/autostart/
)
install(FILES bareos-tray-monitor.xpm DESTINATION ${datarootdir}/pixmaps/)
