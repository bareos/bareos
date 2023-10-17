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

find_package(OpenSSL)
# TLSv1.3 support was added in 1.1.1
# so disable the test if openssl is older than that
if(OPENSSL_VERSION VERSION_LESS "1.1.1")
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})

  set_tests_properties(
    system:tls-suites:check-connections
    PROPERTIES FIXTURES_SETUP "system:tls-suites:connections-fixture"
  )
endif()
