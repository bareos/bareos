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

include_directories(
   ../include .. ../dird ../filed ../stored
   ${PTHREAD_INCLUDE_DIRS}
   ${FASTLZ_INCLUDE_DIRS}
   ${ZLIB_INCLUDE_DIRS}
   ${ACL_INCLUDE_DIRS}
   ${LZO2_INCLUDE_DIRS}
   ${CAP_INCLUDE_DIRS}
   ${WRAP_INCLUDE_DIRS}
   )
include_directories(BEFORE ${OPENSSL_INCLUDE_DIRS} )

MESSAGE( STATUS "src/tools build: " )
get_property(got_d_include_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
foreach(_dir IN LISTS got_d_include_dirs)
   message(STATUS "  -I ${_dir}")
endforeach()


SET(BSMTPSRCS bsmtp.cc)
IF(HAVE_WIN32)
   LIST(APPEND BSMTPSRCS ../win32/tools/bsmtpres.rc)
ENDIF()
add_executable(bsmtp ${BSMTPSRCS})
target_link_libraries(bsmtp bareos ${JANSSON_LIBRARIES})

add_executable(drivetype drivetype.cc)
target_link_libraries(drivetype bareos bareosfind ${JANSSON_LIBRARIES})

add_executable(fstype fstype.cc)
target_link_libraries(fstype bareos bareosfind ${JANSSON_LIBRARIES})

SET(BREGEXSRCS bregex.cc)
IF(HAVE_WIN32)
   LIST(APPEND BREGEXSRCS ../win32/tools/bregexres.rc)
ENDIF()
add_executable(bregex ${BREGEXSRCS})
target_link_libraries(bregex bareos bareosfind ${JANSSON_LIBRARIES})

SET(BWILDSRCS bwild.cc)
IF(HAVE_WIN32)
   LIST(APPEND BWILDSRCS ../win32/tools/bwildres.rc)
ENDIF()
add_executable(bwild ${BWILDSRCS})
target_link_libraries(bwild bareos ${JANSSON_LIBRARIES})

add_executable(bscrypto bscrypto.cc)
target_link_libraries(bscrypto bareos ${JANSSON_LIBRARIES})

IF(NOT HAVE_WIN32)
   add_executable(bpluginfo bpluginfo.cc)
   target_link_libraries(bpluginfo bareos ${DL_LIBRARIES} ${JANSSON_LIBRARIES})

   add_executable(timelimit timelimit.cc)
ENDIF()

set (TOOLS_BIN
   bsmtp bwild bregex
   )

set (TOOLS_SBIN
   bscrypto bwild bregex
   )

IF(NOT HAVE_WIN32)
   LIST(APPEND TOOLS_BIN  timelimit)
   LIST(APPEND TOOLS_SBIN bpluginfo timelimit)
ENDIF()

INSTALL(TARGETS ${TOOLS_BIN} DESTINATION "${bindir}")
INSTALL(TARGETS ${TOOLS_SBIN} DESTINATION "${sbindir}")

# install bsmtp for non-root directors
INSTALL(TARGETS bsmtp DESTINATION "${bindir}")
INSTALL(TARGETS bsmtp DESTINATION "${sbindir}")
