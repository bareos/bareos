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

# requires deprecated directive 'Block Size (Sd -> Device)'
set(IGNORE_DAEMON_CONFIG_WARNINGS CACHE INTERNAL "")

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
if(AUTOCHANGER_TEST_ENABLED)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  # Make sure these tests run sequentially.
  set_tests_properties(
    ${SYSTEMTEST_PREFIX}block-size
    PROPERTIES
      RESOURCE_LOCK
      autochanger_resource
      DEPENDS
      "${SYSTEMTEST_PREFIX}autochanger:cleanup;${SYSTEMTEST_PREFIX}scsicrypto:cleanup"
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
