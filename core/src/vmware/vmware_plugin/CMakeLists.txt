#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2023 Bareos GmbH & Co. KG
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

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if("${LIB64}" STREQUAL "TRUE")
  set(LIBSUFFIX 64)
else()
  set(LIBSUFFIX "")
endif()
set(INSTALL_LIB_DIR
    "lib${LIBSUFFIX}"
    CACHE PATH "Installation directory for libraries"
)
mark_as_advanced(INSTALL_LIB_DIR)

install(
  FILES bareos-fd-vmware.py
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${INSTALL_LIB_DIR}/bareos/plugins"
)
install(DIRECTORY
        DESTINATION "${INSTALL_LIB_DIR}/bareos/plugins/vmware_plugin/"
)
install(
  # as this command is not handled by "INSTALL DESTINATION", the target path
  # must be evaluated manually.
  CODE "
        execute_process(COMMAND ln -sf \
            ../bareos-fd-vmware.py     \
            \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}/bareos/plugins/vmware_plugin/bareos-fd-vmware.py
        )
    "
)
