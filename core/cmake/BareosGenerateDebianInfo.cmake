#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2019 Bareos GmbH & Co. KG
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


IF(GENERATE_DEBIAN_CONTROL)

   IF(HAVE_TRAYMONITOR)
      FILE(READ ./debian/control.bareos-traymonitor DEBIAN_CONTROL_TRAYMONITOR)
   ENDIF()

   IF(HAVE_GLUSTERFS)
      FILE(READ ./debian/control.bareos-filedaemon-glusterfs-plugin DEBIAN_CONTROL_FILEDAEMON_GLUSTERFS_PLUGIN)
      FILE(READ ./debian/control.bareos-storage-glusterfs DEBIAN_CONTROL_STORAGE_GLUSTERFS)
   ENDIF()

   IF(HAVE_CEPHFS)
      FILE(READ ./debian/control.bareos-filedaemon-ceph-plugin DEBIAN_CONTROL_FILEDAEMON_CEPH_PLUGIN)
      FILE(READ ./debian/control.bareos-storage-ceph DEBIAN_CONTROL_STORAGE_CEPH)
   ENDIF()

   IF(HAVE_PYTHON)
      FILE(READ ./debian/control.bareos-filedaemon-python-plugin DEBIAN_CONTROL_FILEDAEMON_PYTHON_PLUGIN)
      FILE(READ ./debian/control.bareos-storage-python-plugin DEBIAN_CONTROL_STORAGE_PYTHON_PLUGIN)
      FILE(READ ./debian/control.bareos-director-python-plugin DEBIAN_CONTROL_DIRECTOR_PYTHON_PLUGIN)
   ENDIF()

ENDIF()
