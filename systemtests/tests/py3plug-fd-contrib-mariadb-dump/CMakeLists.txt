#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2025 Bareos GmbH & Co. KG
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
# -D MARIADB_DAEMON_BINARY=/usr/sbin/mariadbd -D
# MARIADB_CLIENT_BINARY=/usr/bin/mariadb"

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
message("BareosPlatForm ${BAREOS_PLATFORM}\n")

if(NOT TARGET python3-fd)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-contrib-mariadb-dump" DISABLED COMMENT
    "python3-fd is not being build."
  )
elseif(NOT MARIADB_DUMP_BINARY)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-contrib-mariadb-dump" DISABLED COMMENT
    "MARIABACKUP_BINARY is not set."
  )
elseif(NOT MARIADB_DAEMON_BINARY)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-contrib-mariadb-dump" DISABLED COMMENT
    "MARIADB_DAEMON_BINARY is not set."
  )
elseif(NOT MARIADB_CLIENT_BINARY)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-contrib-mariadb-dump" DISABLED COMMENT
    "MARIADB_CLIENT_BINARY is not set."
  )
elseif(NOT MARIADB_INSTALL_DB_SCRIPT)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-contrib-mariadb-dump" DISABLED COMMENT
    "MARIADB_INSTALL_DB_SCRIPT is not set."
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-contrib-mariadb-dump")
endif()
