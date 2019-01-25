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

# VERSION_STRING can override what is stored in version.h
IF (NOT DEFINED VERSION_STRING)
  MESSAGE("VERSION_STRING is not defined, extracting version from version.h")
  # extract version number from version.h
  file(STRINGS ${PROJECT_SOURCE_DIR}/src/include/version.h VERSION_STRING REGEX define.*VERSION.*)
  string(REGEX MATCH \".*\" BAREOS_FULL_VERSION ${VERSION_STRING})
  string(REPLACE "\"" "" BAREOS_FULL_VERSION ${BAREOS_FULL_VERSION})
ELSE()
  string(REGEX MATCH [0-9.a-zA-Z]+ BAREOS_FULL_VERSION ${VERSION_STRING})
ENDIF()

string(REGEX MATCH [0-9.]+ BAREOS_NUMERIC_VERSION ${VERSION_STRING})
string(REGEX MATCH [0-9]+ SOVERSION ${VERSION_STRING})

MESSAGE("BAREOS_NUMERIC_VERSION is ${BAREOS_NUMERIC_VERSION}")
MESSAGE("BAREOS_FULL_VERSION is ${BAREOS_FULL_VERSION}")
MESSAGE("SOVERSION is ${SOVERSION}")

IF (BAREOS_FULL_VERSION STREQUAL "")
  MESSAGE(FATAL_ERROR BAREOS_FULL_VERSION is not set)
ENDIF()

# extract date from version.h
file(STRINGS ${PROJECT_SOURCE_DIR}/src/include/version.h DATE_STRING REGEX define.*BDATE.*)
string(REGEX MATCH \".*\" DATE ${DATE_STRING})
string(REGEX REPLACE "\"" "" DATE ${DATE})


# extract  db version from cats.h
file(STRINGS ${PROJECT_SOURCE_DIR}/src/cats/cats.h DB_VERSION_STRING REGEX .*BDB_VERSION.*)
string(REGEX MATCH [0-9]+ BDB_VERSION ${DB_VERSION_STRING})

