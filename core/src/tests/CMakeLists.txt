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


macro(bareos_gtest_add_tests testname)
  gtest_add_tests(TARGET ${testname}
          TEST_PREFIX gtest:
          TEST_LIST found_tests)
IF(HAVE_WIN32)
   set_tests_properties(${found_tests}
          PROPERTIES ENVIRONMENT "WINEPATH=${WINEPATH}" )
ENDIF() #HAVE_WIN32
endmacro()


macro(bareos_add_test testname additional_sources link_libraries compile_definitions)
  message("bareos_add_test \"${testname}\" with additional sources \"${additional_sources}\" ,link_libraries \"${link_libraries}\" and compile definitions \"${compile_definitions}\" called ")
  add_executable(${testname}
      ${testname}.cc
      ${additional_sources}
      )
  target_link_libraries(${testname} ${link_libraries})
  target_compile_definitions(${testname} PRIVATE ${compile_definitions})
  bareos_gtest_add_tests(${testname})
endmacro()



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


# set some path variables used during test compilation
IF(HAVE_WIN32)
  set(RELATIVE_PROJECT_SOURCE_DIR "Z:${CMAKE_CURRENT_SOURCE_DIR}")
  set(TEST_ORIGINAL_FILE_DIR Z:${CMAKE_CURRENT_SOURCE_DIR}/recent_job_results_list)
  set(TEST_TEMP_DIR Z:${CMAKE_CURRENT_BINARY_DIR}/recent_job_results_list)
  set(CERTDIR Z:${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls)
ELSE()
  file(RELATIVE_PATH RELATIVE_PROJECT_SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  set(TEST_ORIGINAL_FILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/recent_job_results_list)
  set(TEST_TEMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/recent_job_results_list)
  set(CERTDIR ${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls)

ENDIF()
add_definitions(-DRELATIVE_PROJECT_SOURCE_DIR=\"${RELATIVE_PROJECT_SOURCE_DIR}\")

bareos_add_test( run_on_incoming_connect_interval "" "dird_objects;bareos;bareosfind;bareoscats;bareossql;$<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>;${LMDB_LIBS};${NDMP_LIBS};${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( scheduler "" "dird_objects;bareos;bareosfind;bareoscats;bareossql;$<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>;${LMDB_LIBS};${NDMP_LIBS};${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( scheduler_job_item_queue "" "dird_objects;bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( test_acl_entry_syntax "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( test_dir_plugins "${PROJECT_SOURCE_DIR}/src/dird/dir_plugins.cc" "bareos;bareoscats;bareossql;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( test_fd_plugins "${PROJECT_SOURCE_DIR}/src/filed/fd_plugins.cc;${PROJECT_SOURCE_DIR}/src/filed/fileset.cc" "bareos;bareosfind;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( test_is_name_valid "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( test_output_formatter  ""  "${GTEST_MAIN_LIBRARIES};${GTEST_LIBRARIES};${JANSSON_LIBRARIES};bareos" "")
bareos_add_test( test_sd_plugins "" "bareos;bareossd;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( version_strings "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")

  bareos_add_test( job_control_record "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( multiplied_device_test "" "${LINK_LIBRARIES}" "")
  bareos_add_test( ndmp_address_translate_test "../dird/ndmp_slot2elemaddr.cc" "${LINK_LIBRARIES}" "")
  bareos_add_test( recent_job_results_list "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "TEST_TEMP_DIR=\"${TEST_TEMP_DIR}\";TEST_ORIGINAL_FILE_DIR=\"${TEST_ORIGINAL_FILE_DIR}\"")

  bareos_add_test( show_cmd_available_resources_equals_config_resources "" "dird_objects;bareos;bareosfind;bareoscats;bareossql;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_config_parser_console "" "console_objects;bareos;bareosfind;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_config_parser_dir "" "dird_objects;bareos;bareosfind;bareoscats;bareossql;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_config_parser_fd "" "fd_objects;bareos;bareosfind;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_config_parser_sd "" "stored_objects;bareossd;bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_crc32 "../stored/crc32/crc32.cc" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_fileindex_list "" "dird_objects;bareos;bareosfind;bareoscats;bareossql;$<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>;${LMDB_LIBS};${NDMP_LIBS};${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( test_setdebug "" "dird_objects;bareos;bareosfind;bareoscats;bareossql;$<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>;${LMDB_LIBS};${NDMP_LIBS};${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( thread_list "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( thread_specific_data "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
  bareos_add_test( timer_thread "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")

IF(HAVE_EXECINFO_H AND HAVE_BACKTRACE AND HAVE_BACKTRACE_SYMBOLS)
  bareos_add_test(test_backtrace "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
ENDIF()

if(GMOCK_FOUND)
  bareos_add_test( sd_reservation "" "${LINK_LIBRARIES};${GMOCK_LIBRARIES}" "")
ENDIF()

bareos_add_test( lib_tests "alist_test.cc;bareos_test_sockets.cc;dlist_test.cc;htable_test.cc;qualified_resource_name_type_converter_test.cc;${PROJECT_SOURCE_DIR}/src/filed/evaluate_job_command.cc" "stored_objects;bareossd;bareos;${JANSSON_LIBRARIES};${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
bareos_add_test( bsock_test_connection_setup "${SSL_UNIT_TEST_FILES}" "${LINK_LIBRARIES}" "")
IF(NOT HAVE_WIN32)
bareos_add_test( test_bsock "bareos_test_sockets.cc;bsock_constructor_test.cc;bsock_cert_verify_common_names_test.cc;create_resource.cc;${SSL_UNIT_TEST_FILES}" "${LINK_LIBRARIES}" "-DCERTDIR=\"${CERTDIR}\"")
bareos_add_test( watchdog_timer "" "bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")
endif()
