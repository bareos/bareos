#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2023 Bareos GmbH & Co. KG
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
create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})

set_tests_properties(
  system:restore-test:create-backup
  PROPERTIES FIXTURES_SETUP "system:restore-test:backup-job-fixture"
)
set_tests_properties(
  system:restore-test:archive-restore-file
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:archive-restore-dir
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:archive-full-restore
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:restore-file
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:restore-dir
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:full-restore
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:check-hints
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
set_tests_properties(
  system:restore-test:restore-old-archive
  PROPERTIES
    FIXTURES_REQUIRED
    "system:restore-test:backup-job-fixture;system:restore-test-fixture"
)
