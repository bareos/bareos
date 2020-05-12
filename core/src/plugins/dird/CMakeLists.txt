#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/dird
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include
  )
  link_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS}
  )
  add_definitions(-DMS_WIN${WINDOWS_BITS})
endif()

if(HAVE_PYTHON)
  add_library(python-dir MODULE python/python-dir.cc)
  # do not prefix with "lib"
  set_target_properties(python-dir PROPERTIES PREFIX "")
  install(TARGETS python-dir DESTINATION ${plugindir})
  target_link_libraries(python-dir ${PYTHON_LIBRARIES} bareos)
  add_dependencies(python-dir bareosdir-pymod)

  set(PYFILES
      python/pyfiles/bareos-dir.py.template
      python/pyfiles/bareos-dir-class-plugin.py
      python/pyfiles/BareosDirPluginBaseclass.py
      python/pyfiles/BareosDirWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(HAVE_PYTHON AND NOT HAVE_WIN32)
  configure_file(python/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py)
  add_custom_command(
    OUTPUT pythonmodules/bareosdir.so
    MAIN_DEPENDENCY ./python/bareosdir.cc
    DEPENDS ./python/bareosdir.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python_EXECUTABLE} python/setup.py build
    COMMAND ${Python_EXECUTABLE} python/setup.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/pythonmodules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python module pythonmodules/bareosdir.so"
  )

  add_custom_target(bareosdir-pymod DEPENDS pythonmodules/bareosdir.so)

  add_test(NAME bareosdir-python-module
           COMMAND ${Python_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/python/test/bareosdir_test.py
  )
  set_property(
    TEST bareosdir-python-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/python/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
  )
endif()
