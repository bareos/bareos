#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2026 Bareos GmbH & Co. KG
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

  if(ENABLE_PYTHON AND Python3_FOUND)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-filedaemon-python3-plugin")
    list(APPEND DEBIAN_CONTROL_SNIPPETS
         "bareos-filedaemon-python-plugins-common"
    )
  endif()

  if(ENABLE_GRPC)
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-filedaemon-grpc-python3-plugin")
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

  if(ENABLE_BARRI)
    list(APPEND DEBIAN_CONTROL_SNIPPETS
         "bareos-filedaemon-barri-python3-plugin"
    )
    list(APPEND DEBIAN_CONTROL_SNIPPETS "bareos-barri-cli")
  endif()

  if(ENABLE_PROXMOX_PLUGIN)
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
