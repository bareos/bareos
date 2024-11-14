#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2024 Bareos GmbH & Co. KG
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

get_test_property(${SYSTEMTEST_PREFIX}${BASENAME} DISABLED ISDISABLED)
if (NOT ISDISABLED)
  set_tests_properties(
    system:always-incremental-consolidate:01-full+incr
    PROPERTIES FIXTURES_SETUP
    "system:always-incremental-consolidate:full+incr-fixture"
    FIXTURES_REQURIED "system:always-incremental-consolidate-fixture"
  )
  set_tests_properties(
    system:always-incremental-consolidate:02-empty-incrementals
    PROPERTIES
    FIXTURES_REQUIRED
    "system:always-incremental-consolidate:full+incr-fixture;system:always-incremental-consolidate-fixture"
    FIXTURES_SETUP
    "system:always-incremental-consolidate:empty-incrementals-fixture"
  )
  set_tests_properties(
    system:always-incremental-consolidate:03-zero-file
    PROPERTIES
    FIXTURES_REQUIRED
    "system:always-incremental-consolidate:full+incr-fixture;system:always-incremental-consolidate-fixture"
  )
  set_tests_properties(
    system:always-incremental-consolidate:04-virtualfull
    PROPERTIES
    FIXTURES_REQUIRED
    "system:always-incremental-consolidate:full+incr-fixture;system:always-incremental-consolidate-fixture"
  )
  set_tests_properties(
    system:always-incremental-consolidate:05-priority
    PROPERTIES
    FIXTURES_REQUIRED
    "system:always-incremental-consolidate:full+incr-fixture;system:always-incremental-consolidate-fixture"
  )
  set_tests_properties(
    system:always-incremental-consolidate:06-consolidate-duplicates
    PROPERTIES
    FIXTURES_REQUIRED
    "system:always-incremental-consolidate:full+incr-fixture;system:always-incremental-consolidate-fixture"
  )
endif()
