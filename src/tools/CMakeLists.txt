#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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

include_directories(../include .. ../dird ../filed ../stored)


add_executable(bsmtp bsmtp.cc)
target_link_libraries(bsmtp bareos ${JANSSON_LIBRARIES})

add_executable(drivetype drivetype.cc)
target_link_libraries(drivetype bareos bareosfind ${JANSSON_LIBRARIES})

add_executable(fstype fstype.cc)
target_link_libraries(fstype bareos bareosfind ${JANSSON_LIBRARIES})

add_executable(bregex bregex.cc)
target_link_libraries(bregex bareos bareosfind ${JANSSON_LIBRARIES})

add_executable(bwild bwild.cc)
target_link_libraries(bwild bareos ${JANSSON_LIBRARIES})

add_executable(bpluginfo bpluginfo.cc)
target_link_libraries(bpluginfo bareos ${DL_LIBRARIES} ${JANSSON_LIBRARIES})

add_executable(bscrypto bscrypto.cc)
target_link_libraries(bscrypto bareos ${JANSSON_LIBRARIES})

add_executable(timelimit timelimit.cc)

set (TOOLS_BIN
   bsmtp bwild bregex timelimit
   )
set (TOOLS_SBIN
   bpluginfo bscrypto bwild bregex timelimit
   )

INSTALL(TARGETS ${TOOLS_BIN} DESTINATION "${bindir}")
INSTALL(TARGETS ${TOOLS_SBIN} DESTINATION "${sbindir}")

# install bsmtp for non-root directors
INSTALL(TARGETS bsmtp DESTINATION "${bindir}")
INSTALL(TARGETS bsmtp DESTINATION "${sbindir}")
