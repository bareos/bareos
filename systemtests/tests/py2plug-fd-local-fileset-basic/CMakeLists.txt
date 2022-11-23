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

# if(TARGET python-fd) create_systemtest(${SYSTEMTEST_PREFIX}
# "py2plug-fd-local-fileset-basic") else() create_systemtest(
# ${SYSTEMTEST_PREFIX} "py2plug-fd-local-fileset-basic" DISABLED ) endif()

# if(TARGET python3-fd) create_systemtest(${SYSTEMTEST_PREFIX}
# "py3plug-fd-local-fileset-basic") # Fails with UnicodeEncodeError: 'utf-8'
# codec can't encode characters in # position 133-142: surrogates not allowed
# set_tests_properties( "${SYSTEMTEST_PREFIX}py3plug-fd-local-fileset-basic"
# PROPERTIES LABELS "broken" ) else() create_systemtest( ${SYSTEMTEST_PREFIX}
# "py3plug-fd-local-fileset-basic" DISABLED ) endif()
