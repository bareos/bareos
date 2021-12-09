#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2021 Bareos GmbH & Co. KG
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

if(BUILD_UNIVERSAL_CLIENT)
  file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-universal-client
       DEBIAN_CONTROL_BAREOS_FILEDAEMON
  )
  file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-universal-client-dbg
       DEBIAN_CONTROL_BAREOS_DBG
  )
else()

  if(NOT client-only)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos DEBIAN_CONTROL_BAREOS)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-database
         DEBIAN_CONTROL_BAREOS_DATABASE
    )
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-director
         DEBIAN_CONTROL_BAREOS_DIRECTOR
    )
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage
         DEBIAN_CONTROL_BAREOS_STORAGE
    )
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-tools
         DEBIAN_CONTROL_BAREOS_TOOLS
    )
  endif()

  file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon
       DEBIAN_CONTROL_BAREOS_FILEDAEMON
  )
  file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-common
       DEBIAN_CONTROL_BAREOS_COMMON
  )
  file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-dbg
       DEBIAN_CONTROL_BAREOS_DBG
  )

  if(ENABLE_BCONSOLE)
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-client
         DEBIAN_CONTROL_BAREOS_CLIENT
    )
    file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-bconsole
         DEBIAN_CONTROL_BAREOS_BCONSOLE
    )
  endif()

  if(GENERATE_DEBIAN_CONTROL)

    if(HAVE_TRAYMONITOR)
      file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-traymonitor
           DEBIAN_CONTROL_TRAYMONITOR
      )
    endif()

    if(HAVE_GLUSTERFS)
      file(READ
           ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-glusterfs-plugin
           DEBIAN_CONTROL_FILEDAEMON_GLUSTERFS_PLUGIN
      )
      if(NOT client-only)
        file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-glusterfs
             DEBIAN_CONTROL_STORAGE_GLUSTERFS
        )
      endif()
    endif()

    if(HAVE_CEPHFS)
      file(READ
           ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-ceph-plugin
           DEBIAN_CONTROL_FILEDAEMON_CEPH_PLUGIN
      )
      if(NOT client-only)
        file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-ceph
             DEBIAN_CONTROL_STORAGE_CEPH
        )
      endif()
    endif()

    if(NOT client-only AND TARGET droplet)
      file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-droplet
           DEBIAN_CONTROL_STORAGE_DROPLET
      )
    endif()

    # python (2) plugins
    if(ENABLE_PYTHON AND Python2_FOUND)
      file(READ
           ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-python2-plugin
           DEBIAN_CONTROL_FILEDAEMON_PYTHON_PLUGIN
      )
      if(NOT client-only)
        file(READ
             ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-python2-plugin
             DEBIAN_CONTROL_STORAGE_PYTHON_PLUGIN
        )
        file(READ
             ${CMAKE_SOURCE_DIR}/debian/control.bareos-director-python2-plugin
             DEBIAN_CONTROL_DIRECTOR_PYTHON_PLUGIN
        )
      endif()
    endif()

    # python 3 plugins
    if(ENABLE_PYTHON AND Python3_FOUND)
      file(READ
           ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-python3-plugin
           DEBIAN_CONTROL_FILEDAEMON_PYTHON3_PLUGIN
      )
      if(NOT client-only)
        file(READ
             ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-python3-plugin
             DEBIAN_CONTROL_STORAGE_PYTHON3_PLUGIN
        )
        file(READ
             ${CMAKE_SOURCE_DIR}/debian/control.bareos-director-python3-plugin
             DEBIAN_CONTROL_DIRECTOR_PYTHON3_PLUGIN
        )
      endif()
    endif()

    # python plugin common files
    if(ENABLE_PYTHON AND (Python2_FOUND OR Python3_FOUND))
      file(
        READ
        ${CMAKE_SOURCE_DIR}/debian/control.bareos-filedaemon-python-plugins-common
        DEBIAN_CONTROL_FILEDAEMON_PYTHON_PLUGINS_COMMON
      )
      if(NOT client-only)
        file(
          READ
          ${CMAKE_SOURCE_DIR}/debian/control.bareos-storage-python-plugins-common
          DEBIAN_CONTROL_STORAGEDAEMON_PYTHON_PLUGINS_COMMON
        )
        file(
          READ
          ${CMAKE_SOURCE_DIR}/debian/control.bareos-director-python-plugins-common
          DEBIAN_CONTROL_DIRECTOR_PYTHON_PLUGINS_COMMON
        )
      endif()
    endif()

    if(${BAREOS_PLATFORM} MATCHES "univention")
      # only required for univention
      file(READ ${CMAKE_SOURCE_DIR}/debian/control.univention-bareos
           DEBIAN_CONTROL_UNIVENTION_BAREOS
      )
    endif()

    if(NOT client-only)
      file(READ ${CMAKE_SOURCE_DIR}/debian/control.bareos-webui
           DEBIAN_CONTROL_BAREOS_WEBUI
      )
    endif()

    if(NOT client-only AND VIXDISKLIB_FOUND)
      file(READ ${CMAKE_SOURCE_DIR}/debian/control.vmware DEBIAN_CONTROL_VMWARE)
    endif()

  endif()
endif()
