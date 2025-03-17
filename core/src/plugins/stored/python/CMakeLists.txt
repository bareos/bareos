#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

include(BareosPlugins)

if(Python3_FOUND)
  bareos_add_plugin(python3-sd)
  target_sources(python3-sd PRIVATE python-sd.cc)
  install(TARGETS python3-sd DESTINATION ${plugindir})
  target_link_libraries(python3-sd ${Python3_LIBRARIES} Bareos::Lib)
  target_compile_definitions(
    python3-sd PUBLIC PY3MODDIR=\"${Python3_SITEARCH}\"
  )
  target_include_directories(python3-sd PUBLIC ${Python3_INCLUDE_DIRS})
  add_dependencies(python3-sd bareossd-python3-module)

  set(PYFILES pyfiles/bareos-sd-class-plugin.py
              pyfiles/BareosSdPluginBaseclass.py pyfiles/BareosSdWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(Python3_FOUND)
  add_library(bareossd-python3-module MODULE module/bareossd.cc)
  install(TARGETS bareossd-python3-module DESTINATION ${Python3_SITEARCH})
  set_target_properties(
    bareossd-python3-module
    PROPERTIES PREFIX ""
               C_STANDARD 11
               SUFFIX ${Python3_EXT_SUFFIX}
               LIBRARY_OUTPUT_DIRECTORY python3modules
               LIBRARY_OUTPUT_NAME bareossd
               LINK_FLAGS "${Python3_BLDSHARED_FLAGS}"
               LINKER_LANGUAGE CXX
               INCLUDE_DIRECTORIES "${pymod3_include_dirs}"
  )
  if(NOT "${Python3_CCSHARED}" STREQUAL "")
    set_property(
      TARGET bareossd-python3-module PROPERTY COMPILE_OPTIONS
                                              ${Python3_CCSHARED}
    )
  endif()
  target_link_libraries(
    bareossd-python3-module Bareos::Lib ${Python3_LIBRARIES}
  )

  if(NOT MSVC) # TODO: fix on MSVC
    add_test(NAME bareossd-python3-module
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/bareossd_test.py
    )
    set_property(
      TEST bareossd-python3-module
      PROPERTY
        ENVIRONMENT
        "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/test"
        LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
        "ASAN_OPTIONS=detect_leaks=0 verify_asan_link_order=0"
    )
  endif()
endif()

include(DebugEdit)
