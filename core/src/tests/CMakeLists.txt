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

# where to find the source dir
file(RELATIVE_PATH RELATIVE_PROJECT_SOURCE_DIR  ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR} )
add_definitions(-DRELATIVE_PROJECT_SOURCE_DIR=\"${RELATIVE_PROJECT_SOURCE_DIR}\")

####### test_lib ###############################
add_executable(test_lib
    alist_test.cc
    #    bareos_test_sockets.cc
    dlist_test.cc
    htable_test.cc
    #qualified_resource_name_type_converter_test.cc
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

target_compile_definitions(test_lib PRIVATE -DTEST_SMALL_HTABLE)
bareos_gtest_add_tests(test_lib)

####### test_fd_plugins #####################################
bareos_add_test(test_fd_plugins
  "${PROJECT_SOURCE_DIR}/src/filed/fd_plugins.cc;${PROJECT_SOURCE_DIR}/src/filed/fileset.cc"
  "bareos;bareosfind;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}"
  ""
  )

####### test_sd_plugins #####################################
bareos_add_test(test_sd_plugins "" "bareos;bareossd;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")

####### test_dir_plugins #####################################
bareos_add_test(test_dir_plugins "${PROJECT_SOURCE_DIR}/src/dird/dir_plugins.cc" "bareos;bareoscats;bareossql;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")

####### test_output_formatter ###############################
bareos_add_test(test_output_formatter  ""  "${GTEST_MAIN_LIBRARIES};${GTEST_LIBRARIES};${JANSSON_LIBRARIES};bareos" "")

IF(NOT HAVE_WIN32)
####### test_bsock ###############################
bareos_add_test(test_bsock
  "bareos_test_sockets.cc;bsock_constructor_test.cc;bsock_cert_verify_common_names_test.cc;create_resource.cc;${SSL_UNIT_TEST_FILES}"
  "${LINK_LIBRARIES}"
  "-DCERTDIR=\"${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls\""
  )

####### test_connection_setup ###############################
bareos_add_test( bsock_test_connection_setup "${SSL_UNIT_TEST_FILES}" "${LINK_LIBRARIES}" "")

####### test_stored_multiplied_device ###############################
bareos_add_test( multiplied_device_test "" "${LINK_LIBRARIES}" "")

####### test_ndmp_address_translate ###############################
bareos_add_test(ndmp_address_translate_test "../dird/ndmp_slot2elemaddr.cc" "${LINK_LIBRARIES}" "")

####### test_sd_reservation #####################################
if(GMOCK_FOUND)
  bareos_add_test(sd_reservation "" "${LINK_LIBRARIES};${GMOCK_LIBRARIES}" "")
endif()

####### test_config_parser_sd #####################################
bareos_add_test(test_config_parser_sd "" "stored_objects;bareossd;bareos;${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARIES}" "")

# add_executable(test_config_parser_sd test_config_parser_sd.cc)

# target_link_libraries(test_config_parser_sd
#    stored_objects
#    bareossd
#    bareos
#    ${GTEST_LIBRARIES}
#    ${GTEST_MAIN_LIBRARIES}
# )

#    bareos_gtest_add_tests(test_config_parser_sd)


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

   bareos_gtest_add_tests(test_config_parser_dir)

####### test_config_parser_console  #####################################
add_executable(test_config_parser_console test_config_parser_console.cc)

target_link_libraries(test_config_parser_console
   bareos
   bareosfind
   console_objects
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_config_parser_console)

####### test_config_parser_fd  #####################################
add_executable(test_config_parser_fd test_config_parser_fd.cc ${FDSRCS})

target_link_libraries(test_config_parser_fd
   bareos
   bareosfind
   fd_objects
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_config_parser_fd)

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
   bareos_gtest_add_tests(test_setdebug)


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

   bareos_gtest_add_tests(test_fileindex_list)

####### test_backtrace #####################################
IF(HAVE_EXECINFO_H AND HAVE_BACKTRACE AND HAVE_BACKTRACE_SYMBOLS)
  add_executable(test_backtrace test_backtrace.cc)

  target_link_libraries(test_backtrace
     bareos
     ${GTEST_LIBRARIES}
     ${GTEST_MAIN_LIBRARIES}
  )

   bareos_gtest_add_tests(test_backtrace)

ENDIF()

####### test_crc32 #####################################
add_executable(test_crc32 test_crc32.cc ../stored/crc32/crc32.cc)

target_link_libraries(test_crc32
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_crc32)
####### thread_list  #####################################
add_executable(thread_list thread_list.cc)

target_link_libraries(thread_list
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(thread_list)

####### ua_available_resources_equals_config_resources  ###########
add_executable(show_cmd_available_resources_equals_config_resources show_cmd_available_resources_equals_config_resources.cc)

target_link_libraries(show_cmd_available_resources_equals_config_resources
   dird_objects
   bareos
   bareosfind
   bareoscats
   bareossql
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(show_cmd_available_resources_equals_config_resources)

####### test_job_control_record #####################################
add_executable(test_job_control_record job_control_record.cc)

target_link_libraries(test_job_control_record
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)
   bareos_gtest_add_tests(test_job_control_record)

####### test_recent_job_results_list #####################################

add_executable(test_recent_job_results_list recent_job_results_list.cc)

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/src/tests/test_recent_job_results_list_tmp)
target_compile_definitions(test_recent_job_results_list
  PRIVATE TEST_TEMP_DIR=\"${PROJECT_BINARY_DIR}/src/tests/test_recent_job_results_list_tmp\")
target_compile_definitions(test_recent_job_results_list
  PRIVATE TEST_ORIGINAL_FILE_DIR=\"${PROJECT_SOURCE_DIR}/src/tests/recent_job_results_list\")

target_link_libraries(test_recent_job_results_list
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_recent_job_results_list)

####### test_job_control_record #####################################
add_executable(test_thread_specific_data thread_specific_data.cc)

target_link_libraries(test_thread_specific_data
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_thread_specific_data)


####### test_timer_thread  #####################################
add_executable(test_timer_thread timer_thread.cc)

target_link_libraries(test_timer_thread
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)
bareos_gtest_add_tests(test_timer_thread)


####### test_watchdog_timer ##########################################
add_executable(test_watchdog_timer watchdog_timer.cc)

target_link_libraries(test_watchdog_timer
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)
   bareos_gtest_add_tests(test_watchdog_timer)


####### run_on_incoming_connect_interval  #####################################
add_executable(run_on_incoming_connect_interval run_on_incoming_connect_interval.cc)

target_link_libraries(run_on_incoming_connect_interval
   dird_objects
   bareos
   bareosfind
   bareoscats
   bareossql
   ${LMDB_LIBS}
   ${NDMP_LIBS}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

IF(HAVE_PAM)
   target_link_libraries(run_on_incoming_connect_interval ${PAM_LIBRARIES})
ENDIF()

   bareos_gtest_add_tests(client-initiated-reconnect)

####### scheduler_job_item_queue  #####################################
add_executable(scheduler_job_item_queue scheduler_job_item_queue.cc)

target_link_libraries(scheduler_job_item_queue
   dird_objects
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

IF(HAVE_PAM)
   target_link_libraries(scheduler_job_item_queue ${PAM_LIBRARIES})
ENDIF()

   bareos_gtest_add_tests(scheduler_job_item_queue)

####### scheduler  #####################################
add_executable(scheduler scheduler.cc)

target_link_libraries(scheduler
   dird_objects
   bareos
   bareosfind
   bareoscats
   bareossql
   ${LMDB_LIBS}
   ${NDMP_LIBS}
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

IF(HAVE_PAM)
   target_link_libraries(scheduler ${PAM_LIBRARIES})
ENDIF()

   bareos_gtest_add_tests(scheduler)

####### test_is_name_valid  #####################################
add_executable(test_is_name_valid test_is_name_valid.cc)

target_link_libraries(test_is_name_valid
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_is_name_valid)


####### test_acl_entry_syntax  #####################################
add_executable(test_acl_entry_syntax test_acl_entry_syntax.cc)

target_link_libraries(test_acl_entry_syntax
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)

   bareos_gtest_add_tests(test_acl_entry_syntax)



####### version_strings  #####################################
add_executable(version_strings version_strings.cc)

target_link_libraries(version_strings
   bareos
   ${GTEST_LIBRARIES}
   ${GTEST_MAIN_LIBRARIES}
)
   bareos_gtest_add_tests(version_strings)


