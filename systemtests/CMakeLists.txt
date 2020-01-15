#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2020 Bareos GmbH & Co. KG
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

cmake_policy(SET CMP0057 NEW) # IF(.. IN_LIST ..)

function(ConfigureFilesToSystemtest srcbasedir dirname globexpression
         configure_option srcdirname
)
  if(srcdirname STREQUAL "")
    set(srcdirname "${dirname}")
  endif()
  set(COUNT 1)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${dirname})
  file(GLOB_RECURSE ALL_FILES
       "${CMAKE_SOURCE_DIR}/${srcbasedir}/${srcdirname}/${globexpression}"
  )
  foreach(CURRENT_FILE ${ALL_FILES})
    math(EXPR COUNT "${COUNT}+1")
    string(REPLACE "${CMAKE_SOURCE_DIR}/" "" TARGET_FILE ${CURRENT_FILE})
    string(REGEX REPLACE ".in$" "" TARGET_FILE ${TARGET_FILE}) # do not mess
                                                               # with .ini files
    string(REPLACE "${srcbasedir}/${srcdirname}" "" TARGET_FILE ${TARGET_FILE})
    # MESSAGE(STATUS "configuring  ${TARGET_FILE}" )
    configure_file(
      ${CURRENT_FILE} ${PROJECT_BINARY_DIR}/${dirname}/${TARGET_FILE}
      ${configure_option}
    )
  endforeach()
endfunction()

# generic function to probe for a python module
function(PythonModuleStatus module)
  execute_process(
    COMMAND "${PYTHON}" "-c" "import ${module}"
    RESULT_VARIABLE ${module}_status
    ERROR_QUIET
  )
  string(TOUPPER ${module} module_uppercase)
  if(${module}_status EQUAL 0)
    set("PYTHON_${module_uppercase}_FOUND"
        TRUE
        PARENT_SCOPE
    )
  else()
    set("PYTHON_${module_uppercase}_FOUND"
        FALSE
        PARENT_SCOPE
    )
  endif()
endfunction()

macro(CheckForEnabledAndDisabledListEntry TEST_NAME_TO_CHECK)
  if(${TEST_NAME_TO_CHECK} IN_LIST SYSTEM_TESTS
     AND ${TEST_NAME_TO_CHECK} IN_LIST SYSTEM_TESTS_DISABLED
  )
    message(
      FATAL_ERROR
        "The test name: ${TEST_NAME} is listed ambiguously in SYSTEM_TESTS and SYSTEM_TESTS_DISABLED at the same time"
    )
  endif()
endmacro()

macro(handle_python_plugin_modules test_name)
  set(PYMODULES_TO_LINK_TO_SRC)

  string(REGEX MATCH ^python-fd.* starts_with_python-fd ${test_name})
  if(starts_with_python-fd)
    list(APPEND PYMODULES_TO_LINK_TO_SRC filed/BareosFdWrapper.py
                filed/BareosFdPluginBaseclass.py filed/bareos_fd_consts.py
                filed/bareos-fd-mock-test.py
    )
  endif()

  string(REGEX MATCH ^python-sd.* starts_with_python-sd ${test_name})
  if(starts_with_python-sd)
    list(APPEND PYMODULES_TO_LINK_TO_SRC stored/bareos_sd_consts.py
                stored/bareos-sd-class-plugin.py
                stored/BareosSdPluginBaseclass.py stored/BareosSdWrapper.py
    )
  endif()

  string(REGEX MATCH ^python-dir.* starts_with_python-dir ${test_name})
  if(starts_with_python-dir)
    list(APPEND PYMODULES_TO_LINK_TO_SRC dird/bareos_dir_consts.py
                dird/BareosDirPluginBaseclass.py
                dird/bareos-dir-class-plugin.py dird/BareosDirWrapper.py
    )
  endif()

  if(${test_name} STREQUAL python-dir-plugin-test)
    list(APPEND PYMODULES_TO_LINK_TO_SRC)
  endif()

  if(${test_name} STREQUAL python-fd-percona-xtrabackup-plugin-test)
    list(APPEND PYMODULES_TO_LINK_TO_SRC filed/BareosFdPluginPerconaXtraBackup.py
                filed/bareos-fd-percona-xtrabackup.py
    )
  endif()

  if(${test_name} STREQUAL python-fd-ovirt-plugin-test)
    list(APPEND PYMODULES_TO_LINK_TO_SRC filed/BareosFdPluginOvirt.py
                filed/bareos-fd-ovirt.py
    )
  endif()

  if(${test_name} STREQUAL python-fd-plugin-local-fileset-test)
    list(APPEND PYMODULES_TO_LINK_TO_SRC filed/bareos-fd-local-fileset.py
                filed/BareosFdPluginLocalFileset.py
    )
  endif()

  # still missing: filed/BareosFdPluginLDAP.py filed/bareos-fd-ldap.py

  if(NOT EXISTS ${python_plugin_module_src_test_dir})
    file(MAKE_DIRECTORY ${python_plugin_module_src_test_dir})
  endif()

  foreach(PYMODULE_SOURCEPATH ${PYMODULES_TO_LINK_TO_SRC})
    get_filename_component(PYMODULE_NAME ${PYMODULE_SOURCEPATH} NAME)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink
              ${PROJECT_SOURCE_DIR}/../core/src/plugins/${PYMODULE_SOURCEPATH}
              ${python_plugin_module_src_test_dir}/${PYMODULE_NAME}
    )
  endforeach()
  # endif()
