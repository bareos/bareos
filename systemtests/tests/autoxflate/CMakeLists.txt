#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

# We have two storage daemons with two devices each. The first daemon has
# autoxflate on replication on and the second has autoxflate on replication off.

# We copy from one device to the next on each daemon and also create copies
# between them.

set_tests_properties(
  system:autoxflate:create-backup
  PROPERTIES FIXTURES_SETUP "system:autoxflate:create-backup-fixture"
             FIXTURES_REQUIRED "system:autoxflate-fixture"
)

set_tests_properties(
  system:autoxflate:replication-local
  PROPERTIES FIXTURES_SETUP "system:autoxflate:replication-local-fixture"
             FIXTURES_REQUIRED
             "system:autoxflate:create-backup-fixture;system:autoxflate-fixture"
)

set_tests_properties(
  system:autoxflate:copy-to-offsite
  PROPERTIES
    FIXTURES_SETUP "system:autoxflate:copy-to-offsite-fixture"
    FIXTURES_REQUIRED
    "system:autoxflate:replication-local-fixture;system:autoxflate-fixture"
)

set_tests_properties(
  system:autoxflate:replication-offsite
  PROPERTIES
    FIXTURES_SETUP "system:autoxflate:replication-offsite-fixture"
    FIXTURES_REQUIRED
    "system:autoxflate:copy-to-offsite-fixture;system:autoxflate-fixture"
)

set_tests_properties(
  system:autoxflate:copy-to-local
  PROPERTIES
    FIXTURES_REQUIRED
    "system:autoxflate:replication-offsite-fixture;system:autoxflate-fixture"
)
