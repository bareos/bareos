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
  if(${HAVE_GLUSTERFS})
    list(APPEND BACKENDS gfapi_device.d)
  endif()
  if(TARGET droplet)
    list(APPEND BACKENDS droplet_device.d)
  endif()
  list(APPEND BACKENDS dplcompat_device.d)
  set(BACKENDS
      ${BACKENDS}
      PARENT_SCOPE
  )
  message("Backends are now ${BACKENDS}")
endif()

set(LIBBAREOSSD_SRCS
    acquire.cc
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

set(SDSRCS
    append.cc
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
  list(APPEND SDSRCS ../win32/generic/main.cc ../win32/generic/service.cc)
endif()

set(BTAPESRCS btape.cc)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/stored/btaperes.rc.in)
  list(APPEND BTAPESRCS ${CMAKE_CURRENT_BINARY_DIR}/../win32/stored/btaperes.rc)
endif()

set(BLSSRCS bls.cc)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/stored/blsres.rc.in)
  list(APPEND BLSSRCS ${CMAKE_CURRENT_BINARY_DIR}/../win32/stored/blsres.rc)
endif()

set(BEXTRACTSRS bextract.cc)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/stored/bextractres.rc.in)
  list(APPEND BEXTRACTSRS
       ${CMAKE_CURRENT_BINARY_DIR}/../win32/stored/bextractres.rc
  )
endif()

set(BSCANSRCS bscan.cc)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/stored/bscanres.rc.in)
  list(APPEND BSCANSRCS ${CMAKE_CURRENT_BINARY_DIR}/../win32/stored/bscanres.rc)
endif()

set(BCOPYSRCS bcopy.cc)

set(STORED_RESTYPES autochanger device director ndmp messages storage)

set(BACKEND_OBJECTS "") # backend shared objects

if(${HAVE_DYNAMIC_SD_BACKENDS})
  if(GFAPI_LIBRARIES)
    list(APPEND BACKEND_OBJECTS libbareossd-gfapi)
  endif()

  if(TARGET droplet)
    list(APPEND BACKEND_OBJECTS libbareossd-droplet)
    list(APPEND BACKEND_OBJECTS libbareossd-chunked)
  endif()

  set(LIBBAREOSSD_LIBRARIES Bareos::Lib ${THREADS_THREADS} ${BACKEND_OBJECTS})
  message(STATUS "BACKEND_OBJECTS ARE ${BACKEND_OBJECTS}")
else()
  if(TARGET droplet)
    list(APPEND LIBBAREOSSD_LIBRARIES droplet)
  endif()
  if(${HAVE_GFAPI})
    list(APPEND LIBBAREOSSD_LIBRARIES ${GFAPI_LIBRARIES})
  endif()
  set(LIBBAREOSSD_LIBRARIES Bareos::Lib ${LIBBAREOSSD_LIBRARIES})
  message(STATUS "LIBBAREOSSD_LIBRARIES ARE ${LIBBAREOSSD_LIBRARIES}")
endif()

add_library(bareossd SHARED ${LIBBAREOSSD_SRCS})
target_link_libraries(
  bareossd PRIVATE Bareos::Lib ${THREADS_THREADS} fmt::fmt-header-only
)
set_target_properties(
  bareossd PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                      SOVERSION "${BAREOS_VERSION_MAJOR}"
)

if(${HAVE_DYNAMIC_SD_BACKENDS})
  target_sources(bareossd PRIVATE sd_backends_dynamic.cc)
else()
  target_link_libraries(bareossd PRIVATE ${LIBBAREOSSD_LIBRARIES})
endif()

add_library(stored_objects STATIC ${SDSRCS})

if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/stored/storedres.rc.in)
  add_executable(
    bareos-sd WIN32 stored.cc
                    ${CMAKE_CURRENT_BINARY_DIR}/../win32/stored/storedres.rc
  )
else()
  add_executable(bareos-sd WIN32 stored.cc)
endif()

target_link_libraries(stored_objects PRIVATE ${THREADS_THREADS})
target_link_libraries(
  bareos-sd PRIVATE stored_objects Bareos::Lib bareossd Bareos::Findlib
                    CLI11::CLI11
)

if(HAVE_WIN32)
  target_sources(bareos-sd PRIVATE ../win32/generic/main.cc)
  target_link_libraries(bareos-sd PRIVATE comctl32)
endif()
if(HAVE_NDMP)
  target_link_libraries(stored_objects PRIVATE bareosndmp)
  target_link_libraries(bareos-sd PRIVATE bareosndmp)
endif()

add_executable(bls ${BLSSRCS})
target_link_libraries(
  bls PRIVATE Bareos::Lib bareossd Bareos::Findlib CLI11::CLI11
)

add_executable(bextract ${BEXTRACTSRS})
target_link_libraries(
  bextract PRIVATE bareossd Bareos::Findlib Bareos::Lib CLI11::CLI11
)

add_executable(bscan ${BSCANSRCS})
target_link_libraries(
  bscan PRIVATE Bareos::Lib bareossd Bareos::Findlib Bareos::SQL CLI11::CLI11
)

add_executable(btape ${BTAPESRCS})
target_link_libraries(btape PRIVATE bareossd Bareos::Lib CLI11::CLI11)

add_executable(bcopy ${BCOPYSRCS})

target_link_libraries(bcopy PRIVATE bareossd Bareos::Lib CLI11::CLI11)

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

if(TARGET droplet)
  target_compile_definitions(bareossd PRIVATE HAVE_BAREOSSD_DROPLET_DEVICE)
endif()

include(DebugEdit)
