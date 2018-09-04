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

IF(HAVE_WIN32)
   link_libraries(kernel32
      user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
ENDIF()
set (TEST_SRC
    alist_test.cc
    dlist_test.cc
    htable_test.cc
    qualified_resource_name_type_converter_test.cc
   )


add_definitions(-DTEST_SMALL_HTABLE)

# where to find the certificates
add_definitions(-DCERTDIR=\"${CMAKE_SOURCE_DIR}/../regress/configs/BASE/tls\")

add_executable(test_lib
    ${TEST_SRC}
    )
target_link_libraries(test_lib
   stored_objects
   bareossd
   bareos
   bareoscfg
   ${JANSSON_LIBRARIES}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
   )

add_test(NAME test_lib
   COMMAND test_lib
   )

set_property(TEST test_lib
   PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/lib:${CMAKE_BINARY_DIR}/src/stored"
   PROPERTY ENVIRONMENT "GTEST_COLOR=yes"
   )

add_dependencies(check test_lib)

add_executable(test_bsock
    bsock_test.cc 
    bsock_dir_sd_connection_test.cc 
    bsock_constructor_test.cc 
    bsock_cert_verify_common_names_test.cc
    create_resource.cc
)

target_link_libraries(test_bsock
   stored_objects
   dird_objects
   console_objects
   bareossd
   bareos
   bareoscats
   bareoscfg
   bareossql
   bareosfind
   ${LMDB_LIBS}
   ${NDMP_LIBS}
   ${JANSSON_LIBRARIES}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
   )

add_test(NAME test_bsock
   COMMAND test_bsock
   )

set_property(TEST test_bsock
   PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/src/lib:${CMAKE_BINARY_DIR}/src/stored:${CMAKE_BINARY_DIR}/src/cats:${CMAKE_BINARY_DIR}/src/findlib:${CMAKE_BINARY_DIR}/src/lmdb:${CMAKE_BINARY_DIR}/src/ndmp"
   PROPERTY ENVIRONMENT "GTEST_COLOR=yes"
   )

add_dependencies(check test_bsock)
