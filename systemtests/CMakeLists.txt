#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2022 Bareos GmbH & Co. KG
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

project(bareos-systemtests)

include(CMakePrintHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BareosSystemtestFunctions)

set(USER $ENV{USER})

set(wine_drive_prefix "")
if(${SYSTEMTESTS_USE_WINE})
  set(wine_drive_prefix "Z:")
  # Wine must use same DLLs installed on the cross-build system.
  # These are located in CMAKE_BINARY_DIR.
  string(APPEND WINEPATH "Z:${CMAKE_BINARY_DIR}\\;")
  string(REPLACE "/" "\\" WINEPATH ${WINEPATH})
endif()

# ${wine_drive_prefix} can be used for the pki_* and tls_* settings, as these
# are only used inside Bareos configuration files.

# set the data encryption and signature keys
set(pki_keypair ${CMAKE_CURRENT_SOURCE_DIR}/pki/fd.pem)
set(pki_master_key ${CMAKE_CURRENT_SOURCE_DIR}/pki/master.cert)

# set the tls ca keys
set(tls_ca_certificate ${CMAKE_CURRENT_SOURCE_DIR}/tls/bareos-ca.pem)

set(tls_fd_certificate
    ${CMAKE_CURRENT_SOURCE_DIR}/tls/client1.bareos.org-cert.pem
)
set(tls_fd_key ${CMAKE_CURRENT_SOURCE_DIR}/tls/client1.bareos.org-key.pem)

set(tls_sd_certificate
    ${CMAKE_CURRENT_SOURCE_DIR}/tls/bareos-sd1.bareos.org-cert.pem
)
set(tls_sd_key ${CMAKE_CURRENT_SOURCE_DIR}/tls/bareos-sd1.bareos.org-key.pem)

set(tls_dir_certificate
    ${CMAKE_CURRENT_SOURCE_DIR}/tls/bareos-dir.bareos.org-cert.pem
)
set(tls_dir_key ${CMAKE_CURRENT_SOURCE_DIR}/tls/bareos-dir.bareos.org-key.pem)

set(tls_con_certificate
    ${CMAKE_CURRENT_SOURCE_DIR}/tls/console.bareos.org-cert.pem
)
set(tls_con_key ${CMAKE_CURRENT_SOURCE_DIR}/tls/console.bareos.org-key.pem)

option(SYSTEMTESTS_S3_USE_HTTPS "Use tls for S3 connections in systemtests"
       "yes"
)
message("   SYSTEMTESTS_S3_USE_HTTPS: ${SYSTEMTESTS_S3_USE_HTTPS}")

if(SYSTEMTESTS_S3_USE_HTTPS)
  set(systemtests_s3_use_https "false")
else()
  set(systemtests_s3_use_https "true")
endif()

# Main starts here...

