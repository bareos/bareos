#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2024 Bareos GmbH & Co. KG
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
bareos_configure_file(FILES bareos-fd.in bareos-sd.in bareos-dir.in)

set(FIREWALL_DIR ${sysconfdir}/sysconfig/SuSEfirewall2.d/services/)
set(DESTSTARTDIR ${sysconfdir}/init.d/)

message(STATUS "installing startfiles to ${DESTSTARTDIR}")

install(DIRECTORY DESTINATION ${DESTSTARTDIR})

# Install autostart fd
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/bareos-fd"
  DESTINATION "${DESTSTARTDIR}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)
# Install autostart sd
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/bareos-sd"
  DESTINATION "${DESTSTARTDIR}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)
# Install autostart dir
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/bareos-dir"
  DESTINATION "${DESTSTARTDIR}"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
)

message(STATUS "installing firewall files to ${DESTSTARTDIR}")

# Install firewall fd
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/bareos-fd.fw"
  DESTINATION "${FIREWALL_DIR}"
  RENAME bareos-fd
)
# Install firewall sd
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/bareos-sd.fw"
  DESTINATION "${FIREWALL_DIR}"
  RENAME bareos-sd
)
# Install firewall dir
install(
  FILES "${CMAKE_CURRENT_LIST_DIR}/bareos-dir.fw"
  DESTINATION "${FIREWALL_DIR}"
  RENAME bareos-dir
)

install(DIRECTORY DESTINATION ${sbindir})

install(
  CODE "execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink ${DESTSTARTDIR}/bareos-fd $ENV{RPM_BUILD_ROOT}/${sbindir}/rcbareos-fd )"
  CODE "MESSAGE(STATUS ${CMAKE_COMMAND}           -E create_symlink ${DESTSTARTDIR}/bareos-fd $ENV{RPM_BUILD_ROOT}/${sbindir}/rcbareos-fd )"
  CODE "execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink ${DESTSTARTDIR}/bareos-sd $ENV{RPM_BUILD_ROOT}/${sbindir}/rcbareos-sd )"
  CODE "MESSAGE(STATUS ${CMAKE_COMMAND}           -E create_symlink ${DESTSTARTDIR}/bareos-sd $ENV{RPM_BUILD_ROOT}/${sbindir}/rcbareos-sd )"
  CODE "execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink ${DESTSTARTDIR}/bareos-dir $ENV{RPM_BUILD_ROOT}/${sbindir}/rcbareos-dir )"
  CODE "MESSAGE(STATUS ${CMAKE_COMMAND}           -E create_symlink ${DESTSTARTDIR}/bareos-dir $ENV{RPM_BUILD_ROOT}/${sbindir}/rcbareos-dir )"
)
