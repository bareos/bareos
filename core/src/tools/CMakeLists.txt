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

set(TOOLS_BIN "")
set(TOOLS_SBIN "")

add_executable(bsmtp bsmtp.cc)
bareos_windows_resource(bsmtp bsmtpres.rc.in)
target_link_libraries(bsmtp Bareos::Lib CLI11::CLI11)
list(APPEND TOOLS_BIN bsmtp)

add_executable(bregex bregex.cc)
target_link_libraries(bregex Bareos::Lib Bareos::Findlib CLI11::CLI11)
bareos_windows_resource(bregex bregexres.rc.in)
list(APPEND TOOLS_BIN bregex)
list(APPEND TOOLS_SBIN bregex)

add_executable(bwild bwild.cc)
target_link_libraries(bwild Bareos::Lib CLI11::CLI11)
bareos_windows_resource(bwild bwildres.rc.in)
list(APPEND TOOLS_BIN bwild)
list(APPEND TOOLS_SBIN bwild)

add_executable(gentestdata gentestdata.cc)
target_link_libraries(gentestdata PRIVATE CLI11::CLI11)

if(NOT HAVE_MSVC)
  add_executable(drivetype drivetype.cc)
  target_link_libraries(drivetype Bareos::Lib Bareos::Findlib)

  add_executable(fstype fstype.cc)
  target_link_libraries(fstype Bareos::Lib Bareos::Findlib)

  add_executable(bscrypto bscrypto.cc)
  target_link_libraries(bscrypto Bareos::Lib CLI11::CLI11)
  list(APPEND TOOLS_SBIN bscrypto)
endif()

if(NOT HAVE_WIN32)
  add_executable(btestls btestls.cc)
  target_link_libraries(btestls Bareos::Findlib Bareos::Lib)
  list(APPEND TOOLS_SBIN btestls)

  add_executable(bpluginfo bpluginfo.cc)
  target_link_libraries(bpluginfo Bareos::Lib ${CMAKE_DL_LIBS} CLI11::CLI11)
  list(APPEND TOOLS_SBIN bpluginfo)
endif()

if(NOT client-only)
  set(TSTFNDSRCS testfind.cc testfind_fd.cc dummysockets.cc)
  add_executable(testfind ${TSTFNDSRCS})
  target_link_libraries(
    testfind Bareos::Dir Bareos::FD Bareos::Findlib CLI11::CLI11
  )
  list(APPEND TOOLS_SBIN testfind)

  add_executable(bdedupestimate bdedup-estimate.cc)
  target_link_libraries(bdedupestimate Bareos::Lib Bareos::LibSD CLI11::CLI11)
  list(APPEND TOOLS_SBIN bdedupestimate)

  add_executable(bareos-sd-discover bareos_sd_discover.cc sd_discovery_cli.cc)
  target_link_libraries(
    bareos-sd-discover Bareos::Lib CLI11::CLI11 Bareos::LibSD
  )
  list(APPEND TOOLS_SBIN bareos-sd-discover)

  if(NOT HAVE_WIN32)
    add_executable(
      dedup-conf dedup_conf.cc ../stored/backends/dedupable/volume.cc
    )

    target_link_libraries(dedup-conf Bareos::Lib CLI11::CLI11)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION
                                                VERSION_LESS "8.6.0"
    )
      # workaround for rhel 8.  It uses an old stdlibc++ that does not yet
      # contain the filesystem library.
      message(INFO "enabling rhel 8 filesystem workaround")
      target_link_libraries(dedup-conf stdc++fs)
    endif()
  endif()
endif()

add_executable(random-commands random_commands.cc)
target_link_libraries(random-commands fmt::fmt CLI11::CLI11)

add_library(bconfig_parser_core STATIC bconfig_lib.cc)
target_link_libraries(
  bconfig_parser_core PUBLIC Bareos::Dir Bareos::FD Bareos::SD Bareos::LibSD
                             Bareos::Lib Bareos::Findlib
)
if(ENABLE_BCONSOLE)
  target_link_libraries(bconfig_parser_core PUBLIC Bareos::Console)
  target_compile_definitions(bconfig_parser_core PUBLIC BCONFIG_HAVE_CONSOLE)
endif()
if(${traymonitor})
  target_sources(bconfig_parser_core PRIVATE ../qt-tray-monitor/tray_conf.cc)
  target_compile_definitions(
    bconfig_parser_core PUBLIC BCONFIG_HAVE_TRAYMONITOR
  )
endif()

add_executable(bconfig)
target_sources(bconfig PRIVATE bconfig.cc)
target_link_libraries(bconfig PRIVATE bconfig_parser_core CLI11::CLI11)
list(APPEND TOOLS_SBIN bconfig)

add_library(bconfig_service_core STATIC bconfig_service.cc)
target_link_libraries(
  bconfig_service_core PUBLIC bconfig_parser_core Bareos::Lib
)

add_executable(bconfig-service bconfig_service_server.cc)
target_link_libraries(
  bconfig-service PRIVATE bconfig_service_core bconfig_parser_core CLI11::CLI11
                          Bareos::Lib
)
list(APPEND TOOLS_SBIN bconfig-service)

install(TARGETS ${TOOLS_BIN} DESTINATION "${bindir}")
install(TARGETS ${TOOLS_SBIN} DESTINATION "${sbindir}")

# install bsmtp for non-root directors
install(TARGETS bsmtp DESTINATION "${bindir}")
install(TARGETS bsmtp DESTINATION "${sbindir}")

include(DebugEdit)
