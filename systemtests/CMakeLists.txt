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

cmake_minimum_required(VERSION 3.0)
project(bareos-systemtests)


function(ConfigureFilesToSystemtest srcdir subdirectory globexpression configure_option)
  MESSAGE(STATUS "ConfigureFilesToSystemtest from ${srcdir} to ${subdirectory} with glob ${globexpression} ${configure_option}" )
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${subdirectory})
  file(GLOB_RECURSE ALL_FILES "${CMAKE_SOURCE_DIR}/${srcdir}/${subdirectory}/${globexpression}")
  foreach(CURRENT_FILE ${ALL_FILES})
    string(REPLACE "${CMAKE_SOURCE_DIR}/" "" TARGET_FILE ${CURRENT_FILE})
    string(REPLACE ".in" "" TARGET_FILE ${TARGET_FILE})
    MESSAGE(STATUS "configuring  ${TARGET_FILE}" )
    string(REPLACE "${srcdir}/${subdirectory}" "" TARGET_FILE ${TARGET_FILE})
    configure_file(${CURRENT_FILE} ${PROJECT_BINARY_DIR}/${subdirectory}/${TARGET_FILE} ${configure_option})
  endforeach()
endfunction()



# extract  db version from cats.h
file(STRINGS ${CMAKE_SOURCE_DIR}/core/src/cats/cats.h DB_VERSION_STRING REGEX .*BDB_VERSION.*)
string(REGEX MATCH [0-9]+ BDB_VERSION ${DB_VERSION_STRING})

find_program(PERL perl)



# set variable values to be replaced by configure_file
#set(DEFAULT_DB_TYPE  ${default_db_backend})
message(STATUS "DEFAULT_DB_TYPE is ${DEFAULT_DB_TYPE}")

#set(TAPEDRIVE "TAPEDRIVE")

set(archivedir "${PROJECT_BINARY_DIR}/archivedir")
set(backenddir "${PROJECT_BINARY_DIR}/../core/src/cats")

set(basename test)
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
set(piddir ${PROJECT_BINARY_DIR}/piddir)
set(plugindir ${PROJECT_BINARY_DIR}/plugindir)
set(plugindirtmp ${PROJECT_BINARY_DIR}/plugindirtmp)
set(rscripts ${PROJECT_BINARY_DIR}/scripts)

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

file(MAKE_DIRECTORY ${piddir})
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
  MESSAGE(STATUS "moved  ${scripts}/${TARGET_FILE}")
endforeach()


set(tests_dir ${PROJECT_BINARY_DIR}/tests)
set(SYSTEM_TESTS
  backup-bareos-test
  backup-bareos-test2)

set(BASEPORT 42001)

foreach(TEST_NAME ${SYSTEM_TESTS})
  # base directory for this test
  set(current_test_directory ${PROJECT_BINARY_DIR}/tests/${TEST_NAME})

  #set(basename ${current_test_directory}/)

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

  # common to all tests, set above and commented out here

  #set(hostname ${current_test_directory}/)
  # directories where things are loaded
  #set(backenddir ${current_test_directory}/)
  #set(plugindir ${current_test_directory}/)

  #set(sbindir ${current_test_directory}/sbin)
  #set(bindir ${current_test_directory}/)
  #set(scriptdir ${current_test_directory}/)

  ConfigureFilesToSystemtest("systemtests"  "tests/${TEST_NAME}" "*" @ONLY)

  configure_file("environment.in" "tests/${TEST_NAME}/environment" @ONLY)
  # create a bin/bareos and bin/bconsole script
  # in every testdir for start/stop and bconsole
  file(MAKE_DIRECTORY "tests/${TEST_NAME}/bin")
  configure_file("bin/bconsole" "tests/${TEST_NAME}/bin/bconsole" COPYONLY)
  configure_file("bin/bareos" "tests/${TEST_NAME}/bin/bareos" COPYONLY)

  add_test(NAME "system:${TEST_NAME}"
    COMMAND ${tests_dir}/${TEST_NAME}/testrunner
    WORKING_DIRECTORY ${tests_dir}/${TEST_NAME})
  math(EXPR BASEPORT "${BASEPORT} + 10")

endforeach()


# create symlinks for the binaries

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/dird/bareos-dir ${sbindir}/bareos-dir)

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/dird/bareos-dbcheck ${sbindir}/bareos-dbcheck)

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/stored/bareos-sd ${sbindir}/bareos-sd)

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/filed/bareos-fd ${sbindir}/bareos-fd)

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${PROJECT_BINARY_DIR}/../core/src/console/bconsole ${bindir}/bconsole)

execute_process(COMMAND
  ${CMAKE_COMMAND} -E create_symlink ${scriptdir}/bareos ${bindir}/bareos)
