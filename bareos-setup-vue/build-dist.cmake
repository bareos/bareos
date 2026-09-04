#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

cmake_minimum_required(VERSION 3.17...3.31)

set(SETUP_VUE_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT_DIR "${SETUP_VUE_SRC_DIR}/..")
set(SOURCE_DIST_DIR "${SETUP_VUE_SRC_DIR}/dist")

if(NOT DEFINED DIST_DIR)
  set(DIST_DIR "${SOURCE_DIST_DIR}")
endif()

if(NOT DEFINED NPM_EXECUTABLE)
  find_program(NPM_EXECUTABLE npm)
endif()

if(EXISTS "${SOURCE_DIST_DIR}/index.html"
   AND (NOT EXISTS "${REPO_ROOT_DIR}/.git" OR NOT NPM_EXECUTABLE)
)
  message(STATUS "Using prebuilt bareos-setup-vue dist from ${SOURCE_DIST_DIR}")
  if(NOT DIST_DIR STREQUAL SOURCE_DIST_DIR)
    file(REMOVE_RECURSE "${DIST_DIR}")
    file(MAKE_DIRECTORY "${DIST_DIR}")
    file(COPY "${SOURCE_DIST_DIR}/" DESTINATION "${DIST_DIR}")
  endif()
  return()
endif()

if(NOT NPM_EXECUTABLE)
  message(FATAL_ERROR "npm executable not found")
endif()

file(REMOVE_RECURSE "${DIST_DIR}")

message(STATUS "Running npm ci in ${SETUP_VUE_SRC_DIR}")
execute_process(
  COMMAND "${NPM_EXECUTABLE}" ci
  WORKING_DIRECTORY "${SETUP_VUE_SRC_DIR}"
  RESULT_VARIABLE npm_ci_result
)
if(NOT npm_ci_result EQUAL 0)
  message(FATAL_ERROR "npm ci failed with exit code ${npm_ci_result}")
endif()

message(STATUS "Running npm audit in ${SETUP_VUE_SRC_DIR}")
execute_process(
  COMMAND "${NPM_EXECUTABLE}" audit
  WORKING_DIRECTORY "${SETUP_VUE_SRC_DIR}"
  RESULT_VARIABLE npm_audit_result
)
if(NOT npm_audit_result EQUAL 0)
  message(WARNING "npm audit failed with exit code ${npm_audit_result}")
endif()

message(STATUS "Building bareos-setup-vue dist in ${DIST_DIR}")
execute_process(
  COMMAND "${NPM_EXECUTABLE}" run build -- --outDir "${DIST_DIR}"
  WORKING_DIRECTORY "${SETUP_VUE_SRC_DIR}"
  RESULT_VARIABLE npm_build_result
)
if(NOT npm_build_result EQUAL 0)
  message(FATAL_ERROR "npm run build failed with exit code ${npm_build_result}")
endif()
