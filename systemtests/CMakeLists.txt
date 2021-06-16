#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2021 Bareos GmbH & Co. KG
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

cmake_minimum_required(VERSION 3.3)
project(bareos-systemtests)
cmake_policy(SET CMP0057 NEW) # IF(.. IN_LIST ..)
cmake_policy(SET CMP0053 NEW)
cmake_policy(SET CMP0064 NEW) # IF(TEST ...)

# create a variable BINARY_NAME_TO_TEST for each binary name bareos-dir ->
# BAREOS_DIR_TO_TEST bconsole -> BCONSOLE_TO_TEST
macro(create_variable_BINARY_NAME_TO_TEST_for_binary_name binary_name)
  string(TOUPPER ${binary_name} binary_name_to_test_upcase)
  string(REPLACE "-" "_" binary_name_to_test_upcase
                 ${binary_name_to_test_upcase}
  )
  string(APPEND binary_name_to_test_upcase _TO_TEST)
endmacro()

# find the full path of the given binary when *compiling* the software and
# create and set the BINARY_NAME_TO_TEST variable to the full path of it
macro(find_compiled_binary_and_set_BINARY_NAME_TO_TEST_variable_for binary_name)
  create_variable_binary_name_to_test_for_binary_name(${binary_name})
  get_target_property(
    "${binary_name_to_test_upcase}" "${binary_name}" BINARY_DIR
  )
  set("${binary_name_to_test_upcase}"
      "${${binary_name_to_test_upcase}}/${binary_name}"
  )
  message(
    "   ${binary_name_to_test_upcase} is ${${binary_name_to_test_upcase}}"
  )
endmacro()

# find the full path of the given binary in the *installed* binaries and create
# and set the BINARY_NAME_TO_TEST variable to the full path of it
macro(find_installed_binary_and_set_BINARY_NAME_TO_TEST_variable_for
      binary_name
)
  create_variable_binary_name_to_test_for_binary_name(${binary_name})
  find_program(
    "${binary_name_to_test_upcase}" ${binary_name} PATHS /usr/bin /usr/sbin
                                                         /bin /sbin
  )
  set("${binary_name_to_test_upcase}" "${${binary_name_to_test_upcase}}")
  message(
    "   ${binary_name_to_test_upcase} is ${${binary_name_to_test_upcase}}"
  )

endmacro()

function(configurefilestosystemtest srcbasedir dirname globexpression
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
    get_filename_component(DIR_NAME ${TARGET_FILE} DIRECTORY)

    if(NOT EXISTS ${PROJECT_BINARY_DIR}/${dirname}/${DIR_NAME})
      file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${dirname}/${DIR_NAME})
    endif()
    if(${CURRENT_FILE} MATCHES ".in$")
      configure_file(
        ${CURRENT_FILE} ${PROJECT_BINARY_DIR}/${dirname}/${TARGET_FILE}
        ${configure_option}
      )
    else()
      create_symlink(
        ${CURRENT_FILE} ${PROJECT_BINARY_DIR}/${dirname}/${TARGET_FILE}
      )
    endif()
  endforeach()
endfunction()