endmacro()




macro(prepare_testdir_for_daemon_run)
  set(archivedir ${current_test_directory}/storage)
  set(confdir ${current_test_directory}/etc/bareos)
  set(config_directory_dir_additional_test_config
      ${current_test_directory}/etc/bareos/bareos-dir.d/additional_test_config)
  set(logdir ${current_test_directory}/log)
  set(tmpdir ${current_test_directory}/tmp)
  set(tmp ${tmpdir})
  set(working_dir ${current_test_directory}/working)
  set(python_plugin_module_src_test_dir
      ${current_test_directory}/python-modules
  )
  set(piddir ${current_test_directory}/piddir)

  file(MAKE_DIRECTORY ${piddir})
  file(MAKE_DIRECTORY ${tmpdir})
  file(MAKE_DIRECTORY ${archivedir})
  file(MAKE_DIRECTORY ${logdir})
  file(MAKE_DIRECTORY ${confdir})
  file(MAKE_DIRECTORY ${working_dir})
  file(MAKE_DIRECTORY ${config_directory_dir_additional_test_config})
  # create a bin/bareos and bin/bconsole script in every testdir for
  # start/stop and bconsole
  file(MAKE_DIRECTORY "${current_test_directory}/bin")
  configure_file(
    "bin/bconsole" "${current_test_directory}/bin/bconsole" COPYONLY
  )
  configure_file("bin/bareos" "${current_test_directory}/bin/bareos" COPYONLY)

  set(BAREOS_DIRECTOR_BINARY
      ${current_test_directory}/sbin/bareos-dir-${TEST_NAME}
  )
  set(BAREOS_STORAGEDAEMON_BINARY
      ${current_test_directory}/sbin/bareos-sd-${TEST_NAME}
  )
  set(BAREOS_FILEDAEMON_BINARY
      ${current_test_directory}/sbin/bareos-fd-${TEST_NAME}
  )
  file(MAKE_DIRECTORY "${current_test_directory}/sbin")
  create_symlink(
    "/sbin/bareos-dir" "${BAREOS_DIRECTOR_BINARY}"
  )
  create_symlink(
    "/sbin/bareos-sd"
    "${BAREOS_STORAGEDAEMON_BINARY}"
  )
  create_symlink(
    "/sbin/bareos-fd"
    "${BAREOS_FILEDAEMON_BINARY}"
  )
endmacro()


macro(prepare_test)
  # base directory for this test
  set(current_test_directory ${PROJECT_BINARY_DIR}/tests/${TEST_NAME})
  set(current_test_source_directory ${PROJECT_SOURCE_DIR}/tests/${TEST_NAME})
  set(basename ${TEST_NAME})

  # db parameters
  set(db_password "")
  # db_name is regress-${TEST_NAME} replacing - by _
  string(REPLACE "-" "_" db_name "regress-${TEST_NAME}")
  # set(db_name "regress-${TEST_NAME}")
  set(db_user "regress")

  set(job_email root@localhost)

  set(dir_password dir_password)
  set(fd_password fd_password)
  set(mon_dir_password mon_dir_password)
  set(mon_fd_password mon_fd_password)
  set(mon_sd_password mon_sd_password)
  set(sd_password sd_password)

  math(EXPR dir_port "${BASEPORT} + 0")
  math(EXPR fd_port "${BASEPORT} + 1")
  math(EXPR sd_port "${BASEPORT} + 2")
  math(EXPR sd2_port "${BASEPORT} + 3")
  math(EXPR php_port "${BASEPORT} + 4")


  # skip for tests without etc/bareos ("catalog" test)
  if(EXISTS ${current_test_source_directory}/etc/bareos)
    prepare_testdir_for_daemon_run()
  endif()
