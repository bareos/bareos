#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2024 Bareos GmbH & Co. KG
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

message("BareosPlatForm ${BAREOS_PLATFORM}\n")

if(NOT TARGET python3-fd)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup" DISABLED COMMENT
    "python3-fd is not being build."
  )
elseif(NOT MARIABACKUP_BINARY)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup" DISABLED COMMENT
    "MARIABACKUP_BINARY is not set."
  )
elseif(NOT MARIADB_DAEMON_BINARY)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup" DISABLED COMMENT
    "MARIADB_DAEMON_BINARY is not set."
  )
elseif(NOT MARIADB_CLIENT_BINARY)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup" DISABLED COMMENT
    "MARIADB_CLIENT_BINARY is not set."
  )
elseif(NOT MARIADB_MYSQL_INSTALL_DB_SCRIPT)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup" DISABLED COMMENT
    "MARIADB_MYSQL_INSTALL_DB_SCRIPT is not set."
  )
elseif(${BAREOS_PLATFORM} STREQUAL "suse")
  # We currently disable the test on suse as the plugin does not support loading
  # a local defaults file when the MySQLdb module is found. As the test only
  # works with that defaults file (and we do not want to potentially overwrite
  # the current users configuration), we instead disable the test.
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup" DISABLED COMMENT
    "Test is currently disabled on SUSE."
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-mariabackup")
endif()
