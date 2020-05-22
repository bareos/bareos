#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

if(Python2_FOUND)
  add_library(python-dir MODULE python-dir.cc)
  # do not prefix with "lib"
  set_target_properties(
    python-dir PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ..
  )
  install(TARGETS python-dir DESTINATION ${plugindir})
  target_link_libraries(python-dir ${Python2_LIBRARIES} bareos)
  target_include_directories(python-dir PUBLIC SYSTEM ${Python2_INCLUDE_DIRS})
  add_dependencies(python-dir bareosdir-python2-module)

  set(PYFILES pyfiles/bareos-dir.py.template pyfiles/bareos-dir-class-plugin.py
              pyfiles/BareosDirPluginBaseclass.py pyfiles/BareosDirWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(Python2_FOUND AND NOT HAVE_WIN32)
  configure_file(module/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/module/setup.py)
  add_custom_command(
    OUTPUT pythonmodules/bareosdir.so
    MAIN_DEPENDENCY module/bareosdir.cc
    DEPENDS module/bareosdir.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/module/setup.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python2_EXECUTABLE} module/setup.py build
    COMMAND ${Python2_EXECUTABLE} module/setup.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/pythonmodules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python2 module pythonmodules/bareosdir.so"
  )
  add_custom_target(bareosdir-python2-module DEPENDS pythonmodules/bareosdir.so)

  add_test(NAME bareosdir-python2-module
           COMMAND ${Python2_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test/bareosdir_test.py
  )
  set_property(
    TEST bareosdir-python2-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
  )
endif()

if(Python3_FOUND)
  add_library(python3-dir MODULE python-dir.cc)
  # do not prefix with "lib"
  set_target_properties(
    python3-dir PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ..
  )
  install(TARGETS python3-dir DESTINATION ${plugindir})
  target_link_libraries(python3-dir ${Python3_LIBRARIES} bareos)
  target_include_directories(python3-dir PUBLIC SYSTEM ${Python3_INCLUDE_DIRS})
  add_dependencies(python3-dir bareosdir-python3-module)

  set(PYFILES pyfiles/bareos-dir.py.template pyfiles/bareos-dir-class-plugin.py
              pyfiles/BareosDirPluginBaseclass.py pyfiles/BareosDirWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  configure_file(module/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/module/setup.py)
  add_custom_command(
    OUTPUT python3modules/bareosdir.so
    MAIN_DEPENDENCY module/bareosdir.cc
    DEPENDS module/bareosdir.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/module/setup.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python3_EXECUTABLE} module/setup.py build
    COMMAND ${Python3_EXECUTABLE} module/setup.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/python3modules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python3 module python3modules/bareosdir.so"
  )
  add_custom_target(
    bareosdir-python3-module DEPENDS python3modules/bareosdir.so
  )

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
  )
endif()
