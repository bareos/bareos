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


include_directories(../.. . ../../include ../../stored)

if(${HAVE_CEPHFS})
add_library(libbareossd-cephfs MODULE cephfs_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-cephfs PROPERTIES PREFIX "")
INSTALL(TARGETS libbareossd-cephfs DESTINATION ${backenddir})
target_link_libraries(libbareossd-cephfs ${CEPHFS_LIBRARIES})
endif()

if(${HAVE_CEPH_RADOS})
add_library(libbareossd-rados MODULE rados_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-rados PROPERTIES PREFIX "")
INSTALL(TARGETS libbareossd-rados DESTINATION ${backenddir})
target_link_libraries(libbareossd-rados ${RADOS_LIBRARIES})
endif()


if(${HAVE_GLUSTERFS})
add_library(libbareossd-gfapi MODULE gfapi_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-gfapi PROPERTIES PREFIX "")
INSTALL(TARGETS libbareossd-gfapi DESTINATION ${backenddir})
target_link_libraries(libbareossd-gfapi ${GFAPI_LIBRARIES})
endif()

if(${HAVE_DROPLET})
add_library(libbareossd-droplet MODULE droplet_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-droplet PROPERTIES PREFIX "")
target_link_libraries(libbareossd-droplet ${DROPLET_LIBRARIES})
#INSTALL(TARGETS libbareossd-droplet DESTINATION ${backenddir})

add_library(libbareossd-chunked MODULE chunked_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-chunked PROPERTIES PREFIX "")
target_link_libraries(libbareossd-chunked ${DROPLET_LIBRARIES})
#INSTALL(TARGETS libbareossd-chunked DESTINATION ${backenddir})
endif()

add_library(libbareossd-elasto MODULE elasto_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-elasto PROPERTIES PREFIX "")
#INSTALL(TARGETS libbareossd-elasto DESTINATION ${backenddir})

add_library(libbareossd-fifo MODULE unix_fifo_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-fifo PROPERTIES PREFIX "")
INSTALL(TARGETS libbareossd-fifo DESTINATION ${backenddir})

add_library(libbareossd-gentape MODULE generic_tape_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-gentape PROPERTIES PREFIX "")
INSTALL(TARGETS libbareossd-gentape DESTINATION ${backenddir})

add_library(libbareossd-tape MODULE unix_tape_device.cc)
# do not prefix with "lib"
set_target_properties(libbareossd-tape PROPERTIES PREFIX "")
INSTALL(TARGETS libbareossd-tape DESTINATION ${backenddir})
