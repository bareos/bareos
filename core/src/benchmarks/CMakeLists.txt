#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2021 Bareos GmbH & Co. KG
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

macro(bareos_add_benchmark BAREOS_ADD_BENCHMARK_BENCHMARKNAME)
  set(multiValueArgs ADDITIONAL_SOURCES LINK_LIBRARIES COMPILE_DEFINITIONS)
  cmake_parse_arguments(
    BAREOS_ADD_BENCHMARK "${options}" "${oneValueArgs}" "${multiValueArgs}"
    ${ARGN}
  )

  add_executable(
    ${BAREOS_ADD_BENCHMARK_BENCHMARKNAME}
    ${BAREOS_ADD_BENCHMARK_BENCHMARKNAME}.cc
    ${BAREOS_ADD_BENCHMARK_ADDITIONAL_SOURCES}
  )
  target_link_libraries(
    ${BAREOS_ADD_BENCHMARK_BENCHMARKNAME}
    ${BAREOS_ADD_BENCHMARK_LINK_LIBRARIES}
  )
  target_compile_definitions(
    ${BAREOS_ADD_BENCHMARK_BENCHMARKNAME}
    PRIVATE ${BAREOS_ADD_BENCHMARK_COMPILE_DEFINITIONS}
  )

endmacro()

set(LINK_LIBRARIES stored_objects dird_objects console_objects bareossd bareos
                   bareoscats bareossql bareosfind
)

set (ENABLED_BENCHMARKS
#    restore_browser_stress_test
    )

foreach(BENCHMARK ${ENABLED_BENCHMARKS})
    add_test(
        NAME "bench:${BENCHMARK}"
        COMMAND ${BENCHMARK}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
endforeach()

bareos_add_benchmark(
  restore_browser_stress_test
  LINK_LIBRARIES
  bareos
  dird_objects
  bareosfind
  bareoscats
  bareossql
  benchmark::benchmark_main
)
