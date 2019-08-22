#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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

cmake_minimum_required(VERSION 3.3)
project(bareos-systemtests)

cmake_policy(SET CMP0057 NEW) #IF(.. IN_LIST ..)

function(ConfigureFilesToSystemtest srcdir subdirectory globexpression configure_option)
  set(count 1)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${subdirectory})
  file(GLOB_RECURSE ALL_FILES "${CMAKE_SOURCE_DIR}/${srcdir}/${subdirectory}/${globexpression}")
  foreach(CURRENT_FILE ${ALL_FILES})
    MATH(EXPR COUNT "${COUNT}+1")
    string(REPLACE "${CMAKE_SOURCE_DIR}/" "" TARGET_FILE ${CURRENT_FILE})
    string(REGEX REPLACE ".in$" "" TARGET_FILE ${TARGET_FILE}) # do not mess with .ini files
    #MESSAGE(STATUS "configuring  ${TARGET_FILE}" )
    string(REPLACE "${srcdir}/${subdirectory}" "" TARGET_FILE ${TARGET_FILE})
    configure_file(${CURRENT_FILE} ${PROJECT_BINARY_DIR}/${subdirectory}/${TARGET_FILE} ${configure_option})
  endforeach()
  MESSAGE(STATUS "Configured ${COUNT} files from ${srcdir} to ${subdirectory} with glob ${globexpression} ${configure_option}" )
endfunction()

# generic function to probe for a python module
function(PythonModuleStatus module)
  EXECUTE_PROCESS(COMMAND "${PYTHON}" "-c" "import ${module}" RESULT_VARIABLE ${module}_status ERROR_QUIET)
  STRING(TOUPPER ${module} module_uppercase)
  IF(${module}_status EQUAL 0)
    SET("PYTHON_${module_uppercase}_FOUND" TRUE PARENT_SCOPE)
  ELSE()
    SET("PYTHON_${module_uppercase}_FOUND" FALSE PARENT_SCOPE)
  ENDIF()
endfunction()

macro(CheckForEnabledAndDisabledListEntry TEST_NAME_TO_CHECK)
  IF(${TEST_NAME_TO_CHECK} IN_LIST SYSTEM_TESTS AND ${TEST_NAME_TO_CHECK} IN_LIST SYSTEM_TESTS_DISABLED)
    message(FATAL_ERROR "The test name: ${TEST_NAME} is listed ambiguously in SYSTEM_TESTS and SYSTEM_TESTS_DISABLED at the same time")
  ENDIF()
endmacro()

# extract  db version from cats.h
file(STRINGS ${CMAKE_SOURCE_DIR}/core/src/cats/cats.h DB_VERSION_STRING REGEX .*BDB_VERSION.*)
string(REGEX MATCH [0-9]+ BDB_VERSION ${DB_VERSION_STRING})

find_program(PERL perl)
find_program(PYTHON python)
find_program(PHP php)

# set variable values to be replaced by configure_file
#set(DEFAULT_DB_TYPE  ${default_db_backend})
message(STATUS "DEFAULT_DB_TYPE is ${DEFAULT_DB_TYPE}")

#set(TAPEDRIVE "TAPEDRIVE")

set(archivedir "${PROJECT_BINARY_DIR}/archivedir")
set(backenddir "${PROJECT_BINARY_DIR}/../core/src/cats")

set(bin ${PROJECT_BINARY_DIR}/bin)
set(bindir ${PROJECT_BINARY_DIR}/bin)

#set(conf ${PROJECT_BINARY_DIR}/defaultconfigs)
#set(confdir ${conf})
#set(configs ${conf})

set(BAREOS_CONFIG_DIR ${conf})
set(db_name "regress")
set(db_password "")
set(db_port "5432")
set(db_user "regress")
set(dir_password "dir_password")
set(dir_port 8101)
set(dumps ${PROJECT_BINARY_DIR}/dumps)
set(fd_password "fd_password")
set(fd_port 8102)

cmake_host_system_information(RESULT hostname QUERY HOSTNAME)

