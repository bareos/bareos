#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026 Bareos GmbH & Co. KG
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
# webui-common by a previous cmake run.  If it exists, add_systemtest_from_directory
# would (wrongly) treat this as a single-test directory and skip the
# testrunner-* discovery.  Our tests use the testrunner-* naming convention.
file(REMOVE "${PROJECT_BINARY_DIR}/tests/webui-vue/testrunner")

# Detect playwright-python.  The testrunner-playwright is disabled when the
# package is not available so the remaining tests still run in plain CI.
execute_process(
  COMMAND "${PYTHON_EXECUTABLE}" -c "import playwright"
  RESULT_VARIABLE _playwright_missing
  OUTPUT_QUIET ERROR_QUIET
)

# Register the test (and its testrunner-* files) from our source directory.
# This MUST happen before the webui-common configurefilestosystemtest call
# below so that create_systemtest sees our testrunner-* files (not
# webui-common's single testrunner) when it calls add_systemtest_from_directory.
create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})

if(_playwright_missing)
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:playwright" PROPERTIES DISABLED TRUE
  )
else()
  # Playwright and ws-proxy both start a Vite dev server + WS proxy before
  # running their assertions.  The startup overhead pushes them past the default
  # 300 s SYSTEMTEST_TIMEOUT, so give them extra headroom.
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}${BASENAME}:playwright"
    "${SYSTEMTEST_PREFIX}${BASENAME}:ws-proxy"
    PROPERTIES TIMEOUT 600
  )
endif()

# Pull in the shared Bareos daemon configuration from webui-common (director,
# storage, file-daemon configs, catalog, pool, schedule, fileset, etc.).
# This must happen AFTER create_systemtest() so that the testrunner-* file
# glob happens before webui-common's single "testrunner" is copied here.
configurefilestosystemtest(
  "systemtests" "tests/webui-vue" "*" @ONLY "tests/webui-common"
)

# The playwright testrunner also requires the backup job to have completed,
# so it has data to display.  Use systemtest_requires() to append to – not
# replace – the FIXTURES_REQUIRED property set by create_systemtest().
if(NOT _playwright_missing)
  systemtest_requires(playwright simple-backup)
endif()

