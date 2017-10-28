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

include_directories(.. ../.. ../../include/)

set (TEST_SRC
   alist_test.cc
   base64_test.cc
   bsnprintf_test.cc
   devlock_test.cc
   dlist_test.cc
   edit_test.cc
   htable_test.cc
   #ini_test.cc
   junction_test.cc
   passphrase_test.cc
   rblist_test.cc
   rwlock_test.cc
   scan_test.cc
   sellist_test.cc
   test_lib.cc
   #tree_test.cc
   )


add_executable(test_lib
    ${TEST_SRC}
    )
target_link_libraries(test_lib
   bareos
   ${JANSSON_LIBRARIES}
   ${CMOCKA_LIBRARIES}
   )

add_test(NAME test_lib
   COMMAND test_lib)
add_dependencies(check test_lib)