endmacro()

macro(create_symlink target link)
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${target} ${link})
endmacro()

# extract  db version from cats.h
file(STRINGS ${CMAKE_SOURCE_DIR}/core/src/cats/cats.h DB_VERSION_STRING
     REGEX .*BDB_VERSION.*
)
string(REGEX MATCH [0-9]+ BDB_VERSION ${DB_VERSION_STRING})

find_program(PERL perl)
find_program(PYTHON python)
find_program(PHP php)

# set variable values to be replaced by configure_file set(DEFAULT_DB_TYPE
# ${default_db_backend})
message(STATUS "DEFAULT_DB_TYPE is ${DEFAULT_DB_TYPE}")

# set(TAPEDRIVE "TAPEDRIVE")

set(archivedir "${PROJECT_BINARY_DIR}/archivedir")
set(backenddir "${PROJECT_BINARY_DIR}/../core/src/cats")

set(bin ${PROJECT_BINARY_DIR}/bin)
set(bindir ${PROJECT_BINARY_DIR}/bin)

# set(conf ${PROJECT_BINARY_DIR}/defaultconfigs) set(confdir ${conf})
# set(configs ${conf})

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

if(TARGET python-dir)
  get_target_property(dir_plugin_binary_path python-dir BINARY_DIR)
endif()
if(TARGET python-sd)
  get_target_property(sd_plugin_binary_path python-sd BINARY_DIR)
endif()
if(TARGET python-fd)
  get_target_property(fd_plugin_binary_path python-fd BINARY_DIR)
endif()

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


# set(BASEPORT ${dir_port})

# for config-lib.sh: do not remove @DB___@ but replace them with the original
set(DB_NAME @DB_NAME@)
set(DB_USER @DB_USER@)
set(DB_PASS @DB_PASS@)
set(DB_VERSION @DB_VERSION@)

configurefilestosystemtest("systemtests" "data" "*.tgz" COPYONLY "")

configurefilestosystemtest("systemtests" "scripts" "functions" @ONLY "")
configurefilestosystemtest("systemtests" "scripts" "cleanup" @ONLY "")
configurefilestosystemtest("systemtests" "scripts" "setup" @ONLY "")
configurefilestosystemtest(
  "systemtests" "scripts" "check_for_zombie_jobs" @ONLY ""
)
configurefilestosystemtest("systemtests" "scripts" "diff.pl.in" @ONLY "")

configurefilestosystemtest("core/src" "defaultconfigs" "*.conf" @ONLY "")
configurefilestosystemtest("core/src" "defaultconfigs" "*.in" @ONLY "")

configurefilestosystemtest("core" "scripts" "*.in" @ONLY "")
configurefilestosystemtest("core" "scripts" "bareos-ctl-funcs" @ONLY "")
configurefilestosystemtest("core" "scripts" "btraceback.gdb" @ONLY "")

configurefilestosystemtest("core/src/cats" "ddl" "${DEFAULT_DB_TYPE}*" @ONLY "")
configurefilestosystemtest("core/src" "cats" "*.in" @ONLY "")
configurefilestosystemtest("core/src" "cats" "*_bareos_*" @ONLY "")

configurefilestosystemtest("core/src" "console" "*.in" @ONLY "")

file(MAKE_DIRECTORY ${subsysdir})
file(MAKE_DIRECTORY ${sbindir})
file(MAKE_DIRECTORY ${bindir})
file(MAKE_DIRECTORY ${scripts})
file(MAKE_DIRECTORY ${working})
file(MAKE_DIRECTORY ${archivedir})

create_symlink(${scriptdir}/btraceback ${sbindir}/btraceback)

file(REMOVE_RECURSE ${scripts}/ddl)
file(RENAME ${PROJECT_BINARY_DIR}/ddl ${scripts}/ddl)

