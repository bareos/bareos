#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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

set(DESTSTARTDIR "${sysconfdir}/init.d/")

message(STATUS "installing startfiles to ${DESTSTARTDIR}")

# Install autostart fd
install(
  FILES
    "${CMAKE_SOURCE_DIR}/debian/bareos-filedaemon.bareos-fd.init"
  DESTINATION "${DESTSTARTDIR}/bareos-fd"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

# Install autostart sd
install(
  FILES "${CMAKE_SOURCE_DIR}/debian/bareos-storage.bareos-sd.init"
  DESTINATION "${DESTSTARTDIR}/bareos-sd"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

# Install autostart dir
install(
  FILES "${CMAKE_SOURCE_DIR}/debian/bareos-director.bareos-dir.init"
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

macro(bareos_install_sql_files_to_dbconfig_common)
  set(oneValueArgs BAREOS_DB_NAME DEBIAN_DB_NAME)
  cmake_parse_arguments(
    DB_CONFIG_COMMON "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
  )

  if(DB_CONFIG_COMMON_BAREOS_DB_NAME STREQUAL "postgresql")
    install(
      DIRECTORY
      DESTINATION
        "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/install/"
    )

    install(
      FILES
        "${CMAKE_SOURCE_DIR}/core/src/cats/ddl/creates/${DB_CONFIG_COMMON_BAREOS_DB_NAME}.sql"
      DESTINATION
        "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/install/"
      RENAME "${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
    )
  endif()

  install(
    DIRECTORY
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/"
  )
  file(
    GLOB
    SQLFILES
    "${CMAKE_SOURCE_DIR}/core/src/cats/ddl/updates/${DB_CONFIG_COMMON_BAREOS_DB_NAME}.*_*.sql"
  )
  foreach(SQLFILE ${SQLFILES})
    get_filename_component(BASENAME ${SQLFILE} NAME)
    string(REGEX MATCH "([0-9]+)_([0-9]+)\\.sql" DUMMMY ${SQLFILE})
    set(FROM_VERSION ${CMAKE_MATCH_1})
    set(TO_VERSION ${CMAKE_MATCH_2})
    # file(CREATE_LINK ${NAME} ${UPGRADE_DIR}/${TO_VERSION} SYMBOLIC)
    message(
      "WILL INSTALL  ${SQLFILE} to ${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/${TO_VERSION}"
    )
    install(
      FILES ${SQLFILE}
      DESTINATION
        ${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/
      RENAME ${TO_VERSION}
    )
  endforeach()

  install(
    DIRECTORY
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade"
  )

  install(
    DIRECTORY
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/scripts/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
  )
  install(
    FILES
      "${CMAKE_CURRENT_LIST_DIR}/dbconfig-common/scripts/bareos-database-common/upgrade/pgsql/2170"
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/scripts/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
    RENAME 2170
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ
  )
  install(
    FILES
      "${CMAKE_CURRENT_LIST_DIR}/dbconfig-common/scripts/bareos-database-common/upgrade/pgsql/2170"
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/scripts/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
    RENAME 2003
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ
  )
  install(
    FILES
      "${CMAKE_CURRENT_LIST_DIR}/dbconfig-common/scripts/bareos-database-common/upgrade/pgsql/2170"
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/scripts/bareos-database-common/upgrade/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
    RENAME 2004
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ
  )

  install(
    DIRECTORY
    DESTINATION
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
  )
  if(EXISTS
     "${CMAKE_CURRENT_LIST_DIR}/share/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/2170"
  )
    install(
      FILES
        "${CMAKE_CURRENT_LIST_DIR}/share/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/2170"
      DESTINATION
        "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
      PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                  WORLD_READ
    )
    install(
      FILES
        "${CMAKE_CURRENT_LIST_DIR}/share/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/2170"
      DESTINATION
        "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
      RENAME 2003
      PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                  WORLD_READ
    )
    install(
      FILES
        "${CMAKE_CURRENT_LIST_DIR}/share/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}/2170"
      DESTINATION
        "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbconfig-common/data/bareos-database-common/upgrade-dbadmin/${DB_CONFIG_COMMON_DEBIAN_DB_NAME}"
      RENAME 2004
      PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                  WORLD_READ
    )
  endif()

endmacro()

list(FIND db_backends "postgresql" _index)
if(${_index} GREATER -1)
  bareos_install_sql_files_to_dbconfig_common(
    BAREOS_DB_NAME postgresql DEBIAN_DB_NAME pgsql
  )
endif()

list(FIND db_backends "sqlite3" _index)
if(${_index} GREATER -1)
  bareos_install_sql_files_to_dbconfig_common(
    BAREOS_DB_NAME sqlite3 DEBIAN_DB_NAME sqlite3
  )
endif()

list(FIND db_backends "mysql" _index)
if(${_index} GREATER -1)
  bareos_install_sql_files_to_dbconfig_common(
    BAREOS_DB_NAME mysql DEBIAN_DB_NAME mysql
  )
endif()
