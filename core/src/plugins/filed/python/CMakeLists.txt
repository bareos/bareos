#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2022 Bareos GmbH & Co. KG
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
  target_include_directories(python-fd PUBLIC ${Python2_INCLUDE_DIRS})
  target_compile_definitions(python-fd PUBLIC PY2MODDIR=\"${Python2_SITEARCH}\")
  target_compile_options(
    python-fd PUBLIC ${Python2_CCSHARED} -Wno-missing-field-initializers
  )
  add_dependencies(bareos-fd python-fd)
  add_dependencies(python-fd bareosfd-python2-module)
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
  target_include_directories(python3-fd PUBLIC ${Python3_INCLUDE_DIRS})
  target_compile_definitions(
    python3-fd PUBLIC PY3MODDIR=\"${Python3_SITEARCH}\"
  )
  target_compile_options(python3-fd PUBLIC -Wno-missing-field-initializers)
  add_dependencies(bareos-fd python3-fd)
  add_dependencies(python3-fd bareosfd-python3-module)
endif()

if(Python2_FOUND)
  add_executable(bareosfd-python2-module-tester test/python-fd-module-tester.cc)
  target_link_libraries(
    bareosfd-python2-module-tester ${Python2_LIBRARIES} bareos
  )
  target_include_directories(
    bareosfd-python2-module-tester PUBLIC ${Python2_INCLUDE_DIRS}
  )
  target_compile_options(
    bareosfd-python2-module-tester PUBLIC ${Python2_CCSHARED}
  )
  add_test(NAME bareosfd-python2-module-tester
           COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bareosfd-python2-module-tester
  )
  set_property(
    TEST bareosfd-python2-module-tester
    PROPERTY
      ENVIRONMENT
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/test/:${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/pyfiles
      "LSAN_OPTIONS=fast_unwind_on_malloc=0 suppressions=${CMAKE_CURRENT_SOURCE_DIR}/test/lsan-suppressions.txt"
  )
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_property(TEST bareosfd-python2-module-tester PROPERTY DISABLED true)
  endif()
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  add_executable(bareosfd-python3-module-tester test/python-fd-module-tester.cc)
  target_link_libraries(
    bareosfd-python3-module-tester ${Python3_LIBRARIES} bareos
  )
  target_include_directories(
    bareosfd-python3-module-tester PUBLIC ${Python3_INCLUDE_DIRS}
  )

  add_test(NAME bareosfd-python3-module-tester
           COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bareosfd-python3-module-tester
  )
  set_property(
    TEST bareosfd-python3-module-tester
    PROPERTY
      ENVIRONMENT
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/test/:${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/pyfiles
      "LSAN_OPTIONS=fast_unwind_on_malloc=0 suppressions=${CMAKE_CURRENT_SOURCE_DIR}/test/lsan-suppressions.txt"
  )
endif()

if(Python2_FOUND)
  add_library(bareosfd-python2-module MODULE module/bareosfd.cc)
  install(TARGETS bareosfd-python2-module DESTINATION ${Python2_SITEARCH})
  set_target_properties(
    bareosfd-python2-module
    PROPERTIES PREFIX ""
               C_STANDARD 11
               # SUFFIX ${Python2_EXT_SUFFIX}
               LIBRARY_OUTPUT_DIRECTORY pythonmodules
               LIBRARY_OUTPUT_NAME bareosfd
               LINK_FLAGS "${Python2_BLDSHARED_FLAGS}"
               LINKER_LANGUAGE CXX
               INCLUDE_DIRECTORIES "${pymod2_include_dirs}"
  )
  target_compile_options(
    bareosfd-python2-module PUBLIC -fno-strict-aliasing
                                   -Wno-missing-field-initializers
  )
  if(NOT "${Python2_CCSHARED}" STREQUAL "")
    target_compile_options(bareosfd-python2-module PUBLIC ${Python2_CCSHARED})
  endif()
  target_link_libraries(bareosfd-python2-module bareos ${Python2_LIBRARIES})

  add_test(NAME bareosfd-python2-module
           COMMAND ${Python2_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/test/bareosfd_test.py
  )
  set_property(
    TEST bareosfd-python2-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../../lib
      "ASAN_OPTIONS=detect_leaks=0 verify_asan_link_order=0"
  )
  set_tests_properties(bareosfd-python2-module PROPERTIES LABELS "broken")
endif()

if(Python3_FOUND)
  add_library(bareosfd-python3-module MODULE module/bareosfd.cc)
  install(TARGETS bareosfd-python3-module DESTINATION ${Python3_SITEARCH})
  set_target_properties(
    bareosfd-python3-module
    PROPERTIES PREFIX ""
               C_STANDARD 11
               SUFFIX ${Python3_EXT_SUFFIX}
               LIBRARY_OUTPUT_DIRECTORY python3modules
               LIBRARY_OUTPUT_NAME bareosfd
               LINK_FLAGS "${Python3_BLDSHARED_FLAGS}"
               LINKER_LANGUAGE CXX
               INCLUDE_DIRECTORIES "${pymod3_include_dirs}"
  )
  if(NOT "${Python3_CCSHARED}" STREQUAL "")
    set_property(
      TARGET bareosfd-python3-module
      PROPERTY COMPILE_OPTIONS ${Python3_CCSHARED}
               -Wno-missing-field-initializers
    )
  endif()
  target_link_libraries(bareosfd-python3-module bareos ${Python3_LIBRARIES})

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
      "ASAN_OPTIONS=detect_leaks=0 verify_asan_link_order=0"
  )
  add_test(
    NAME python-simple-test-example
    COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/test/simple-test-example.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
  )
  set_property(
    # add current directory for documentation only
    TEST python-simple-test-example PROPERTY ENVIRONMENT PYTHONPATH=./
  )
  set_property(TEST python-simple-test-example PROPERTY DISABLED true)
endif()

set(PYFILES
    pyfiles/BareosFdPluginBaseclass.py
    pyfiles/BareosFdWrapper.py
    pyfiles/BareosFdPluginLocalFileset.py
    pyfiles/BareosFdPluginLocalFilesBaseclass.py
    pyfiles/bareos-fd-local-fileset.py
    ldap/bareos-fd-ldap.py
    ldap/BareosFdPluginLDAP.py
    mariabackup/BareosFdPluginMariabackup.py
    mariabackup/bareos-fd-mariabackup.py
    ovirt/bareos-fd-ovirt.py
    ovirt/BareosFdPluginOvirt.py
    percona-xtrabackup/bareos-fd-percona-xtrabackup.py
    percona-xtrabackup/BareosFdPluginPerconaXtraBackup.py
    postgres/bareos-fd-postgres.py
    postgres/BareosFdPluginPostgres.py
    vmware/bareos-fd-vmware.py
    vmware/BareosFdPluginVMware.py
    libcloud/BareosLibcloudApi.py
    libcloud/bareos-fd-libcloud.py
    libcloud/BareosFdPluginLibcloud.py
)

install(
  FILES ${PYFILES}
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)

install(
  DIRECTORY libcloud/bareos_libcloud_api
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
