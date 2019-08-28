#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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

INSTALL(FILES  resources/com.bareos.bareos-fd.plist
        DESTINATION "${scriptdir}"
	COMPONENT client)

INSTALL(FILES  resources/uninstall-bareos
        DESTINATION "${scriptdir}"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
	COMPONENT common)

# cpack settings
SET(CPACK_PACKAGE_VENDOR "bareos")
SET(CPACK_PACKAGE_NAME "bareos")
SET(CPACK_PACKAGE_HOMEPAGE_URL "https://www.bareos.com/")
SET(CPACK_PACKAGE_VERSION_MAJOR "${BAREOS_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${BAREOS_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${BAREOS_VERSION_PATCH}")
SET(CPACK_MONOLITHIC_INSTALL FALSE)
SET(CPACK_COMPONENTS_ALL common client bconsole)

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bareos File Daemon")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/description.txt")

SET(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/bareos-logo.png")


SET(CPACK_PRODUCTBUILD_RESOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/resources")

SET(CPACK_RESOURCE_FILE_WELCOME "${CMAKE_CURRENT_LIST_DIR}/resources/welcome.txt")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_LIST_DIR}/resources/ReadMe.html")
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/AGPL-3.0.txt")

SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${BAREOS_FULL_VERSION}")
SET(CPACK_GENERATOR "productbuild")

# if unset, next points to goes to /Application
SET(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

SET(CPACK_PREFLIGHT_CLIENT_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/resources/preinstall")
SET(CPACK_POSTFLIGHT_CLIENT_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/resources/postinstall")

INCLUDE(CPack)
