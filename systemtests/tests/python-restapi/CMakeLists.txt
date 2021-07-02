#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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
check_pymodule_available(3 fastapi)
check_pymodule_available(3 uvicorn)
find_program(CURL curl)

# python-restapi-test does not work on installed files and needs some modules
if(PYTHON_EXECUTABLE
   AND CURL
   AND PYMODULE_3_FASTAPI_FOUND
   AND PYMODULE_3_UVICORN_FOUND
   AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  set_tests_properties(
    ${SYSTEMTEST_PREFIX}${BASENAME} PROPERTIES LABELS "broken"
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
