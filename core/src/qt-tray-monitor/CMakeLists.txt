#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2024 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

if(HAVE_WIN32)
  add_definitions(-D_STAT_DEFINED=1)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Widgets)
if(Qt6Widgets_FOUND)
  message(STATUS "Found QT6Widgets")
else()
  message(STATUS "QT6Widgets NOT found, checking for Qt5 ...")
  find_package(Qt5 COMPONENTS Core Widgets)
  if(Qt5Widgets_FOUND)
    message(STATUS "Found QT5Widgets")
  else()
    message(STATUS "QT5Widgets NOT found, checking for Qt4 ...")
    find_package(Qt4)
    if(NOT Qt4_FOUND)
      message(
        FATAL_ERROR "Both Qt5 and Qt4 not found, cannot build tray-monitor"
      )
    endif()
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

if(HAVE_WIN32)
  add_executable(bareos-tray-monitor WIN32 ${SOURCES} main.qrc)
else()
  add_executable(bareos-tray-monitor ${SOURCES} main.qrc)
endif()

target_link_libraries(bareos-tray-monitor PRIVATE bareos CLI11::CLI11)

if(Qt4_FOUND)
  target_link_libraries(bareos-tray-monitor PRIVATE Qt4::QtGui)
endif()

if(TARGET Qt5::Widgets)
  target_link_libraries(bareos-tray-monitor PRIVATE Qt5::Widgets)
endif()

if(TARGET Qt6::Widgets)
  target_link_libraries(bareos-tray-monitor PRIVATE Qt6::Widgets)
endif()

include(BareosConfigureFile)
bareos_configure_file(FILES bareos-tray-monitor.desktop.in)

install(TARGETS bareos-tray-monitor DESTINATION "${bindir}")

install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")

install(
  SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-tray-monitor-configfiles.cmake
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bareos-tray-monitor.desktop
        DESTINATION ${datarootdir}/applications/
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bareos-tray-monitor.desktop
        DESTINATION ${sysconfdir}/xdg/autostart/
)
install(
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/../images/bareos-logo_128x128.png
  DESTINATION ${datarootdir}/pixmaps/
  RENAME bareos-tray-monitor.png
)

include(DebugEdit)
