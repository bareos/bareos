#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

cmake_minimum_required(VERSION 3.17...3.31)

set(WEBUI_VUE_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT_DIR "${WEBUI_VUE_SRC_DIR}/..")

if(NOT DEFINED DIST_DIR)
  set(DIST_DIR "${WEBUI_VUE_SRC_DIR}/dist")
endif()

set(VERSION_TEMPLATE_FILE
    "${WEBUI_VUE_SRC_DIR}/src/generated/bareos-version.js.in"
)
set(VERSION_OUTPUT_FILE "${WEBUI_VUE_SRC_DIR}/src/generated/bareos-version.js")

if(NOT DEFINED NPM_EXECUTABLE)
  find_program(NPM_EXECUTABLE npm)
endif()

if(NOT NPM_EXECUTABLE)
  message(FATAL_ERROR "npm executable not found")
endif()

set(PROJECT_SOURCE_DIR "${REPO_ROOT_DIR}")
set(CMAKE_MODULE_PATH "${REPO_ROOT_DIR}/cmake" "${REPO_ROOT_DIR}/core/cmake"
                      "${REPO_ROOT_DIR}/webui/cmake"
)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -P "${REPO_ROOT_DIR}/write_version_files.cmake"
  WORKING_DIRECTORY "${REPO_ROOT_DIR}"
  RESULT_VARIABLE write_version_result
)
if(NOT write_version_result EQUAL 0)
  message(
    FATAL_ERROR
      "write_version_files.cmake failed with exit code ${write_version_result}"
  )
endif()

include(BareosExtractVersionInfo)

file(READ "${VERSION_TEMPLATE_FILE}" VERSION_FILE_TEMPLATE)
string(REPLACE "@BAREOS_FULL_VERSION@" "${BAREOS_FULL_VERSION}"
               GENERATED_VERSION_FILE "${VERSION_FILE_TEMPLATE}"
)

set(WRITE_VERSION_FILE TRUE)
if(EXISTS "${VERSION_OUTPUT_FILE}")
  file(READ "${VERSION_OUTPUT_FILE}" CURRENT_VERSION_FILE)
  if(CURRENT_VERSION_FILE STREQUAL GENERATED_VERSION_FILE)
    set(WRITE_VERSION_FILE FALSE)
  endif()
endif()

if(WRITE_VERSION_FILE)
  file(WRITE "${VERSION_OUTPUT_FILE}" "${GENERATED_VERSION_FILE}")
endif()

file(REMOVE_RECURSE "${DIST_DIR}")

message(STATUS "Running npm ci in ${WEBUI_VUE_SRC_DIR}")
execute_process(
  COMMAND "${NPM_EXECUTABLE}" ci
  WORKING_DIRECTORY "${WEBUI_VUE_SRC_DIR}"
  RESULT_VARIABLE npm_ci_result
)
if(NOT npm_ci_result EQUAL 0)
  message(FATAL_ERROR "npm ci failed with exit code ${npm_ci_result}")
endif()

message(STATUS "Running npm audit in ${WEBUI_VUE_SRC_DIR}")
execute_process(
  COMMAND "${NPM_EXECUTABLE}" audit
  WORKING_DIRECTORY "${WEBUI_VUE_SRC_DIR}"
  RESULT_VARIABLE npm_audit_result
)
if(NOT npm_audit_result EQUAL 0)
  message(FATAL_ERROR "npm audit failed with exit code ${npm_audit_result}")
endif()

message(STATUS "Building webui-vue dist in ${DIST_DIR}")
execute_process(
  COMMAND "${NPM_EXECUTABLE}" run build -- --outDir "${DIST_DIR}"
  WORKING_DIRECTORY "${WEBUI_VUE_SRC_DIR}"
  RESULT_VARIABLE npm_build_result
)
if(NOT npm_build_result EQUAL 0)
  message(FATAL_ERROR "npm run build failed with exit code ${npm_build_result}")
endif()
