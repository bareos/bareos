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

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/filed
    ${PROJECT_SOURCE_DIR}/src/win32/vss/include
  )
endif()

set(FDSRCS
    accurate.cc
    authenticate.cc
    crypto.cc
    evaluate_job_command.cc
    fd_plugins.cc
    fileset.cc
    sd_cmds.cc
    verify.cc
    accurate_htable.cc
    backup.cc
    dir_cmd.cc
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
)

if(HAVE_WIN32)
  list(APPEND FDSRCS ../win32/filed/vss.cc ../win32/filed/vss_XP.cc
              ../win32/filed/vss_W2K3.cc ../win32/filed/vss_Vista.cc
              ../win32/generic/service.cc ../win32/generic/main.cc
  )
endif()

if(HAVE_WIN32)
  list(APPEND FDSRCS ../win32/filed/filedres.rc)
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

add_library(fd_objects STATIC ${FDSRCS})
target_link_libraries(fd_objects PUBLIC bareos)

add_executable(bareos-fd filed.cc)

set(BAREOS_FD_LIBRARIES bareosfind bareos pthread ${LMDB_LIBS})
if(HAVE_WIN32)
  list(APPEND BAREOS_FD_LIBRARIES comctl32)

  set(FD_COMPILE_DEFINITIONS -DWIN32_VSS)

  if("${WINDOWS_BITS}" STREQUAL "64")
    list(APPEND FD_COMPILE_DEFINITIONS -DHAVE_VSS64)
  endif()

  target_compile_definitions(fd_objects PRIVATE ${FD_COMPILE_DEFINITIONS})
  target_compile_definitions(bareos-fd PRIVATE ${FD_COMPILE_DEFINITIONS})
endif()

target_link_libraries(bareos-fd fd_objects ${BAREOS_FD_LIBRARIES})

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
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")" COMPONENT filedaemon)
install(SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-filed-configfiles.cmake
        COMPONENT filedaemon
)
