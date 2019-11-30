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

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/../../SOURCES
    ${PROJECT_SOURCE_DIR}/../SOURCES ${PROJECT_SOURCE_DIR}/cmake
)

include(BareosLocalBuildDefinitions OPTIONAL
        RESULT_VARIABLE BareosLocalBuildDefinitionsFile
)

if(NOT DEFINED VERSION_STRING)
  # try to get version info from version.h
  include(BareosExtractVersionInfo)
else()
  # we got VERSION_STRING from cmake commandline
  set(BAREOS_FULL_VERSION ${VERSION_STRING})
endif()

if(BAREOS_FULL_VERSION STREQUAL "")
  message(FATAL_ERROR BAREOS_FULL_VERSION is not set)
endif()

find_program(PHP php)

# set defaults
if(NOT DEFINED sysconfdir)
  set(sysconfdir ${CMAKE_INSTALL_FULL_SYSCONFDIR})
endif()
if(NOT DEFINED confdir)
  set(confdir "${sysconfdir}/bareos")
endif()
if(NOT DEFINED webuiconfdir)
  set(webuiconfdir "${sysconfdir}/bareos-webui")
endif()
if(NOT DEFINED baseport)
  set(baseport 9101)
endif()

set(dirport ${baseport})
math(EXPR webuiport "${baseport} + 3")

# upcase variables for CONFIGURE_FILE
set(SYSCONFDIR "${sysconfdir}")
set(CONFDIR "${confdir}")
set(WEBUICONFDIR "${webuiconfdir}")

message("   PHP is ${PHP} ")
message("   Install system config files:   ${sysconfdir} ")
message("   Install Bareos config dir:     ${confdir} ")
message("   Install BareosWebui configdir: ${webuiconfdir} ")
message("   VERSION_STRING:                ${VERSION_STRING} ")
message("   BAREOS_FULL_VERSION:           ${BAREOS_FULL_VERSION} ")
message("   LocalBuildDefinitionsFile:     ${BareosLocalBuildDefinitionsFile}")

if(EXISTS ${sysconfdir}/httpd/conf.d)
  set(HTTPD_CONF ${sysconfdir}/httpd/conf.d)
elseif(EXISTS ${sysconfdir}/apache2/conf.d)
  set(HTTPD_CONF ${sysconfdir}/apache2/conf.d)
elseif(EXISTS ${sysconfdir}/apache/conf.d)
  set(HTTPD_CONF ${sysconfdir}/apache/conf.d)
elseif(EXISTS ${sysconfdir}/apache2/conf-available)
  set(HTTPD_CONF ${sysconfdir}/apache2/conf-available)
else()
  set(HTTPD_CONF ${sysconfdir}/httpd/conf.d)
endif()

configure_file(
  config/autoload/global.php.in
  ${CMAKE_CURRENT_SOURCE_DIR}/config/autoload/global.php @ONLY
)
configure_file(
  install/directors.ini.in ${CMAKE_CURRENT_SOURCE_DIR}/install/directors.ini
  @ONLY
)
configure_file(
  install/configuration.ini.in
  ${CMAKE_CURRENT_SOURCE_DIR}/install/configuration.ini @ONLY
)

configure_file(version.php.in ${CMAKE_CURRENT_SOURCE_DIR}/version.php @ONLY)
configure_file(
  module/Application/view/layout/layout.phtml.in
  ${CMAKE_CURRENT_SOURCE_DIR}/module/Application/view/layout/layout.phtml @ONLY
)
configure_file(
  module/Application/view/layout/login.phtml.in
  ${CMAKE_CURRENT_SOURCE_DIR}/module/Application/view/layout/login.phtml @ONLY
)

set(WEBUI_DIR ${SHARE_INSTALL_PREFIX}/bareos-webui)
set(WEBUI_TESTS_DIR ${WEBUI_DIR}/tests/regress)
if(PHP)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test.in
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test @ONLY
  )
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test-tls.in
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test-tls
    @ONLY
  )
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test-tls.php.in
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/webui-bsock-connection-test-tls.php
    @ONLY
  )
else()
  message("PHP not found")
endif()

if(NOT client-only)
  install(FILES version.php init_autoloader.php
          DESTINATION ${SHARE_INSTALL_PREFIX}/bareos-webui
  )
  install(DIRECTORY tests config data module public vendor
          DESTINATION ${SHARE_INSTALL_PREFIX}/bareos-webui
  )

  install(FILES install/bareos/bareos-dir.d/console/admin.conf.example
          DESTINATION ${confdir}/bareos-dir.d/console/
  )
  install(FILES install/bareos/bareos-dir.d/profile/webui-admin.conf
          DESTINATION ${confdir}/bareos-dir.d/profile/
  )
  install(FILES install/bareos/bareos-dir.d/profile/webui-limited.conf.example
          DESTINATION ${confdir}/bareos-dir.d/profile/
  )
  install(FILES install/bareos/bareos-dir.d/profile/webui-readonly.conf
          DESTINATION ${confdir}/bareos-dir.d/profile/
  )
  install(FILES install/apache/bareos-webui.conf DESTINATION ${HTTPD_CONF}/)

  install(FILES install/directors.ini DESTINATION ${webuiconfdir})
  install(FILES install/configuration.ini DESTINATION ${webuiconfdir})
endif()
