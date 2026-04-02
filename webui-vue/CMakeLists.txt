#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

cmake_minimum_required(VERSION 3.17)

set(WEBUI_VUE_DIR ${CMAKE_INSTALL_FULL_DATAROOTDIR}/bareos-webui-vue)

# Detect Apache configuration directory
if(HAVE_FREEBSD_OS)
  # FreeBSD apache24 stores extra configs in ${PREFIX}/etc/apache24/Includes
  set(HTTPD_CONF ${sysconfdir}/apache24/Includes)
elseif(EXISTS ${sysconfdir}/httpd/conf.d)
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
  install/apache/bareos-webui-vue.conf.in
  ${CMAKE_CURRENT_BINARY_DIR}/install/apache/bareos-webui-vue.conf @ONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/bareos-version.js.in
  ${CMAKE_CURRENT_BINARY_DIR}/bareos-version.js.tmp @ONLY
)
# Only overwrite bareos-version.js when the content changes, to avoid
# making dist/index.html appear stale on every cmake run.
execute_process(
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/bareos-version.js.tmp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/bareos-version.js
)

set(WEBUI_VUE_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
set(WEBUI_VUE_DIST "${WEBUI_VUE_SRC}/dist")

# dist/ is pre-built by developers and committed to the repository.
# CMake does not run npm — just installs the committed dist/ files.
# To update the frontend, run:
#   cd webui-vue && npm install && npm run build
# then commit the updated dist/.
add_custom_target(bareos-webui-vue-build ALL)

install(
  DIRECTORY ${WEBUI_VUE_DIST}/
  DESTINATION ${WEBUI_VUE_DIR}
  COMPONENT webui-vue
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/install/apache/bareos-webui-vue.conf
  DESTINATION ${HTTPD_CONF}/
  COMPONENT webui-vue
)
