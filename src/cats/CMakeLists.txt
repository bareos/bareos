#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


include_directories(. .. ../include ${MYSQL_INCLUDE_DIR} ${PostgreSQL_INCLUDE_DIR})


add_definitions( -D_BDB_PRIV_INTERFACE_)

set(LIBBAREOSSQL_SRCS
    bvfs.cc cats.cc sql.cc sql_create.cc sql_delete.cc sql_find.cc
    sql_get.cc sql_list.cc sql_pooling.cc sql_query.cc sql_update.cc
    )
 set(LIBBAREOSCATS_SRCS
    cats_backends.cc
    )

set(SQLITE_SRCS sqlite.cc)

set(MYSQL_SRCS mysql.cc)

set(POSTGRESQL_SRCS postgresql.cc)


add_library(bareossql SHARED ${LIBBAREOSSQL_SRCS})

set_target_properties(bareossql PROPERTIES VERSION "${VERSION}"
                                          SOVERSION "${SOVERSION}"
                                          )
target_link_libraries (bareossql bareoscats bareos)


# TODO: install default db backend

# if we have only one database backend, we do not create the dummy libbareoscats
# but instead we create a symlink to the db backend lib

list(LENGTH db_backends nr_db_backends)



add_library(bareoscats SHARED ${LIBBAREOSCATS_SRCS})
target_link_libraries (bareoscats bareos)
set_target_properties(bareoscats PROPERTIES
   VERSION ${VERSION}
   SOVERSION "${SOVERSION}"
   )
INSTALL(TARGETS bareoscats
   DESTINATION ${libdir})


IF (HAVE_SQLITE3)
add_library(bareoscats-sqlite3 MODULE ${SQLITE_SRCS})
target_link_libraries (bareoscats-sqlite3 bareossql bareos sqlite3)
INSTALL(TARGETS bareoscats-sqlite3 DESTINATION ${backenddir})

INSTALL(DIRECTORY ddl
        DESTINATION ${scriptdir}
        FILES_MATCHING PATTERN *sqlite3*.sql
        )
ENDIF()


IF (HAVE_MYSQL)
add_library(bareoscats-mysql MODULE ${MYSQL_SRCS})
target_link_libraries(bareoscats-mysql bareossql bareos ${MYSQL_LIBRARIES})
INSTALL(TARGETS bareoscats-mysql DESTINATION ${backenddir})

INSTALL(DIRECTORY ddl
        DESTINATION ${scriptdir}
        FILES_MATCHING PATTERN *mysql*.sql
        )
ENDIF()


IF (HAVE_POSTGRESQL)
add_library(bareoscats-postgresql MODULE ${POSTGRESQL_SRCS})
target_link_libraries(bareoscats-postgresql bareossql bareos ${PostgreSQL_LIBRARIES} )
INSTALL(TARGETS bareoscats-postgresql DESTINATION ${backenddir})

INSTALL(DIRECTORY ddl
        DESTINATION ${scriptdir}
        FILES_MATCHING PATTERN *postgresql*.sql
        )
ENDIF()


INSTALL(TARGETS bareossql
                DESTINATION ${libdir})

INSTALL(FILES
                  create_bareos_database
                  update_bareos_tables
                  make_bareos_tables
                  grant_bareos_privileges
                  drop_bareos_tables
                  drop_bareos_database
                  make_catalog_backup.pl
                  make_catalog_backup
                  delete_catalog_backup
                  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                              GROUP_READ GROUP_EXECUTE
                              WORLD_READ WORLD_EXECUTE
                  DESTINATION ${scriptdir})

INSTALL(DIRECTORY ddl
        DESTINATION ${scriptdir}
        PATTERN version.map
        PATTERN *.in EXCLUDE
        PATTERN *.sql EXCLUDE
        )



IF(${nr_db_backends} EQUAL 1)
   install(CODE "set(libdir \"${libdir}\")")
   install(CODE "set(backenddir \"${backenddir}\")")
   install(CODE "set(default_db_backend \"${default_db_backend}\")")
   install(CODE "set(CMAKE_SHARED_LIBRARY_SUFFIX \"${CMAKE_SHARED_LIBRARY_SUFFIX}\")")
   install(CODE "set(VERSION \"${VERSION}\")")
   install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")

   INSTALL(SCRIPT ${CMAKE_SOURCE_DIR}/cmake/bareos-symlink-default-db-backend.cmake)
ENDIF()
