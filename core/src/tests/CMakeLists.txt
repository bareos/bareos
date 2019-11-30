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


macro(bareos_add_test)
  set(options "")# no options
  set(oneValueArgs TESTNAME)
  set(multiValueArgs ADDITIONAL_SOURCES LINK_LIBRARIES COMPILE_DEFINITIONS)
  cmake_parse_arguments(BAREOS_ADD_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
  # testname additional_sources link_libraries compile_definitions)

  message("bareos_add_test: TESTNAME:\"${BAREOS_ADD_TEST_TESTNAME}\" \nADDITIONAL_SOURCES:\"${BAREOS_ADD_TEST_ADDITIONAL_SOURCES}\"\nLINK_LIBRARIES:\"${BAREOS_ADD_TEST_LINK_LIBRARIES}\"\nCOMPILE_DEFINITIONS:\"${BAREOS_ADD_TEST_COMPILE_DEFINITIONS}\"\n\n")

  add_executable(${BAREOS_ADD_TEST_TESTNAME}
      ${BAREOS_ADD_TEST_TESTNAME}.cc
      ${BAREOS_ADD_TEST_ADDITIONAL_SOURCES}
      )
    target_link_libraries(${BAREOS_ADD_TEST_TESTNAME} ${BAREOS_ADD_TEST_LINK_LIBRARIES})
  target_compile_definitions (${BAREOS_ADD_TEST_TESTNAME} PRIVATE ${BAREOS_ADD_TEST_COMPILE_DEFINITIONS})
  bareos_gtest_add_tests (${BAREOS_ADD_TEST_TESTNAME})
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

bareos_add_test( TESTNAME run_on_incoming_connect_interval LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> ${LMDB_LIBS} ${NDMP_LIBS} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
bareos_add_test( TESTNAME scheduler LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> ${LMDB_LIBS} ${NDMP_LIBS} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
bareos_add_test( TESTNAME scheduler_job_item_queue LINK_LIBRARIES  dird_objects bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_acl_entry_syntax LINK_LIBRARIES   bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_dir_plugins ADDITIONAL_SOURCES  ${PROJECT_SOURCE_DIR}/src/dird/dir_plugins.cc LINK_LIBRARIES  bareos bareoscats bareossql ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_fd_plugins ADDITIONAL_SOURCES ${PROJECT_SOURCE_DIR}/src/filed/fd_plugins.cc ${PROJECT_SOURCE_DIR}/src/filed/fileset.cc  LINK_LIBRARIES bareos bareosfind ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_is_name_valid LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_output_formatter  LINK_LIBRARIES   ${GTEST_MAIN_LIBRARIES} ${GTEST_LIBRARIES} ${JANSSON_LIBRARIES} bareos  )
bareos_add_test( TESTNAME test_sd_plugins LINK_LIBRARIES  bareos bareossd ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME version_strings  LINK_LIBRARIES bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )

bareos_add_test( TESTNAME job_control_record LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME multiplied_device_test  LINK_LIBRARIES ${LINK_LIBRARIES}  )
bareos_add_test( TESTNAME ndmp_address_translate_test ADDITIONAL_SOURCES  ../dird/ndmp_slot2elemaddr.cc  LINK_LIBRARIES  ${LINK_LIBRARIES}  )
bareos_add_test( TESTNAME recent_job_results_list LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}   COMPILE_DEFINITIONS TEST_TEMP_DIR=\"${TEST_TEMP_DIR}\"  TEST_ORIGINAL_FILE_DIR=\"${TEST_ORIGINAL_FILE_DIR}\")

bareos_add_test( TESTNAME show_cmd_available_resources_equals_config_resources LINK_LIBRARIES  dird_objects bareos bareosfind bareoscats bareossql ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_config_parser_console LINK_LIBRARIES   console_objects bareos bareosfind ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_config_parser_dir LINK_LIBRARIES  dird_objects bareos bareosfind bareoscats bareossql ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_config_parser_fd  LINK_LIBRARIES fd_objects bareos bareosfind ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_config_parser_sd LINK_LIBRARIES  stored_objects bareossd bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_crc32  ADDITIONAL_SOURCES ../stored/crc32/crc32.cc  LINK_LIBRARIES bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_fileindex_list LINK_LIBRARIES  dird_objects bareos bareosfind bareoscats bareossql $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> ${LMDB_LIBS} ${NDMP_LIBS} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME test_setdebug  LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> ${LMDB_LIBS} ${NDMP_LIBS} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME thread_list LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME thread_specific_data LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME timer_thread LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )

IF(HAVE_EXECINFO_H AND HAVE_BACKTRACE AND HAVE_BACKTRACE_SYMBOLS)
  bareos_add_test( TESTNAME test_backtrace LINK_LIBRARIES  bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
ENDIF()

if(GMOCK_FOUND)
  bareos_add_test( TESTNAME sd_reservation LINK_LIBRARIES  ${LINK_LIBRARIES} ${GMOCK_LIBRARIES}  )
ENDIF()

bareos_add_test( TESTNAME lib_tests ADDITIONAL_SOURCES alist_test.cc bareos_test_sockets.cc dlist_test.cc htable_test.cc qualified_resource_name_type_converter_test.cc ${PROJECT_SOURCE_DIR}/src/filed/evaluate_job_command.cc  LINK_LIBRARIES stored_objects bareossd bareos ${JANSSON_LIBRARIES} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
bareos_add_test( TESTNAME bsock_test_connection_setup ADDITIONAL_SOURCES ${SSL_UNIT_TEST_FILES} LINK_LIBRARIES   ${LINK_LIBRARIES}  )
IF(NOT HAVE_WIN32)
  bareos_add_test( TESTNAME test_bsock ADDITIONAL_SOURCES bareos_test_sockets.cc bsock_constructor_test.cc bsock_cert_verify_common_names_test.cc create_resource.cc ${SSL_UNIT_TEST_FILES} LINK_LIBRARIES  ${LINK_LIBRARIES}  COMPILE_DEFINITIONS -DCERTDIR=\"${CERTDIR}\")
    bareos_add_test( TESTNAME watchdog_timer LINK_LIBRARIES   bareos ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES}  )
endif()
