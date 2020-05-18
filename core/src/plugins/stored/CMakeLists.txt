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
    ${PROJECT_SOURCE_DIR}/src/win32/stored
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include
  )
  link_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS}
  )
  add_definitions(-DMS_WIN${WINDOWS_BITS})
endif()

add_library(autoxflate-sd MODULE autoxflate/autoxflate-sd.cc)
set_target_properties(autoxflate-sd PROPERTIES PREFIX "")
target_link_libraries(autoxflate-sd bareos)
install(TARGETS autoxflate-sd DESTINATION ${plugindir})

if(NOT HAVE_WIN32 AND NOT HAVE_DARWIN_OS)
  add_library(scsicrypto-sd MODULE scsicrypto/scsicrypto-sd.cc)
  set_target_properties(scsicrypto-sd PROPERTIES PREFIX "")
  install(TARGETS scsicrypto-sd DESTINATION ${plugindir})

  add_library(scsitapealert-sd MODULE scsitapealert/scsitapealert-sd.cc)
  set_target_properties(scsitapealert-sd PROPERTIES PREFIX "")
  install(TARGETS scsitapealert-sd DESTINATION ${plugindir})
  if(HAVE_DARWIN_OS)
    target_link_libraries(scsitapealert-sd bareossd)
  endif()
endif()

if(Python2_FOUND)
  add_library(python-sd MODULE python/python-sd.cc)
  set_target_properties(python-sd PROPERTIES PREFIX "")
  install(TARGETS python-sd DESTINATION ${plugindir})
  target_link_libraries(python-sd ${Python2_LIBRARIES} bareos)
  target_include_directories(python-sd PUBLIC SYSTEM ${Python2_INCLUDE_DIRS})
  add_dependencies(python-sd bareossd-python2-module)

  set(PYFILES
      python/pyfiles/bareos-sd-class-plugin.py
      python/pyfiles/BareosSdPluginBaseclass.py
      python/pyfiles/bareos-sd.py.template python/pyfiles/BareosSdWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(Python2_FOUND AND NOT HAVE_WIN32)
  configure_file(python/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py)
  add_custom_command(
    OUTPUT pythonmodules/bareossd.so
    MAIN_DEPENDENCY ./python/bareossd.cc
    DEPENDS ./python/bareossd.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python2_EXECUTABLE} python/setup.py build
    COMMAND ${Python2_EXECUTABLE} python/setup.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/pythonmodules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python module pythonmodules/bareossd.so"
  )

  add_custom_target(bareossd-python2-module DEPENDS pythonmodules/bareossd.so)

  add_test(NAME bareossd-python2-module
           COMMAND ${Python2_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/python/test/bareossd_test.py
  )
  set_property(
    TEST bareossd-python2-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/python/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
  )
endif()

if(Python3_FOUND)
  add_library(python3-sd MODULE python/python-sd.cc)
  set_target_properties(python3-sd PROPERTIES PREFIX "")
  install(TARGETS python3-sd DESTINATION ${plugindir})
  target_link_libraries(python3-sd ${Python3_LIBRARIES} bareos)
  target_include_directories(python3-sd PUBLIC SYSTEM ${Python3_INCLUDE_DIRS})
  add_dependencies(python3-sd bareossd-python3-module)

  set(PYFILES
      python/pyfiles/bareos-sd-class-plugin.py
      python/pyfiles/BareosSdPluginBaseclass.py
      python/pyfiles/bareos-sd.py.template python/pyfiles/BareosSdWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  configure_file(python/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py)
  add_custom_command(
    OUTPUT python3modules/bareossd.so
    MAIN_DEPENDENCY ./python/bareossd.cc
    DEPENDS ./python/bareossd.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python/setup.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python3_EXECUTABLE} python/setup.py build
    COMMAND ${Python3_EXECUTABLE} python/setup.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/python3modules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python module python3modules/bareossd.so"
  )

  add_custom_target(bareossd-python3-module DEPENDS python3modules/bareossd.so)

  add_test(NAME bareossd-python3-module
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/python/test/bareossd_test.py
  )
  set_property(
    TEST bareossd-python3-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/python/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
  )
endif()
