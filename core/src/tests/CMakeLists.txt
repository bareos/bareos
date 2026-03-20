#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2026 Bareos GmbH & Co. KG
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

# if we built the object files as fat lto objects, we add -fno-lto to the
# linker-flags of our unit-tests, so we don't spend time optimizing binaries
# that will only run a few times during testing
if(CMAKE_CXX_FLAGS MATCHES -ffat-lto-objects)
  add_link_options(-fno-lto)
endif()

include(BareosConfigureFile)

# required to generate configuration for some SD tests
include(BareosTargetTools)
if(NOT client-only)
  get_target_output_dir(BACKEND_OUTPUT_DIRECTORY bareossd-file)
endif()

bareos_configure_file(GLOB_RECURSE "configs/*")

if(NOT (${CMAKE_SYSTEM_NAME} MATCHES "Linux"))
  set(LINUX_KERNEL_VERSION "0.0")
else()
  execute_process(
    COMMAND uname -r
    OUTPUT_VARIABLE UNAME_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(-- " Kernel version: " ${UNAME_RESULT})
  string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${UNAME_RESULT})
endif()

set(KTLS_DEFINITIONS)
set(DISABLE_KTLS OFF)
find_package(OpenSSL)
if((OPENSSL_VERSION VERSION_LESS "3.0.0") OR (LINUX_KERNEL_VERSION VERSION_LESS
                                              "5.3")
)
  set(DISABLE_KTLS ON)
else()
  cmake_push_check_state()
  set(CMAKE_REQUIRED_INCLUDES "${OPENSSL_INCLUDE_DIR}")
  check_cxx_source_compiles(
    "
#include <openssl/ssl.h>
int main(void)
{
#ifndef OPENSSL_NO_KTLS
#error ktls is enabled
#endif
return 0;
}
"
    KTLS_DISABLED
  )
  cmake_pop_check_state()
  if(KTLS_DISABLED)
    set(DISABLE_KTLS ON)
  endif()
  if(LINUX_KERNEL_VERSION VERSION_LESS "5.16")
    list(APPEND KTLS_DEFINITIONS DISABLE_KTLS_12_RECV)
  endif()
  if(LINUX_KERNEL_VERSION VERSION_LESS "5.20" OR OPENSSL_VERSION VERSION_LESS
                                                 "3.2.0"
  )
    list(APPEND KTLS_DEFINITIONS DISABLE_KTLS_13_RECV)
  endif()

  if(LINUX_KERNEL_VERSION VERSION_LESS "6.1")
    list(APPEND KTLS_DEFINITIONS DISABLE_KTLS_12_256_SEND
         DISABLE_KTLS_13_256_SEND
    )
  endif()
endif()

macro(bareos_gtest_add_tests testname)
  set(options OPTIONAL DISABLE)
  cmake_parse_arguments(
    BAREOS_GTEST_ADD_TESTS "${options}" "${oneValueArgs}" "${multiValueArgs}"
    ${ARGN}
  )
  gtest_discover_tests(
    ${testname}
    TEST_PREFIX gtest:
    PROPERTIES
    TIMEOUT 120 SKIP_RETURN_CODE 77 DISABLED ${BAREOS_GTEST_ADD_TESTS_DISABLE}
  )
endmacro() # bareos_gtest_add_tests

