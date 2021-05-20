#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2021 Bareos GmbH & Co. KG
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

include_directories(${OPENSSL_INCLUDE_DIR})

macro(bareos_gtest_add_tests testname)
  set(options OPTIONAL DISABLE)
  cmake_parse_arguments(
    BAREOS_GTEST_ADD_TESTS "${options}" "${oneValueArgs}" "${multiValueArgs}"
    ${ARGN}
  )
  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX gtest:
    TEST_LIST found_tests
  )
  if(BAREOS_GTEST_ADD_TESTS_DISABLE)
    message("DISABELING ${found_tests}")
    set_tests_properties(${found_tests} PROPERTIES DISABLED TRUE)
  else()
    message("NOT DISABELING ${found_tests}")
  endif()

  set_tests_properties(${found_tests} PROPERTIES TIMEOUT 120)
  if(HAVE_WIN32)
    set_tests_properties(
      ${found_tests} PROPERTIES ENVIRONMENT "WINEPATH=${WINEPATH}"
    )
  endif() # HAVE_WIN32
endmacro() # bareos_gtest_add_tests

macro(bareos_add_test BAREOS_ADD_TEST_TESTNAME)
  set(options SKIP_GTEST DISABLE)
  set(multiValueArgs ADDITIONAL_SOURCES LINK_LIBRARIES COMPILE_DEFINITIONS)
  cmake_parse_arguments(
    BAREOS_ADD_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
  )

  add_executable(
    ${BAREOS_ADD_TEST_TESTNAME} ${BAREOS_ADD_TEST_TESTNAME}.cc
                                ${BAREOS_ADD_TEST_ADDITIONAL_SOURCES}
  )
  target_link_libraries(
    ${BAREOS_ADD_TEST_TESTNAME} ${BAREOS_ADD_TEST_LINK_LIBRARIES}
  )
  target_compile_definitions(
    ${BAREOS_ADD_TEST_TESTNAME} PRIVATE ${BAREOS_ADD_TEST_COMPILE_DEFINITIONS}
  )
  if(BAREOS_ADD_TEST_DISABLE)
    set(disable "DISABLE")
  else()
    set(disable "")
  endif()

  if(NOT BAREOS_ADD_TEST_SKIP_GTEST)
    bareos_gtest_add_tests(${BAREOS_ADD_TEST_TESTNAME} ${disable})
  endif()
endmacro() # bareos_add_test

include(GoogleTest)

if(HAVE_WIN32)
  link_libraries(
    kernel32
    user32
    gdi32
    winspool
    shell32
    ole32
    oleaut32
    uuid
    comdlg32
    advapi32
  )
endif() # HAVE_WIN32

if(HAVE_OPENSSL)
  set(SSL_UNIT_TEST_FILES init_openssl.cc)
endif()

set(LINK_LIBRARIES
    stored_objects
    dird_objects
    console_objects
    bareossd
    bareos
    bareoscats
    bareossql
    bareosfind
    GTest::gtest_main
)

if(HAVE_PAM)
  list(APPEND LINK_LIBRARIES ${PAM_LIBRARIES})
endif()

