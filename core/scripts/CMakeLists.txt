#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2023 Bareos GmbH & Co. KG
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

install(
  FILES bareos btraceback
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${sbindir}"
)

install(
  FILES bareos-ctl-fd
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${scriptdir}"
  COMPONENT filedaemon
)

install(
  FILES bareos bareos-config
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${scriptdir}"
)

install(
  FILES bareos-config-lib.sh bareos-ctl-funcs btraceback.gdb btraceback.dbx
        btraceback.mdb
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  DESTINATION "${scriptdir}"
)
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
    FILES mtx-changer disk-changer bareos-explorer bareos-glusterfind-wrapper
          bareos-ctl-dir bareos-ctl-sd
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    DESTINATION "${scriptdir}"
  )

  install(FILES mtx-changer.conf DESTINATION "${confdir}")

  install(
    FILES logrotate
    DESTINATION "${sysconfdir}/logrotate.d"
    RENAME bareos-dir
  )
endif()
