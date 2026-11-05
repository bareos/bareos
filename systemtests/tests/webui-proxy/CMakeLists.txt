#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

set(proxy_test_basename "${SYSTEMTEST_PREFIX}webui-proxy")
if(NOT PYTHON_EXECUTABLE
   OR HAVE_WIN32
   OR RUN_SYSTEMTESTS_ON_INSTALLED_FILES
   OR NOT TARGET bareos-webui-proxy-startup
)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} webui-proxy DISABLED COMMENT
    "requires Python and the proxy startup test target"
  )
  return()
endif()

prepare_test(webui-proxy webui-proxy webui-proxy)
# Reuse the real Director and PAM resources, not the browser/Vue fixture.
configurefilestosystemtest(
  "systemtests" "tests/webui-proxy/etc/bareos" "*" @ONLY
  "tests/python-pam/etc/bareos"
)
configurefilestosystemtest(
  "systemtests" "tests/webui-proxy" "*" @ONLY "tests/webui-proxy"
)
configure_file(
  "${PROJECT_SOURCE_DIR}/environment.in"
  "${current_test_directory}/environment" @ONLY
)
file(
  GENERATE
  OUTPUT "${current_test_directory}/startup-binary"
  CONTENT "$<TARGET_FILE:bareos-webui-proxy-startup>\n"
)

foreach(mode configured defaults pam)
  set(test_name "${proxy_test_basename}:${mode}")
  add_systemtest("${test_name}" "${current_test_directory}/testrunner")
  set_tests_properties(
    "${test_name}" PROPERTIES ENVIRONMENT "PROXY_TEST_MODE=${mode}"
                              RESOURCE_LOCK "webui-proxy-fixture"
  )
endforeach()
# Existing address-binding unit tests also use the built-in Director port.
set_tests_properties(
  "${proxy_test_basename}:defaults"
  PROPERTIES RUN_SERIAL TRUE RESOURCE_LOCK
             "webui-proxy-fixture;webui-proxy-default-ports"
)
if(NOT ENABLE_BCONSOLE_PAM_TEST)
  set_tests_properties("${proxy_test_basename}:pam" PROPERTIES DISABLED TRUE)
endif()
math(EXPR BASEPORT "${BASEPORT} + 10")
set(BASEPORT
    "${BASEPORT}"
    PARENT_SCOPE
)