# set some path variables used during test compilation
if(HAVE_WIN32)
  set(RELATIVE_PROJECT_SOURCE_DIR "Z:${CMAKE_CURRENT_SOURCE_DIR}")
  set(TEST_ORIGINAL_FILE_DIR Z:${CMAKE_CURRENT_SOURCE_DIR}/statefile)
  set(TEST_TEMP_DIR Z:${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp/write-test)

  set(CERTDIR Z:${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls)
else() # NOT HAVE_WIN32
  file(RELATIVE_PATH RELATIVE_PROJECT_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}
       ${CMAKE_CURRENT_SOURCE_DIR}
  )
  set(TEST_ORIGINAL_FILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/statefile)
  set(TEST_TEMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp/write-test)

  set(CERTDIR ${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls)
endif() # HAVE_WIN32

add_definitions(
  -DRELATIVE_PROJECT_SOURCE_DIR=\"${RELATIVE_PROJECT_SOURCE_DIR}\"
)

if(NOT client-only)
  bareos_add_test(
    run_on_incoming_connect_interval
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
  )
endif() # NOT client-only

if(NOT client-only)
  bareos_add_test(
    scheduler
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
  )
endif() # NOT client-only

if(NOT client-only)
  bareos_add_test(
    scheduler_job_item_queue
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   GTest::gtest_main
  )
endif() # NOT client-only

bareos_add_test(test_acl_entry_syntax LINK_LIBRARIES bareos GTest::gtest_main)

if(NOT client-only)
  bareos_add_test(
    test_db_list_ctx LINK_LIBRARIES bareos bareoscats bareossql
                                    GTest::gtest_main
  )
endif()

if(NOT client-only)
  bareos_add_test(
    test_dir_plugins
    ADDITIONAL_SOURCES ${PROJECT_SOURCE_DIR}/src/dird/dir_plugins.cc
    LINK_LIBRARIES bareos bareoscats bareossql GTest::gtest_main
  )
endif() # NOT client-only

bareos_add_test(
  test_fd_plugins
  ADDITIONAL_SOURCES ${PROJECT_SOURCE_DIR}/src/filed/fd_plugins.cc
                     ${PROJECT_SOURCE_DIR}/src/filed/fileset.cc
  LINK_LIBRARIES bareos bareosfind GTest::gtest_main
)

bareos_add_test(test_is_name_valid LINK_LIBRARIES bareos GTest::gtest_main)

bareos_add_test(test_output_formatter LINK_LIBRARIES GTest::gtest_main bareos)

if(NOT client-only)
  bareos_add_test(
    test_sd_plugins LINK_LIBRARIES bareos bareossd GTest::gtest_main
  )
endif() # NOT client-only

bareos_add_test(version_strings LINK_LIBRARIES bareos GTest::gtest_main)

bareos_add_test(job_control_record LINK_LIBRARIES bareos GTest::gtest_main)

bareos_add_test(
  cram_md5
  LINK_LIBRARIES bareos Threads::Threads GTest::gtest_main
  ADDITIONAL_SOURCES bareos_test_sockets.cc
)

if(NOT client-only)
  bareos_add_test(multiplied_device_test LINK_LIBRARIES ${LINK_LIBRARIES})
endif()

if(NOT client-only)
  bareos_add_test(
    ndmp_address_translate_test
    ADDITIONAL_SOURCES ../dird/ndmp_slot2elemaddr.cc
    LINK_LIBRARIES ${LINK_LIBRARIES}
  )
endif() # NOT client-only

bareos_add_test(
  statefile
  LINK_LIBRARIES bareos GTest::gtest_main
  COMPILE_DEFINITIONS TEST_TEMP_DIR=\"${TEST_TEMP_DIR}\"
                      TEST_ORIGINAL_FILE_DIR=\"${TEST_ORIGINAL_FILE_DIR}\"
)

if(HAVE_BIG_ENDIAN)
  set_tests_properties(gtest:statefile.read PROPERTIES DISABLED TRUE)
  set_tests_properties(gtest:statefile.write PROPERTIES DISABLED TRUE)
  set_tests_properties(
    gtest:statefile.handle_truncated_jobs PROPERTIES DISABLED TRUE
  )
  set_tests_properties(
    gtest:statefile.handle_truncated_headers PROPERTIES DISABLED TRUE
  )
  set_tests_properties(
    gtest:statefile.handle_nonexisting_file PROPERTIES DISABLED TRUE
  )
endif()

if(NOT client-only)
  bareos_add_test(
    show_cmd_available_resources_equals_config_resources
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   GTest::gtest_main
  )
endif() # NOT client-only

bareos_add_test(
  test_config_parser_console LINK_LIBRARIES console_objects bareos bareosfind
                                            GTest::gtest_main
)

if(NOT client-only)
  bareos_add_test(
    test_config_parser_dir
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   GTest::gtest_main
  )
endif() # NOT client-only

bareos_add_test(
  test_config_parser_fd LINK_LIBRARIES fd_objects bareos bareosfind
                                       GTest::gtest_main
)

if(NOT client-only)
  bareos_add_test(
    test_config_parser_sd LINK_LIBRARIES stored_objects bareossd bareos
                                         GTest::gtest_main
  )
endif() # NOT client-only

if(NOT client-only)
  bareos_add_test(
    test_crc32
    ADDITIONAL_SOURCES ../stored/crc32/crc32.cc
    LINK_LIBRARIES bareos GTest::gtest_main
  )

  bareos_add_test(
    test_fileindex_list
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
  )
endif() # NOT client-only

if(NOT client-only)
  bareos_add_test(
    test_setdebug
    LINK_LIBRARIES dird_objects bareos bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
  )
endif() # NOT client-only

bareos_add_test(thread_list LINK_LIBRARIES bareos GTest::gtest_main)
bareos_add_test(
  thread_specific_data LINK_LIBRARIES bareos Threads::Threads GTest::gtest_main
)

bareos_add_test(timer_thread LINK_LIBRARIES bareos GTest::gtest_main)

if(NOT client-only)
  bareos_add_test(
    catalog
    LINK_LIBRARIES bareos dird_objects bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
    SKIP_GTEST
  )
endif()

if(HAVE_EXECINFO_H
   AND HAVE_BACKTRACE
   AND HAVE_BACKTRACE_SYMBOLS
)
  bareos_add_test(test_backtrace LINK_LIBRARIES bareos GTest::gtest_main)
endif() # HAVE_EXECINFO_H ..

if(NOT client-only)
  if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS") # disable on solaris
    set(disable "DISABLE")
  else()
    set(disable "")
  endif()
  bareos_add_test(
    sd_reservation
    ${disable}
    LINK_LIBRARIES ${LINK_LIBRARIES} GTest::gmock
  )
endif()

if(NOT client-only)
  bareos_add_test(sd_backend LINK_LIBRARIES ${LINK_LIBRARIES} GTest::gmock)
endif()

if(NOT client-only)
  bareos_add_test(
    lib_tests
    ADDITIONAL_SOURCES
      alist_test.cc bareos_test_sockets.cc bsys_test.cc dlist_test.cc
      htable_test.cc qualified_resource_name_type_converter_test.cc
      ${PROJECT_SOURCE_DIR}/src/filed/evaluate_job_command.cc
    LINK_LIBRARIES stored_objects bareossd bareos GTest::gtest_main
  )

  bareos_add_test(
    bsock_test_connection_setup
    ADDITIONAL_SOURCES ${SSL_UNIT_TEST_FILES}
    LINK_LIBRARIES ${LINK_LIBRARIES}
  )

  bareos_add_test(bool_string LINK_LIBRARIES bareos GTest::gtest_main)

endif() # NOT client-only

if(NOT HAVE_WIN32 AND NOT client-only)
  bareos_add_test(
    test_bsock
    ADDITIONAL_SOURCES
      bareos_test_sockets.cc bsock_constructor_test.cc
      bsock_cert_verify_common_names_test.cc create_resource.cc
      ${SSL_UNIT_TEST_FILES}
    LINK_LIBRARIES ${LINK_LIBRARIES}
    COMPILE_DEFINITIONS -DCERTDIR=\"${CERTDIR}\"
  )
  bareos_add_test(watchdog_timer LINK_LIBRARIES bareos GTest::gtest_main)
endif() # NOT HAVE_WIN32 AND NOT client-only

if(NOT client-only)
  bareos_add_test(sort_stringvector LINK_LIBRARIES bareos GTest::gtest_main)
endif() # NOT client-only

if(NOT client-only)
  bareos_add_test(
    messages_resource
    LINK_LIBRARIES bareos dird_objects bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
    SKIP_GTEST
  )
endif() # NOT client-only

bareos_add_test(test_edit LINK_LIBRARIES bareos GTest::gtest_main)

if(NOT client-only)
  bareos_add_test(
    setdevice
    LINK_LIBRARIES bareos dird_objects bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
  )
endif() # NOT client-only

if(NOT client-only)
  bareos_add_test(
    time_format_test
    LINK_LIBRARIES bareos dird_objects bareosfind bareoscats bareossql
                   $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}> GTest::gtest_main
  )
endif() # NOT client-only
