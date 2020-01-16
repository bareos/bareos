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

set(DIRDSRCS dird.cc)

# DIRD_OBJECTS_SRCS also used in a separate library for unittests
set(DIRD_OBJECTS_SRCS
    admin.cc
    archive.cc
    authenticate.cc
    authenticate_console.cc
    autoprune.cc
    backup.cc
    bsr.cc
    catreq.cc
    check_catalog.cc
    consolidate.cc
    dird_globals.cc
    dir_plugins.cc
    dird_conf.cc
    expand.cc
    fd_cmds.cc
    get_database_connection.cc
    getmsg.cc
    inc_conf.cc
    job.cc
    jobq.cc
    migrate.cc
    msgchan.cc
    ndmp_dma_storage.cc
    ndmp_dma_backup_common.cc
    ndmp_dma_backup_NDMP_BAREOS.cc
    ndmp_dma_backup_NDMP_NATIVE.cc
    ndmp_dma_generic.cc
    ndmp_dma_restore_common.cc
    ndmp_dma_restore_NDMP_BAREOS.cc
    ndmp_dma_restore_NDMP_NATIVE.cc
    ndmp_fhdb_common.cc
    ndmp_fhdb_helpers.cc
    ndmp_slot2elemaddr.cc
    ndmp_fhdb_mem.cc
    ndmp_fhdb_lmdb.cc
    ndmp_ndmmedia_db_helpers.cc
    newvol.cc
    next_vol.cc
    quota.cc
    run_hour_validator.cc
    socket_server.cc
    recycle.cc
    restore.cc
    run_conf.cc
    run_on_incoming_connect_interval.cc
    sd_cmds.cc
    scheduler.cc
    scheduler_job_item_queue.cc
    scheduler_private.cc
    stats.cc
    storage.cc
    ua.cc
    ua_acl.cc
    ua_audit.cc
    ua_cmds.cc
    ua_configure.cc
    ua_db.cc
    ua_dotcmds.cc
    ua_input.cc
    ua_impexp.cc
    ua_label.cc
    ua_output.cc
    ua_prune.cc
    ua_purge.cc
    ua_query.cc
    ua_restore.cc
    ua_run.cc
    ua_select.cc
    ua_server.cc
    ua_status.cc
    ua_tree.cc
    ua_update.cc
    vbackup.cc
    verify.cc
    pthread_detach_if_not_detached.cc
)
if(HAVE_WIN32)
  list(APPEND DIRD_OBJECTS_SRCS ../win32/dird/dirdres.rc)
endif()

if(HAVE_PAM)
  include_directories(${PAM_INCLUDE_DIRS})
  list(APPEND DIRD_OBJECTS_SRCS auth_pam.cc)
endif()

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/dird
  )
  list(APPEND DIRDSRCS ../win32/generic/main.cc ../win32/generic/service.cc)
endif()

set(DBCHKSRCS dbcheck.cc dird_conf.cc dird_globals.cc ua_acl.cc ua_audit.cc
              run_conf.cc inc_conf.cc
)
if(HAVE_WIN32)
  list(APPEND DBCHKSRCS ../win32/dird/dbcheckres.rc)
endif()

set(TSTFNDSRCS testfind.cc dird_conf.cc dird_globals.cc ua_acl.cc ua_audit.cc
               run_conf.cc inc_conf.cc
)

set(DIRD_RESTYPES
    catalog
    client
    console
    counter
    director
    fileset
    job
    jobdefs
    messages
    pool
    profile
    schedule
    storage
)

# dird_objects is also used as library for unittests
add_library(dird_objects STATIC ${DIRD_OBJECTS_SRCS})
target_link_libraries(dird_objects ${NDMP_LIBS})

add_executable(bareos-dir ${DIRDSRCS})

set(BAREOS_DIR_LIBRARIES dird_objects bareos bareoscats bareossql bareosfind
                         ${LMDB_LIBS} ${NDMP_LIBS}
)

if(HAVE_WIN32)
  list(APPEND BAREOS_DIR_LIBRARIES comctl32)
endif()

if(HAVE_PAM)
  target_link_libraries(bareos-dir ${BAREOS_DIR_LIBRARIES} ${PAM_LIBRARIES})
else()
  target_link_libraries(bareos-dir ${BAREOS_DIR_LIBRARIES})
endif()

add_subdirectory(dbconvert)

add_executable(bareos-dbcheck ${DBCHKSRCS})
set(DBCHECK_LIBRARIES bareossql bareos bareosfind bareoscats)

target_link_libraries(bareos-dbcheck ${DBCHECK_LIBRARIES})

# is not built by default

# add_executable(testfind ${TSTFNDSRCS}) target_link_libraries(testfind bareos
# bareosfind jansson ${OPENSSL_LIBS} acl cap lzo2 )

install(TARGETS
  bareos-dir
  bareos-dbcheck
  bareos-dbconvert
  DESTINATION "${sbindir}"
)

install(FILES query.sql DESTINATION "${scriptdir}")

install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")

install(SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-dird-configfiles.cmake)

