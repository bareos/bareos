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
add_library(Bareos::SQL ALIAS bareossql)
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
)
target_link_libraries(bareossql PUBLIC Bareos::Lib PostgreSQL::PostgreSQL)

set(cats_generated_query_files
    core/src/cats/bdb_query_enum_class.h
    core/src/cats/bdb_query_names.inc
    core/src/cats/postgresql_queries.inc
)

add_custom_target(
  check-cats-queryfiles
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/dml
          ./create_queryfiles.sh
  COMMAND git -C ${CMAKE_SOURCE_DIR} diff --no-ext-diff -p --exit-code --stat=80 --
          ${cats_generated_query_files}
  COMMENT "Comparing generated catalog query files with git repo"
  VERBATIM
)
add_dependencies(check-git check-cats-queryfiles)

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
