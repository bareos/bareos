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
  system:copy:01-backup PROPERTIES FIXTURES_SETUP "system:copy:01-backup"
                                   FIXTURES_REQUIRED "system:copy-fixture"
)
set_tests_properties(
  system:copy:02-copy
  PROPERTIES FIXTURES_SETUP "system:copy:02-copy" FIXTURES_REQUIRED
             "system:copy:01-backup;system:copy-fixture"
)
set_tests_properties(
  system:copy:03-impossible-copy
  PROPERTIES FIXTURES_SETUP "system:copy:03-impossible-copy" FIXTURES_REQUIRED
             "system:copy:02-copy;system:copy:01-backup;system:copy-fixture"
)
set_tests_properties(
  system:copy:04-bscan
  PROPERTIES
    FIXTURES_SETUP
    "system:copy:04-bscan"
    FIXTURES_REQUIRED
    "system:copy:03-impossible-copy;system:copy:02-copy;system:copy:01-backup;system:copy-fixture"
)
