#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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
else()
  include_directories(/usr/include/python2.7)
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

# set(DLLIBS dl)

add_library(bpipe-fd MODULE bpipe-fd.cc)
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
  add_library(cephfs-fd MODULE cephfs-fd.cc)
  set_target_properties(cephfs-fd PROPERTIES PREFIX "")
  install(
    TARGETS cephfs-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

if(${HAVE_CEPH_RADOS})
  add_library(rados-fd MODULE rados-fd.cc)
  set_target_properties(rados-fd PROPERTIES PREFIX "")
  install(
    TARGETS rados-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

if(NOT HAVE_WIN32)
  add_library(example-plugin-fd MODULE example-plugin-fd.cc)
  set_target_properties(example-plugin-fd PROPERTIES PREFIX "")
endif()

if(HAVE_PYTHON)
  add_library(python-fd MODULE python-fd.cc)
  set_target_properties(python-fd PROPERTIES PREFIX "")
  install(
    TARGETS python-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(python-fd ${PYTHON_LIBRARIES} bareos)
endif()

if(${HAVE_GLUSTERFS})
  add_library(gfapi-fd MODULE gfapi-fd.cc)
  set_target_properties(gfapi-fd PROPERTIES PREFIX "")
  install(
    TARGETS gfapi-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(gfapi-fd gfapi)
endif()

if(${HAVE_TEST_PLUGIN})
  add_library(test-plugin-fd MODULE test-plugin-fd.cc)
  # do not prefix with "lib"
  set_target_properties(test-plugin-fd PROPERTIES PREFIX "")
  install(
    TARGETS test-plugin-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

set(PYFILES
    bareos-fd.py.template
    bareos-fd-local-fileset.py
    bareos-fd-mock-test.py
    BareosFdPluginBaseclass.py
    BareosFdPluginLocalFileset.py
    BareosFdWrapper.py
    bareos_fd_consts.py
    bareos-fd-ldap.py
    BareosFdPluginLDAP.py
    bareos-fd-ovirt.py
    BareosFdPluginOvirt.py
    )

install(
  FILES ${PYFILES}
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
