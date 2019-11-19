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

include(BareosVersion OPTIONAL RESULT_VARIABLE BareosVersionFile)
if(BareosVersionFile STREQUAL "NOTFOUND")
  # no version file, try data from git
  if(GIT_DESCRIBE_VERSION)
    message(STATUS "Using version information from Git")
    if(DEFINED VERSION_STRING)
      message(STATUS "VERSION_STRING already set to ${VERSION_STRING}. Will not overwrite")
    else()
      set(VERSION_STRING "${GIT_DESCRIBE_VERSION}")
    endif()
    set(VERSION_TIMESTAMP "${GIT_COMMIT_TIMESTAMP}")
  else()
    message(
      FATAL_ERROR
        "VERSION_STRING not set, BareosVersion.cmake not found and no version data from git available."
      )
  endif()
else()
  message(STATUS "Using version information from ${BareosVersionFile}")
endif()

string(REGEX MATCH
             [0-9.a-zA-Z~]+
             BAREOS_FULL_VERSION
             ${VERSION_STRING})

if(BAREOS_FULL_VERSION STREQUAL "")
  message(FATAL_ERROR "BAREOS_FULL_VERSION is not set")
endif()

set(BAREOS_FULL_VERSION ${BAREOS_FULL_VERSION} PARENT_SCOPE)

string(REGEX MATCH
             [0-9]+.[0-9]+.[0-9]+
             BAREOS_NUMERIC_VERSION
             ${VERSION_STRING})

string(REPLACE "."
               ";"
               VERSION_LIST
               ${BAREOS_NUMERIC_VERSION})
list(GET VERSION_LIST 0 BAREOS_VERSION_MAJOR)
list(GET VERSION_LIST 1 BAREOS_VERSION_MINOR)
list(GET VERSION_LIST 2 BAREOS_VERSION_PATCH)

message("BAREOS_NUMERIC_VERSION is ${BAREOS_NUMERIC_VERSION}")
message("BAREOS_FULL_VERSION is ${BAREOS_FULL_VERSION}")

if(VERSION_TIMESTAMP GREATER 0)
  include(BareosTimeFunctions)
  timestamp_at("${VERSION_TIMESTAMP}" DATE "%d %B %Y")
  timestamp_at("${VERSION_TIMESTAMP}" BAREOS_SHORT_DATE "%d%b%y")
  timestamp_at("${VERSION_TIMESTAMP}" BAREOS_YEAR "%Y")
  timestamp_at("${VERSION_TIMESTAMP}" BAREOS_PROG_DATE_TIME "%Y-%m-%d %H:%M:%S")
else()
  message(FATAL_ERROR "VERSION_TIMESTAMP is not set")
endif()

# extract  db version from cats.h
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../src/cats/cats.h DB_VERSION_STRING
     REGEX .*BDB_VERSION.*)
string(REGEX MATCH
             [0-9]+
             BDB_VERSION
             ${DB_VERSION_STRING})
