#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2026 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

include(BareosConfigureFile)
bareos_configure_file(
  FILES bareos-config-lib.sh.in bareos-config.in btraceback.in disk-changer.in
  logrotate.in mtx-changer.in mtx-logrotate.in
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/btraceback
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${sbindir}"
)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/bareos-config-lib.sh btraceback.gdb
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  DESTINATION "${scriptdir}"
)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/bareos-config
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${scriptdir}"
)
if(NOT client-only)
  install(
    FILES btraceback.dbx btraceback.mdb
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DESTINATION "${scriptdir}"
  )
endif()
if(TARGET python3-fd)
  install(
    FILES bareos_encode_string.py
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    DESTINATION "${scriptdir}"
  )
endif()

if(NOT client-only)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/mtx-changer
          ${CMAKE_CURRENT_BINARY_DIR}/disk-changer s3cmd-wrapper.sh
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    DESTINATION "${scriptdir}"
  )

  install(FILES mtx-changer.conf DESTINATION "${confdir}")

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/logrotate
    DESTINATION "${sysconfdir}/logrotate.d"
    RENAME bareos-dir
  )

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/mtx-logrotate
    DESTINATION "${sysconfdir}/logrotate.d"
    RENAME bareos-mtx
  )
endif()
