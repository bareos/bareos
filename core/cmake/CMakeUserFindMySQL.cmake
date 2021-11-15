#   BAREOS® - Backup Archiving REcovery Open Sourced
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

# * Find mysqlclient Find the native MySQL includes and library
#
# MYSQL_INCLUDE_DIR - where to find mysql.h, etc. MYSQL_LIBRARIES   - List of
# libraries when using MySQL. MYSQL_FOUND       - True if MySQL found.

if(MYSQL_INCLUDE_DIR)
  # Already in cache, be silent
  set(MYSQL_FIND_QUIETLY TRUE)
endif(MYSQL_INCLUDE_DIR)

find_path(MYSQL_INCLUDE_DIR mysql.h /usr/local/include/mysql /usr/include/mysql)

set(MYSQL_NAMES mysqlclient mysqlclient_r)
find_library(
  MYSQL_LIBRARY
  NAMES ${MYSQL_NAMES}
  PATHS /usr/lib /usr/local/lib
  PATH_SUFFIXES mysql
)

if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  set(MYSQL_FOUND TRUE)
  set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
else(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
  set(MYSQL_FOUND FALSE)
  set(MYSQL_LIBRARIES)
endif(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)

if(MYSQL_FOUND)
  if(NOT MYSQL_FIND_QUIETLY)
    message(STATUS "Found MySQL: ${MYSQL_LIBRARY}")
  endif(NOT MYSQL_FIND_QUIETLY)
else(MYSQL_FOUND)
  if(MYSQL_FIND_REQUIRED)
    message(STATUS "Looked for MySQL libraries named ${MYSQL_NAMES}.")
    message(FATAL_ERROR "Could NOT find MySQL library")
  endif(MYSQL_FIND_REQUIRED)
endif(MYSQL_FOUND)

mark_as_advanced(MYSQL_LIBRARY MYSQL_INCLUDE_DIR)
