#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2024 Bareos GmbH & Co. KG
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

macro(create_systemtests_directory)
  configurefilestosystemtest("systemtests" "data" "*.tgz" COPYONLY "")
  configurefilestosystemtest("systemtests" "data" "*.gz" COPYONLY "")

  configurefilestosystemtest("systemtests" "scripts" "functions" @ONLY "")
  configurefilestosystemtest("systemtests" "scripts" "cleanup" @ONLY "")
  configurefilestosystemtest("systemtests" "scripts" "mysql.sh" @ONLY "")
  configurefilestosystemtest(
    "systemtests" "scripts" "run_python_unittests.sh" @ONLY ""
  )
  configurefilestosystemtest("systemtests" "scripts" "webui.sh" @ONLY "")
  configurefilestosystemtest("systemtests" "scripts" "setup" @ONLY "")
  configurefilestosystemtest("systemtests" "scripts" "start_bareos.sh" @ONLY "")
  configurefilestosystemtest("systemtests" "scripts" "start_minio.sh" @ONLY "")
  configurefilestosystemtest("systemtests" "scripts" "stop_minio.sh" @ONLY "")
  configurefilestosystemtest(
    "systemtests" "scripts" "create_sparse_file.sh" @ONLY ""
  )
  configurefilestosystemtest(
    "systemtests" "scripts" "check_for_zombie_jobs" @ONLY ""
  )
  configurefilestosystemtest("systemtests" "scripts" "diff.pl.in" @ONLY "")
  configurefilestosystemtest(
    "systemtests" "scripts" "generate_minio_certs.sh.in" @ONLY ""
  )
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/systemtests/tls/minio/)

  configurefilestosystemtest("core/src" "defaultconfigs" "*.conf" @ONLY "")
  configurefilestosystemtest("core/src" "defaultconfigs" "*.in" @ONLY "")

  configurefilestosystemtest("core" "scripts" "*.in" @ONLY "")
  configurefilestosystemtest("core" "scripts" "bareos-ctl-funcs" @ONLY "")
  configurefilestosystemtest("core" "scripts" "btraceback.gdb" @ONLY "")

  configurefilestosystemtest("core/src/cats" "scripts/ddl" "*" @ONLY "ddl")
  configurefilestosystemtest("core/src" "scripts" "*_catalog_*" @ONLY "cats")
  configurefilestosystemtest("core/src" "scripts" "*_bareos_*" @ONLY "cats")

  configurefilestosystemtest("core/src" "console" "*.in" @ONLY "")

  file(MAKE_DIRECTORY ${subsysdir})
  file(MAKE_DIRECTORY ${sbindir})
  file(MAKE_DIRECTORY ${bindir})
  file(MAKE_DIRECTORY ${scripts})
  file(MAKE_DIRECTORY ${working})
  file(MAKE_DIRECTORY ${archivedir})
endmacro()

# create a variable BINARY_NAME_TO_TEST for each binary name bareos-dir ->
# BAREOS_DIR_TO_TEST bconsole -> BCONSOLE_TO_TEST
macro(create_variable_binary_name_to_test_for_binary_name binary_name)
  string(TOUPPER ${binary_name} binary_name_to_test_upcase)
  string(REPLACE "-" "_" binary_name_to_test_upcase
                 ${binary_name_to_test_upcase}
  )
  string(APPEND binary_name_to_test_upcase _TO_TEST)
endmacro()

# find the full path of the given binary when *compiling* the software and
# create and set the BINARY_NAME_TO_TEST variable to the full path of it
macro(find_compiled_binary_and_set_binary_name_to_test_variable_for binary_name)
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

macro(find_systemtests_binary_paths SYSTEMTESTS_BINARIES)
  if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)

    foreach(BINARY ${SYSTEMTESTS_BINARIES})
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

    get_filename_component(
      PLUGINS_DIR_TO_TEST ${PYTHON_PLUGIN_TO_TEST} DIRECTORY
    )
    get_filename_component(
      PYTHON_PLUGINS_DIR_TO_TEST ${PYTHON_PLUGINS_DIR_TO_TEST} DIRECTORY
    )
    get_filename_component(
      SCRIPTS_DIR_TO_TEST ${CREATE_BAREOS_DATABASE_TO_TEST} DIRECTORY
    )

    set(FD_PLUGINS_DIR_TO_TEST ${PLUGINS_DIR_TO_TEST})
    set(SD_PLUGINS_DIR_TO_TEST ${PLUGINS_DIR_TO_TEST})
    set(DIR_PLUGINS_DIR_TO_TEST ${PLUGINS_DIR_TO_TEST})
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

    set(SCRIPTS_DIR_TO_TEST ${CMAKE_BINARY_DIR}/core/scripts)
    set(WEBUI_PUBLIC_DIR_TO_TEST ${PROJECT_SOURCE_DIR}/../webui/public)

  endif()

  cmake_print_variables(FD_PLUGINS_DIR_TO_TEST)
  cmake_print_variables(SD_PLUGINS_DIR_TO_TEST)
  cmake_print_variables(DIR_PLUGINS_DIR_TO_TEST)
  cmake_print_variables(SD_BACKEND_DIR_TO_TEST)
  cmake_print_variables(WEBUI_PUBLIC_DIR_TO_TEST)

