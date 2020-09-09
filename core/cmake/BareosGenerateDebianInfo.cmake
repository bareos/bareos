#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2020 Bareos GmbH & Co. KG
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

if(GENERATE_DEBIAN_CONTROL)

  if(HAVE_TRAYMONITOR)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-traymonitor DEBIAN_CONTROL_TRAYMONITOR)
  endif()

  if(HAVE_GLUSTERFS)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-glusterfs-plugin
         DEBIAN_CONTROL_FILEDAEMON_GLUSTERFS_PLUGIN)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-glusterfs
         DEBIAN_CONTROL_STORAGE_GLUSTERFS)
  endif()

  if(HAVE_CEPHFS)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-ceph-plugin
         DEBIAN_CONTROL_FILEDAEMON_CEPH_PLUGIN)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-ceph DEBIAN_CONTROL_STORAGE_CEPH)
  endif()

  #python (2) plugins
  if(TARGET python-fd)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-python-plugin
         DEBIAN_CONTROL_FILEDAEMON_PYTHON_PLUGIN)
  endif()
  if(TARGET python-sd)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-python-plugin
         DEBIAN_CONTROL_STORAGE_PYTHON_PLUGIN)
  endif()
  if(TARGET python-dir)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-director-python-plugin
         DEBIAN_CONTROL_DIRECTOR_PYTHON_PLUGIN)
  endif()

  #python 3 plugins
  if(TARGET python3-fd)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-python3-plugin
         DEBIAN_CONTROL_FILEDAEMON_PYTHON3_PLUGIN)
  endif()
  if(TARGET python3-sd)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-python3-plugin
         DEBIAN_CONTROL_STORAGE_PYTHON3_PLUGIN)
  endif()
  if(TARGET python3-dir)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-director-python3-plugin
         DEBIAN_CONTROL_DIRECTOR_PYTHON3_PLUGIN)
  endif()

  # python plugin common files
  if(TARGET python-fd OR TARGET python3-fd)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-python-plugins-common
      DEBIAN_CONTROL_FILEDAEMON_PYTHON_PLUGINS_COMMON)
  endif()
  if(TARGET python-sd OR TARGET python3-sd)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storagedaemon-python-plugins-common
      DEBIAN_CONTROL_STORAGEDAEMON_PYTHON_PLUGINS_COMMON)
  endif()
  if(TARGET python-dir OR TARGET python3-dir)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-director-python-plugins-common
      DEBIAN_CONTROL_DIRECTOR_PYTHON_PLUGINS_COMMON)
  endif()



  if(${PLATFORM} MATCHES "univention")
    # only required for univention
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.univention-bareos
         DEBIAN_CONTROL_UNIVENTION_BAREOS)
  endif()

  file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-webui DEBIAN_CONTROL_BAREOS_WEBUI)

  if (VIXDISKLIB_FOUND)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.vmware
      DEBIAN_CONTROL_VMWARE)
  endif()

endif()
