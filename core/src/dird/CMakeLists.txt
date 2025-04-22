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

include(BareosConfigureFile)
include(BareosWindowsResource)

if(NOT ENABLE_JANSSON)
  message(FATAL_ERROR "Bareos Director requires Jansson support")
endif()
find_package(Jansson REQUIRED)

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/dird
  )
endif()

add_library(dird_objects STATIC)
add_library(Bareos::Dir ALIAS dird_objects)
target_sources(
  dird_objects
  PRIVATE admin.cc
          archive.cc
          authenticate.cc
          authenticate_console.cc
          autoprune.cc
          backup.cc
          bsr.cc
          catreq.cc
          check_catalog.cc
          consolidate.cc
          console_connection_lease.cc
          date_time.cc
          dbcheck_utils.cc
          dird_globals.cc
          dir_plugins.cc
          dird_conf.cc
          expand.cc
          fd_cmds.cc
          fd_sendfileset.cc
          get_database_connection.cc
          getmsg.cc
          inc_conf.cc
          job.cc
          jcr_util.cc
          jobq.cc
          job_trigger.cc
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
          socket_server.cc
          recycle.cc
          reload.cc
          restore.cc
          run_conf.cc
          run_on_incoming_connect_interval.cc
          sd_cmds.cc
          schedule.cc
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

if(HAVE_PAM)
  target_sources(dird_objects PRIVATE auth_pam.cc)
endif()

target_link_libraries(dird_objects PRIVATE Bareos::Lib OpenSSL::SSL)

add_executable(bareos-dir)
target_sources(bareos-dir PRIVATE dird.cc)
bareos_windows_resource(bareos-dir dirdres.rc.in)
if(HAVE_WIN32)
  set_target_properties(bareos-dir PROPERTIES WIN32_EXECUTABLE ON)
  target_sources(
    bareos-dir PRIVATE ../win32/generic/main.cc ../win32/generic/service.cc
  )
  target_link_libraries(bareos-dir PRIVATE comctl32)
endif()

target_link_libraries(
  bareos-dir PRIVATE dird_objects Bareos::SQL Bareos::Findlib CLI11::CLI11
)

if(HAVE_NDMP)
  target_link_libraries(dird_objects PRIVATE bareosndmp)
endif()

if(HAVE_LMDB)
  target_link_libraries(dird_objects PRIVATE bareoslmdb)
endif()

if(HAVE_PAM)
  target_link_libraries(dird_objects PRIVATE PAM::pam)
endif()

add_executable(bareos-dbcheck)
target_sources(
  bareos-dbcheck
  PRIVATE date_time.cc
          dbcheck.cc
          dbcheck_utils.cc
          dird_conf.cc
          dird_globals.cc
          schedule.cc
          ua_acl.cc
          ua_audit.cc
          run_conf.cc
          inc_conf.cc
)
bareos_windows_resource(bareos-dbcheck dbcheckres.rc.in)
target_link_libraries(
  bareos-dbcheck PRIVATE Bareos::SQL Bareos::Lib Bareos::Findlib OpenSSL::SSL
                         CLI11::CLI11
)

install(TARGETS bareos-dir bareos-dbcheck DESTINATION "${sbindir}")

install(FILES query.sql DESTINATION "${scriptdir}")

message("using BACKENDS: ${BACKENDS}")
message("using PLUGINS: ${PLUGINS}")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_BINARY_DIR}\")")

install(SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-dird-configfiles.cmake)
include(DebugEdit)
