#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2026 Bareos GmbH & Co. KG
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

set(COVERAGE_COMPILER_FLAGS
    "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
    CACHE INTERNAL ""
)
message(
  STATUS
    "coverage requested, adding COVERAGE_COMPILER_FLAGS : ${COVERAGE_COMPILER_FLAGS}"
)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  link_libraries(gcov)
else()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}")

add_custom_target(
  cov-clean
  COMMENT "Cleaning gcda files from ${CMAKE_BINARY_DIR} and report."
  COMMAND find "${CMAKE_BINARY_DIR}" -name '*.gcda' -delete
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}/coverage"
)

find_program(PROGRAM_GCOVR gcovr)
if(PROGRAM_GCOVR)
  add_custom_target(
    cov-report
    COMMENT "Creating HTML coverage report in ${CMAKE_BINARY_DIR}/coverage"
    # avoid irritating gcov error by removing a broken gcno file
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/coverage"
    COMMAND
      "${PROGRAM_GCOVR}" --html-details
      "${CMAKE_BINARY_DIR}/coverage/index.html" --root "${CMAKE_SOURCE_DIR}"
      --filter "${CMAKE_SOURCE_DIR}" --filter "${CMAKE_BINARY_DIR}"
      "${CMAKE_BINARY_DIR}"
  )
else()
  add_custom_target(
    cov-report
    COMMAND
      "${CMAKE_COMMAND}" -E echo
      "WARNING: gcovr binary not found, cannot create HTML coverage report"
  )
endif()