# generic function to probe for a python module for the given python version (2
# or 3)
function(check_pymodule_available python_version module)
  if(NOT python_version)
    message(FATAL_ERROR "python_version ist not set")
  endif()
  if(${python_version} EQUAL 2)
    set(python_exe ${Python2_EXECUTABLE})
  elseif(${python_version} EQUAL 3)
    set(python_exe ${Python3_EXECUTABLE})
  else()
    message(FATAL_ERROR "unsupported python_version ${python_version}")
  endif()
  # message(STATUS "running  ${python_exe} -c import ${module}")
  execute_process(
    COMMAND "${python_exe}" "-c" "import ${module}"
    RESULT_VARIABLE ${module}_status
    ERROR_QUIET
  )
  string(TOUPPER ${module} module_uppercase)
  if(${module}_status EQUAL 0)
    set("PYMODULE_${python_version}_${module_uppercase}_FOUND"
        TRUE
        PARENT_SCOPE
    )
    message(STATUS "python module pyversion ${python_version} ${module} found")
  else()
    set("PYMODULE_${python_version}_${module_uppercase}_FOUND"
        FALSE
        PARENT_SCOPE
    )
    message(
      STATUS "python module pyversion ${python_version} ${module} NOT found"
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

macro(link_binaries_to_test_to_current_sbin_dir_with_individual_filename)
  foreach(binary_name ${ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS})
    create_variable_binary_name_to_test_for_binary_name(${binary_name})
    string(REPLACE "-" "_" binary_name ${binary_name})
    string(TOUPPER ${binary_name} binary_name_upcase)
    string(CONCAT bareos_XXX_binary ${binary_name_upcase} "_BINARY")
    # message (STATUS "${bareos_XXX_binary}")
    set(${bareos_XXX_binary} ${CURRENT_SBIN_DIR}/${binary_name}-${TEST_NAME})
    # message( "creating symlink ${${bareos_XXX_binary}}  ->
    # ${${binary_name_to_test_upcase}}" )
    create_symlink(${${binary_name_to_test_upcase}} ${${bareos_XXX_binary}})
  endforeach()
  create_symlink(${scriptdir}/btraceback ${CURRENT_SBIN_DIR}/btraceback)

  if(TARGET bareos_vadp_dumper)
    if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
      create_symlink(
        "/usr/sbin/bareos_vadp_dumper_wrapper.sh"
        "${CURRENT_SBIN_DIR}/bareos_vadp_dumper_wrapper.sh"
      )
    else()
      create_symlink(
        "${CMAKE_SOURCE_DIR}/core/src/vmware/vadp_dumper/bareos_vadp_dumper_wrapper.sh"
        "${CURRENT_SBIN_DIR}/bareos_vadp_dumper_wrapper.sh"
      )

    endif()
    create_symlink(
      "${CURRENT_SBIN_DIR}/bareos_vadp_dumper-${TEST_NAME}"
      "${CURRENT_SBIN_DIR}/bareos_vadp_dumper"
    )
  endif()
endmacro()

macro(prepare_testdir_for_daemon_run)

  set(archivedir ${current_test_directory}/storage)
  set(confdir ${current_test_directory}/etc/bareos)
  set(config_directory_dir_additional_test_config
      ${current_test_directory}/etc/bareos/bareos-dir.d/additional_test_config
  )
  set(logdir ${current_test_directory}/log)
  set(tmpdir ${current_test_directory}/tmp)
  set(tmp ${tmpdir})
  set(working_dir ${current_test_directory}/working)
  set(piddir ${current_test_directory}/piddir)

  set(backenddir ${BACKEND_DIR_TO_TEST})
  set(sd_backenddir ${SD_BACKEND_DIR_TO_TEST})
  set(BAREOS_WEBUI_PUBLIC_DIR ${WEBUI_PUBLIC_DIR_TO_TEST})

  set(dbHost ${current_test_directory}/tmp)
  string(LENGTH ${dbHost} dbHostLength)
  if(${dbHostLength} GREATER 90)
    # unix domain sockets (used by mysql and psql) cannot be longer than 107
    # chars. If too long, the socket is created under /tmp
    set(dbHost /tmp/${TEST_NAME})
    file(MAKE_DIRECTORY ${dbHost})
  endif()

  if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
    set(bin /bin)
    set(sbin /sbin)
    set(scripts ${SCRIPTS_DIR_TO_TEST})
    set(python_plugin_module_src_dir ${PYTHON_PLUGINS_DIR_TO_TEST})
    set(python_plugin_module_src_test_dir ${PYTHON_PLUGINS_DIR_TO_TEST})
  else()
    set(bin ${PROJECT_BINARY_DIR}/bin)
    set(sbin ${PROJECT_BINARY_DIR}/sbin)
    set(scripts ${PROJECT_BINARY_DIR}/scripts)
    set(python_plugin_module_src_dir ${CMAKE_SOURCE_DIR}/core/src/plugins)
    set(python_plugin_module_src_test_dir
        ${current_test_directory}/python-modules
    )
  endif()

  file(MAKE_DIRECTORY ${piddir})
  file(MAKE_DIRECTORY ${tmpdir})
  file(MAKE_DIRECTORY ${archivedir})
  file(MAKE_DIRECTORY ${logdir})
  file(MAKE_DIRECTORY ${confdir})
  file(MAKE_DIRECTORY ${working_dir})
  file(MAKE_DIRECTORY ${config_directory_dir_additional_test_config})
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/systemtests/tls/minio/)

  # create a bin/bareos and bin/bconsole script in every testdir for start/stop
  # and bconsole
  file(MAKE_DIRECTORY "${current_test_directory}/bin")
  configure_file(
    "bin/bconsole" "${current_test_directory}/bin/bconsole" COPYONLY
  )
  configure_file("bin/bareos" "${current_test_directory}/bin/bareos" COPYONLY)

  set(CURRENT_SBIN_DIR ${current_test_directory}/sbin)
  file(MAKE_DIRECTORY ${CURRENT_SBIN_DIR})

  link_binaries_to_test_to_current_sbin_dir_with_individual_filename()
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
  set(db_address "$current_test_directory/database/tmp")

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
  math(EXPR test_db_port "${BASEPORT} + 5")
  math(EXPR minio_port "${BASEPORT} + 6")
  math(EXPR restapi_port "${BASEPORT} + 7")

  # skip for tests without etc/bareos ("catalog" test)
  if(EXISTS ${current_test_source_directory}/etc/bareos)
    prepare_testdir_for_daemon_run()
    create_symlink(
      ${CMAKE_SOURCE_DIR}/core/scripts/mtx-changer.conf
      ${current_test_directory}/etc/bareos/mtx-changer.conf
    )
  endif()
endmacro()

macro(create_symlink target link)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.14")
    file(CREATE_LINK ${target} ${link} SYMBOLIC)
  else()
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink ${target} ${link}
    )
  endif()
endmacro()

# Main starts here...

message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

