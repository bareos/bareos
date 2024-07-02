#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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
project(bareos-contrib)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
else()
  include(GNUInstallDirs)
endif()
include(BareosSetVariableDefaults)

add_subdirectory(fd-plugins)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  add_subdirectory(misc)
endif()
if(NOT client-only)
  add_subdirectory(dir-plugins)
endif()
