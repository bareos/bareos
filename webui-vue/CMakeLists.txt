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
  install/apache/bareos-webui-vue.conf.in
  ${CMAKE_CURRENT_BINARY_DIR}/install/apache/bareos-webui-vue.conf @ONLY
)

set(WEBUI_VUE_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
set(WEBUI_VUE_DIST "${WEBUI_VUE_SRC}/dist")

file(GLOB_RECURSE WEBUI_VUE_SOURCES "${WEBUI_VUE_SRC}/src/*.vue"
     "${WEBUI_VUE_SRC}/src/*.js" "${WEBUI_VUE_SRC}/src/*.scss"
)
file(GLOB WEBUI_VUE_SOURCES_ROOT "${WEBUI_VUE_SRC}/index.html"
     "${WEBUI_VUE_SRC}/package.json" "${WEBUI_VUE_SRC}/vite.config.js"
)
list(APPEND WEBUI_VUE_SOURCES ${WEBUI_VUE_SOURCES_ROOT})

set(WEBUI_VUE_DIST_INDEX "${WEBUI_VUE_DIST}/index.html")

# Use dist/index.html as the build output so CMake's dependency tracking skips
# the npm build when the checked-in dist is already up-to-date. Run `npm run
# build` (not `npm ci`) so no network access is required; run `npm ci` manually
# once when updating dependencies.
add_custom_command(
  OUTPUT "${WEBUI_VUE_DIST_INDEX}"
  COMMAND npm run build
  WORKING_DIRECTORY "${WEBUI_VUE_SRC}"
  DEPENDS ${WEBUI_VUE_SOURCES}
  COMMENT "Building bareos-webui-vue frontend"
  VERBATIM
)

add_custom_target(bareos-webui-vue-build ALL DEPENDS "${WEBUI_VUE_DIST_INDEX}")

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
