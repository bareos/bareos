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

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/stored
    ${PROJECT_SOURCE_DIR}/src/stored/backends
  )
endif()

# BACKENDS  ####
if(NOT build_client_only)
  set(BACKENDS "")
  list(APPEND BACKENDS unix_tape_device.d)
  list(APPEND BACKENDS unix_file_device.d)
  list(APPEND BACKENDS unix_fifo_device.d)
  if(NOT HAVE_WIN32)
    list(APPEND BACKENDS dedupable_device.d)
  endif()
  list(APPEND BACKENDS dplcompat_device.d)
  set(BACKENDS
      ${BACKENDS}
      PARENT_SCOPE
  )
  message("Backends are now ${BACKENDS}")
endif()

add_library(bareossd SHARED)
add_library(Bareos::LibSD ALIAS bareossd)
target_sources(
  bareossd
  PRIVATE acquire.cc
          ansi_label.cc
          askdir.cc
          autochanger.cc
          autochanger_resource.cc
          block.cc
          bsr.cc
          butil.cc
          crc32/crc32.cc
          dev.cc
          device.cc
          device_control_record.cc
          device_resource.cc
          ebcdic.cc
          label.cc
          lock.cc
          mount.cc
          read_record.cc
          record.cc
          reserve.cc
          scan.cc
          sd_device_control_record.cc
          sd_plugins.cc
          sd_stats.cc
          spool.cc
          stored_globals.cc
          stored_conf.cc
          vol_mgr.cc
          wait.cc
)

target_link_libraries(bareossd PRIVATE Bareos::Lib fmt::fmt)
set_target_properties(
  bareossd PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                      SOVERSION "${BAREOS_VERSION_MAJOR}"
)

if(HAVE_DYNAMIC_SD_BACKENDS)
  target_sources(bareossd PRIVATE sd_backends_dynamic.cc)
  target_link_libraries(bareossd PRIVATE ${CMAKE_DL_LIBS})
endif()

add_library(stored_objects STATIC)
add_library(Bareos::SD ALIAS stored_objects)
target_sources(
  stored_objects
  PRIVATE append.cc
          authenticate.cc
          checkpoint_handler.cc
          dir_cmd.cc
          fd_cmds.cc
          job.cc
          mac.cc
          ndmp_tape.cc
          read.cc
          sd_cmds.cc
          socket_server.cc
          status.cc
)
if(HAVE_WIN32)
  target_sources(
    stored_objects PRIVATE ../win32/generic/main.cc ../win32/generic/service.cc
  )
endif()

if(HAVE_NDMP)
  target_link_libraries(stored_objects PRIVATE bareosndmp)
endif()

add_executable(bareos-sd stored.cc)
bareos_windows_resource(bareos-sd storedres.rc.in)
if(HAVE_WIN32)
  set_target_properties(bareos-sd PROPERTIES WIN32_EXECUTABLE ON)
  target_sources(bareos-sd PRIVATE ../win32/generic/main.cc)
  target_link_libraries(bareos-sd PRIVATE comctl32)
endif()
target_link_libraries(
  bareos-sd PRIVATE Bareos::SD Bareos::Lib Bareos::LibSD Bareos::Findlib
                    CLI11::CLI11
)

add_executable(bls)
target_sources(bls PRIVATE bls.cc)
bareos_windows_resource(bls blsres.rc.in)
target_link_libraries(
  bls PRIVATE Bareos::Lib Bareos::LibSD Bareos::Findlib CLI11::CLI11
)

add_executable(bextract)
target_sources(bextract PRIVATE bextract.cc)
bareos_windows_resource(bextract bextractres.rc.in)
target_link_libraries(
  bextract PRIVATE Bareos::LibSD Bareos::Findlib Bareos::Lib CLI11::CLI11
)

add_executable(bscan)
target_sources(bscan PRIVATE bscan.cc)
bareos_windows_resource(bscan bscanres.rc.in)
target_link_libraries(
  bscan PRIVATE Bareos::Lib Bareos::LibSD Bareos::Findlib Bareos::SQL
                CLI11::CLI11
)

add_executable(btape)
target_sources(btape PRIVATE btape.cc)
bareos_windows_resource(btape btaperes.rc.in)
target_link_libraries(btape PRIVATE Bareos::LibSD Bareos::Lib CLI11::CLI11)

add_executable(bcopy)
target_sources(bcopy PRIVATE bcopy.cc)
target_link_libraries(bcopy PRIVATE Bareos::LibSD Bareos::Lib CLI11::CLI11)

install(TARGETS bareossd DESTINATION ${libdir})

install(TARGETS bareos-sd bls bextract bscan btape bcopy
        DESTINATION "${sbindir}"
)

install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_BINARY_DIR}\")")
install(SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-stored-configfiles.cmake)

install(DIRECTORY DESTINATION "${archivedir}")

add_subdirectory(backends)

include(DebugEdit)
