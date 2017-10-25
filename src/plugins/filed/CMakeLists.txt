#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


include_directories(../.. . ../../include ../../filed /usr/include/python2.7)


#set(DLLIBS dl)

add_library(bpipe-fd MODULE bpipe-fd.cc)
# do not prefix with "lib"
set_target_properties(bpipe-fd PROPERTIES PREFIX "")
INSTALL(TARGETS bpipe-fd DESTINATION ${plugindir})


if(${HAVE_CEPHFS})
add_library(cephfs-fd MODULE cephfs-fd.cc)
# do not prefix with "lib"
set_target_properties(cephfs-fd PROPERTIES PREFIX "")
INSTALL(TARGETS cephfs-fd DESTINATION ${plugindir})
endif()

if(${HAVE_CEPH_RADOS})
add_library(rados-fd MODULE rados-fd.cc)
# do not prefix with "lib"
set_target_properties(rados-fd PROPERTIES PREFIX "")
INSTALL(TARGETS rados-fd DESTINATION ${plugindir})
endif()

add_library(example-plugin-fd MODULE example-plugin-fd.cc)
# do not prefix with "lib"
set_target_properties(example-plugin-fd PROPERTIES PREFIX "")

add_library(python-fd MODULE python-fd.cc)
# do not prefix with "lib"
set_target_properties(python-fd PROPERTIES PREFIX "")
INSTALL(TARGETS python-fd DESTINATION ${plugindir})

if(${HAVE_GLUSTERFS})
add_library(gfapi-fd MODULE gfapi-fd.cc)
# do not prefix with "lib"
set_target_properties(gfapi-fd PROPERTIES PREFIX "")
INSTALL(TARGETS gfapi-fd DESTINATION ${plugindir})
endif()

set (PYFILES
    bareos-fd.py.template
    bareos-fd-local-fileset.py
    bareos-fd-mock-test.py
    BareosFdPluginBaseclass.py
    BareosFdPluginLocalFileset.py
    BareosFdWrapper.py
    bareos_fd_consts.py
    bareos-fd-ldap.py
    BareosFdPluginLDAP.py
    )

INSTALL(FILES ${PYFILES} DESTINATION ${plugindir})


#add_library(test-plugin-fd MODULE test-plugin-fd.cc)
## do not prefix with "lib"
#set_target_properties(test-plugin-fd PROPERTIES PREFIX "")
#INSTALL(TARGETS test-plugin-fd DESTINATION ${plugindir})
