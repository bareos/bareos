#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2023 Bareos GmbH & Co. KG
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

set(BSMTPSRCS bsmtp.cc)
if(HAVE_WIN32)
  list(APPEND BSMTPSRCS ../win32/tools/bsmtpres.rc)
endif()
add_executable(bsmtp ${BSMTPSRCS})
target_link_libraries(bsmtp bareos CLI11::CLI11)

add_executable(drivetype drivetype.cc)
target_link_libraries(drivetype bareos bareosfind)

add_executable(fstype fstype.cc)
target_link_libraries(fstype bareos bareosfind)

set(BREGEXSRCS bregex.cc)
if(HAVE_WIN32)
  list(APPEND BREGEXSRCS ../win32/tools/bregexres.rc)
endif()
add_executable(bregex ${BREGEXSRCS})
target_link_libraries(bregex bareos bareosfind CLI11::CLI11)

set(BWILDSRCS bwild.cc)
if(HAVE_WIN32)
  list(APPEND BWILDSRCS ../win32/tools/bwildres.rc)
endif()
add_executable(bwild ${BWILDSRCS})
target_link_libraries(bwild bareos CLI11::CLI11)

add_executable(bscrypto bscrypto.cc)
target_link_libraries(bscrypto bareos)

if(NOT HAVE_WIN32)
  add_executable(btestls btestls.cc)
  target_link_libraries(btestls bareosfind bareos)

  add_executable(bpluginfo bpluginfo.cc)
  target_link_libraries(bpluginfo bareos ${DL_LIBRARIES} CLI11::CLI11)
endif()

add_executable(gentestdata)
target_sources(gentestdata PRIVATE gentestdata.cc)
target_link_libraries(gentestdata PRIVATE CLI11::CLI11)

set(TOOLS_BIN bsmtp bwild bregex)

set(TOOLS_SBIN bscrypto bwild bregex)

if(NOT HAVE_WIN32)
  list(APPEND TOOLS_SBIN bpluginfo btestls)
endif()

if(NOT client-only)
  set(TSTFNDSRCS testfind.cc testfind_fd.cc dummysockets.cc)
  add_executable(testfind ${TSTFNDSRCS})
  target_link_libraries(
    testfind dird_objects fd_objects bareosfind CLI11::CLI11
  )
  list(APPEND TOOLS_SBIN testfind)
endif()

add_executable(bdedupestimate bdedup-estimate.cc)
target_link_libraries(bdedupestimate bareos bareossd CLI11::CLI11)

# drydedup.cc does not support api < openssl 1.1.1
if (OPENSSL_VERSION VERSION_GREATER_EQUAL 1.1.1)
  add_executable(drydedup drydedup.cc)
  target_link_libraries(drydedup bareos CLI11::CLI11 ${OPENSSL_LIBRARIES})
endif()

add_executable(
  dedup-stats dedup_stats.cc ../stored/backends/dedup/dedup_volume.cc
              ../stored/backends/dedup/dedup_config.cc
)
target_link_libraries(dedup-stats PRIVATE bareos CLI11::CLI11 stdc++fs)

add_executable(
  dedup-conf dedup_conf.cc ../stored/backends/dedup/dedup_volume.cc
             ../stored/backends/dedup/dedup_config.cc
)
target_link_libraries(dedup-conf PRIVATE bareos CLI11::CLI11 stdc++fs)

install(TARGETS ${TOOLS_BIN} DESTINATION "${bindir}")
install(TARGETS ${TOOLS_SBIN} DESTINATION "${sbindir}")

# install bsmtp for non-root directors
install(TARGETS bsmtp DESTINATION "${bindir}")
install(TARGETS bsmtp DESTINATION "${sbindir}")
