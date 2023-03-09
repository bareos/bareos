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
  system:bareos-basic:simple-backup-and-restore
  PROPERTIES FIXTURES_SETUP "system:bareos-basic:backup-job-fixture"
)
set_tests_properties(
  system:bareos-basic:status-subscriptions
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic:backup-job-fixture;system:bareos-basic-fixture"
)
set_tests_properties(
  system:bareos-basic:bls-autoxflate
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic:backup-job-fixture;system:bareos-basic-fixture"
)
set_tests_properties(
  system:bareos-basic:bextract-autoxflate
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic:backup-job-fixture;system:bareos-basic-fixture"
)
set_tests_properties(
  system:bareos-basic:bscan-autoxflate
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic:backup-job-fixture;system:bareos-basic-fixture"
)
set_tests_properties(
  system:bareos-basic:bcopy-autoxflate
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic:backup-job-fixture;system:bareos-basic-fixture"
)
set_tests_properties(
  system:bareos-basic:filesettext-is-updated
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic:backup-job-fixture;system:bareos-basic-fixture"
)
set_tests_properties(
  system:bareos-basic:check-size
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic-fixture"
  DEPENDS system:bareos-basic:filesettext-is-updated
)
set_tests_properties(
  system:bareos-basic:check-strip
  PROPERTIES
    FIXTURES_REQUIRED
    "system:bareos-basic-fixture"
  DEPENDS system:bareos-basic:filesettext-is-updated
)
