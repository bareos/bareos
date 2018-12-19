#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


# extract version number from version.h
file(STRINGS ${CMAKE_SOURCE_DIR}/src/include/version.h VERSION_INFO REGEX define.*VERSION.*)
string(REGEX MATCH [0-9.]+ VERSION ${VERSION_INFO})
string(REGEX MATCH [0-9]+ SOVERSION ${VERSION_INFO})
string(REGEX MATCH \".*\" FULLVERSION ${VERSION_INFO})

# extract date from version.h
file(STRINGS ${CMAKE_SOURCE_DIR}/src/include/version.h DATE_INFO REGEX define.*BDATE.*)
string(REGEX MATCH \".*\" DATE ${DATE_INFO})
string(REGEX REPLACE "\"" "" DATE ${DATE})


# extract  db version from cats.h
file(STRINGS ${CMAKE_SOURCE_DIR}/src/cats/cats.h DB_VERSION_INFO REGEX .*BDB_VERSION.*)
string(REGEX MATCH [0-9]+ BDB_VERSION ${DB_VERSION_INFO})