macro(bareos_add_test BAREOS_ADD_TEST_TESTNAME)
  # special option for gtests that aren't supposed to run alone, since they are
  # executed by systemtests with which they have dependencies.
  set(options SKIP_GTEST DISABLE)
  set(multiValueArgs ADDITIONAL_SOURCES LINK_LIBRARIES COMPILE_DEFINITIONS
                     COMPILE_OPTIONS INCLUDE_DIRECTORIES
  )
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
  target_compile_options(
    ${BAREOS_ADD_TEST_TESTNAME} PRIVATE ${BAREOS_ADD_TEST_COMPILE_OPTIONS}
  )
  target_include_directories(
    ${BAREOS_ADD_TEST_TESTNAME} PRIVATE ${BAREOS_ADD_TEST_INCLUDE_DIRECTORIES}
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

set(LINK_LIBRARIES Bareos::SD Bareos::Dir Bareos::Console Bareos::LibSD
                   Bareos::Lib Bareos::SQL Bareos::Findlib GTest::gtest_main
)

# set some path variables used during test compilation
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
  set(RELATIVE_PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  set(TEST_ORIGINAL_FILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/statefile)
  set(TEST_TEMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp/write-test)
  set(CERTDIR ${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls)
else() # NOT HAVE_WIN32
  set(TEST_ORIGINAL_FILE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/statefile)
  set(TEST_TEMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/statefile_tmp/write-test)
  set(CERTDIR ${CMAKE_CURRENT_SOURCE_DIR}/configs/test_bsock/tls)
endif() # HAVE_WIN32

# Keep alphabetically ordered
if(NOT client-only)
  set(COMMON_SRC testing_dir_common.cc testing_sd_common.cc)
  add_library(testing_common STATIC ${COMMON_SRC})
  target_link_libraries(testing_common PRIVATE ${LINK_LIBRARIES})

  bareos_add_test(
    addresses_and_ports_config
    LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib Bareos::SQL
                   testing_common GTest::gtest_main
  )

  bareos_add_test(
    addresses_and_ports_functions
    LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib Bareos::SQL
                   testing_common GTest::gtest_main
  )

  bareos_add_test(
    append_test LINK_LIBRARIES Bareos::Lib Bareos::SD Bareos::LibSD
                               Bareos::Findlib GTest::gtest_main
  )

  bareos_add_test(
    bad_configs
    configure
    LINK_LIBRARIES testing_common Bareos::Dir Bareos::Findlib Bareos::SQL
                   GTest::gtest_main
  )

  bareos_add_test(
    berrno_test LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib
                               Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(bool_string LINK_LIBRARIES Bareos::Lib GTest::gtest_main)
  bareos_add_test(
    bsock_test_connection_setup
    ADDITIONAL_SOURCES init_openssl.cc
    LINK_LIBRARIES testing_common ${LINK_LIBRARIES}
  )

  if(NOT DISABLE_KTLS)
    bareos_add_test(
      ktls
      ADDITIONAL_SOURCES init_openssl.cc
      LINK_LIBRARIES testing_common ${LINK_LIBRARIES}
      COMPILE_DEFINITIONS
        ${KTLS_DEFINITIONS} DISABLE TRUE # we need to find a good solution for
                                         # this
    )
  endif()

  bareos_add_test(
    catalog
    LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib Bareos::SQL
                   GTest::gtest_main
    SKIP_GTEST # used by systemtest catalog
  )

  bareos_add_test(
    cli_test LINK_LIBRARIES Bareos::Lib CLI11::CLI11 GTest::gtest_main
  )

  bareos_add_test(
    configure LINK_LIBRARIES testing_common Bareos::Dir Bareos::Findlib
                             Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    dir_fd_connection
    LINK_LIBRARIES testing_common Bareos::Dir Bareos::Lib Bareos::SQL
                   Bareos::Findlib GTest::gtest_main
  )
  bareos_add_test(
    dir_statistics_thread
    LINK_LIBRARIES testing_common Bareos::Dir Bareos::Lib Bareos::SQL
                   Bareos::Findlib GTest::gtest_main
  )
  bareos_add_test(
    globbing_test LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib
                                 Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    lib_tests
    ADDITIONAL_SOURCES
      alist_test.cc bareos_test_sockets.cc bsys_test.cc dlist_test.cc
      htable_test.cc qualified_resource_name_type_converter_test.cc
      ${PROJECT_SOURCE_DIR}/src/filed/evaluate_job_command.cc
    LINK_LIBRARIES Bareos::SD Bareos::LibSD Bareos::Lib GTest::gtest_main
  )
  bareos_add_test(
    messages_resource
    LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib Bareos::SQL
                   GTest::gtest_main
    SKIP_GTEST # used by systemtest messages
  )
  bareos_add_test(
    multicolumn_prompts LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib
                                       Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(multiplied_device_test LINK_LIBRARIES ${LINK_LIBRARIES})
  bareos_add_test(
    ndmp_address_translate_test
    ADDITIONAL_SOURCES ../dird/ndmp_slot2elemaddr.cc
    LINK_LIBRARIES ${LINK_LIBRARIES}
  )

  bareos_add_test(pruning LINK_LIBRARIES testing_common GTest::gtest_main)
  bareos_add_test(
    runjob LINK_LIBRARIES Bareos::Dir Bareos::Findlib Bareos::SQL
                          GTest::gtest_main
  )

  bareos_add_test(
    run_on_incoming_connect_interval
    LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib Bareos::SQL
                   GTest::gtest_main
  )
  bareos_add_test(
    show_cmd_available_resources_equals_config_resources
    LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib Bareos::SQL
                   GTest::gtest_main
  )
  bareos_add_test(
    scheduler LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib
                             Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    scheduler_job_item_queue
    LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib Bareos::SQL
                   GTest::gtest_main
  )
  bareos_add_test(sd_backend LINK_LIBRARIES ${LINK_LIBRARIES})
  if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS") # disable on solaris
    set(disable "DISABLE")
  else()
    set(disable "")
  endif()
  bareos_add_test(
    sd_reservation
    ${disable}
    LINK_LIBRARIES Bareos::SD Bareos::LibSD Bareos::Lib GTest::gtest_main
                   GTest::gmock
  )
  bareos_add_test(
    sd_statistics_thread
    LINK_LIBRARIES testing_common Bareos::Dir Bareos::Lib Bareos::SQL
                   Bareos::Findlib GTest::gtest_main
  )
  bareos_add_test(
    select_functions LINK_LIBRARIES Bareos::Dir Bareos::Findlib Bareos::SQL
                                    GTest::gtest_main
  )
  bareos_add_test(
    setdevice LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib
                             Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    sort_stringvector LINK_LIBRARIES Bareos::Lib GTest::gtest_main
  )
  bareos_add_test(
    test_config_parser_dir
    LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib testing_common
                   Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    test_crc32
    ADDITIONAL_SOURCES ../stored/crc32/crc32.cc
    LINK_LIBRARIES Bareos::Lib GTest::gtest_main
  )
  bareos_add_test(
    test_config_parser_sd LINK_LIBRARIES Bareos::SD Bareos::LibSD Bareos::Lib
                                         GTest::gtest_main
  )
  bareos_add_test(
    test_db_list_ctx LINK_LIBRARIES Bareos::Lib Bareos::SQL GTest::gtest_main
  )
  if(NOT MSVC)
    bareos_add_test(
      test_dir_plugins
      ADDITIONAL_SOURCES
        ${PROJECT_SOURCE_DIR}/src/dird/date_time.cc
        ${PROJECT_SOURCE_DIR}/src/dird/date_time_mask.cc
        ${PROJECT_SOURCE_DIR}/src/dird/dir_plugins.cc
        ${PROJECT_SOURCE_DIR}/src/dird/dird_conf.cc
        ${PROJECT_SOURCE_DIR}/src/dird/dird_globals.cc
        ${PROJECT_SOURCE_DIR}/src/dird/run_conf.cc
        ${PROJECT_SOURCE_DIR}/src/dird/inc_conf.cc
        ${PROJECT_SOURCE_DIR}/src/dird/ua_acl.cc
        ${PROJECT_SOURCE_DIR}/src/dird/ua_audit.cc
      LINK_LIBRARIES Bareos::Lib Bareos::SQL Bareos::Findlib GTest::gtest_main
                     OpenSSL::SSL OpenSSL::Crypto
    )
  endif()
  bareos_add_test(
    test_fileindex_list LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib
                                       Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    test_sd_plugins LINK_LIBRARIES Bareos::Lib Bareos::LibSD GTest::gtest_main
  )
  bareos_add_test(
    test_setdebug LINK_LIBRARIES Bareos::Dir Bareos::Lib Bareos::Findlib
                                 Bareos::SQL GTest::gtest_main
  )
  bareos_add_test(
    time_format_test LINK_LIBRARIES Bareos::Lib Bareos::Dir Bareos::Findlib
                                    Bareos::SQL GTest::gtest_main
  )

endif() # NOT client-only

if(HAVE_EXECINFO_H
   AND HAVE_BACKTRACE
   AND HAVE_BACKTRACE_SYMBOLS
)
  bareos_add_test(test_backtrace LINK_LIBRARIES Bareos::Lib GTest::gtest_main)
endif() # HAVE_EXECINFO_H ..

if(NOT HAVE_WIN32 AND NOT client-only)
  bareos_add_test(
    test_bsock
    ADDITIONAL_SOURCES
      bareos_test_sockets.cc bsock_constructor_test.cc
      bsock_cert_verify_common_names_test.cc create_resource.cc init_openssl.cc
    LINK_LIBRARIES ${LINK_LIBRARIES}
    COMPILE_DEFINITIONS CERTDIR=\"${CERTDIR}\"
  )
endif() # NOT HAVE_WIN32 AND NOT client-only

if(HAVE_WIN32)
  bareos_add_test(
    test_unicode_conversion LINK_LIBRARIES Bareos::Lib GTest::gtest_main
  )
  bareos_add_test(
    test_path_conversion
    ADDITIONAL_SOURCES compat_old_conversion.cc
    LINK_LIBRARIES Bareos::Lib GTest::gtest_main
  )
endif()
if(ENABLE_BCONSOLE)
  bareos_add_test(
    test_config_parser_console LINK_LIBRARIES Bareos::Console Bareos::Lib
                                              Bareos::Findlib GTest::gtest_main
  )
endif()

# Keep alphabetically ordered
bareos_add_test(
  cram_md5
  LINK_LIBRARIES Bareos::Lib GTest::gtest_main
  ADDITIONAL_SOURCES bareos_test_sockets.cc
)

bareos_add_test(job_control_record LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

bareos_add_test(
  test_acl_entry_syntax LINK_LIBRARIES Bareos::Lib GTest::gtest_main
)

bareos_add_test(test_bsnprintf LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

add_executable(test_bpipe_prog)
target_sources(test_bpipe_prog PRIVATE test_bpipe_prog.cc)
bareos_add_test(test_bpipe LINK_LIBRARIES Bareos::Lib GTest::gtest_main)
target_compile_definitions(
  test_bpipe PRIVATE "-DTEST_PROGRAM=\"$<TARGET_FILE:test_bpipe_prog>\""
)

bareos_add_test(
  test_config_parser_fd LINK_LIBRARIES Bareos::FD Bareos::Lib Bareos::Findlib
                                       GTest::gtest_main
)
bareos_add_test(test_edit LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

if(NOT MSVC)
  bareos_add_test(
    test_fd_plugins
    ADDITIONAL_SOURCES
      ${PROJECT_SOURCE_DIR}/src/filed/fd_plugins.cc
      ${PROJECT_SOURCE_DIR}/src/filed/fileset.cc
      ${PROJECT_SOURCE_DIR}/src/filed/filed_globals.cc
    LINK_LIBRARIES Bareos::Lib Bareos::Findlib GTest::gtest_main
  )
endif()
bareos_add_test(test_is_name_valid LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

bareos_add_test(test_poolmem LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

bareos_add_test(
  test_output_formatter LINK_LIBRARIES GTest::gtest_main Bareos::Lib
)

if(NOT HAVE_BIG_ENDIAN)
  bareos_add_test(
    statefile
    LINK_LIBRARIES Bareos::Lib GTest::gtest_main
    COMPILE_DEFINITIONS TEST_TEMP_DIR=\"${TEST_TEMP_DIR}\"
                        TEST_ORIGINAL_FILE_DIR=\"${TEST_ORIGINAL_FILE_DIR}\"
  )
endif()

bareos_add_test(thread_list LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

bareos_add_test(
  thread_specific_data LINK_LIBRARIES Bareos::Lib GTest::gtest_main
)

bareos_add_test(version_strings LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

bareos_add_test(channel LINK_LIBRARIES Bareos::Lib GTest::gtest_main)

bareos_add_test(
  wrap LINK_LIBRARIES Bareos::Lib GTest::gtest_main OpenSSL::SSL
                      OpenSSL::Crypto
)

if(NOT HAVE_WIN32)
  bareos_add_test(fvec LINK_LIBRARIES GTest::gtest_main)
endif()

if(NOT client-only)
  bareos_add_test(
    backend_parse LINK_LIBRARIES Bareos::Lib GTest::gtest_main backend-utils
  )
endif()

add_executable(env_tester env_tester.cc)
target_link_libraries(env_tester PRIVATE Bareos::Lib)
bareos_add_test(bpipe_env_test LINK_LIBRARIES GTest::gtest_main Bareos::Lib)
target_compile_definitions(
  bpipe_env_test PRIVATE "-DTEST_PROGRAM=\"$<TARGET_FILE:env_tester>\""
)

if(ENABLE_BARRI)
  if(HAVE_WIN32)
    bareos_add_test(
      data-dumper LINK_LIBRARIES GTest::gtest_main Bareos::BarriDump
    )
  endif()

  bareos_add_test(
    find_holes
    LINK_LIBRARIES GTest::gtest_main
    ADDITIONAL_SOURCES
      "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/windows_dr/remove_holes.cc"
  )

  target_include_directories(
    find_holes PRIVATE "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/windows_dr"
  )

  bareos_add_test(
    test_file_format LINK_LIBRARIES GTest::gtest_main Bareos::BarriParser
  )
endif()

include(DebugEdit)
