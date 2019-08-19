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

# "gtest_discover_tests" was added in cmake 3.10
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
include(GoogleTest)

IF(HAVE_WIN32)
   link_libraries(kernel32
      user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
ENDIF()

IF(HAVE_OPENSSL)
  set(SSL_UNIT_TEST_FILES init_openssl.cc)
ENDIF()

set(LINK_LIBRARIES
   stored_objects
   dird_objects
   console_objects
   bareossd
   bareos
   bareoscats
   bareossql
   bareosfind
   ${LMDB_LIBS}
   ${NDMP_LIBS}
   ${JANSSON_LIBRARIES}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES})

IF(HAVE_PAM)
  LIST(APPEND LINK_LIBRARIES ${PAM_LIBRARIES})
ENDIF()
add_definitions(-DTEST_SMALL_HTABLE)

# where to find the certificates
add_definitions(-DCERTDIR=\"${PROJECT_SOURCE_DIR}/../regress/configs/BASE/tls\")

# where to find the source dir
add_definitions(-DPROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\")

####### test_lib ###############################
add_executable(test_lib
    alist_test.cc
    bareos_test_sockets.cc
    dlist_test.cc
    htable_test.cc
    qualified_resource_name_type_converter_test.cc
    lib_tests.cc
    ${PROJECT_SOURCE_DIR}/src/filed/evaluate_job_command.cc
    )

target_link_libraries(test_lib
   stored_objects
   bareossd
   bareos
   ${JANSSON_LIBRARIES}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
   )

 gtest_discover_tests(test_lib TEST_PREFIX gtest:)

####### test_fd_plugins #####################################
add_executable(test_fd_plugins
    test_fd_plugins.cc
    ${PROJECT_SOURCE_DIR}/src/filed/fd_plugins.cc
    ${PROJECT_SOURCE_DIR}/src/filed/fileset.cc
    )

target_link_libraries(test_fd_plugins
    bareos
    bareosfind
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    )

  gtest_discover_tests(test_fd_plugins TEST_PREFIX gtest:)

####### test_sd_plugins #####################################
add_executable(test_sd_plugins
    test_sd_plugins.cc
    )

target_link_libraries(test_sd_plugins
    bareos
    bareossd
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    )

  gtest_discover_tests(test_sd_plugins TEST_PREFIX gtest:)

####### test_dir_plugins #####################################
add_executable(test_dir_plugins
    test_dir_plugins.cc
    ${PROJECT_SOURCE_DIR}/src/dird/dir_plugins.cc
    )

target_link_libraries(test_dir_plugins
    bareos
    bareoscats
    bareossql
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    )

  gtest_discover_tests(test_dir_plugins TEST_PREFIX gtest:)

####### test_output_formatter ###############################
add_executable(test_output_formatter
    lib_tests_output_formatter.cc
    )
target_link_libraries(test_output_formatter
   bareos
   ${JANSSON_LIBRARIES}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
   )
 gtest_discover_tests(test_output_formatter TEST_PREFIX gtest:)

####### test_bsock ###############################
add_executable(test_bsock
    bsock_test.cc
    bareos_test_sockets.cc
    bsock_constructor_test.cc
    bsock_cert_verify_common_names_test.cc
    create_resource.cc
    ${SSL_UNIT_TEST_FILES}
    )
target_link_libraries(test_bsock ${LINK_LIBRARIES})
gtest_discover_tests(test_bsock TEST_PREFIX gtest:)

####### test_connection_setup ###############################
add_executable(test_connection_setup
    bsock_test_connection_setup.cc
    ${SSL_UNIT_TEST_FILES}
)

target_link_libraries(test_connection_setup ${LINK_LIBRARIES})

gtest_discover_tests(test_connection_setup TEST_PREFIX gtest:)


####### test_stored_multiplied_device ###############################
add_executable(test_stored_multiplied_device
  multiplied_device_test.cc
)

target_link_libraries(test_stored_multiplied_device ${LINK_LIBRARIES})
gtest_discover_tests(test_stored_multiplied_device TEST_PREFIX gtest:)


####### test_ndmp_address_translate ###############################
add_executable(test_ndmp_address_translate
  ndmp_address_translate_test.cc
  ../dird/ndmp_slot2elemaddr.cc
)

target_link_libraries(test_ndmp_address_translate ${LINK_LIBRARIES})
gtest_discover_tests(test_ndmp_address_translate TEST_PREFIX gtest:)

####### test_sd_reservation #####################################
if(GMOCK_FOUND)
  add_executable(test_sd_reservation sd_reservation.cc)

  target_link_libraries(test_sd_reservation ${LINK_LIBRARIES} ${GMOCK_LIBRARIES})
  gtest_discover_tests(test_sd_reservation TEST_PREFIX gtest:)
endif()

####### test_config_parser_sd #####################################
add_executable(test_config_parser_sd test_config_parser_sd.cc)

target_link_libraries(test_config_parser_sd
   stored_objects
   bareossd
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_config_parser_sd TEST_PREFIX gtest:)

####### test_config_parser_dir  #####################################
add_executable(test_config_parser_dir test_config_parser_dir.cc)

target_link_libraries(test_config_parser_dir
   dird_objects
   bareos
   bareosfind
   bareoscats
   bareossql
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_config_parser_dir TEST_PREFIX gtest:)


####### test_config_parser_console  #####################################
add_executable(test_config_parser_console test_config_parser_console.cc)

target_link_libraries(test_config_parser_console
   bareos
   bareosfind
   console_objects
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_config_parser_console TEST_PREFIX gtest:)


####### test_config_parser_fd  #####################################
add_executable(test_config_parser_fd test_config_parser_fd.cc ${FDSRCS})

target_link_libraries(test_config_parser_fd
   bareos
   bareosfind
   fd_objects
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_config_parser_fd TEST_PREFIX gtest:)

####### test_setdebug #####################################
add_executable(test_setdebug test_setdebug.cc)

target_link_libraries(test_setdebug
   dird_objects
   bareos
   bareosfind
   bareoscats
   bareossql
   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>
   ${LMDB_LIBS}
   ${NDMP_LIBS}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_setdebug TEST_PREFIX gtest:)

####### test_fileindex_list #####################################
add_executable(test_fileindex_list test_fileindex_list.cc)

target_link_libraries(test_fileindex_list
 dird_objects
 bareos
 bareosfind
 bareoscats
 bareossql
 $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>
 ${LMDB_LIBS}
 ${NDMP_LIBS}
 ${GTEST_LIBRARIES}
 ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_fileindex_list TEST_PREFIX gtest:)

####### test_backtrace #####################################
IF(HAVE_EXECINFO_H AND HAVE_BACKTRACE AND HAVE_BACKTRACE_SYMBOLS)
  add_executable(test_backtrace test_backtrace.cc)

  target_link_libraries(test_backtrace
     bareos
     ${GTEST_LIBRARIES}
     ${GTEST_MAIN_LIBRARIES}
  )

  gtest_discover_tests(test_backtrace TEST_PREFIX gtest:)
ENDIF()

####### test_backtrace #####################################
add_executable(test_crc32 test_crc32.cc ../stored/crc32/Crc32.cpp)

target_link_libraries(test_crc32
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

gtest_discover_tests(test_crc32 TEST_PREFIX gtest:)
