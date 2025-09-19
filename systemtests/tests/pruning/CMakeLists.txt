#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2025 Bareos GmbH & Co. KG
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

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  set(global_fileset_options "XAttr Support = No")
else()
  set(global_fileset_options "")
endif()

create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})

# have each test require the previous one (sorted alphabetically) via fixtures
add_alphabetic_requirements(${SYSTEMTEST_PREFIX} ${BASENAME})
