#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2026 Bareos GmbH & Co. KG
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

if(NOT HAVE_WIN32)

  message(STATUS "UNITDIR: " ${UNITDIR})
  message(STATUS "BAREOS_PLATFORM: " ${BAREOS_PLATFORM})

  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
  if(${BAREOS_PLATFORM} STREQUAL ubuntu)
    add_subdirectory(debian)
  elseif(${systemd})
    add_subdirectory(systemd)
  elseif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${BAREOS_PLATFORM}")
    add_subdirectory(${BAREOS_PLATFORM})
  endif()
else()
  add_subdirectory(win32)
endif()