find_program(PG_CTL pg_ctl /usr/lib/postgresql/*/bin /usr/bin /bin)
get_filename_component(POSTGRES_BIN_PATH ${PG_CTL} DIRECTORY)
# message(STATUS "PG_CTL is ${PG_CTL}")
message(STATUS "POSTGRES_BIN_PATH is ${POSTGRES_BIN_PATH}")

find_program(PERL perl)

option(RUN_SYSTEMTESTS_ON_INSTALLED_FILES
       "configure the system tests to run on installed files (from packages)"
)

set(ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS
    bareos-dir
    bareos-dbcheck
    bareos-fd
    bareos-sd
    bareos-dbcopy
    bcopy
    btape
    bextract
    bareos-sd
    bls
    bscan
    bconsole
    bsmtp
    bwild
    bpluginfo
    bsmtp
    bscrypto
    btestls
    drivetype
    fstype
    bregex
)
if(TARGET bareos_vadp_dumper)
  list(APPEND ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS bareos_vadp_dumper)
endif()

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

message(
  STATUS
    "RUN_SYSTEMTESTS_ON_INSTALLED_FILES:   ${RUN_SYSTEMTESTS_ON_INSTALLED_FILES}"
)

message(STATUS "Looking for binaries and paths...")

if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)

  foreach(BINARY ${ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS})
    find_installed_binary_and_set_binary_name_to_test_variable_for(${BINARY})
  endforeach()

  find_program(
    PYTHON_PLUGIN_TO_TEST python-fd.so PATHS /usr/lib/bareos/plugins
                                             /usr/lib64/bareos/plugins
  )
  find_program(
    PYTHON_PLUGIN_TO_TEST python3-fd.so PATHS /usr/lib/bareos/plugins
                                              /usr/lib64/bareos/plugins
  )
  find_program(
    CREATE_BAREOS_DATABASE_TO_TEST create_bareos_database
    PATHS /usr/lib/bareos/scripts
  )
  find_program(
    PYTHON_PLUGINS_DIR_TO_TEST BareosFdWrapper.py
    PATHS /usr/lib/bareos/plugins /usr/lib64/bareos/plugins
  )
  find_program(
    BACKEND_DIR_TO_TEST libbareoscats-${DEFAULT_DB_TYPE}.so
    PATHS /usr/lib/bareos/backends /usr/lib64/bareos/backends
  )

  get_filename_component(PLUGINS_DIR_TO_TEST ${PYTHON_PLUGIN_TO_TEST} DIRECTORY)
  get_filename_component(
    PYTHON_PLUGINS_DIR_TO_TEST ${PYTHON_PLUGINS_DIR_TO_TEST} DIRECTORY
  )
  get_filename_component(
    SCRIPTS_DIR_TO_TEST ${CREATE_BAREOS_DATABASE_TO_TEST} DIRECTORY
  )
  get_filename_component(BACKEND_DIR_TO_TEST ${BACKEND_DIR_TO_TEST} DIRECTORY)

  set(FD_PLUGINS_DIR_TO_TEST ${PLUGINS_DIR_TO_TEST})
  set(SD_PLUGINS_DIR_TO_TEST ${PLUGINS_DIR_TO_TEST})
  set(DIR_PLUGINS_DIR_TO_TEST ${PLUGINS_DIR_TO_TEST})
  set(BACKEND_DIR_TO_TEST ${BACKEND_DIR_TO_TEST})
  set(WEBUI_PUBLIC_DIR_TO_TEST /usr/share/bareos-webui/public)

else() # run systemtests on source and compiled files

  foreach(BINARY ${ALL_BINARIES_BEING_USED_BY_SYSTEMTESTS})
    find_compiled_binary_and_set_binary_name_to_test_variable_for(${BINARY})
  endforeach()

  get_target_property(FD_PLUGINS_DIR_TO_TEST bpipe-fd BINARY_DIR)
  get_target_property(SD_PLUGINS_DIR_TO_TEST autoxflate-sd BINARY_DIR)
  if(TARGET bareossd-droplet)
    get_target_property(SD_BACKEND_DIR_TO_TEST bareossd-droplet BINARY_DIR)
  endif()
  if(TARGET bareossd-gfapi)
    get_target_property(SD_BACKEND_DIR_TO_TEST bareossd-gfapi BINARY_DIR)
  endif()
  if(TARGET bareossd-tape)
    get_target_property(SD_BACKEND_DIR_TO_TEST bareossd-tape BINARY_DIR)
  endif()
  set(DIR_PLUGINS_DIR_TO_TEST ${CMAKE_BINARY_DIR}/core/src/plugins/dird)

  get_target_property(BACKEND_DIR_TO_TEST bareoscats BINARY_DIR)
  set(SCRIPTS_DIR_TO_TEST ${CMAKE_BINARY_DIR}/core/scripts)
  set(WEBUI_PUBLIC_DIR_TO_TEST ${PROJECT_SOURCE_DIR}/../webui/public)

endif()

message("   FD_PLUGINS_DIR_TO_TEST: ${FD_PLUGINS_DIR_TO_TEST}")
message("   SD_PLUGINS_DIR_TO_TEST: ${SD_PLUGINS_DIR_TO_TEST}")
message("   DIR_PLUGINS_DIR_TO_TEST: ${DIR_PLUGINS_DIR_TO_TEST}")
message("   BACKEND_DIR_TO_TEST: ${BACKEND_DIR_TO_TEST}")
message("   SD_BACKEND_DIR_TO_TEST: ${SD_BACKEND_DIR_TO_TEST}")
message("   WEBUI_PUBLIC_DIR_TO_TEST: ${WEBUI_PUBLIC_DIR_TO_TEST}")

# extract  db version from cats.h
file(STRINGS ${CMAKE_SOURCE_DIR}/core/src/cats/cats.h DB_VERSION_STRING
     REGEX .*BDB_VERSION.*
)
string(REGEX MATCH [0-9]+ BDB_VERSION ${DB_VERSION_STRING})
message(STATUS "db version from cat.sh is ${BDB_VERSION}")

# set variable values to be replaced by configure_file set(DEFAULT_DB_TYPE
# ${default_db_backend})
message(STATUS "DEFAULT_DB_TYPE is ${DEFAULT_DB_TYPE}")

# set(TAPEDRIVE "TAPEDRIVE")

set(archivedir "${PROJECT_BINARY_DIR}/archivedir")

set(bin ${PROJECT_BINARY_DIR}/bin)
set(bindir ${PROJECT_BINARY_DIR}/bin)

set(BAREOS_CONFIG_DIR ${conf})
set(db_name "regress")
set(db_password "")
set(db_port "5432")
set(db_user "regress")
set(dir_password "dir_password")
set(dumps ${PROJECT_BINARY_DIR}/dumps)
set(fd_password "fd_password")

set(job_email "job_email")
set(logdir "log")
set(mon_dir_password "mon_dir_password")
set(mon_fd_password "mon_fd_password")
set(mon_sd_password "mon_sd_password")
set(python_plugin_module_src_test_dir_relative "python-modules")
set(plugindirtmp ${PROJECT_BINARY_DIR}/plugindirtmp)
set(rscripts ${PROJECT_BINARY_DIR}/scripts)

if(TARGET python-dir OR TARGET python3-dir)
  set(dir_plugin_binary_path ${DIR_PLUGINS_DIR_TO_TEST})
endif()
if(TARGET python-sd OR TARGET python3-sd)
  set(sd_plugin_binary_path ${SD_PLUGINS_DIR_TO_TEST})
endif()
if(TARGET python-fd OR TARGET python3-fd)
  set(fd_plugin_binary_path ${FD_PLUGINS_DIR_TO_TEST})
endif()

set(sbindir ${PROJECT_BINARY_DIR}/sbin)

set(scriptdir ${PROJECT_BINARY_DIR}/scripts)
set(scripts ${PROJECT_BINARY_DIR}/scripts)
set(sd_password "sd_password")
set(smtp_host "smtp_host")
set(src ${PROJECT_BINARY_DIR}/src)
set(subsysdir ${PROJECT_BINARY_DIR}/subsysdir)
set(tmp ${PROJECT_BINARY_DIR}/tmp)
set(tmpdir ${tmp})
set(tmpsrc ${PROJECT_BINARY_DIR}/tmpsrc)
set(working ${PROJECT_BINARY_DIR}/working)
set(working_dir ${PROJECT_BINARY_DIR}/working)

# for config-lib.sh: do not remove @DB___@ but replace them with the original
set(DB_NAME @DB_NAME@)
set(DB_USER @DB_USER@)
set(DB_PASS @DB_PASS@)
set(DB_VERSION @DB_VERSION@)

configurefilestosystemtest("systemtests" "data" "*.tgz" COPYONLY "")
configurefilestosystemtest("systemtests" "data" "*.gz" COPYONLY "")

configurefilestosystemtest("systemtests" "scripts" "functions" @ONLY "")
configurefilestosystemtest(
  "systemtests" "scripts" "reload_test_functions" @ONLY ""
)
configurefilestosystemtest("systemtests" "scripts" "cleanup" @ONLY "")
configurefilestosystemtest("systemtests" "scripts" "setup" @ONLY "")
configurefilestosystemtest(
  "systemtests" "scripts" "check_for_zombie_jobs" @ONLY ""
)
configurefilestosystemtest("systemtests" "scripts" "diff.pl.in" @ONLY "")
configurefilestosystemtest(
  "systemtests" "scripts" "generate_minio_certs.sh.in" @ONLY ""
)

configurefilestosystemtest("core/src" "defaultconfigs" "*.conf" @ONLY "")
configurefilestosystemtest("core/src" "defaultconfigs" "*.in" @ONLY "")

configurefilestosystemtest("core" "scripts" "*.in" @ONLY "")
configurefilestosystemtest("core" "scripts" "bareos-ctl-funcs" @ONLY "")
configurefilestosystemtest("core" "scripts" "btraceback.gdb" @ONLY "")

configurefilestosystemtest("core/src/cats" "ddl" "*" @ONLY "")
configurefilestosystemtest("core/src" "cats" "*.in" @ONLY "")
configurefilestosystemtest("core/src" "cats" "*_bareos_*" @ONLY "")

configurefilestosystemtest("core/src" "console" "*.in" @ONLY "")

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
  string(REPLACE "${CMAKE_BINARY_DIR}/systemtests/cats/" "" TARGET_FILE
                 ${CURRENT_FILE}
  )
  file(RENAME ${CURRENT_FILE} ${scripts}/${TARGET_FILE})
  # MESSAGE(STATUS "moved  ${scripts}/${TARGET_FILE}")
endforeach()

set(tests_dir ${PROJECT_BINARY_DIR}/tests)
set(SYSTEM_TESTS
    ai-consolidate-ignore-duplicate-job
    bareos
    bareos-acl
    bscan-bextract-bls
    bconsole-status-client
    client-initiated
    config-dump
    config-syntax-crash
    copy-archive-job
    copy-bscan
    copy-remote-bscan
    deprecation
    encrypt-signature
    encrypt-signature-tls-cert
    filesets
    list-backups
    multiplied-device
    notls
    passive
    reload-add-client
    reload-add-duplicate-job
    reload-add-empty-job
    reload-add-second-director
    reload-add-uncommented-string
    reload-unchanged-config
    scheduler-backup
    spool
    upgrade-database
    virtualfull
    virtualfull-bscan
    volume-pruning
)

set(SYSTEM_TESTS_DISABLED # initially empty
)

if(TARGET gfapi-fd
   AND gfapi_fd_host
   AND SUDO
)
  list(APPEND SYSTEM_TESTS gfapi-fd)
  message(
    "SUDO: ${SUDO},  gfapi-fd test settings: gfapi_fd_host=${gfapi_fd_host}, gfapi_fd_testvolume=${gfapi_fd_testvolume}"
  )
else()
  list(APPEND SYSTEM_TESTS_DISABLED gfapi-fd)
endif()

# add tests also here that are unreliable they are excluded when running tests
# during build in jenkins
set(SYSTEM_TESTS_BROKEN
    python-restapi
    py3plug-fd-postgres # fails because of psycopg2
    py3plug-fd-local-fileset # Fails with UnicodeEncodeError: 'utf-8' codec
                             # can't encode characters in position 133-142:
                             # surrogates not allowed
)

set(SYSTEMTEST_TIMEOUT "300" CACHE STRING "Timeout for systemtests (in seconds)")

set(glusterfs_uri
    ""
    CACHE STRING "URI for GlusterFS backend test"
)
mark_as_advanced(glusterfs_uri)

include(BareosCheckXattr)
if(SETFATTR_WORKS)
  list(APPEND SYSTEM_TESTS xattr)
else()
  list(APPEND SYSTEM_TESTS_DISABLED xattr)
endif()

if(AUTOCHANGER_DEVICES_FOUND)
  if(MT AND MTX)
    set(AUTOCHANGER_TEST_ENABLED TRUE)
  else()
    message(FATAL_ERROR "mtx/mt not found for autochanger test")
  endif()
endif()

if(AUTOCHANGER_TEST_ENABLED)
  list(APPEND SYSTEM_TESTS autochanger)
else()
  list(APPEND SYSTEM_TESTS_DISABLED autochanger)
endif()

if(ndmp_data_agent_address)

  find_program(IP_COMMAND ip)
  if(IP_COMMAND)
    execute_process(
      COMMAND ${IP_COMMAND} route get ${ndmp_data_agent_address}
      OUTPUT_VARIABLE ROUTE_INFO_TO_ACCESS_NDMP_DATA_AGENT
    )

    # cmake-format: off
    #
    # Example:
    # 192.168.76.10 via 192.168.77.1 dev eth0 src 192.168.77.101 uid 25031
    string(
      REGEX REPLACE ".* src ([^ ]+).*"
      \\1
      IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT
      ${ROUTE_INFO_TO_ACCESS_NDMP_DATA_AGENT}
    )
    # cmake-format: on

    message(DEBUG "NDMP ROUTE INFO: ${ROUTE_INFO_TO_ACCESS_NDMP_DATA_AGENT}")

    message(
      STATUS
        "IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT (${ndmp_data_agent_address})=${IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT}"
    )

    if(IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT)
      if(NOT ndmp_data_agent_user)
        set(ndmp_data_agent_user ndmpadmin)
      endif()
      if(NOT ndmp_data_agent_password)
        set(ndmp_data_agent_password test)
      endif()

      list(APPEND SYSTEM_TESTS ndmp)
    endif()
  endif()

endif()

if(TARGET (messages_resource))
  list(APPEND SYSTEM_TESTS messages)
else()
  list(APPEND SYSTEM_TESTS_DISABLED messages)
endif()

include(BareosCheckAcl)
if(SETFACL_WORKS)
  list(APPEND SYSTEM_TESTS acl)
else()
  list(APPEND SYSTEM_TESTS_DISABLED acl)
endif()

if(TARGET droplet
   AND S3CMD
   AND MINIO
)
  list(APPEND SYSTEM_TESTS droplet-s3)
else()
  list(APPEND SYSTEM_TESTS_DISABLED droplet-s3)
endif()

if(SD_GFAPI_DIR_TO_TEST AND glusterfs_uri)
  list(APPEND SYSTEM_TESTS glusterfs-backend)
else()
  list(APPEND SYSTEM_TESTS_DISABLED glusterfs-backend)
endif()

if(mysql
   AND postgresql
   AND dynamic-cats-backends
)
  list(APPEND SYSTEM_TESTS "dbcopy-mysql-postgresql")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "dbcopy-mysql-postgresql")
endif()

find_program(PYTHON NAMES python python3 python2)

# python-bareos-test does not work on installed files
if(PYTHON_EXECUTABLE
   AND PYTHON
   AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES
)
  list(APPEND SYSTEM_TESTS "python-bareos")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-bareos")
endif()

check_pymodule_available(3 fastapi)
check_pymodule_available(3 uvicorn)
find_program(CURL curl)

# python-restapi-test does not work on installed files and needs some modules
if(PYTHON_EXECUTABLE
   AND CURL
   AND PYTHON
   AND PYMODULE_3_FASTAPI_FOUND
   AND PYMODULE_3_UVICORN_FOUND
   AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES
)
  list(APPEND SYSTEM_TESTS "python-restapi")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-restapi")
endif()

find_package(GTest 1.8 CONFIG)
if(GTest_FOUND AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  list(APPEND SYSTEM_TESTS "catalog")
  message(
    STATUS
      "adding catalog test as gtest was found and not testing installed files"
  )
else()
  list(APPEND SYSTEM_TESTS_DISABLED "catalog")
  message(STATUS "disabling catalog test as gtest was not found")
endif()

if(TARGET python-fd)
  list(APPEND SYSTEM_TESTS "py2plug-fd-local-fileset")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-local-fileset")
endif()

if(TARGET python3-fd)
  list(APPEND SYSTEM_TESTS "py3plug-fd-local-fileset")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-local-fileset")
endif()

set(SYSTEMTEST_LDAP_ADDRESS
    ""
    CACHE STRING "address of LDAP server for Plugin test"
)
set(SYSTEMTEST_LDAP_BASEDN
    "dc=example,dc=org"
    CACHE STRING "LDAP base DN for plugin test"
)
set(SYSTEMTEST_LDAP_BINDDN
    "cn=admin,dc=example,dc=org"
    CACHE STRING "LDAP bind DN for plugin test"
)
set(SYSTEMTEST_LDAP_PASSWORD
    "admin"
    CACHE STRING "LDAP bind password for plugin test"
)
check_pymodule_available(2 ldap)
if(TARGET python-fd
   AND PYMODULE_2_LDAP_FOUND
   AND Python2_EXECUTABLE
   AND SYSTEMTEST_LDAP_ADDRESS
)
  list(APPEND SYSTEM_TESTS "py2plug-fd-ldap")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-ldap")
endif()

check_pymodule_available(2 libcloud)
check_pymodule_available(3 libcloud)
if(TARGET python-fd)
  if(S3CMD
     AND MINIO
     AND PYMODULE_2_LIBCLOUD_FOUND
  )
    list(APPEND SYSTEM_TESTS "py2plug-fd-libcloud")
  else()
    message(
      "S3CMD, MINIO or LIBCLOUD MODULE not found, disabling py2plug-fd-libcloud"
    )
    list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-libcloud")
  endif()
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-libcloud")
endif()

if(TARGET python3-fd)
  if(S3CMD
     AND MINIO
     AND PYMODULE_3_LIBCLOUD_FOUND
     AND (${Python3_VERSION_MAJOR} EQUAL 3)
     AND (${Python3_VERSION_MINOR} LESS 10)
  )
    list(APPEND SYSTEM_TESTS "py3plug-fd-libcloud")
  else()
    message(
      "S3CMD, MINIO or LIBCLOUD MODULE not found or Python > 3.7, disabling py3plug-fd-libcloud"
    )
    list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-libcloud")
  endif()
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-libcloud")
endif()

if(TARGET python-fd)
  list(APPEND SYSTEM_TESTS "py2plug-fd-local-fileset-restoreobject")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-local-fileset-restoreobject")
endif()

if(TARGET python3-fd)
  list(APPEND SYSTEM_TESTS "py3plug-fd-local-fileset-restoreobject")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-local-fileset-restoreobject")
endif()

message("checking for requirements of py2plug-fd-postgres:")
check_pymodule_available(2 psycopg2)
check_pymodule_available(2 dateutil)
if(TARGET python-fd
   AND PYMODULE_2_PSYCOPG2_FOUND
   AND PYMODULE_2_DATEUTIL_FOUND
)
  message("OK, enabling py2plug-fd-postgres:")
  list(APPEND SYSTEM_TESTS "py2plug-fd-postgres")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-postgres")
  message("NOT OK, disabling py2plug-fd-postgres:")
endif()

message("checking for requirements of py3plug-fd-postgres:")
if(TARGET python3-fd)
  check_pymodule_available(3 psycopg2)
  check_pymodule_available(3 dateutil)
  if(PYMODULE_3_PSYCOPG2_FOUND
     AND PYMODULE_3_DATEUTIL_FOUND
     AND (${Python3_VERSION_MAJOR} EQUAL 3)
     AND (${Python3_VERSION_MINOR} LESS 10)
  )
    list(APPEND SYSTEM_TESTS "py3plug-fd-postgres")
  else()
    list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-postgres")
  endif()
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-postgres")
endif()

if(TARGET python-fd AND ovirt_server)
  list(APPEND SYSTEM_TESTS "py2plug-fd-ovirt")
else()
  message(STATUS "disabling py2plug-fd-ovirt-test as ovirt_server is not set")
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-ovirt")
endif()

if(TARGET python3-fd AND ovirt_server)
  list(APPEND SYSTEM_TESTS "py3plug-fd-ovirt")
else()
  message(STATUS "disabling py3plug-fd-ovirt as ovirt_server is not set")
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-ovirt")
endif()

if(TARGET python-fd AND enable_vmware_test)
  list(APPEND SYSTEM_TESTS "py2plug-fd-vmware")
else()
  message(STATUS "disabling py2plug-fd-vmware as vmware_server was not set")
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-vmware")
endif()

if(TARGET python3-fd AND enable_vmware_test)
  list(APPEND SYSTEM_TESTS "py3plug-fd-vmware")
else()
  message(STATUS "disabling py3plug-fd-vmware as vmware_server was not set")
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-vmware")
endif()

if(TARGET python-fd AND XTRABACKUP)
  list(APPEND SYSTEM_TESTS "py2plug-fd-percona-xtrabackup")
else()
  message(
    STATUS
      "disabling py2plug-fd-percona-xtrabackup-test as XTRABACKUP was not found"
  )
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-fd-percona-xtrabackup")
endif()

if(TARGET python3-fd AND XTRABACKUP)
  list(APPEND SYSTEM_TESTS "py3plug-fd-percona-xtrabackup")
else()
  message(
    STATUS "disabling py3plug-fd-percona-xtrabackup as XTRABACKUP was not found"
  )
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-fd-percona-xtrabackup")
endif()

if(TARGET python-dir)
  list(APPEND SYSTEM_TESTS "py2plug-dir")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-dir")
endif()

if(TARGET python3-dir)
  list(APPEND SYSTEM_TESTS "py3plug-dir")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-dir")
endif()

if(TARGET python-sd)
  list(APPEND SYSTEM_TESTS "py2plug-sd")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py2plug-sd")
endif()

if(TARGET python3-sd)
  list(APPEND SYSTEM_TESTS "py3plug-sd")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "py3plug-sd")
endif()

message(STATUS "Looking for pam test requirements ...")
bareosfindlibraryandheaders("pam" "security/pam_appl.h" "")
find_program(PAMTESTER pamtester)

set(ENV{PAM_WRAPPER_LIBRARIES} "${PAM_WRAPPER_LIBRARIES}")
execute_process(
  COMMAND
    "${CMAKE_SOURCE_DIR}/systemtests/tests/bconsole-pam/bin/check_pam_exec_available.sh"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/systemtests/tests/bconsole-pam/"
  RESULT_VARIABLE PAM_EXEC_AVAILABLE_RC
)
if(PAM_EXEC_AVAILABLE_RC EQUAL "0")
  set(PAM_EXEC_AVAILABLE TRUE)
endif()
message("   PAM_FOUND:                " ${PAM_FOUND})
message("   PAM_WRAPPER_LIBRARIES:    " ${PAM_WRAPPER_LIBRARIES})
message("   PAMTESTER:                " ${PAMTESTER})
message("   PAM_EXEC_AVAILABLE:       " ${PAM_EXEC_AVAILABLE})

if(PAM_WRAPPER_LIBRARIES
   AND PAMTESTER
   AND PAM_EXEC_AVAILABLE
   AND PAM_FOUND
   AND PYTHON
)
  set(ENABLE_BCONSOLE_PAM_TEST TRUE)
  message(STATUS "OK: all requirements for pam tests were met.")
else()
  set(ENABLE_BCONSOLE_PAM_TEST FALSE)
  message(
    STATUS "NOT OK: disabling pam tests as not all requirements were found."
  )
endif()

if(ENABLE_BCONSOLE_PAM_TEST)
  list(APPEND SYSTEM_TESTS "bconsole-pam")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "bconsole-pam")
endif()

# python-bareos does not work on installed files and is used here
if(ENABLE_BCONSOLE_PAM_TEST AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  list(APPEND SYSTEM_TESTS "python-pam")
else()
  list(APPEND SYSTEM_TESTS_DISABLED "python-pam")
endif()

message(STATUS "Looking for webui test requirements ...")

find_program(PHP php)
find_program(CHROMEDRIVER chromedriver)
check_pymodule_available(2 selenium) # sets PYMODULE_2_SELENIUM_FOUND to TRUE or
check_pymodule_available(3 selenium) # sets PYMODULE_3_SELENIUM_FOUND to TRUE or
# FALSE

message("   PHP:                    " ${PHP})
message("   PYTHON_EXECUTABLE:      " ${PYTHON_EXECUTABLE})
message("   PYMODULE_2_SELENIUM_FOUND:" ${PYMODULE_2_SELENIUM_FOUND})
message("   CHROMEDRIVER:           " ${CHROMEDRIVER})

if(PHP
   AND PYTHON_EXECUTABLE
   AND PYMODULE_2_SELENIUM_FOUND
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
    add_test(NAME ${WEBUI_TEST_PREFIX}${TEST_NAME_DISABLED}
             COMMAND empty_command
    )
    set_tests_properties(
      ${WEBUI_TEST_PREFIX}${TEST_NAME_DISABLED} PROPERTIES DISABLED true
    )
    message(STATUS "Disabled test: ${WEBUI_TEST_PREFIX}${TEST_NAME_DISABLED}")
  endforeach()
endif()

set(BASEPORT 30001)

set(SYSTEMTEST_PREFIX "system:")

if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  set(TEST_INFO_TEXT "running system tests on installed files")
else()
  set(TEST_INFO_TEXT "running system tests on the sourcetree")
endif()

foreach(TEST_NAME ${SYSTEM_TESTS})

  message(STATUS "Configuring test: ${SYSTEMTEST_PREFIX}${TEST_NAME}")
  string(REGEX MATCH "py2plug" py_v2 "${TEST_NAME}")
  string(REGEX MATCH "py3plug" py_v3 "${TEST_NAME}")
  if(py_v2)
    set(python_module_name python)
  endif()
  if(py_v3)
    set(python_module_name python3)
  endif()
  prepare_test()

  configurefilestosystemtest("systemtests" "tests/${TEST_NAME}" "*" @ONLY "")

  if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
    string(CONCAT pythonpath ${PYTHON_PLUGINS_DIR_TO_TEST})

  else()
    string(
      CONCAT
        pythonpath
        "${CMAKE_SOURCE_DIR}/python-bareos:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/ldap:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/libcloud:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/percona-xtrabackup:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/ovirt:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/postgres:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/pyfiles:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/stored/python/pyfiles:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/dird/python/pyfiles:"
        "${CMAKE_BINARY_DIR}/core/src/plugins/filed/python/${python_module_name}modules:"
        "${CMAKE_BINARY_DIR}/core/src/plugins/stored/python/${python_module_name}modules:"
        "${CMAKE_BINARY_DIR}/core/src/plugins/dird/python/${python_module_name}modules:"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST_NAME}/python-modules"
    )
  endif()
  configure_file("environment.in" "tests/${TEST_NAME}/environment" @ONLY)

  checkforenabledanddisabledlistentry(${TEST_NAME})

  add_test(
    NAME "${SYSTEMTEST_PREFIX}${TEST_NAME}"
    COMMAND ${tests_dir}/${TEST_NAME}/testrunner
    WORKING_DIRECTORY ${tests_dir}/${TEST_NAME}
  )
  if(${TEST_NAME} IN_LIST SYSTEM_TESTS_BROKEN)
    set_tests_properties(
      ${SYSTEMTEST_PREFIX}${TEST_NAME} PROPERTIES LABELS "broken"
    )
  endif()

  set_tests_properties(
    ${SYSTEMTEST_PREFIX}${TEST_NAME} PROPERTIES TIMEOUT "${SYSTEMTEST_TIMEOUT}"
                                                COST 1.0 SKIP_RETURN_CODE 77
  )
  math(EXPR BASEPORT "${BASEPORT} + 10")

endforeach()

# webui specific settings
if(ENABLE_WEBUI_SELENIUM_TEST)
  foreach(TEST_NAME ${WEBUI_SELENIUM_TESTS})
    message(STATUS "Configuring test: ${WEBUI_TEST_PREFIX}${TEST_NAME}")

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
      NAME "${WEBUI_TEST_PREFIX}${TEST_NAME}"
      COMMAND ${tests_dir}/${TEST_NAME}/testrunner
      WORKING_DIRECTORY ${tests_dir}/${TEST_NAME}
    )
    set_tests_properties(
      ${WEBUI_TEST_PREFIX}${TEST_NAME} PROPERTIES TIMEOUT 120 COST 1.0
    )
    math(EXPR BASEPORT "${BASEPORT} + 10")
  endforeach()
endif()

foreach(TEST_NAME_DISABLED ${SYSTEM_TESTS_DISABLED})
  checkforenabledanddisabledlistentry(${TEST_NAME_DISABLED})
  add_test(NAME ${SYSTEMTEST_PREFIX}${TEST_NAME_DISABLED} COMMAND empty_command)
  set_tests_properties(
    ${SYSTEMTEST_PREFIX}${TEST_NAME_DISABLED} PROPERTIES DISABLED true
  )
  message(STATUS "Disabled test: ${SYSTEMTEST_PREFIX}${TEST_NAME_DISABLED}")
endforeach()

configure_file(
  "CTestCustom.cmake.in" "${CMAKE_BINARY_DIR}/CTestCustom.cmake" @ONLY
)
configure_file(
  "ctest_custom_pretest.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ctest_custom_pretest.sh" @ONLY
)

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
