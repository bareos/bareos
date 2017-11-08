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


include_directories(../include ..)


add_executable(bregtest bregtest.cc )
target_link_libraries(bregtest bareos ${JANSSON_LIBRARIES})

add_executable(testls testls.cc)
target_link_libraries(testls bareosfind bareos ${JANSSON_LIBRARIES})

IF(NOT ${HAVE_WIN32})
   add_executable(bbatch bbatch.cc)
   target_link_libraries(bbatch bareoscats bareos  ${JANSSON_LIBRARIES})
ENDIF()

#add_executable(bvfs_test bvfs_test.cc)
#target_link_libraries(bvfs_test bareoscats bareossql)

#add_executable(cats_test cats_test.cc)
#target_link_libraries(cats_test bareoscats bareossql)

add_executable(gigaslam gigaslam.cc)
target_link_libraries(gigaslam)

add_executable(grow grow.cc)
target_link_libraries(grow bareos ${JANSSON_LIBRARIES})

set (TESTS_SBIN
   bregtest grow testls
   )
IF(NOT ${HAVE_WIN32})
   LIST(APPEND TESTS_SBIN bbatch)
ENDIF()

INSTALL(TARGETS ${TESTS_SBIN} DESTINATION "${sbindir}")
