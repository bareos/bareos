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

# taken from
# https://raw.githubusercontent.com/ximion/limba/master/data/cmake/systemdservice.cmake
#
# Find systemd service dir

include(FindPkgConfig)
if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  pkg_check_modules(SYSTEMD "systemd")
  if(SYSTEMD_FOUND AND "${SYSTEMD_UNITDIR}" STREQUAL "")
    execute_process(
      COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd
      OUTPUT_VARIABLE SYSTEMD_UNITDIR
    )
    string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_UNITDIR "${SYSTEMD_UNITDIR}")
  elseif(NOT SYSTEMD_FOUND AND SYSTEMD_UNITDIR)
    message(FATAL_ERROR "Variable SYSTEMD_UNITDIR is\
      defined, but we can't find systemd using pkg-config"
    )
  endif()
endif()
if(SYSTEMD_FOUND)
  set(WITH_SYSTEMD "ON")
  message(STATUS "systemd services install dir: ${SYSTEMD_UNITDIR}")
else()
  set(WITH_SYSTEMD "OFF")
endif(SYSTEMD_FOUND)
