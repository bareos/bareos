#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2022 Bareos GmbH & Co. KG
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
  system:copy-migrate:01-backup
  PROPERTIES FIXTURES_SETUP "system:copy-migrate:01-backup" FIXTURES_REQUIRED
             "system:copy-migrate-fixture"
)
set_tests_properties(
  system:copy-migrate:02-copy
  PROPERTIES FIXTURES_SETUP "system:copy-migrate:02-copy" FIXTURES_REQUIRED
             "system:copy-migrate:01-backup;system:copy-migrate-fixture"
)
set_tests_properties(
  system:copy-migrate:03-impossible-copy
  PROPERTIES
    FIXTURES_SETUP
    "system:copy-migrate:03-impossible-copy"
    FIXTURES_REQUIRED
    "system:copy-migrate:02-copy;system:copy-migrate:01-backup;system:copy-migrate-fixture"
)
set_tests_properties(
  system:copy-migrate:04-bscan
  PROPERTIES
    FIXTURES_SETUP
    "system:copy-migrate:04-bscan"
    FIXTURES_REQUIRED
    "system:copy-migrate:03-impossible-copy;system:copy-migrate:02-copy;system:copy-migrate:01-backup;system:copy-migrate-fixture"
)

set_tests_properties(
  system:copy-migrate:05-migrate
  PROPERTIES
    FIXTURES_SETUP
    "system:copy-migrate:05-migrate"
    FIXTURES_REQUIRED
    "system:copy-migrate:04-bscan;system:copy-migrate:03-impossible-copy;system:copy-migrate:02-copy;system:copy-migrate:01-backup;system:copy-migrate-fixture"
)