find_program(PG_CTL pg_ctl /usr/lib/postgresql/*/bin /usr/bin /bin)
get_filename_component(POSTGRES_BIN_PATH ${PG_CTL} DIRECTORY)
# message(STATUS "PG_CTL is ${PG_CTL}")
message(STATUS "POSTGRES_BIN_PATH is ${POSTGRES_BIN_PATH}")

find_program(PERL perl)

option(SYSTEMTESTS_USE_WINE
       "Configure the systemtests to use WINE to run Bareos" OFF
)
option(RUN_SYSTEMTESTS_ON_INSTALLED_FILES
       "Configure the system tests to run on installed files (from packages)"
)

set(ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS
    bareos-dbcheck
    bareos-dir
    bareos-fd
    bareos-sd
    bconsole
    bcopy
    bextract
    bls
    bregex
    bscan
    bscrypto
    bsmtp
    btape
    bwild
    testfind
)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  list(APPEND ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS bpluginfo btestls
       drivetype fstype
  )
endif()

if(TARGET bareos_vadp_dumper)
  list(APPEND ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS bareos_vadp_dumper)
endif()

cmake_print_variables(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)

message(STATUS "Looking for binaries and paths...")
find_systemtests_binary_paths(${ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS})

# set variable values to be replaced by configure_file set(DEFAULT_DB_TYPE
# ${default_db_backend}) message(STATUS "DEFAULT_DB_TYPE is ${DEFAULT_DB_TYPE}")
cmake_print_variables(DEFAULT_DB_TYPE)

# set(TAPEDRIVE "TAPEDRIVE")

set(archivedir "${PROJECT_BINARY_DIR}/archivedir")

set(bin ${PROJECT_BINARY_DIR}/bin)
set(bindir ${PROJECT_BINARY_DIR}/bin)

set(BAREOS_CONFIG_DIR ${conf})
set(db_name "regress")
set(db_port "5432")
set(db_user "${systemtest_db_user}")
set(db_password "${systemtest_db_password}")

set(logdir "log")
set(dumps ${PROJECT_BINARY_DIR}/dumps)
set(plugindirtmp ${PROJECT_BINARY_DIR}/plugindirtmp)
set(rscripts ${PROJECT_BINARY_DIR}/scripts)
set(sbindir ${PROJECT_BINARY_DIR}/sbin)
set(scriptdir ${PROJECT_BINARY_DIR}/scripts)
set(scripts ${PROJECT_BINARY_DIR}/scripts)
set(smtp_host "smtp_host")
set(subsysdir ${PROJECT_BINARY_DIR}/subsysdir)
set(tmp ${PROJECT_BINARY_DIR}/tmp)
set(tmpdir ${tmp})
set(working ${PROJECT_BINARY_DIR}/working)
set(working_dir ${PROJECT_BINARY_DIR}/working)

# for config-lib.sh: do not remove @DB___@ but replace them with the original
set(DB_NAME @DB_NAME@)
set(DB_USER @DB_USER@)
set(DB_PASS @DB_PASS@)
set(DB_VERSION @DB_VERSION@)

create_systemtests_directory()

set(tests_dir ${PROJECT_BINARY_DIR}/tests)

set(SYSTEMTEST_TIMEOUT
    "300"
    CACHE STRING "Timeout for systemtests (in seconds)"
)

set(BASEPORT 30001)

set(SYSTEMTEST_PREFIX "system:")

if(AUTOCHANGER_DEVICES_FOUND)
  if(MT AND MTX)
    set(AUTOCHANGER_TEST_ENABLED TRUE)
  else()
    message(FATAL_ERROR "mtx/mt not found for autochanger test")
  endif()
endif()

check_for_pamtest()

if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  set(TEST_INFO_TEXT "running system tests on installed files")
else()
  set(TEST_INFO_TEXT "running system tests on the sourcetree")
endif()

execute_process(
  COMMAND "${CMAKE_BINARY_DIR}/systemtests/scripts/generate_minio_certs.sh"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/systemtests/tls/minio/"
  RESULT_VARIABLE RESULT_GENERATE_MINIO_CERTS
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT "${RESULT_GENERATE_MINIO_CERTS}" STREQUAL "0")
  message(
    FATAL_ERROR
      "Creation of certificates failed: ${RESULT_GENERATE_MINIO_CERTS} ${CMAKE_BINARY_DIR}"
  )
endif()

configure_file(
  "CTestCustom.cmake.in" "${CMAKE_BINARY_DIR}/CTestCustom.cmake" @ONLY
)
configure_file(
  "ctest_custom_pretest.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctest_custom_pretest.sh" @ONLY
)

#
# webui: start
#

message(STATUS "Looking for webui test requirements ...")

set(AVAILABLE_WEBUI_SELENIUM_TESTS
    "admin-client_link_on_dashboard"
    "admin-client_disabling"
    "admin-rerun_job"
    "admin-restore"
    "admin-run_configured_job"
    "admin-run_default_job"
    "readonly-client_disabling"
    "readonly-rerun_job"
    "readonly-restore"
    "readonly-run_configured_job"
    "readonly-run_default_job"
)

set(WEBUI_TESTS_BROKEN
    admin-client_disabling # sometimes fails with a find_element exception
    readonly-client_disabling # sometimes fails with a find_element exception
)

find_program(PHP php)
find_program(CHROMEDRIVER chromedriver)
find_program(PYTHON NAMES python python3 python2)
check_pymodule_available(2 selenium) # sets PYMODULE_2_SELENIUM_FOUND to TRUE or
check_pymodule_available(3 selenium) # sets PYMODULE_3_SELENIUM_FOUND to TRUE or
# FALSE

message("   PHP:                    " ${PHP})
message("   PYTHON_EXECUTABLE:      " ${PYTHON_EXECUTABLE})
message("   PYMODULE_2_SELENIUM_FOUND:" ${PYMODULE_2_SELENIUM_FOUND})
message("   PYMODULE_3_SELENIUM_FOUND:" ${PYMODULE_3_SELENIUM_FOUND})
message("   CHROMEDRIVER:           " ${CHROMEDRIVER})

if(PHP
   AND PYTHON_EXECUTABLE
   AND (PYMODULE_2_SELENIUM_FOUND OR PYMODULE_3_SELENIUM_FOUND)
   AND CHROMEDRIVER
)
  set(ENABLE_WEBUI_SELENIUM_TEST TRUE)
  message(STATUS "OK: all requirements for webui tests were met.")
else()
  set(ENABLE_WEBUI_SELENIUM_TEST FALSE)
  message(
    STATUS "NOT OK: disabling webui tests as not all requirements were found."
  )
endif()

set(WEBUI_TEST_PREFIX "webui:")
if(ENABLE_WEBUI_SELENIUM_TEST)
  set(WEBUI_SELENIUM_TESTS ${AVAILABLE_WEBUI_SELENIUM_TESTS})
else()
  set(WEBUI_SELENIUM_TESTS)
  foreach(TEST_NAME_DISABLED ${AVAILABLE_WEBUI_SELENIUM_TESTS})
    add_test(NAME ${WEBUI_TEST_PREFIX}${TEST_NAME_DISABLED} COMMAND false)
    set_tests_properties(
      ${WEBUI_TEST_PREFIX}${TEST_NAME_DISABLED} PROPERTIES DISABLED true
    )
    message(STATUS "Disabled test: ${WEBUI_TEST_PREFIX}${TEST_NAME_DISABLED}")
  endforeach()
endif()

# webui specific settings
if(ENABLE_WEBUI_SELENIUM_TEST)
  foreach(TEST_NAME ${WEBUI_SELENIUM_TESTS})
    message(STATUS "Configuring test: ${WEBUI_TEST_PREFIX}${TEST_NAME}")

    prepare_test(${TEST_NAME})
    prepare_testdir_for_daemon_run()
    # split WEBUI_TEST_NAME into PROFILE and TESTNAME
    string(REPLACE "-" ";" SELENIUM_TESTCOMPONENTS ${TEST_NAME})
    # "webui-admin-restore"
    list(GET SELENIUM_TESTCOMPONENTS 0 BAREOS_WEBUI_PROFILE)
    list(GET SELENIUM_TESTCOMPONENTS 1 BAREOS_WEBUI_TESTNAME)

    set(WEBUICONFDIR ${current_test_directory}/etc/bareos-webui) # used for test
                                                                 # environment
                                                                 # settings
    file(MAKE_DIRECTORY ${WEBUICONFDIR})
    configure_file(
      "${CMAKE_SOURCE_DIR}/webui/config/autoload/global.php.in"
      "${current_test_directory}/webui/config/autoload/global.php" @ONLY
    )
    configure_file(
      "${CMAKE_SOURCE_DIR}/systemtests/tests/webui-common/testrunner.in"
      "${current_test_directory}/testrunner" @ONLY
    )
    configurefilestosystemtest(
      "systemtests" "tests/${TEST_NAME}" "*" @ONLY "tests/webui-common"
    )

    configure_file("environment.in" "tests/${TEST_NAME}/environment" @ONLY)
    # create a bin/bareos and bin/bconsole script in every testdir for
    # start/stop and bconsole file(MAKE_DIRECTORY
    # "${CMAKE_BINARY_DIR}/tests/${TEST_NAME}/bin")
    configure_file("bin/bconsole" "tests/${TEST_NAME}/bin/bconsole" COPYONLY)
    configure_file("bin/bareos" "tests/${TEST_NAME}/bin/bareos" COPYONLY)

    add_test(
      NAME "${WEBUI_TEST_PREFIX}${TEST_NAME}"
      COMMAND ${tests_dir}/${TEST_NAME}/testrunner
      WORKING_DIRECTORY ${tests_dir}/${TEST_NAME}
    )
    if(${TEST_NAME} IN_LIST WEBUI_TESTS_BROKEN)
      set_tests_properties(
        ${WEBUI_TEST_PREFIX}${TEST_NAME} PROPERTIES LABELS "broken"
      )
    endif()
    set_tests_properties(
      ${WEBUI_TEST_PREFIX}${TEST_NAME} PROPERTIES TIMEOUT 120 COST 1.0
    )
    math(EXPR BASEPORT "${BASEPORT} + 10")
  endforeach()
endif()

#
# webui: end
#

add_subdirectory(tests)
