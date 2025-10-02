#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/../../SOURCES
    ${PROJECT_SOURCE_DIR}/../SOURCES ${PROJECT_SOURCE_DIR}/cmake
)

include(BareosLocalBuildDefinitions OPTIONAL
        RESULT_VARIABLE BareosLocalBuildDefinitionsFile
)

include(BareosConfigureFile)

bareos_configure_file(
  FILES
  media_vault.sh.in
  media_vault.ini.example.in
  job_admin-media_vault.conf.example.in
)

install(
  FILES media_vault.py
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${scriptdir}"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/media_vault.sh"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${scriptdir}"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/media_vault.ini.example"
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ
  DESTINATION "${confdir}"
)

install(
  FILES console_media_vault.conf.example
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${confdir}/bareos-dir.d/console"
)

install(
  FILES profile_media_vault.conf.example
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ
  DESTINATION "${confdir}/bareos-dir.d/profile"
)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/job_admin-media_vault.conf.example"
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ
  DESTINATION "${confdir}/bareos-dir.d/job"
)

