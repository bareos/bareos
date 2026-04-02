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

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

# Remove any stale "testrunner" (no suffix) that may have been copied from
# webui-common by a previous cmake run.  If it exists,
# add_systemtest_from_directory would (wrongly) treat this as a single-test
# directory and skip the testrunner-* discovery.  Our tests use the testrunner-*
# naming convention.
file(REMOVE "${PROJECT_BINARY_DIR}/tests/webui-vue/testrunner")

# Detect playwright-python.  The testrunner-playwright is disabled when the
# package is not available so the remaining tests still run in plain CI.
check_pymodule_available(3 playwright)
check_pymodule_available(3 websockets)

# Register the test (and its testrunner-* files) from our source directory. This
# MUST happen before the webui-common configurefilestosystemtest call below so
# that create_systemtest sees our testrunner-* files (not webui-common's single
# testrunner) when it calls add_systemtest_from_directory.
create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})

if(NOT PYMODULE_3_WEBSOCKETS_FOUND)
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:ws-proxy" PROPERTIES DISABLED TRUE
  )
endif()

if(NOT PYMODULE_3_PLAYWRIGHT_FOUND)
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:playwright" PROPERTIES DISABLED TRUE
  )
endif()

# The tape-backup testrunner is disabled: it caused intermittent failures
# in the CI testing-full stage. The likely root cause is that autochanger
# tests must not run in parallel (multiple tests competing for the same
# physical changer/drives). Re-enable once the tests are serialized
# (e.g. via RUN_SERIAL or a dedicated resource lock) and the CI
# autochanger environment is stable.
set_tests_properties(
  "${SYSTEMTEST_PREFIX}${BASENAME}:tape-backup" PROPERTIES DISABLED TRUE
)
if(changer-device)
  # Create the mtx-changer.conf symlink in the build dir so the SD can
  # call the changer script.  The parent etc/bareos dir is created by
  # create_systemtest() via configurefilestosystemtest() below.
  file(
    CREATE_LINK ${CMAKE_SOURCE_DIR}/core/scripts/mtx-changer.conf
    ${CMAKE_BINARY_DIR}/systemtests/tests/webui-vue/etc/bareos/mtx-changer.conf
    SYMBOLIC
  )
endif()

if(PYMODULE_3_PLAYWRIGHT_FOUND AND PYMODULE_3_WEBSOCKETS_FOUND)
  # Playwright and ws-proxy both start a Vite dev server + WS proxy before
  # running their assertions.  The startup overhead pushes them past the default
  # 300 s SYSTEMTEST_TIMEOUT, so give them extra headroom.
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:playwright"
    "${SYSTEMTEST_PREFIX}${BASENAME}:ws-proxy" PROPERTIES TIMEOUT 600
  )
elseif(PYMODULE_3_PLAYWRIGHT_FOUND)
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:playwright" PROPERTIES TIMEOUT 600
  )
elseif(PYMODULE_3_WEBSOCKETS_FOUND)
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:ws-proxy" PROPERTIES TIMEOUT 600
  )
endif()

# Pull in the shared Bareos daemon configuration from webui-common (director,
# storage, file-daemon configs, catalog, pool, schedule, fileset, etc.). This
# must happen AFTER create_systemtest() so that the testrunner-* file glob
# happens before webui-common's single "testrunner" is copied here.
configurefilestosystemtest(
  "systemtests" "tests/webui-vue" "*" @ONLY "tests/webui-common"
)

# Create the mtx-changer.conf symlink after configurefilestosystemtest() has
# created the etc/bareos build-dir directory.
if(changer-device)
  file(
    CREATE_LINK ${CMAKE_SOURCE_DIR}/core/scripts/mtx-changer.conf
    ${CMAKE_BINARY_DIR}/systemtests/tests/webui-vue/etc/bareos/mtx-changer.conf
    SYMBOLIC
  )
endif()

# The playwright testrunner also requires the backup job to have completed, so
# it has data to display.  Use systemtest_requires() to append to – not replace
# – the FIXTURES_REQUIRED property set by create_systemtest().
if(PYMODULE_3_PLAYWRIGHT_FOUND)
  systemtest_requires(playwright create-backups)
  systemtest_requires(playwright load-subscriptiontestdata)
endif()
