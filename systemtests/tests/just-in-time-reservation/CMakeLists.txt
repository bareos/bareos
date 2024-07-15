#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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
  system:just-in-time-reservation:01-create-backup
  PROPERTIES FIXTURES_SETUP
             "system:just-in-time-reservation:backup-job-fixture"
)
set_tests_properties(
  system:just-in-time-reservation:02-restore-backup
  PROPERTIES
    FIXTURES_REQUIRED
    "system:just-in-time-reservation:backup-job-fixture;system:just-in-time-reservation-fixture"
    FIXTURES_SETUP
    "system:just-in-time-reservation:backup-ok-fixture"
)

set_tests_properties(
  system:just-in-time-reservation:03-copy-local
  PROPERTIES
    FIXTURES_REQUIRED
    "system:just-in-time-reservation:backup-ok-fixture;system:just-in-time-reservation-fixture"
    FIXTURES_SETUP
    "system:just-in-time-reservation:copy-job-fixture"
)
set_tests_properties(
  system:just-in-time-reservation:04-restore-local-copy
  PROPERTIES
    FIXTURES_REQUIRED
    "system:just-in-time-reservation:copy-job-fixture;system:just-in-time-reservation-fixture"
    FIXTURES_SETUP
    "system:just-in-time-reservation:copy-ok-fixture"
)

set_tests_properties(
  system:just-in-time-reservation:05-migrate-remote
  PROPERTIES
    FIXTURES_REQUIRED
    "system:just-in-time-reservation:copy-ok-fixture;system:just-in-time-reservation-fixture"
    FIXTURES_SETUP
    "system:just-in-time-reservation:migration-job-fixture"
)
set_tests_properties(
  system:just-in-time-reservation:06-restore-remote-migration
  PROPERTIES
    FIXTURES_REQUIRED
    "system:just-in-time-reservation:migration-job-fixture;system:just-in-time-reservation-fixture"
)
