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
if(TARGET python3-fd)
  check_pymodule_available(3 libcloud)
  if(S3CMD
     AND MINIO
     AND PYMODULE_3_LIBCLOUD_FOUND
     AND ("${Python3_VERSION_MAJOR}" EQUAL 3)
  )
    create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
      "S3CMD, MINIO or LIBCLOUD MODULE not found or Python != 3.x"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "requires python3-fd"
  )
endif()
