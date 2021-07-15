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

if(TARGET gfapi-fd
   AND gfapi_fd_host
   AND SUDO
)
  list(APPEND SYSTEM_TESTS gfapi-fd)
  message(
    "SUDO: ${SUDO},  gfapi-fd test settings: gfapi_fd_host=${gfapi_fd_host}, gfapi_fd_testvolume=${gfapi_fd_testvolume}"
  )
  create_systemtest(${SYSTEMTEST_PREFIX} gfapi-fd)
else()
  create_systemtest(${SYSTEMTEST_PREFIX} gfapi-fd DISABLED)
endif()
