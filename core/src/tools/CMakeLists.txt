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

set(TOOLS_BIN "")
set(TOOLS_SBIN "")

add_executable(bsmtp bsmtp.cc)
target_link_libraries(bsmtp Bareos::Lib CLI11::CLI11)
list(APPEND TOOLS_BIN bsmtp)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/tools/bsmtpres.rc.in)
  target_sources(
    bsmtp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../win32/tools/bsmtpres.rc
  )
endif()

add_executable(bregex bregex.cc)
target_link_libraries(bregex Bareos::Lib Bareos::Findlib CLI11::CLI11)
list(APPEND TOOLS_BIN bregex)
list(APPEND TOOLS_SBIN bregex)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/tools/bregexres.rc.in)
  target_sources(
    bregex PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../win32/tools/bregexres.rc
  )
endif()

add_executable(bwild bwild.cc)
target_link_libraries(bwild Bareos::Lib CLI11::CLI11)
list(APPEND TOOLS_BIN bwild)
list(APPEND TOOLS_SBIN bwild)
if(HAVE_WIN32)
  bareos_configure_file(FILES ../win32/tools/bwildres.rc.in)
  target_sources(
    bwild PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../win32/tools/bwildres.rc
  )
endif()

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

install(TARGETS ${TOOLS_BIN} DESTINATION "${bindir}")
install(TARGETS ${TOOLS_SBIN} DESTINATION "${sbindir}")

# install bsmtp for non-root directors
install(TARGETS bsmtp DESTINATION "${bindir}")
install(TARGETS bsmtp DESTINATION "${sbindir}")

include(DebugEdit)
