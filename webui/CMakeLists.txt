#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2019 Bareos GmbH & Co. KG
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
cmake_minimum_required(VERSION 3.0)
project(bareos-webui)

INCLUDE(GNUInstallDirs)

set(CMAKE_MODULE_PATH
         ${CMAKE_MODULE_PATH}
         ${PROJECT_SOURCE_DIR}/cmake)

IF (NOT DEFINED VERSION_STRING)
  # try to get version info from version.h
  INCLUDE(BareosExtractVersionInfo)
ELSE()
  # we got VERSION_STRING from cmake commandline
ENDIF()


find_program(PHP php)

# set defaults
IF(NOT DEFINED sysconfdir)
   set(sysconfdir ${CMAKE_INSTALL_FULL_SYSCONFDIR})
ENDIF()
IF(NOT DEFINED confdir)
   set(confdir "${sysconfdir}/bareos")
ENDIF()
IF(NOT DEFINED webuiconfdir)
   set(webuiconfdir "${sysconfdir}/bareos-webui")
ENDIF()
IF(NOT DEFINED baseport)
   set(baseport 9101)
ENDIF()

set (dirport ${baseport})
MATH(EXPR webuiport "${baseport} + 3")

# upcase variables for CONFIGURE_FILE
set(SYSCONFDIR "${sysconfdir}")
set(CONFDIR "${confdir}")
set(WEBUICONFDIR "${webuiconfdir}")


MESSAGE("   PHP is ${PHP} ")
MESSAGE("   Install system config files:  ${sysconfdir} ")
MESSAGE("   Install Bareos config dir:    ${confdir} ")
MESSAGE("   Install BareosWebui configdir:${webuiconfdir} ")
MESSAGE("   VERSION_STRING is ${VERSION_STRING} ")
MESSAGE("   BAREOS_FULL_VERSION is ${BAREOS_FULL_VERSION} ")

IF (EXISTS ${sysconfdir}/httpd/conf.d)
  SET(HTTPD_CONF ${sysconfdir}/httpd/conf.d)
ELSEIF (EXISTS ${sysconfdir}/apache2/conf.d)
  SET(HTTPD_CONF ${sysconfdir}/apache2/conf.d)
ELSEIF (EXISTS ${sysconfdir}/apache/conf.d)
  SET(HTTPD_CONF ${sysconfdir}/apache/conf.d)
ELSEIF (EXISTS ${sysconfdir}/apache2/conf-available)
  SET(HTTPD_CONF ${sysconfdir}/apache2/conf-available)
ELSE()
  SET(HTTPD_CONF ${sysconfdir}/httpd/conf.d)
ENDIF()

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config/autoload/global.php.in ${CMAKE_CURRENT_SOURCE_DIR}/config/autoload/global.php @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/install/directors.ini.in ${CMAKE_CURRENT_SOURCE_DIR}/install/directors.ini @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/install/configuration.ini.in ${CMAKE_CURRENT_SOURCE_DIR}/install/configuration.ini @ONLY)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/version.php.in ${CMAKE_CURRENT_SOURCE_DIR}/version.php @ONLY)


SET(WEBUI_DIR  ${SHARE_INSTALL_PREFIX}/bareos-webui)
SET(WEBUI_TESTS_DIR  ${WEBUI_DIR}/tests/regress)
IF (PHP)
 CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test.in ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test @ONLY)
 # FILE(APPEND ${CMAKE_CURRENT_SOURCE_DIR}/../regress/DartTestfile.txt.in "ADD_TEST(disk:webui-bsock-connection-test \"../webui/tests/regress/webui-bsock-connection-test\")")
ELSE()
 MESSAGE("PHP not found")
ENDIF()

INSTALL(FILES version.php init_autoloader.php DESTINATION ${SHARE_INSTALL_PREFIX}/bareos-webui)
INSTALL(DIRECTORY tests config data module public vendor DESTINATION ${SHARE_INSTALL_PREFIX}/bareos-webui)

INSTALL(FILES install/bareos/bareos-dir.d/console/admin.conf.example DESTINATION ${confdir}/bareos-dir.d/console/)
INSTALL(FILES install/bareos/bareos-dir.d/profile/webui-admin.conf DESTINATION ${confdir}/bareos-dir.d/profile/)
INSTALL(FILES install/apache/bareos-webui.conf DESTINATION ${HTTPD_CONF}/)

INSTALL(FILES install/directors.ini DESTINATION ${webuiconfdir})
INSTALL(FILES install/configuration.ini DESTINATION ${webuiconfdir})


