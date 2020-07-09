#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

if(dynamic-cats-backends)
  add_executable(
    bareos-dbcopy
    dbcopy.cc
    database_table_descriptions.cc
    database_column_descriptions.cc
    column_description.cc
    database_import.cc
    database_import_mysql.cc
    database_export.cc
    database_export_postgresql.cc
    progress.cc
  )
  target_link_libraries(
    bareos-dbcopy bareos dird_objects bareosfind bareoscats bareossql
    $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> ${LMDB_LIBS}
  )

  install(TARGETS bareos-dbcopy DESTINATION "${sbindir}")
endif() # dynamic-cats-backends
