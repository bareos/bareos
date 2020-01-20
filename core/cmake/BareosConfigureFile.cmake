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

#
# configure file all *.in files
#

file(GLOB_RECURSE IN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
foreach(in_file ${IN_FILES})
  string(
    REGEX
    REPLACE
      ".in\$"
      ""
      file
      ${in_file}
  )
  message(STATUS "creating file ${file}")
  configure_file(${in_file} ${file} @ONLY)
endforeach()