set(job_email "job_email")
set(logdir "log")
set(mon_dir_password "mon_dir_password")
set(mon_fd_password "mon_fd_password")
set(mon_sd_password "mon_sd_password")
set(python_plugin_module_src_dir ${CMAKE_SOURCE_DIR}/core/src/plugins)
set(plugindirtmp ${PROJECT_BINARY_DIR}/plugindirtmp)
set(rscripts ${PROJECT_BINARY_DIR}/scripts)
get_target_property(dir_plugin_binary_path python-dir BINARY_DIR)
get_target_property(fd_plugin_binary_path python-fd BINARY_DIR)
get_target_property(sd_plugin_binary_path python-sd BINARY_DIR)

set(sbindir ${PROJECT_BINARY_DIR}/sbin)

set(scriptdir ${PROJECT_BINARY_DIR}/scripts)
set(scripts ${PROJECT_BINARY_DIR}/scripts)
set(sd_password "sd_password")
set(sd_port 8103)
set(smtp_host "smtp_host")
set(src ${PROJECT_BINARY_DIR}/src)
set(subsysdir ${PROJECT_BINARY_DIR}/subsysdir)
set(tmp ${PROJECT_BINARY_DIR}/tmp)
set(tmpdir ${tmp})
set(tmpsrc ${PROJECT_BINARY_DIR}/tmpsrc)
set(working ${PROJECT_BINARY_DIR}/working)
set(working_dir ${PROJECT_BINARY_DIR}/working)

set(piddir ${PROJECT_BINARY_DIR}/piddir)
file(MAKE_DIRECTORY ${piddir})

#set(BASEPORT ${dir_port})

# for config-lib.sh: do not remove @DB___@ but replace them with the original
set(DB_NAME @DB_NAME@)
set(DB_USER @DB_USER@)
set(DB_PASS @DB_PASS@)
set(DB_VERSION @DB_VERSION@)

ConfigureFilesToSystemtest("systemtests"  "data" "*.tgz" COPYONLY)

ConfigureFilesToSystemtest("systemtests"  "scripts" "functions" @ONLY)
ConfigureFilesToSystemtest("systemtests"  "scripts" "cleanup" @ONLY)
ConfigureFilesToSystemtest("systemtests"  "scripts" "setup" @ONLY)
ConfigureFilesToSystemtest("systemtests"  "scripts" "check_for_zombie_jobs" @ONLY)
ConfigureFilesToSystemtest("systemtests"  "scripts" "diff.pl.in" @ONLY)

ConfigureFilesToSystemtest("core/src" "defaultconfigs" "*.conf" @ONLY)
ConfigureFilesToSystemtest("core/src" "defaultconfigs" "*.in" @ONLY)

ConfigureFilesToSystemtest("core" "scripts" "*.in" @ONLY)
ConfigureFilesToSystemtest("core" "scripts" "bareos-ctl-funcs" @ONLY)
ConfigureFilesToSystemtest("core" "scripts" "btraceback.gdb" @ONLY)

ConfigureFilesToSystemtest("core/src/cats" "ddl" "${DEFAULT_DB_TYPE}*" @ONLY)
ConfigureFilesToSystemtest("core/src" "cats" "*.in" @ONLY)
ConfigureFilesToSystemtest("core/src" "cats" "*_bareos_*" @ONLY)

ConfigureFilesToSystemtest("core/src" "console" "*.in" @ONLY)


execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${scriptdir}/btraceback ${sbindir}/btraceback)

file(MAKE_DIRECTORY ${subsysdir})
file(MAKE_DIRECTORY ${sbindir})
file(MAKE_DIRECTORY ${bindir})
file(MAKE_DIRECTORY ${scripts})
file(MAKE_DIRECTORY ${working})
file(MAKE_DIRECTORY ${archivedir})

file(REMOVE_RECURSE ${scripts}/ddl)
file(RENAME ${PROJECT_BINARY_DIR}/ddl ${scripts}/ddl)

file(GLOB ALL_FILES "${CMAKE_BINARY_DIR}/systemtests/cats/*_bareos_*")
foreach(CURRENT_FILE ${ALL_FILES})
  string(REPLACE "${CMAKE_BINARY_DIR}/systemtests/cats/" "" TARGET_FILE ${CURRENT_FILE})
  file(RENAME ${CURRENT_FILE} ${scripts}/${TARGET_FILE})
  # MESSAGE(STATUS "moved  ${scripts}/${TARGET_FILE}")
endforeach()

