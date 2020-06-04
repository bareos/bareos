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
  add_library(python-fd MODULE python-fd.cc)
  set_target_properties(
    python-fd PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ..
  )
  install(
    TARGETS python-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(python-fd ${Python2_LIBRARIES} bareos)
  target_include_directories(python-fd PUBLIC SYSTEM ${Python2_INCLUDE_DIRS})
  # add_dependencies(python-fd bareosfd-pymod)
  add_dependencies(bareos-fd python-fd)
endif()

if(Python3_FOUND)
  add_library(python3-fd MODULE python-fd.cc)
  set_target_properties(
    python3-fd PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ..
  )
  install(
    TARGETS python3-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(python3-fd ${Python3_LIBRARIES} bareos)
  target_include_directories(python3-fd PUBLIC SYSTEM ${Python3_INCLUDE_DIRS})
  add_dependencies(python3-fd bareosfd-pymod3)
  add_dependencies(bareos-fd python3-fd)
endif()

if(Python2_FOUND AND NOT HAVE_WIN22)
  add_executable(bareosfd-python2-module-tester test/python-fd-module-tester.cc)
  target_link_libraries(
    bareosfd-python2-module-tester ${Python2_LIBRARIES} bareos
  )
  target_include_directories(
    bareosfd-python2-module-tester PUBLIC SYSTEM ${Python2_INCLUDE_DIRS}
  )

  add_test(NAME bareosfd-python2-module-tester
           COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bareosfd-python2-module-tester
  )
  set_property(
    TEST bareosfd-python2-module-tester
    PROPERTY
      ENVIRONMENT
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/test/:${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/pyfiles
  )
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  add_executable(bareosfd-python3-module-tester test/python-fd-module-tester.cc)
  target_link_libraries(
    bareosfd-python3-module-tester ${Python3_LIBRARIES} bareos
  )
  target_include_directories(
    bareosfd-python3-module-tester PUBLIC SYSTEM ${Python3_INCLUDE_DIRS}
  )

  add_test(NAME bareosfd-python3-module-tester
           COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bareosfd-python3-module-tester
  )
  set_property(
    TEST bareosfd-python3-module-tester
    PROPERTY
      ENVIRONMENT
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/test/:${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/pyfiles
  )
endif()

if(Python2_FOUND AND NOT HAVE_WIN22)
  # configure_file( module/setup.py.in
  # ${CMAKE_CURRENT_BINARY_DIR}/module/setup_py2.py ) add_custom_command( OUTPUT
  # pythonmodules/bareosfd.so MAIN_DEPENDENCY module/bareosfd.cc DEPENDS
  # module/bareosfd.h DEPENDS bareos DEPENDS
  # ${CMAKE_CURRENT_BINARY_DIR}/module/setup_py2.py # COMMAND rm -Rfv
  # ${CMAKE_CURRENT_BINARY_DIR}/build COMMAND ${Python2_EXECUTABLE}
  # module/setup_py2.py build COMMAND ${Python2_EXECUTABLE} module/setup_py2.py
  # install --install-lib ${CMAKE_CURRENT_BINARY_DIR}/pythonmodules # COMMAND rm
  # -Rf ${CMAKE_CURRENT_BINARY_DIR}/build COMMENT "building python2 module
  # pythonmodules/bareosfd.so" ) add_custom_target(bareosfd-pymod DEPENDS
  # pythonmodules/bareosfd.so)

  add_library(bareosfd MODULE module/bareosfd.cc)
  target_link_libraries(bareosfd bareos)
  set_target_properties(
    bareosfd PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY ..
  )

  add_test(NAME bareosfd-python-module
           COMMAND ${Python2_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test/bareosfd_test.py
  )
  set_property(
    TEST bareosfd-python-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../../lib
  )
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  configure_file(
    module/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/module/setup_py3.py
  )
  add_custom_command(
    OUTPUT python3modules/bareosfd.so
    MAIN_DEPENDENCY module/bareosfd.cc
    DEPENDS module/bareosfd.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/module/setup_py3.py
    # COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python3_EXECUTABLE} module/setup_py3.py build
    COMMAND ${Python3_EXECUTABLE} module/setup_py3.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/python3modules
    # COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python3 module python3modules/bareosfd.so"
  )
  add_custom_target(bareosfd-pymod3 DEPENDS python3modules/bareosfd.so)
  add_test(NAME bareosfd-python3-module
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test/bareosfd_test.py
  )
  set_property(
    TEST bareosfd-python3-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../../lib
  )
endif()

set(PYFILES
    pyfiles/bareos-fd.py.template
    pyfiles/bareos-fd-local-fileset.py
    pyfiles/bareos-fd-mock-test.py
    pyfiles/BareosFdPluginBaseclass.py
    pyfiles/BareosFdPluginLocalFileset.py
    pyfiles/BareosFdWrapper.py
    ldap/bareos-fd-ldap.py
    ldap/BareosFdPluginLDAP.py
    ovirt/bareos-fd-ovirt.py
    ovirt/BareosFdPluginOvirt.py
    percona-xtrabackup/bareos-fd-percona-xtrabackup.py
    percona-xtrabackup/BareosFdPluginPerconaXtraBackup.py
    postgres/bareos-fd-postgres.py
    postgres/BareosFdPluginPostgres.py
)

install(
  FILES ${PYFILES}
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
