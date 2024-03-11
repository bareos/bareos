#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

if(Python3_FOUND)
  add_library(python3-dir MODULE python-dir.cc)
  # do not prefix with "lib"
  set_target_properties(
    python3-dir PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ..
  )
  install(TARGETS python3-dir DESTINATION ${plugindir})
  target_link_libraries(python3-dir ${Python3_LIBRARIES} bareos)
  target_include_directories(python3-dir PUBLIC ${Python3_INCLUDE_DIRS})
  target_compile_definitions(
    python3-dir PUBLIC PY3MODDIR=\"${Python3_SITEARCH}\"
  )
  add_dependencies(python3-dir bareosdir-python3-module)

  set(PYFILES pyfiles/bareos-dir-class-plugin.py
              pyfiles/BareosDirPluginBaseclass.py pyfiles/BareosDirWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(Python3_FOUND)
  add_library(bareosdir-python3-module MODULE module/bareosdir.cc)
  install(TARGETS bareosdir-python3-module DESTINATION ${Python3_SITEARCH})
  set_target_properties(
    bareosdir-python3-module
    PROPERTIES PREFIX ""
               C_STANDARD 11
               SUFFIX ${Python3_EXT_SUFFIX}
               LIBRARY_OUTPUT_DIRECTORY python3modules
               LIBRARY_OUTPUT_NAME bareosdir
               LINK_FLAGS "${Python3_BLDSHARED_FLAGS}"
               LINKER_LANGUAGE CXX
               INCLUDE_DIRECTORIES "${pymod3_include_dirs}"
  )
  if(NOT "${Python3_CCSHARED}" STREQUAL "")
    set_property(
      TARGET bareosdir-python3-module PROPERTY COMPILE_OPTIONS
                                               ${Python3_CCSHARED}
    )
  endif()
  target_link_libraries(bareosdir-python3-module bareos ${Python3_LIBRARIES})

  add_test(NAME bareosdir-python3-module
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test/bareosdir_test.py
  )
  set_property(
    TEST bareosdir-python3-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
      "ASAN_OPTIONS=detect_leaks=0 verify_asan_link_order=0"
  )
endif()

include(DebugEdit)