endmacro()

function(configurefilestosystemtest srcbasedir dirname globexpression
         configure_option srcdirname
)
  if(srcdirname STREQUAL "")
    set(srcdirname "${dirname}")
  endif()
  set(COUNT 1)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${dirname})
  file(
    GLOB_RECURSE ALL_FILES
    RELATIVE "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/${srcbasedir}/${srcdirname}/${globexpression}"
  )
  foreach(TARGET_FILE ${ALL_FILES})
    math(EXPR COUNT "${COUNT}+1")
    set(CURRENT_FILE "${CMAKE_SOURCE_DIR}/")
    string(APPEND CURRENT_FILE ${TARGET_FILE})

    # do not mess with .ini files
    string(REGEX REPLACE ".in$" "" TARGET_FILE "${TARGET_FILE}")

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
      file(CREATE_LINK ${CURRENT_FILE}
           ${PROJECT_BINARY_DIR}/${dirname}/${TARGET_FILE} SYMBOLIC
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
  if(${python_version} EQUAL 3)
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

macro(check_for_pamtest)
  message(STATUS "Looking for pam test requirements ...")
  bareosfindlibraryandheaders("pam" "security/pam_appl.h" "")
  bareosfindlibrary("pam_wrapper")
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
     AND PYTHON_EXECUTABLE
  )
    set(ENABLE_BCONSOLE_PAM_TEST TRUE)
    message(STATUS "OK: all requirements for pam tests were met.")
  else()
    set(ENABLE_BCONSOLE_PAM_TEST FALSE)
    message(
      STATUS "NOT OK: disabling pam tests as not all requirements were found."
    )
  endif()