file(GLOB ALL_FILES "${CMAKE_BINARY_DIR}/systemtests/cats/*_bareos_*")
foreach(CURRENT_FILE ${ALL_FILES})
  string(REPLACE "${CMAKE_BINARY_DIR}/systemtests/cats/" "" TARGET_FILE
                 ${CURRENT_FILE}
  )
  file(RENAME ${CURRENT_FILE} ${scripts}/${TARGET_FILE})
  # MESSAGE(STATUS "moved  ${scripts}/${TARGET_FILE}")
endforeach()

set(tests_dir ${PROJECT_BINARY_DIR}/tests)
set(SYSTEM_TESTS
    backup-bareos-test
    backup-bareos-passive-test
    catalog
    multiplied-device-test
    virtualfull
    virtualfull-bscan
    backup-bscan
    copy-bscan
    copy-remote-bscan
    bconsole-status-client
    scheduler-backup-test
    reload-works-on-adding-duplicate-job-resource
    reload-works-on-unchanged-config
    reload-works-on-adding-uncommented-string
    reload-works-on-adding-empty-job-resource
    reload-works-on-adding-second-director-resource
    reload-works-on-adding-client-resource
    backup-bareos-client-initiated-test
)

set(SYSTEM_TESTS_DISABLED # initialy empty
)

if(PYTHON)
  list(APPEND SYSTEM_TESTS "python-bareos-test")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-bareos-test")
endif()

if(PAM_WRAPPER_LIBRARIES) # BareosFindLibrary(pam_wrapper)
  find_program(PAMTESTER pamtester)
  if(PAMTESTER)
    set(ENV{PAM_WRAPPER_LIBRARIES} "${PAM_WRAPPER_LIBRARIES}")
    execute_process(
      COMMAND
        "${CMAKE_SOURCE_DIR}/systemtests/tests/bconsole-pam/bin/check_pam_exec_available.sh"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/systemtests/tests/bconsole-pam/"
      RESULT_VARIABLE PAM_EXEC_AVAILABLE_RC
    )
    if(${PAM_EXEC_AVAILABLE_RC} EQUAL "0")
      set(PAM_EXEC_FOUND TRUE)
      set(ENABLE_BCONSOLE_PAM_TEST TRUE)
    endif()
  endif()
endif()

message(STATUS "PAM_EXEC_FOUND:               " ${PAM_EXEC_FOUND})

if(ENABLE_BCONSOLE_PAM_TEST)
  list(APPEND SYSTEM_TESTS "bconsole-pam")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "bconsole-pam")
endif()

if(TARGET python-fd)
  list(APPEND SYSTEM_TESTS "python-fd-plugin-local-fileset-test")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-fd-plugin-local-fileset-test")
endif()

if(TARGET python-fd AND ovirt_server)
  list(APPEND SYSTEM_TESTS "python-fd-ovirt-plugin-test")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-fd-ovirt-plugin-test")
endif()

if(XTRABACKUP)
  set (XTRABACKUP_OR_MARIABACKUP ${XTRABACKUP})
endif()
if(MARIABACKUP)
  set (XTRABACKUP_OR_MARIABACKUP ${MARIABACKUP})
  set (extradumpoptions "--user=root")
endif()
if(TARGET python-fd AND XTRABACKUP_OR_MARIABACKUP )
  list(APPEND SYSTEM_TESTS "python-fd-percona-xtrabackup-plugin-test")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-fd-percona-xtrabackup-plugin-test")
endif()

if(TARGET python-dir)
  list(APPEND SYSTEM_TESTS "python-dir-plugin-test")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-dir-plugin-test")
endif()

if(TARGET python-sd)
  list(APPEND SYSTEM_TESTS "python-sd-plugin-test")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-sd-plugin-test")
endif()

set(PHP_FOUND FALSE)
set(PYTHON_FOUND FALSE)
set(PYTHON_SELENIUM_FOUND "NOT SEARCHED")
set(CHROMEDRIVER_FOUND "NOT SEARCHED")

if(PHP)
  set(PHP_FOUND TRUE)
  if(PYTHON)
    set(PYTHON_FOUND TRUE)
    PythonModuleStatus("selenium") # sets PYTHON_SELENIUM_FOUND to TRUE or FALSE
    if(${PYTHON_SELENIUM_FOUND})
      find_program(CHROMEDRIVER chromedriver)
      if(CHROMEDRIVER)
        set(CHROMEDRIVER_FOUND TRUE)
        set(ENABLE_WEBUI_SELENIUM_TEST TRUE)
      else()
        set(CHROMEDRIVER_FOUND FALSE)
      endif()
    endif()
  endif()
