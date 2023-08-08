#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2022 Bareos GmbH & Co. KG
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

# This test requires the following variables to be set:
#
# -D MYSQL_DAEMON_BINARY=/opt/mysql/bin/mysqld -D
# MYSQL_CLIENT_BINARY=/usr/bin/mysql

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

if(TARGET python-fd
   AND MYSQL_DAEMON_BINARY
   AND MYSQL_CLIENT_BINARY
)
  create_systemtest(${SYSTEMTEST_PREFIX} py2plug-fd-contrib-bareos_tasks_mysql)
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX}
    py2plug-fd-contrib-bareos_tasks_mysql
    DISABLED
    COMMENT
    "
    no python-fd or MYSQL_DAEMON_BINARY or MYSQL_CLIENT_BINARY not set.)
    example:
    cmake
    -D MYSQL_DAEMON_BINARY=/opt/mysql/bin/mysqld
    -D MYSQL_CLIENT_BINARY=/usr/bin/mysql"
  )
endif()

if(TARGET python3-fd
   AND MYSQL_DAEMON_BINARY
   AND MYSQL_CLIENT_BINARY
)
  create_systemtest(${SYSTEMTEST_PREFIX} py3plug-fd-contrib-bareos_tasks_mysql)
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX}
    py3plug-fd-contrib-bareos_tasks_mysql
    DISABLED
    COMMENT
    "
    no python3-fd or MYSQL_DAEMON_BINARY or MYSQL_CLIENT_BINARY not set.)
    example:
    cmake
    -D MYSQL_DAEMON_BINARY=/opt/mysql/bin/mysqld
    -D MYSQL_CLIENT_BINARY=/usr/bin/mysql"
  )
endif()
