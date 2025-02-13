#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2025 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

add_library(bareossql SHARED)
target_sources(
  bareossql
  PRIVATE bvfs.cc
          cats.cc
          sql.cc
          sql_create.cc
          sql_delete.cc
          sql_find.cc
          sql_get.cc
          sql_list.cc
          sql_pooling.cc
          sql_query.cc
          sql_update.cc
          postgresql.cc
          postgresql_batch.cc
)
target_link_libraries(bareossql PUBLIC bareos PostgreSQL::PostgreSQL)

set_target_properties(
  bareossql PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                       SOVERSION "${BAREOS_VERSION_MAJOR}"
)

include(BareosConfigureFile)
bareos_configure_file(
  FILES
  create_bareos_database.in
  delete_catalog_backup.in
  drop_bareos_database.in
  drop_bareos_tables.in
  grant_bareos_privileges.in
  make_bareos_tables.in
  make_catalog_backup.in
  update_bareos_tables.in
  ddl/versions.map.in
)

install(
  DIRECTORY ddl
  DESTINATION ${scriptdir}
  FILES_MATCHING
  PATTERN *postgresql*.sql
)

install(TARGETS bareossql DESTINATION ${libdir})

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/create_bareos_database
           ${CMAKE_CURRENT_BINARY_DIR}/delete_catalog_backup
           ${CMAKE_CURRENT_BINARY_DIR}/drop_bareos_database
           ${CMAKE_CURRENT_BINARY_DIR}/drop_bareos_tables
           ${CMAKE_CURRENT_BINARY_DIR}/grant_bareos_privileges
           ${CMAKE_CURRENT_BINARY_DIR}/make_bareos_tables
           ${CMAKE_CURRENT_BINARY_DIR}/make_catalog_backup
           ${CMAKE_CURRENT_BINARY_DIR}/update_bareos_tables
  DESTINATION ${scriptdir}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ddl/versions.map
        DESTINATION ${scriptdir}/ddl
)

include(DebugEdit)
