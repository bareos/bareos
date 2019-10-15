# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2017-2019 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

if(NOT DEFINED VERSION_STRING)
  include(BareosVersion OPTIONAL RESULT_VARIABLE BareosVersionFile)
  if(BareosVersionFile STREQUAL "NOTFOUND")
    message(
      FATAL_ERROR "VERSION_STRING not set and BareosVersion.cmake was not found"
      )
  else()
    message("Using version information from ${BareosVersionFile}")
  endif()
endif()

string(REGEX MATCH
             [0-9.a-zA-Z]+
             BAREOS_FULL_VERSION
             ${VERSION_STRING})
string(REGEX MATCH
             [0-9]+.[0-9]+.[0-9]+
             BAREOS_NUMERIC_VERSION
             ${VERSION_STRING})
string(REGEX MATCH
             [0-9]+
             SOVERSION
             ${VERSION_STRING})
string(REPLACE "\""
               ""
               BAREOS_FULL_VERSION
               ${BAREOS_FULL_VERSION})

message(STATUS "BareosExtractVersionInfo: BAREOS_FULL_VERSION is "
               ${BAREOS_FULL_VERSION})
