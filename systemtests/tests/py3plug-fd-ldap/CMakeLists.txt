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

set(SYSTEMTEST_LDAP_ADDRESS
    ""
    CACHE STRING "address of LDAP server for Plugin test"
)
set(SYSTEMTEST_LDAP_BASEDN
    "dc=example,dc=org"
    CACHE STRING "LDAP base DN for plugin test"
)
set(SYSTEMTEST_LDAP_BINDDN
    "cn=admin,dc=example,dc=org"
    CACHE STRING "LDAP bind DN for plugin test"
)
set(SYSTEMTEST_LDAP_PASSWORD
    "admin"
    CACHE STRING "LDAP bind password for plugin test"
)

if(TARGET python3-fd)
  check_pymodule_available(3 ldap)
  if(PYMODULE_3_LDAP_FOUND AND SYSTEMTEST_LDAP_ADDRESS)
    string(REPLACE ":" "\\\\:" SYSTEMTEST_LDAP_ADDRESS_ESCAPED
                   ${SYSTEMTEST_LDAP_ADDRESS}
    )
    cmake_print_variables(SYSTEMTEST_LDAP_ADDRESS_ESCAPED)
    create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  else()
    create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
  endif()
endif()