endmacro()

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
    file(CREATE_LINK ${${binary_name_to_test_upcase}} ${${bareos_XXX_binary}}
         SYMBOLIC
    )
  endforeach()
  file(CREATE_LINK ${scriptdir}/btraceback ${CURRENT_SBIN_DIR}/btraceback
       SYMBOLIC
  )

  if(TARGET bareos_vadp_dumper)
    if(RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
      file(CREATE_LINK "/usr/sbin/bareos_vadp_dumper_wrapper.sh"
           "${CURRENT_SBIN_DIR}/bareos_vadp_dumper_wrapper.sh" SYMBOLIC
      )
    else()
      file(
        CREATE_LINK
        "${CMAKE_SOURCE_DIR}/core/src/vmware/vadp_dumper/bareos_vadp_dumper_wrapper.sh"
        "${CURRENT_SBIN_DIR}/bareos_vadp_dumper_wrapper.sh"
        SYMBOLIC
      )

    endif()
    file(CREATE_LINK "${CURRENT_SBIN_DIR}/bareos_vadp_dumper-${TEST_NAME}"
         "${CURRENT_SBIN_DIR}/bareos_vadp_dumper" SYMBOLIC
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

  set(sd_backenddir ${SD_BACKEND_DIR_TO_TEST})
  # the SD will not suppot the BackendDirectory setting if it was not built with
  # HAVE_DYNAMIC_SD_BACKENDS, thus we declare `sd_backend_config` that will
  # evaluate to nothing if we don't have dynamic backends enabled.
  if(HAVE_DYNAMIC_SD_BACKENDS)
    set(sd_backend_config "BackendDirectory = \"${SD_BACKEND_DIR_TO_TEST}\"")
  else()
    set(sd_backend_config "")
  endif()
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

  file(MAKE_DIRECTORY ${tmpdir})
  file(MAKE_DIRECTORY ${archivedir})
  file(MAKE_DIRECTORY ${logdir})
  file(MAKE_DIRECTORY ${confdir})
  file(MAKE_DIRECTORY ${working_dir})
  file(MAKE_DIRECTORY ${config_directory_dir_additional_test_config})

  # create a bin/bareos and bin/bconsole script in every testdir for start/stop
  # and bconsole
  file(MAKE_DIRECTORY "${current_test_directory}/bin")
  file(CREATE_LINK "${PROJECT_SOURCE_DIR}/bin/bconsole"
       "${current_test_directory}/bin/bconsole" SYMBOLIC
  )
  file(CREATE_LINK "${PROJECT_SOURCE_DIR}/bin/bareos"
       "${current_test_directory}/bin/bareos" SYMBOLIC
  )

  set(CURRENT_SBIN_DIR ${current_test_directory}/sbin)
  file(MAKE_DIRECTORY ${CURRENT_SBIN_DIR})

  link_binaries_to_test_to_current_sbin_dir_with_individual_filename()
endmacro()

macro(prepare_test_python)
  string(REGEX MATCH "py2plug" py_v2 "${TEST_NAME}")
  string(REGEX MATCH "py3plug" py_v3 "${TEST_NAME}")
  # use python3 by default, exepts the name of test starts with py2plug.
  set(python_module_name python3)
  if(py_v2)
    set(python_module_name python)
  endif()
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
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/mariabackup:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/postgres:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/postgresql:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/filed/python/pyfiles:"
        "${CMAKE_SOURCE_DIR}/contrib/fd-plugins:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/stored/python/pyfiles:"
        "${CMAKE_SOURCE_DIR}/core/src/plugins/dird/python/pyfiles:"
        "${CMAKE_BINARY_DIR}/core/src/plugins/filed/python/${python_module_name}modules:"
        "${CMAKE_BINARY_DIR}/core/src/plugins/stored/python/${python_module_name}modules:"
        "${CMAKE_BINARY_DIR}/core/src/plugins/dird/python/${python_module_name}modules:"
        "${CMAKE_SOURCE_DIR}/systemtests/python-modules:"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/${TEST_NAME}/python-modules"
    )
  endif()
endmacro()

macro(prepare_test test_subdir)
  set(TEST_NAME ${test_subdir})
  # base directory for this test
  set(current_test_directory ${PROJECT_BINARY_DIR}/tests/${TEST_NAME})
  set(current_test_source_directory ${PROJECT_SOURCE_DIR}/tests/${TEST_NAME})
  set(basename ${TEST_NAME})

  # db parameters

  # db_name is regress-${TEST_NAME} replacing - by _
  string(REPLACE "-" "_" db_name "regress-${TEST_NAME}")
  # set(db_name "regress-${TEST_NAME}")
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
  math(EXPR fd2_port "${BASEPORT} + 3")
  math(EXPR sd2_port "${BASEPORT} + 4")
  math(EXPR php_port "${BASEPORT} + 5")
  math(EXPR test_db_port "${BASEPORT} + 6")
  math(EXPR minio_port "${BASEPORT} + 7")
  math(EXPR restapi_port "${BASEPORT} + 8")

  prepare_testdir_for_daemon_run()

  # skip for tests without etc/bareos ("catalog" test)
  if(EXISTS ${current_test_source_directory}/etc/bareos)
    file(CREATE_LINK ${CMAKE_SOURCE_DIR}/core/scripts/mtx-changer.conf
         ${current_test_directory}/etc/bareos/mtx-changer.conf SYMBOLIC
    )
  endif()

  prepare_test_python()
endmacro()

function(add_disabled_systemtest PREFIX TEST_NAME)
  set(FULL_TEST_NAME "${PREFIX}${TEST_NAME}")
  cmake_parse_arguments(PARSE_ARGV 2 ARG "DISABLED" "COMMENT" "")
  if(NOT TEST ${FULL_TEST_NAME})
    add_test(NAME ${FULL_TEST_NAME} COMMAND false)
  endif()
  set_tests_properties(${FULL_TEST_NAME} PROPERTIES DISABLED true)
  message(STATUS "✘ ${FULL_TEST_NAME} => disabled (${ARG_COMMENT})")
endfunction()

function(add_systemtest name file)
  cmake_parse_arguments(PARSE_ARGV 2 ARG "PYTHON" "WORKING_DIRECTORY" "")
  message(STATUS "     └─ ✓ ${name}")

  if(ARG_WORKING_DIRECTORY)
    set(directory "${ARG_WORKING_DIRECTORY}")
  else()
    get_filename_component(directory ${file} DIRECTORY)
  endif()

  if(ARG_PYTHON)
    get_filename_component(filename_without_ext ${file} NAME_WE)
    add_test(
      NAME ${name}
      COMMAND ${PROJECT_BINARY_DIR}/scripts/run_python_unittests.sh
              ${filename_without_ext}
      WORKING_DIRECTORY ${directory}
    )
  else()
    add_test(
      NAME ${name}
      COMMAND ${file}
      WORKING_DIRECTORY ${directory}
    )
  endif()
  set_tests_properties(
    ${name} PROPERTIES TIMEOUT "${SYSTEMTEST_TIMEOUT}" COST 1.0
                       SKIP_RETURN_CODE 77
  )
endfunction()

function(add_systemtest_from_directory tests_basedir prefix test_subdir)
  set(test_dir "${tests_basedir}/${test_subdir}")
  set(test_basename "${prefix}${test_subdir}")

  if(EXISTS ${test_dir}/testrunner)
    # single test directory
    add_systemtest(${test_basename} ${test_dir}/testrunner)
    return()
  endif()

  #
  # Multiple tests in this directory.
  #
  if(NOT EXISTS "${test_dir}/test-setup")
    file(CREATE_LINK "${PROJECT_BINARY_DIR}/scripts/start_bareos.sh"
         "${test_dir}/test-setup" SYMBOLIC
    )
  endif()
  add_systemtest("${test_basename}:setup" "${test_dir}/test-setup")

  set_tests_properties(
    "${test_basename}:setup" PROPERTIES FIXTURES_SETUP
                                        "${test_basename}-fixture"
  )

  # add all scripts named "testrunner-*" as tests.
  file(
    GLOB all_tests
    LIST_DIRECTORIES false
    RELATIVE "${test_dir}"
    CONFIGURE_DEPENDS "${test_dir}/testrunner-*"
  )
  foreach(testfilename ${all_tests})
    string(REPLACE "testrunner-" "" test ${testfilename})
    add_systemtest(${test_basename}:${test} ${test_dir}/${testfilename})
    set_tests_properties(
      "${test_basename}:${test}"
      PROPERTIES FIXTURES_REQUIRED
                 "${test_basename}-fixture"
                 # add a SETUP fixture, so we can express ordering requirements
                 FIXTURES_SETUP
                 "${test_basename}/${test}-fixture"
                 # use RESOURCE_LOCK to run tests sequential
                 RESOURCE_LOCK
                 "${test_basename}-lock"
    )
  endforeach()

  # add all Python unittests named "test_*.py*" as tests.
  file(
    GLOB all_tests
    LIST_DIRECTORIES false
    RELATIVE "${test_dir}"
    CONFIGURE_DEPENDS "${test_dir}/test_*.py"
  )
  foreach(testfilename ${all_tests})
    string(REPLACE ".py" "" test0 ${testfilename})
    string(REPLACE "test_" "" test ${test0})
    add_systemtest(${test_basename}:${test} ${test_dir}/${testfilename} PYTHON)
    set_tests_properties(
      "${test_basename}:${test}"
      PROPERTIES FIXTURES_REQUIRED
                 "${test_basename}-fixture"
                 # add a SETUP fixture, so we can express ordering requirements
                 FIXTURES_SETUP
                 "${test_basename}/${test}-fixture"
                 # use RESOURCE_LOCK to run tests sequential
                 RESOURCE_LOCK
                 "${test_basename}-lock"
    )
  endforeach()

  if(NOT EXISTS ${test_dir}/test-cleanup)
    file(CREATE_LINK "${PROJECT_BINARY_DIR}/scripts/cleanup"
         "${test_dir}/test-cleanup" SYMBOLIC
    )
  endif()
  add_systemtest(${test_basename}:cleanup "${test_dir}/test-cleanup")

  set_tests_properties(
    ${test_basename}:cleanup PROPERTIES FIXTURES_CLEANUP
                                        "${test_basename}-fixture"
  )

endfunction()

macro(create_systemtest prefix test_subdir)
  # cmake-format: off
  #
  # Parameter:
  #   * prefix STRING
  #   * test_subdir STRING
  #   * DISABLED option
  #   * COMMENT "..." (optional)
  #
  # Made as a macro, not as a function to be able to update BASEPORT.
  #
  # cmake-format: on
  cmake_parse_arguments(ARG "DISABLED" "COMMENT" "" ${ARGN})
  set(test_basename "${prefix}${test_subdir}")
  if(ARG_DISABLED)
    add_disabled_systemtest(${prefix} ${test_subdir} COMMENT "${ARG_COMMENT}")
  else()
    message(STATUS "✓ ${test_basename} (baseport=${BASEPORT})")

    prepare_test(${test_subdir})
    configurefilestosystemtest(
      "systemtests" "tests/${test_subdir}" "*" @ONLY ""
    )

    configure_file(
      "${PROJECT_SOURCE_DIR}/environment.in"
      "${PROJECT_BINARY_DIR}/tests/${test_subdir}/environment" @ONLY
    )

    add_systemtest_from_directory(${tests_dir} ${prefix} ${test_subdir})

    # Increase global BASEPORT variable
    math(EXPR BASEPORT "${BASEPORT} + 10")
    set(BASEPORT
        "${BASEPORT}"
        PARENT_SCOPE
    )
  endif()
endmacro()

function(systemtest_requires test required_test)
  get_filename_component(basename ${CMAKE_CURRENT_BINARY_DIR} NAME)
  get_test_property(
    "${SYSTEMTEST_PREFIX}${basename}:${test}" FIXTURES_REQUIRED _fixtures
  )
  list(APPEND _fixtures
       "${SYSTEMTEST_PREFIX}${basename}/${required_test}-fixture"
  )
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${basename}:${test}" PROPERTIES FIXTURES_REQUIRED
                                                         "${_fixtures}"
  )
endfunction()
