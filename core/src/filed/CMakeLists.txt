#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2026 Bareos GmbH & Co. KG
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

option(FD_CLIENT_SLEEP_INHIBITION
       "Enable FD sleep inhibition during backup/restore jobs"
       ON
)

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/filed
    ${PROJECT_SOURCE_DIR}/src/win32/vss/include
  )
endif()

if(FD_CLIENT_SLEEP_INHIBITION AND HAVE_LINUX_OS)
  if(NOT HAVE_SYSTEMD)
    message(
      FATAL_ERROR
        "FD_CLIENT_SLEEP_INHIBITION requires systemd support on Linux. "
        "Set -DFD_CLIENT_SLEEP_INHIBITION=OFF to disable manually."
    )
  endif()
  if(NOT HAVE_SYSTEMD_SD_BUS_H)
    message(
      FATAL_ERROR
        "FD_CLIENT_SLEEP_INHIBITION requires <systemd/sd-bus.h> on Linux. "
        "Install systemd development headers or set "
        "-DFD_CLIENT_SLEEP_INHIBITION=OFF to disable manually."
    )
  endif()
endif()

add_library(fd_objects_common OBJECT)
target_sources(
  fd_objects_common
  PRIVATE accurate.cc
          authenticate.cc
          crypto.cc
          evaluate_job_command.cc
          fd_plugins.cc
          fileset.cc
          sd_cmds.cc
          verify.cc
          accurate_htable.cc
          dir_cmd.cc
          os_suspension.cc
          filed_globals.cc
          heartbeat.cc
          socket_server.cc
          verify_vol.cc
          accurate_lmdb.cc
          compression.cc
          estimate.cc
          filed_conf.cc
          restore.cc
          status.cc
          filed_utils.cc
)

if(FD_CLIENT_SLEEP_INHIBITION AND HAVE_WIN32)
  target_sources(fd_objects_common PRIVATE os_suspension_win32.cc)
endif()
if(FD_CLIENT_SLEEP_INHIBITION AND HAVE_LINUX_OS AND HAVE_SYSTEMD)
  target_sources(fd_objects_common PRIVATE os_suspension_linux.cc)
endif()
if(FD_CLIENT_SLEEP_INHIBITION AND HAVE_DARWIN_OS)
  target_sources(fd_objects_common PRIVATE os_suspension_darwin.cc)
endif()

target_link_libraries(
  fd_objects_common PRIVATE Bareos::Lib Bareos::Findlib Bareos::Fastlz
                            ZLIB::ZLIB
)
if(FD_CLIENT_SLEEP_INHIBITION)
  target_compile_definitions(fd_objects_common
                             PRIVATE FILED_CLIENT_SLEEP_INHIBITION=1
  )
  if(HAVE_SYSTEMD)
    target_include_directories(fd_objects_common
                               PRIVATE ${SYSTEMD_INCLUDE_DIRS}
    )
    target_compile_options(fd_objects_common PRIVATE ${SYSTEMD_CFLAGS_OTHER})
    if(SYSTEMD_LIBRARIES)
      target_link_libraries(fd_objects_common PRIVATE ${SYSTEMD_LIBRARIES})
    else()
      target_link_libraries(fd_objects_common PRIVATE systemd)
    endif()
  endif()

  if(HAVE_DARWIN_OS)
    target_link_libraries(fd_objects_common PRIVATE "-framework IOKit")
  endif()
endif()
if(HAVE_LMDB)
  target_link_libraries(fd_objects_common PRIVATE bareoslmdb)
endif()

add_library(fd_backup_test OBJECT)
target_sources(fd_backup_test PRIVATE backup.cc)
target_compile_definitions(
  fd_backup_test PRIVATE MAXIMUM_ALLOWED_FILES_PER_JOB=10
)

add_library(fd_backup_notest OBJECT)
target_sources(fd_backup_notest PRIVATE backup.cc)

add_library(fd_objects_notest STATIC)
add_library(Bareos::FD ALIAS fd_objects_notest)
target_link_libraries(
  fd_objects_notest PRIVATE fd_objects_common fd_backup_notest
)

add_executable(bareos-fd filed.cc)
target_link_libraries(bareos-fd PRIVATE fd_objects_notest CLI11::CLI11)

add_library(fd_objects_test STATIC)
target_link_libraries(fd_objects_test PRIVATE fd_objects_common fd_backup_test)
add_executable(bareos-test-fd filed.cc)
target_link_libraries(bareos-test-fd PRIVATE fd_objects_test CLI11::CLI11)

if(HAVE_WIN32)
  target_sources(
    fd_objects_common
    PRIVATE ../win32/filed/vss.cc ../win32/filed/vss_Vista.cc
            ../win32/generic/service.cc ../win32/generic/main.cc
  )
  target_compile_definitions(fd_objects_common PUBLIC WIN32_VSS HAVE_VSS64)

  foreach(tgt IN ITEMS bareos-fd bareos-test-fd)
    set_target_properties("${tgt}" PROPERTIES WIN32_EXECUTABLE ON)
    bareos_windows_resource("${tgt}" filedres.rc.in)
    target_sources("${tgt}" PRIVATE ../win32/generic/main.cc)
    target_link_libraries("${tgt}" PRIVATE comctl32)
  endforeach()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_target_properties(
    bareos-fd PROPERTIES INSTALL_RPATH "@loader_path/../${libdir}"
  )
endif()

install(
  TARGETS bareos-fd
  DESTINATION "${sbindir}"
  COMPONENT filedaemon
)

install(CODE "set(PLUGINS \"${PLUGINS}\")" COMPONENT filedaemon)
install(CODE "set(BACKENDS \"${BACKENDS}\")" COMPONENT filedaemon)
install(CODE "set(configtemplatedir \"${configtemplatedir}\")"
        COMPONENT filedaemon
)
install(CODE "set(SRC_DIR \"${PROJECT_BINARY_DIR}\")" COMPONENT filedaemon)
install(SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-filed-configfiles.cmake
        COMPONENT filedaemon
)

include(DebugEdit)
