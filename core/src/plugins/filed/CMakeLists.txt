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
    ${PROJECT_SOURCE_DIR}/src/win32
    ${PROJECT_SOURCE_DIR}/src/win32/include
    ${PROJECT_SOURCE_DIR}/src/win32/filed
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/filed
    ${PROJECT_SOURCE_DIR}/src/win32/compat/include
    ${PROJECT_SOURCE_DIR}/src/win32/vdi/include
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/filed
    ${PROJECT_SOURCE_DIR}/src/plugins/filed
  )

  link_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS}
  )
  add_definitions(-DMS_WIN${WINDOWS_BITS})
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

# set(DLLIBS dl)

add_library(bpipe-fd MODULE bpipe/bpipe-fd.cc)
set_target_properties(bpipe-fd PROPERTIES PREFIX "")
install(
  TARGETS bpipe-fd
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
if(HAVE_WIN32 OR HAVE_DARWIN_OS)
  target_link_libraries(bpipe-fd bareos)
endif()

if(HAVE_WIN32)
  add_library(mssqlvdi-fd MODULE ../../win32/plugins/filed/mssqlvdi-fd.cc)
  target_compile_options(mssqlvdi-fd PUBLIC -fpermissive)
  set_target_properties(
    mssqlvdi-fd PROPERTIES PREFIX "" DEFINE_SYMBOL "BUILDING_DLL"
  )

  install(
    TARGETS mssqlvdi-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(mssqlvdi-fd bareos)
endif()

if(${HAVE_CEPHFS})
  add_library(cephfs-fd MODULE cephfs/cephfs-fd.cc)
  set_target_properties(cephfs-fd PROPERTIES PREFIX "")
  install(
    TARGETS cephfs-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

if(${HAVE_CEPH_RADOS})
  add_library(rados-fd MODULE rados/rados-fd.cc)
  set_target_properties(rados-fd PROPERTIES PREFIX "")
  install(
    TARGETS rados-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

if(NOT HAVE_WIN32)
  add_library(example-plugin-fd MODULE example/example-plugin-fd.cc)
  set_target_properties(example-plugin-fd PROPERTIES PREFIX "")
endif()

if(Python2_FOUND)
  add_library(python-fd MODULE python/python-fd.cc)
  set_target_properties(python-fd PROPERTIES PREFIX "")
  install(
    TARGETS python-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(python-fd ${Python2_LIBRARIES} bareos)
  target_include_directories(python-fd PUBLIC SYSTEM ${Python2_INCLUDE_DIRS})
  add_dependencies(python-fd bareosfd-pymod2)
  add_dependencies(bareos-fd python-fd)
endif()

if(Python3_FOUND)
  add_library(python3-fd MODULE python/python-fd.cc)
  set_target_properties(python3-fd PROPERTIES PREFIX "")
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
  add_executable(
    bareosfd-python2-module-tester python/test/python-fd-module-tester.cc
  )
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
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python/test/:${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/python/pyfiles
  )
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  add_executable(
    bareosfd-python3-module-tester python/test/python-fd-module-tester.cc
  )
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
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python/test/:${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/python/pyfiles
  )
endif()

if(Python2_FOUND AND NOT HAVE_WIN22)
  configure_file(
    python/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/python/setup_py2.py
  )
  add_custom_command(
    OUTPUT pythonmodules/bareosfd.so
    MAIN_DEPENDENCY ./python/bareosfd.cc
    DEPENDS ./python/bareosfd.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python/setup_py2.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python2_EXECUTABLE} python/setup_py2.py build
    COMMAND ${Python2_EXECUTABLE} python/setup_py2.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/pythonmodules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python2 module pythonmodules/bareosfd.so"
  )
  add_custom_target(bareosfd-pymod2 DEPENDS pythonmodules/bareosfd.so)
  add_test(NAME bareosfd-python2-module
           COMMAND ${Python2_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/python/test/bareosfd_test.py
  )
  set_property(
    TEST bareosfd-python2-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/pythonmodules:${CMAKE_CURRENT_SOURCE_DIR}/python/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
  )
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  configure_file(
    python/setup.py.in ${CMAKE_CURRENT_BINARY_DIR}/python/setup_py3.py
  )
  add_custom_command(
    OUTPUT python3modules/bareosfd.so
    MAIN_DEPENDENCY ./python/bareosfd.cc
    DEPENDS ./python/bareosfd.h
    DEPENDS bareos
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/python/setup_py3.py
    COMMAND rm -Rfv ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMAND ${Python3_EXECUTABLE} python/setup_py3.py build
    COMMAND ${Python3_EXECUTABLE} python/setup_py3.py install --install-lib
            ${CMAKE_CURRENT_BINARY_DIR}/python3modules
    COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/build
    COMMENT "building python3 module python3modules/bareosfd.so"
  )
  add_custom_target(bareosfd-pymod3 DEPENDS python3modules/bareosfd.so)
  add_test(NAME bareosfd-python3-module
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/python/test/bareosfd_test.py
  )
  set_property(
    TEST bareosfd-python3-module
    PROPERTY
      ENVIRONMENT
      "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/python/test"
      LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../lib
  )
endif()

if(${HAVE_GLUSTERFS})
  add_library(gfapi-fd MODULE gfapi/gfapi-fd.cc)
  set_target_properties(gfapi-fd PROPERTIES PREFIX "")
  install(
    TARGETS gfapi-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(gfapi-fd gfapi)
  target_include_directories(gfapi-fd PRIVATE ${GFAPI_INCLUDE_DIRS})
endif()

if(${HAVE_TEST_PLUGIN})
  add_library(test-plugin-fd MODULE test-plugin/test-plugin-fd.cc)
  # do not prefix with "lib"
  set_target_properties(test-plugin-fd PROPERTIES PREFIX "")
  install(
    TARGETS test-plugin-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

set(PYFILES
    python/pyfiles/bareos-fd.py.template
    python/pyfiles/bareos-fd-local-fileset.py
    python/pyfiles/bareos-fd-mock-test.py
    python/pyfiles/BareosFdPluginBaseclass.py
    python/pyfiles/BareosFdPluginLocalFileset.py
    python/pyfiles/BareosFdWrapper.py
    python/pyfiles/bareos-fd.py.template
    python/pyfiles/bareos-fd-postgres.py
    python/pyfiles/BareosFdPluginPostgres.py
    python/pyfiles/bareos-fd-vmware.py
    python/pyfiles/BareosFdPluginVMware.py
    python/pyfiles/ldap/BareosFdPluginLDAP.py
    python/pyfiles/ldap/bareos-fd-ldap.py
    python/pyfiles/ovirt/BareosFdPluginOvirt.py
    python/pyfiles/ovirt/bareos-fd-ovirt.py
    python/pyfiles/percona-xtrabackup/BareosFdPluginPerconaXtraBackup.py
    python/pyfiles/percona-xtrabackup/bareos-fd-percona-xtrabackup.py
)
if(${HAVE_VIXDISKLIB})
  list(APPEND PYFILES BareosFdPluginVMware.py bareos-fd-vmware.py)
endif()

install(
  FILES ${PYFILES}
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
