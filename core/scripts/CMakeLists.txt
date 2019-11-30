#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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
  FILES bareos btraceback.gdb btraceback.dbx btraceback.mdb bareos-config
        bareos-config-lib.sh bareos-ctl-funcs
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${scriptdir}"
)

if(NOT client-only)
  install(
    FILES mtx-changer disk-changer bareos-explorer bareos-glusterfind-wrapper
          bareos-ctl-dir bareos-ctl-sd bareos-ctl-funcs
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
    DESTINATION "${scriptdir}"
  )

  install(FILES mtx-changer.conf bareos-regress.conf DESTINATION "${confdir}")

  install(
    FILES logrotate
    DESTINATION "${sysconfdir}/logrotate.d"
    RENAME bareos-dir
  )
endif()
