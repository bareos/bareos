#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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

project(bareos NONE)

IF(docs-build-json)
# check for wrong combinations of build flags
  IF(docs-only)
    MESSAGE(FATAL_ERROR "\n\"docs-build-json\" needs bareos-executables and cannot be combined with \"docs-only\"")
  ENDIF()
  IF(NOT traymonitor)
    MESSAGE(FATAL_ERROR "\n\"docs-build-json\" needs the traymonitor executable: add \"-Dtraymonitor=yes\"")
  ENDIF()
ENDIF()

IF(docs-only)
  set(BUILD_BAREOS_BINARIES no)
ELSE()
  set(BUILD_BAREOS_BINARIES yes)
ENDIF()

IF(BUILD_BAREOS_BINARIES)
  add_subdirectory(core)
  add_subdirectory(webui)
ENDIF()

IF(docs OR docs-only OR docs-build-json)
   add_subdirectory(docs/manuals)
ENDIF()

