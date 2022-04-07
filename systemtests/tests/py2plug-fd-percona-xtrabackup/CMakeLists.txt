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
# -D XTRABACKUP_BINARY=/usr/bin/xtrabackup \ -D
# MYSQL_DAEMON_BINARY=/opt/mysql/bin/mysqld \ -D
# MYSQL_CLIENT_BINARY=/usr/bin/mysql \

if(TARGET python-fd
   AND XTRABACKUP_BINARY
   AND MYSQL_DAEMON_BINARY
   AND MYSQL_CLIENT_BINARY
)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-percona-xtrabackup")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX}
    "py2plug-fd-percona-xtrabackup"
    DISABLED
    COMMENT
    "
    no python-fd or XTRABACKUP_BINARY,MYSQL_DAEMON_BINARY or MYSQL_CLIENT_BINARY not set.)
    example:
    cmake -D XTRABACKUP_BINARY=/usr/bin/xtrabackup
    -D MYSQL_DAEMON_BINARY=/opt/mysql/bin/mysqld
    -D MYSQL_CLIENT_BINARY=/usr/bin/mysql"
  )
endif()

if(TARGET python3-fd
   AND XTRABACKUP_BINARY
   AND MYSQL_DAEMON_BINARY
   AND MYSQL_CLIENT_BINARY
)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-percona-xtrabackup")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX}
    "py3plug-fd-percona-xtrabackup"
    DISABLED
    COMMENT
    "
    no python3-fd or XTRABACKUP_BINARY,MYSQL_DAEMON_BINARY or MYSQL_CLIENT_BINARY not set.)
    example:
    cmake -D XTRABACKUP_BINARY=/usr/bin/xtrabackup
    -D MYSQL_DAEMON_BINARY=/opt/mysql/bin/mysqld
    -D MYSQL_CLIENT_BINARY=/usr/bin/mysql"
  )
endif()
