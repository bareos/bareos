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

set(DESTSTARTDIR "${sysconfdir}/init.d/")

message(STATUS "installing startfiles to ${DESTSTARTDIR}")

# Install autostart fd
install(
  FILES
    "${CMAKE_CURRENT_LIST_DIR}/../../debian/bareos-filedaemon.bareos-fd.init"
  DESTINATION "${DESTSTARTDIR}/bareos-fd"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

# Install autostart sd
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../debian/bareos-storage.bareos-sd.init"
  DESTINATION "${DESTSTARTDIR}/bareos-sd"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

# Install autostart dir
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../debian/bareos-director.bareos-dir.init"
  DESTINATION "${DESTSTARTDIR}/bareos-dir"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

# logrotate

# dbconfig
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/../debian/set_dbconfig_vars.sh"
  DESTINATION "${scriptdir}/"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

install(
  DIRECTORY
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/install/"
)

#
# mysql
#
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/creates/mysql.sql"
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/install"
  RENAME "mysql"
)
install(
  DIRECTORY
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/mysql"
)

file(GLOB SQLFILES
     "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/updates/mysql.*.sql"
)
foreach(SQLFILE ${SQLFILES})
  get_filename_component(BASENAME ${SQLFILE} NAME)
  string(
    REGEX MATCH "[0-9]*_([0-9]*)" DUMMMY ${SQLFILE}
  ) # match the regex, we only are interested in submatch in parentheses
  set(VERSION ${CMAKE_MATCH_1})
  install(
    FILES ${SQLFILE}
    DESTINATION
      ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/mysql
    RENAME ${BAREOS_NUMERIC_VERSION}
  )
endforeach()

#
# postgresql
#
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/creates/postgresql.sql"
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/install"
  RENAME "pgsql"
)
install(
  DIRECTORY
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/pgsql/"
)
install(
  DIRECTORY
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/pgsql/"
)

set(UPGRADE_DIR
    ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/upgrade/pgsql/
)
set(UPGRADE_DBADMIN_DIR
    ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/upgrade-dbadmin/pgsql/
)
file(MAKE_DIRECTORY ${UPGRADE_DIR})
file(MAKE_DIRECTORY ${UPGRADE_DBADMIN_DIR})

file(GLOB SQLFILES
     "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/updates/postgresql.*.sql"
)
foreach(SQLFILE ${SQLFILES})
  get_filename_component(BASENAME ${SQLFILE} NAME)
  string(
    REGEX MATCH "[0-9]*_([0-9]*)" DUMMMY ${SQLFILE}
  ) # match the regex, we only are interested in submatch in parentheses
  set(VERSION ${CMAKE_MATCH_1})

  file(STRINGS ${SQLFILE} SQLFILE_LINE)
  foreach(LINE ${SQLFILE_LINE})
    string(REGEX MATCH "^ALTER|^DROP|^--|^$" IS_UPGRADE_LINE ${LINE})
    if(NOT ${IS_UPGRADE_LINE} STREQUAL "")
      file(APPEND ${UPGRADE_DBADMIN_DIR}/${BAREOS_NUMERIC_VERSION} "${LINE}\n")
      set(LINE "-- upgrade-dbadmin: ${LINE}")
      file(APPEND ${UPGRADE_DIR}/${BAREOS_NUMERIC_VERSION} "${LINE}\n")
    else()
      file(APPEND ${UPGRADE_DIR}/${BAREOS_NUMERIC_VERSION} "${LINE}\n")
    endif()
  endforeach()
endforeach()

install(
  DIRECTORY ${UPGRADE_DBADMIN_DIR}
  DESTINATION
    ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/pgsql
)
install(
  DIRECTORY ${UPGRADE_DIR}
  DESTINATION
    ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/pgsql
)

#
# sqlite3
#
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/creates/sqlite3.sql"
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/install"
  RENAME "sqlite3"
)
install(
  DIRECTORY
  DESTINATION
    "${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/sqlite/"
)

file(GLOB SQLFILES
     "${CMAKE_CURRENT_LIST_DIR}/../../src/cats/ddl/updates/sqlite3.*.sql"
)
foreach(SQLFILE ${SQLFILES})
  get_filename_component(BASENAME ${SQLFILE} NAME)
  string(
    REGEX MATCH "[0-9]*_([0-9]*)" DUMMMY ${SQLFILE}
  ) # match the regex, we only are interested in submatch in parentheses
  set(VERSION ${CMAKE_MATCH_1})
  install(
    FILES ${SQLFILE}
    DESTINATION
      ${datarootdir}/dbconfig-common/data/bareos-database-common/upgrade/sqlite3
    RENAME ${BAREOS_NUMERIC_VERSION}
  )
endforeach()
