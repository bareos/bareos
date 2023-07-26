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
  system:restore:create-backup PROPERTIES FIXTURES_SETUP
                                          "system:restore:backup-job-fixture"
)
set_tests_properties(
  system:restore:archive-restore-file
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:archive-restore-dir
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:archive-full-restore
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:restore-file
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:restore-dir
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:full-restore
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:check-hints
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:multi
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:restore-old-archive
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
set_tests_properties(
  system:restore:error-full-restore
  PROPERTIES FIXTURES_REQUIRED
             "system:restore:backup-job-fixture;system:restore-fixture"
)
