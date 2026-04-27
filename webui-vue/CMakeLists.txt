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
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
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
# Only overwrite bareos-version.js when the content changes, to avoid making
# dist/index.html appear stale on every cmake run.
execute_process(
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/bareos-version.js.tmp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/bareos-version.js
)

set(WEBUI_VUE_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
set(WEBUI_VUE_DIST "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(WEBUI_VUE_PACKAGE_JSON "${WEBUI_VUE_SRC}/package.json")
set(WEBUI_VUE_PACKAGE_LOCK "${WEBUI_VUE_SRC}/package-lock.json")
set(WEBUI_VUE_BUILD_SCRIPT "${WEBUI_VUE_SRC}/build-dist.cmake")
set(WEBUI_VUE_BUILD_STAMP "${CMAKE_CURRENT_BINARY_DIR}/webui-vue-build.stamp")
set(WEBUI_VUE_AUDIT_STAMP "${CMAKE_CURRENT_BINARY_DIR}/webui-vue-audit.stamp")
set(WEBUI_VUE_NPM_CI_STAMP "${CMAKE_CURRENT_BINARY_DIR}/webui-vue-npm-ci.stamp")
set(WEBUI_VUE_NPM_MISSING_SCRIPT
    "${CMAKE_CURRENT_BINARY_DIR}/webui-vue-npm-missing.cmake"
)

find_program(NPM_EXECUTABLE npm)
file(
  GENERATE
  OUTPUT "${WEBUI_VUE_NPM_MISSING_SCRIPT}"
  CONTENT
    "message(FATAL_ERROR \"npm executable not found. Install npm and re-run CMake to use bareos-webui-vue-build.\")\n"
)

file(GLOB_RECURSE WEBUI_VUE_BUILD_INPUTS CONFIGURE_DEPENDS
     "${WEBUI_VUE_SRC}/src/*" "${WEBUI_VUE_SRC}/public/*"
     "${WEBUI_VUE_SRC}/scripts/*"
)
list(APPEND WEBUI_VUE_BUILD_INPUTS "${WEBUI_VUE_SRC}/index.html"
     "${WEBUI_VUE_SRC}/vite.config.js" "${WEBUI_VUE_BUILD_SCRIPT}"
     "${WEBUI_VUE_PACKAGE_JSON}" "${WEBUI_VUE_PACKAGE_LOCK}"
)

# The standalone bareos-webui-vue-build target generates the installable bundle
# in the build tree, keeping source-tree dist/ artifacts untracked. Keep it
# opt-in so generic CMake builds do not require npm to be installed.
if(NPM_EXECUTABLE)
  add_custom_command(
    OUTPUT "${WEBUI_VUE_NPM_CI_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${WEBUI_VUE_SRC}" "${NPM_EXECUTABLE}" ci
    COMMAND ${CMAKE_COMMAND} -E touch "${WEBUI_VUE_NPM_CI_STAMP}"
    DEPENDS "${WEBUI_VUE_PACKAGE_JSON}" "${WEBUI_VUE_PACKAGE_LOCK}"
    COMMENT "Installing Bareos WebUI Vue npm dependencies"
    USES_TERMINAL VERBATIM
  )

  add_custom_command(
    OUTPUT "${WEBUI_VUE_AUDIT_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${WEBUI_VUE_SRC}" "${NPM_EXECUTABLE}"
            audit
    COMMAND ${CMAKE_COMMAND} -E touch "${WEBUI_VUE_AUDIT_STAMP}"
    DEPENDS "${WEBUI_VUE_NPM_CI_STAMP}" "${WEBUI_VUE_PACKAGE_JSON}"
            "${WEBUI_VUE_PACKAGE_LOCK}"
    COMMENT "Auditing Bareos WebUI Vue npm packages"
    USES_TERMINAL VERBATIM
  )

  add_custom_command(
    OUTPUT "${WEBUI_VUE_BUILD_STAMP}"
    COMMAND ${CMAKE_COMMAND} -DDIST_DIR="${WEBUI_VUE_DIST}" -P
            "${WEBUI_VUE_BUILD_SCRIPT}"
    COMMAND ${CMAKE_COMMAND} -E touch "${WEBUI_VUE_BUILD_STAMP}"
    DEPENDS "${WEBUI_VUE_AUDIT_STAMP}" ${WEBUI_VUE_BUILD_INPUTS}
    COMMENT "Building Bareos WebUI Vue dist bundle"
    USES_TERMINAL VERBATIM
  )

  add_custom_target(bareos-webui-vue-audit DEPENDS "${WEBUI_VUE_AUDIT_STAMP}")
  add_custom_target(bareos-webui-vue-build DEPENDS "${WEBUI_VUE_BUILD_STAMP}")
else()
  add_custom_target(
    bareos-webui-vue-audit
    COMMAND ${CMAKE_COMMAND} -P "${WEBUI_VUE_NPM_MISSING_SCRIPT}"
    VERBATIM
  )
  add_custom_target(bareos-webui-vue-build DEPENDS bareos-webui-vue-audit)
endif()

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
