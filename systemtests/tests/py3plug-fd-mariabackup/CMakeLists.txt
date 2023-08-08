#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2023 Bareos GmbH & Co. KG
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
# -D MARIABACKUP_BINARY=/usr/bin/mariabackup \ -D
# MARIADB_DAEMON_BINARY=/usr/libexec/mysqld \ -D
# MARIADB_CLIENT_BINARY=/usr/bin/mysql \ -D
# MARIADB_MYSQL_INSTALL_DB_SCRIPT=/usr/bin/mysql_install_db

if(TARGET python3-fd
   AND MARIABACKUP_BINARY
   AND MARIADB_DAEMON_BINARY
   AND MARIADB_CLIENT_BINARY
   AND MARIADB_MYSQL_INSTALL_DB_SCRIPT
)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX}
    "py3plug-fd-mariabackup"
    DISABLED
    COMMENT
    "
    no python3-fd or MARIABACKUP_BINARY,MARIADB_DAEMON_BINARY,MARIADB_CLIENT_BINARY or MARIADB_MYSQL_INSTALL_DB_SCRIPT not set.
    example:
    cmake -D MARIABACKUP_BINARY=/usr/bin/mariabackup \
    -D MARIADB_DAEMON_BINARY=/usr/libexec/mysqld \
    -D MARIADB_CLIENT_BINARY=/usr/bin/mysql \
    -D MARIADB_MYSQL_INSTALL_DB_SCRIPT=/usr/bin/mysql_install_db"
  )
endif()
