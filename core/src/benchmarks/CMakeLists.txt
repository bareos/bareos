#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2025 Bareos GmbH & Co. KG
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

find_package(benchmark 1.5 CONFIG REQUIRED)

configure_file(
  "${CMAKE_SOURCE_DIR}/core/src/benchmarks/runbenchmarks"
  "${CMAKE_BINARY_DIR}/core/src/benchmarks/runbenchmarks" @ONLY
)

add_custom_target(
  benchmarks
  COMMENT "\nStarting benchmarks: \n "
  COMMAND ./runbenchmarks
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

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

bareos_add_benchmark(
  restore_browser_stress_test LINK_LIBRARIES Bareos::Lib dird_objects
  Bareos::Findlib bareossql benchmark::benchmark_main
)

bareos_add_benchmark(
  poolmem_fragmentation LINK_LIBRARIES Bareos::Lib benchmark::benchmark_main
  ${THREADS_THREADS}
)

bareos_add_benchmark(
  digest LINK_LIBRARIES Bareos::Lib benchmark::benchmark_main
)

include(DebugEdit)