set(tests_dir ${PROJECT_BINARY_DIR}/tests)
set(SYSTEM_TESTS
  backup-bareos-test
  backup-bareos-passive-test
  multiplied-device-test
  virtualfull
  virtualfull-bscan
  backup-bscan
  copy-bscan
  copy-remote-bscan
  bconsole-status-client
  python-fd-plugin-local-fileset-test
)

set(SYSTEM_TESTS_DISABLED
  #initialy empty
)

IF(PAM_WRAPPER_LIBRARIES) # BareosFindLibrary(pam_wrapper)
  find_program(PAMTESTER pamtester)
  IF(PAMTESTER)
    set(ENV{PAM_WRAPPER_LIBRARIES} "${PAM_WRAPPER_LIBRARIES}")
    execute_process(
      COMMAND           "${CMAKE_SOURCE_DIR}/systemtests/tests/bconsole-pam/bin/check_pam_exec_available.sh"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/systemtests/tests/bconsole-pam/"
      RESULT_VARIABLE   PAM_EXEC_AVAILABLE_RC
    )
    IF(${PAM_EXEC_AVAILABLE_RC} EQUAL "0")
      set(PAM_EXEC_FOUND TRUE)
      set(ENABLE_BCONSOLE_PAM_TEST TRUE)
    ENDIF()
  ENDIF()
ENDIF()


MESSAGE( STATUS "PAM_EXEC_FOUND:               " ${PAM_EXEC_FOUND} )

IF(ENABLE_BCONSOLE_PAM_TEST)
  list(APPEND SYSTEM_TESTS "bconsole-pam")
ELSE()
  list(APPEND SYSTEM_TESTS_DISABLED "bconsole-pam")
ENDIF()

SET(PHP_FOUND FALSE)
SET(PYTHON_FOUND FALSE)
SET(PYTHON_SELENIUM_FOUND "NOT SEARCHED")
SET(CHROMEDRIVER_FOUND "NOT SEARCHED")

IF(PHP)
  SET(PHP_FOUND TRUE)
  IF(PYTHON)
    SET(PYTHON_FOUND TRUE)
    PythonModuleStatus("selenium") # sets PYTHON_SELENIUM_FOUND to TRUE or FALSE
    IF(${PYTHON_SELENIUM_FOUND})
      find_program(CHROMEDRIVER chromedriver)
      IF(CHROMEDRIVER)
        SET(CHROMEDRIVER_FOUND TRUE)
        SET(ENABLE_WEBUI_SELENIUM_TEST TRUE)
      ELSE()
        SET(CHROMEDRIVER_FOUND FALSE)
      ENDIF()
    ENDIF()
 ENDIF()
ENDIF()

MESSAGE(STATUS "PHP_FOUND:                    " ${PHP_FOUND})
MESSAGE(STATUS "PYTHON_FOUND:                 " ${PYTHON_FOUND})
MESSAGE(STATUS "PYTHON_SELENIUM_FOUND:        " ${PYTHON_SELENIUM_FOUND})
MESSAGE(STATUS "CHROMEDRIVER_FOUND:           " ${CHROMEDRIVER_FOUND})

IF(ENABLE_WEBUI_SELENIUM_TEST)
  list(APPEND SYSTEM_TESTS "webui-selenium")
ELSE()
  list(APPEND SYSTEM_TESTS_DISABLED "webui-selenium")
ENDIF()

set(BASEPORT 42001)

