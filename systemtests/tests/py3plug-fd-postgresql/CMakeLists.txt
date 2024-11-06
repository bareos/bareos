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

if(TARGET python3-fd)
  check_pymodule_available(3 pg8000)
  check_pymodule_available(3 dateutil)
  include(FindPostgreSQL)
  if(PYMODULE_3_PG8000_FOUND
     AND PostgreSQL_FOUND
     AND PYMODULE_3_DATEUTIL_FOUND
     AND (${PostgreSQL_VERSION_STRING} VERSION_GREATER_EQUAL "10")
  )
    create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-postgresql")
  else()
    create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-postgresql" DISABLED)
  endif()
endif()
