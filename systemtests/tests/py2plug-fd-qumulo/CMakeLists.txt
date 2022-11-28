#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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

check_pymodule_available(2 concurrent.futures)
check_pymodule_available(2 configparser)

if(TARGET python-fd
   AND qumulo_host
   AND PYMODULE_2_CONCURRENT.FUTURES_FOUND
   AND PYMODULE_2_CONFIGPARSER
)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-qumulo")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX}
    "py2plug-fd-qumulo"
    DISABLED
    COMMENT
    "no python-fd or qumulo_host not set or pymodule futures of configparser missing"
  )
endif()

if(TARGET python3-fd AND qumulo_host)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-qumulo")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-qumulo" DISABLED COMMENT
    "no python3-fd or qumulo_host not set"
  )
endif()
