#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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



set (DESTSTARTDIR "${sysconfdir}/init.d/")

MESSAGE(STATUS  "installing startfiles to ${DESTSTARTDIR}")

# Install autostart fd
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../../debian/bareos-filedaemon.bareos-fd.init" DESTINATION "${DESTSTARTDIR}/bareos-fd"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)

# Install autostart sd
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../../debian/bareos-storage.bareos-sd.init" DESTINATION "${DESTSTARTDIR}/bareos-sd"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)

# Install autostart dir
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../../debian/bareos-director.bareos-dir.init" DESTINATION "${DESTSTARTDIR}/bareos-dir"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)


# logrotate



# dbconfig
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../debian/set_dbconfig_vars.sh" DESTINATION "${scriptdir}/"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)

INSTALL (DIRECTORY DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/install/")


#
# mysql
#
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/creates/mysql.sql" DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/install" RENAME "mysql")
INSTALL (DIRECTORY DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/mysql")

FILE(GLOB SQLFILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/updates/mysql.*.sql")
FOREACH(SQLFILE ${SQLFILES})
   GET_FILENAME_COMPONENT(BASENAME ${SQLFILE} NAME)
   STRING(REGEX MATCH  "[0-9]*_([0-9]*)" DUMMMY ${SQLFILE}) # match the regex, we only are interested in submatch in parentheses
   SET(VERSION ${CMAKE_MATCH_1})
   INSTALL (FILES ${SQLFILE} DESTINATION ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/mysql RENAME ${VERSION})
ENDFOREACH()


#
# postgresql
#
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/creates/postgresql.sql" DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/install" RENAME "pgsql")
INSTALL (DIRECTORY DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/pgsql/")
INSTALL (DIRECTORY DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/pgsql/")

SET(UPGRADE_DIR ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/upgrade/pgsql/)
SET(UPGRADE_DBADMIN_DIR ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/upgrade-dbadmin/pgsql/)
FILE (MAKE_DIRECTORY ${UPGRADE_DIR})
FILE (MAKE_DIRECTORY ${UPGRADE_DBADMIN_DIR})

FILE(GLOB SQLFILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/updates/postgresql.*.sql")
FOREACH(SQLFILE ${SQLFILES})
   GET_FILENAME_COMPONENT(BASENAME ${SQLFILE} NAME)
   STRING(REGEX MATCH  "[0-9]*_([0-9]*)" DUMMMY ${SQLFILE}) # match the regex, we only are interested in submatch in parentheses
   SET(VERSION ${CMAKE_MATCH_1})

   FILE(STRINGS ${SQLFILE} SQLFILE_LINE)
   FOREACH(LINE ${SQLFILE_LINE})
     STRING(REGEX MATCH "^ALTER|^DROP|^--|^$" IS_UPGRADE_LINE ${LINE})
     IF (NOT ${IS_UPGRADE_LINE} STREQUAL "")
        FILE(APPEND ${UPGRADE_DBADMIN_DIR}/${VERSION} "${LINE}\n")
        SET(LINE "-- upgrade-dbadmin: ${LINE}")
        FILE(APPEND ${UPGRADE_DIR}/${VERSION} "${LINE}\n")
     ELSE()
        FILE(APPEND ${UPGRADE_DIR}/${VERSION} "${LINE}\n")
     ENDIF()
   ENDFOREACH()
ENDFOREACH()

INSTALL (DIRECTORY ${UPGRADE_DBADMIN_DIR} DESTINATION ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/pgsql)
INSTALL (DIRECTORY ${UPGRADE_DIR} DESTINATION ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/pgsql)

#
# sqlite3
#
INSTALL (FILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/creates/sqlite3.sql" DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/install" RENAME "sqlite3")
INSTALL (DIRECTORY DESTINATION "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/sqlite/")

FILE(GLOB SQLFILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/updates/sqlite3.*.sql")
FOREACH(SQLFILE ${SQLFILES})
   GET_FILENAME_COMPONENT(BASENAME ${SQLFILE} NAME)
   STRING(REGEX MATCH  "[0-9]*_([0-9]*)" DUMMMY ${SQLFILE}) # match the regex, we only are interested in submatch in parentheses
   SET(VERSION ${CMAKE_MATCH_1})
   INSTALL (FILES ${SQLFILE} DESTINATION ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/sqlite3 RENAME ${VERSION})
ENDFOREACH()
