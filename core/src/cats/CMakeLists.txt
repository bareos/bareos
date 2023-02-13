#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2023 Bareos GmbH & Co. KG
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

include_directories(${PostgreSQL_INCLUDE_DIR})

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
target_link_libraries(bareossql PUBLIC bareos ${PostgreSQL_LIBRARY})

set_target_properties(
  bareossql PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                       SOVERSION "${BAREOS_VERSION_MAJOR}"
)

install(
  DIRECTORY ddl
  DESTINATION ${scriptdir}
  FILES_MATCHING
  PATTERN *postgresql*.sql
)

install(TARGETS bareossql DESTINATION ${libdir})

install(
  FILES create_bareos_database update_bareos_tables make_bareos_tables
        grant_bareos_privileges drop_bareos_tables drop_bareos_database
        make_catalog_backup delete_catalog_backup
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION ${scriptdir}
)

install(
  DIRECTORY ddl
  DESTINATION ${scriptdir}
  PATTERN version.map
  PATTERN *.in EXCLUDE
  PATTERN *.sql EXCLUDE
)
