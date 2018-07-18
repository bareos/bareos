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
IF(HAVE_WIN32)
   link_libraries(kernel32
      user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
ENDIF()
set (TEST_SRC
    alist_test.cc
    dlist_test.cc
    htable_test.cc
    bsock_test.cc
   )


add_definitions(-DTEST_SMALL_HTABLE)

add_executable(test_lib
    ${TEST_SRC}
    )
target_link_libraries(test_lib
   bareos
   ${JANSSON_LIBRARIES}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
   )

add_test(NAME test_lib
   COMMAND test_lib
   )

set_property(TEST test_lib
   PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/lib"
   PROPERTY ENVIRONMENT "GTEST_COLOR=yes"
   )

add_dependencies(check test_lib)
