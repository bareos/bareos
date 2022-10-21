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

if(TARGET python-fd)
  check_pymodule_available(2 libcloud)
  if(S3CMD
     AND MINIO
     AND PYMODULE_2_LIBCLOUD_FOUND
  )
    create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-libcloud")
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} "py2plug-fd-libcloud" DISABLED COMMENT
      "S3CMD, MINIO or LIBCLOUD MODULE not found"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-libcloud" DISABLED COMMENT
    "requires python-fd (Python2)"
  )
endif()

if(TARGET python3-fd)
  if(NOT Python3_FOUND)
    create_systemtest(
      ${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud" DISABLED COMMENT
      "Python3 not found"
    )
    return()
  endif()
  check_pymodule_available(3 libcloud)
  if(S3CMD
     AND MINIO
     AND PYMODULE_3_LIBCLOUD_FOUND
     AND (${Python3_VERSION_MAJOR} EQUAL 3)
     AND (${Python3_VERSION_MINOR} LESS 10)
  )
    create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud")
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud" DISABLED COMMENT
      "S3CMD, MINIO or LIBCLOUD MODULE not found or Python > 3.7"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud" DISABLED COMMENT
    "requires python3-fd"
  )
endif()
