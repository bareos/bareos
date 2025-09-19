#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2025 Bareos GmbH & Co. KG
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

if(MSVC)
  return()
endif()

# cmake_host_system_information() was added in 3.22
if(CMAKE_VERSION VERSION_GREATER 3.22)
  cmake_host_system_information(RESULT PRETTY_NAME QUERY DISTRIB_PRETTY_NAME)
  message(STATUS "${PRETTY_NAME}")
  
  cmake_host_system_information(RESULT DIST_ID QUERY DISTRIB_ID)
  message(STATUS "${DIST_ID}")
  
  cmake_host_system_information(RESULT DIST_VERSION_ID QUERY DISTRIB_VERSION_ID)
  message(STATUS "${DIST_VERSION_ID}")


#--  DISTRO_ANSI_COLOR=`0;38;2;60;110;180` 
#--  DISTRO_BUG_REPORT_URL=`https://bugzilla.redhat.com/` 
#--  DISTRO_CPE_NAME=`cpe:/o:fedoraproject:fedora:42` 
#--  DISTRO_DEFAULT_HOSTNAME=`fedora` 
#--  DISTRO_DOCUMENTATION_URL=`https://docs.fedoraproject.org/en-US/fedora/f42/` 
#--  DISTRO_HOME_URL=`https://fedoraproject.org/` 
#--  DISTRO_ID=`fedora` 
#--  DISTRO_LOGO=`fedora-logo-icon` 
#--  DISTRO_NAME=`Fedora Linux` 
#--  DISTRO_PLATFORM_ID=`platform:f42` 
#--  DISTRO_PRETTY_NAME=`Fedora Linux 42 (Adams)` 
#--  DISTRO_REDHAT_BUGZILLA_PRODUCT=`Fedora` 
#--  DISTRO_REDHAT_BUGZILLA_PRODUCT_VERSION=`42` 
#--  DISTRO_REDHAT_SUPPORT_PRODUCT=`Fedora` 
#--  DISTRO_REDHAT_SUPPORT_PRODUCT_VERSION=`42` 
#--  DISTRO_RELEASE_TYPE=`stable` 
#--  DISTRO_SUPPORT_END=`2026-05-13` 
#--  DISTRO_SUPPORT_URL=`https://ask.fedoraproject.org/` 
#--  DISTRO_VERSION=`42 (Adams)` 
#--  DISTRO_VERSION_ID=`42` 

  # foreach(VAR IN LISTS DISTRO)
  #   message(STATUS " ${VAR}=`${${VAR}}` ")
  # endforeach()
endif()

# always add "src" package snippet
set(DEBIAN_CONTROL_SNIPPETS "src")

if(BUILD_UNIVERSAL_CLIENT)
  list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-universal-client")
  list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-universal-client-dbg")
else()
  if(NOT client-only)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos")
  endif()
  if(ENABLE_BCONSOLE)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-bconsole")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-client")
  endif()
  list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-common")
  if(NOT client-only)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-database")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-dbg")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-director")
  endif()
  list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-filedaemon")
  if(NOT client-only)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-storage")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-tools")
  endif()

  if(NOT client-only
     AND ENABLE_PYTHON
     AND (Python3_FOUND)
  )
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-director-python3-plugin")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-director-python-plugins-common")
  endif()

  if(HAVE_GFAPI)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-filedaemon-glusterfs-plugin")
  endif()
  if(ENABLE_PYTHON AND Python3_FOUND)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-filedaemon-python3-plugin")
    list(APPEND DEBIAN_CONTROL_SNIPPETS
         "bareos-filedaemon-python-plugins-common"
    )
  endif()

  if(ENABLE_GRPC)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-filedaemon-grpc-python3-plugin")
  endif()
  if(NOT client-only AND TARGET droplet)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-storage-droplet")
  endif()
  if(NOT client-only AND HAVE_GFAPI)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-storage-glusterfs")
  endif()
  if(NOT client-only
     AND ENABLE_PYTHON
     AND Python3_FOUND
  )
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-storage-python3-plugin")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-storage-python-plugins-common")
  endif()

  if(traymonitor)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-traymonitor")
  endif()

  if(${BAREOS_PLATFORM} MATCHES "univention")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "univention-bareos")
  endif()

  if(NOT client-only)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-webui")
  endif()

  if(NOT client-only AND VIXDISKLIB_FOUND)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "vmware")
  endif()

  if(NOT client-only)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-contrib-tools")
  endif()

  if(DIST_ID STREQUAL "debian" AND DIST_VERSION_ID MATCHES "13.*")
    list(APPEND DEBIAN_CONTROL_SNIPPETS "proxmox")
  endif()

endif()

include(BareosConfigureFile)
bareos_configure_file(GLOB "${CMAKE_SOURCE_DIR}/debian/*.in")

file(GLOB templated_file_path_list "${CMAKE_BINARY_DIR}/debian/*")
set(DEBIAN_TEMPLATED_FILE_LIST "")
foreach(templated_file_path ${templated_file_path_list})
  get_filename_component(templated_file ${templated_file_path} NAME)
  list(APPEND DEBIAN_TEMPLATED_FILE_LIST "${templated_file}")
endforeach()

configure_file(
  ${CMAKE_SOURCE_DIR}/core/cmake/generate-debian-control.cmake.in
  ${CMAKE_BINARY_DIR}/generate-debian-control.cmake @ONLY
)

add_custom_target(
  generate-debian-control
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/generate-debian-control.cmake
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/debian"
)