endif()

message(STATUS "PHP_FOUND:                    " ${PHP_FOUND})
message(STATUS "PYTHON_FOUND:                 " ${PYTHON_FOUND})
message(STATUS "PYTHON_SELENIUM_FOUND:        " ${PYTHON_SELENIUM_FOUND})
message(STATUS "CHROMEDRIVER_FOUND:           " ${CHROMEDRIVER_FOUND})

set(AVAILABLE_WEBUI_SELENIUM_TESTS
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

if(ENABLE_WEBUI_SELENIUM_TEST)
  set(WEBUI_SELENIUM_TESTS ${AVAILABLE_WEBUI_SELENIUM_TESTS})
else()
  set(WEBUI_SELENIUM_TESTS)
  foreach(TEST_NAME_DISABLED ${AVAILABLE_WEBUI_SELENIUM_TESTS})
    add_test(NAME webui:${TEST_NAME_DISABLED} COMMAND empty_command)
    set_tests_properties(webui:${TEST_NAME_DISABLED} PROPERTIES DISABLED true)
    message(STATUS "Disabled test: webui:${TEST_NAME_DISABLED}")
  endforeach()
endif()

set(BASEPORT 42001)

foreach(TEST_NAME ${SYSTEM_TESTS})

  prepare_test()

  configurefilestosystemtest("systemtests" "tests/${TEST_NAME}" "*" @ONLY "")
  handle_python_plugin_modules(${TEST_NAME})

  configure_file("environment.in" "tests/${TEST_NAME}/environment" @ONLY)

  checkforenabledanddisabledlistentry(${TEST_NAME})

  add_test(
    NAME "system:${TEST_NAME}"
    COMMAND ${tests_dir}/${TEST_NAME}/testrunner
    WORKING_DIRECTORY ${tests_dir}/${TEST_NAME}
  )

  math(EXPR BASEPORT "${BASEPORT} + 10")

endforeach()

# webui specific settings
if(ENABLE_WEBUI_SELENIUM_TEST)
  foreach(TEST_NAME ${WEBUI_SELENIUM_TESTS})

    prepare_test()
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
      "${CMAKE_SOURCE_DIR}/systemtests/tests/webui-common/directors.ini.in"
      "${current_test_directory}/etc/bareos-webui/directors.ini" @ONLY
    )
    file(COPY "${CMAKE_SOURCE_DIR}/webui/install/configuration.ini"
         DESTINATION "${current_test_directory}/etc/bareos-webui/"
    )
    configure_file(
      "${CMAKE_SOURCE_DIR}/systemtests/tests/webui-common/testrunner.in"
      "${current_test_directory}/testrunner" @ONLY
    )
    # webui start script
    configure_file(
      "${CMAKE_SOURCE_DIR}/systemtests/tests/webui-common/webui.in"
      "${current_test_directory}/bin/webui" @ONLY
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

    checkforenabledanddisabledlistentry(${TEST_NAME})

    add_test(
      NAME "webui:${TEST_NAME}"
      COMMAND ${tests_dir}/${TEST_NAME}/testrunner
      WORKING_DIRECTORY ${tests_dir}/${TEST_NAME}
    )
    math(EXPR BASEPORT "${BASEPORT} + 10")
  endforeach()
endif()

foreach(TEST_NAME_DISABLED ${SYSTEM_TESTS_DISABLED})
  checkforenabledanddisabledlistentry(${TEST_NAME_DISABLED})
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

foreach(BINARY_SOURCEPATH ${BINARIES_TO_LINK_TO_SBIN})
  get_filename_component(BINARY_NAME ${BINARY_SOURCEPATH} NAME)
  create_symlink(
    ${PROJECT_BINARY_DIR}/../core/src/${BINARY_SOURCEPATH}
    ${sbindir}/${BINARY_NAME}
  )
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

foreach(BINARY_SOURCEPATH ${BINARIES_TO_LINK_TO_BIN})
  get_filename_component(BINARY_NAME ${BINARY_SOURCEPATH} NAME)
  create_symlink(
    ${PROJECT_BINARY_DIR}/../core/src/${BINARY_SOURCEPATH}
    ${bindir}/${BINARY_NAME}
  )
endforeach()

create_symlink(
  ${PROJECT_BINARY_DIR}/../core/src/console/bconsole ${bindir}/bconsole
)
create_symlink(${scriptdir}/bareos ${bindir}/bareos)
