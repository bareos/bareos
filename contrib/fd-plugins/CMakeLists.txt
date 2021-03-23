#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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

set(PYFILES
    bareos-tasks-plugins/BareosFdTaskClass.py
    bareos-tasks-plugins/mysql/BareosFdMySQLClass.py
    bareos-tasks-plugins/mysql/bareos-fd-mysql.py
    bareos-tasks-plugins/oracle/BareosFdOracleClass.py
    bareos-tasks-plugins/oracle/bareos-fd-oracle.py
    bareos-tasks-plugins/pgsql/BareosFdPgSQLClass.py
    bareos-tasks-plugins/pgsql/bareos-fd-pgsql.py
    bareos-tasks-plugins/xenserver/BareosFdXenServerClass.py
    bareos-tasks-plugins/xenserver/bareos-fd-xenserver.py
)

# mysql-python/BareosFdMySQLclass.py mysql-python/bareos-fd-mysql.py

install(
  FILES ${PYFILES}
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