foreach(TEST_NAME ${SYSTEM_TESTS})
  # base directory for this test
  set(current_test_directory ${PROJECT_BINARY_DIR}/tests/${TEST_NAME})
  set(basename ${TEST_NAME})

  #db parameters
  set(db_password "")
  #db_name is regress-${TEST_NAME} replacing - by _
  string(REPLACE "-" "_" db_name "regress-${TEST_NAME}")
  #set(db_name "regress-${TEST_NAME}")
  set(db_user "regress")

  set(job_email root@localhost)

  set(dir_password dir_password)
  set(fd_password fd_password)
  set(mon_dir_password mon_dir_password)
  set(mon_fd_password mon_fd_password)
  set(mon_sd_password mon_sd_password)
  set(sd_password sd_password)

  math(EXPR dir_port "${BASEPORT} + 0")
  math(EXPR  fd_port "${BASEPORT} + 1")
  math(EXPR  sd_port "${BASEPORT} + 2")
  math(EXPR sd2_port "${BASEPORT} + 3")
  math(EXPR php_port "${BASEPORT} + 4")

  # set(DEFAULT_DB_TYPE )
  set(archivedir ${current_test_directory}/storage)
  set(confdir ${current_test_directory}/etc/bareos)

  set(logdir ${current_test_directory}/log)
  set(tmpdir ${current_test_directory}/tmp)
  set(tmp ${tmpdir}/tmp)
  set(working_dir ${current_test_directory}/working)

  file(MAKE_DIRECTORY ${archivedir})
  file(MAKE_DIRECTORY ${confdir})
  file(MAKE_DIRECTORY ${logdir})
  file(MAKE_DIRECTORY ${tmpdir})
  file(MAKE_DIRECTORY ${working_dir})

  # webui-selenium
  IF(${TEST_NAME} STREQUAL "webui-selenium")
    set(WEBUICONFDIR ${current_test_directory}/etc/bareos-webui) #used for testenvironment settings
    file(MAKE_DIRECTORY ${WEBUICONFDIR})
    configure_file("${CMAKE_SOURCE_DIR}/webui/config/autoload/global.php.in" "${current_test_directory}/webui/config/autoload/global.php" @ONLY)
    configure_file("${CMAKE_SOURCE_DIR}/systemtests/tests/webui-selenium/directors.ini.in" "${current_test_directory}/etc/bareos-webui/directors.ini" @ONLY)
    file(COPY "${CMAKE_SOURCE_DIR}/webui/install/configuration.ini" DESTINATION "${current_test_directory}/etc/bareos-webui/")
  ENDIF()

  ConfigureFilesToSystemtest("systemtests"  "tests/${TEST_NAME}" "*" @ONLY)

  configure_file("environment.in" "tests/${TEST_NAME}/environment" @ONLY)
  # create a bin/bareos and bin/bconsole script
  # in every testdir for start/stop and bconsole
  #file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/${TEST_NAME}/bin")
  configure_file("bin/bconsole" "tests/${TEST_NAME}/bin/bconsole" COPYONLY)
  configure_file("bin/bareos" "tests/${TEST_NAME}/bin/bareos" COPYONLY)

  CheckForEnabledAndDisabledListEntry(${TEST_NAME})

  add_test(NAME "system:${TEST_NAME}"
    COMMAND ${tests_dir}/${TEST_NAME}/testrunner
    WORKING_DIRECTORY ${tests_dir}/${TEST_NAME})

  math(EXPR BASEPORT "${BASEPORT} + 10")

endforeach()

foreach(TEST_NAME_DISABLED ${SYSTEM_TESTS_DISABLED})
  CheckForEnabledAndDisabledListEntry(${TEST_NAME_DISABLED})
  add_test(NAME system:${TEST_NAME_DISABLED} COMMAND empty_command)
  set_tests_properties(system:${TEST_NAME_DISABLED} PROPERTIES DISABLED true)
  message(STATUS "Disabled test: system:${TEST_NAME_DISABLED}")
endforeach()

# create symlinks for the binaries

set(BINARIES_TO_LINK_TO_SBIN
  dird/bareos-dir
  dird/bareos-dbcheck
  filed/bareos-fd
  stored/bareos-sd
  stored/bcopy
  stored/btape
  stored/bextract
  stored/bareos-sd
  stored/bls
  stored/bscan
  )

foreach (BINARY_SOURCEPATH ${BINARIES_TO_LINK_TO_SBIN})
  get_filename_component(BINARY_NAME ${BINARY_SOURCEPATH} NAME)
execute_process(COMMAND
    ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/${BINARY_SOURCEPATH} ${sbindir}/${BINARY_NAME})
endforeach()

set(BINARIES_TO_LINK_TO_BIN
  console/bconsole
  tools/bsmtp
  tools/timelimit
  tools/bwild
  tools/bpluginfo
  tools/bsmtp
  tools/bscrypto
  tools/btestls
  tools/drivetype
  tools/fstype
  tools/bregex
  )

foreach (BINARY_SOURCEPATH ${BINARIES_TO_LINK_TO_BIN})
  get_filename_component(BINARY_NAME ${BINARY_SOURCEPATH} NAME)
execute_process(COMMAND
    ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/${BINARY_SOURCEPATH} ${bindir}/${BINARY_NAME})
endforeach()

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/console/bconsole ${bindir}/bconsole)

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${scriptdir}/bareos ${bindir}/bareos)

