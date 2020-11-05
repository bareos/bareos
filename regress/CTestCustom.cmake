#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2014-2018 Bareos GmbH & Co. KG
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

set(
  CTEST_CUSTOM_ERROR_EXCEPTION
  ${CTEST_CUSTOM_ERROR_EXCEPTION}
  "ERROR: *role \".*\" already exists"
  "ERROR: *database \".*\" already exists"
  "ERROR: *table \".*\" does not exist"
  "NOTICE: *table \".*\" does not exist, skipping"
  "NOTICE: .*will create implicit sequence"
  "NOTICE: .*will create implicit index"
)

set(
  CTEST_CUSTOM_WARNING_EXCEPTION
  ${CTEST_CUSTOM_WARNING_EXCEPTION}
  "libtool: install: warning: relinking .*"
  "libtool: link: warning: .* seems to be moved"
  "libtool: relink: warning: .* seems to be moved"
  "libtool: warning: relinking .*"
)

set(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 100000)
set(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 1048576)
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 500)

set(CTEST_NIGHTLY_START_TIME "23:00:00 CET")

set(CTEST_CUSTOM_PRE_TEST scripts/pretest)
