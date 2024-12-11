# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2024-2024 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#[=======================================================================[.rst:
FindgRPC
--------

Find grpc headers and libraries.
`
IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``gRPC::grpc++``
  grpc c++ library.
``gRPC::grpc++_reflection``
  grpc c++ reflection library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``gRPC_FOUND``
  TRUE if gRPC found.
``gRPC_CPP_PROTO_PLUGIN``
  path of plugin for protoc that generates grpc bindings
``gRPC_VERSION``
  version string of the grpc library

#]=======================================================================]

include(FindPackageHandleStandardArgs)

# Find gRPC installation Looks for gRPCConfig.cmake file installed by gRPC's
# cmake installation.
find_package(gRPC CONFIG)

# On some systems this file is not distributed, so if we cannot find it, then
# try with pkg-config instead.
if(NOT gRPC_FOUND)
  message(DEBUG "grpc not (yet) found, fallback to pkg_check_modules()")
  find_package(PkgConfig QUIET REQUIRED)
  pkg_check_modules(gRPC grpc++ REQUIRED)

  find_package_handle_standard_args(
    gRPC
    REQUIRED_VARS gRPC_LINK_LIBRARIES
    VERSION_VAR gRPC_VERSION
  )

  find_library(
    REFLECTION_LIBRARY
    NAMES grpc++_reflection
    HINTS ${gRPC_LIBRARY_DIRS} REQUIRED
  )

  if(NOT TARGET grpc++_reflection AND NOT TARGET gRPC::grpc++_reflection)
    message(DEBUG "adding grpc++_reflection library")
    add_library(grpc++_reflection UNKNOWN IMPORTED)
    set_target_properties(
      grpc++_reflection PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                   "${gRPC_INCLUDE_DIRS}"
    )
    set_target_properties(
      grpc++_reflection PROPERTIES INTERFACE_LINK_LIBRARIES
                                   "${gRPC_LINK_LIBRARIES}"
    )
    set_property(
      TARGET grpc++_reflection
      APPEND
      PROPERTY IMPORTED_LOCATION "${REFLECTION_LIBRARY}"
    )
  endif()

  find_library(
    CPP_BINDINGS
    NAMES grpc++
    HINTS ${gRPC_LIBRARY_DIRS} REQUIRED
  )

  if(NOT TARGET grpc++ AND NOT TARGET gRPC::grpc++)
    message(DEBUG "adding grpc++ library")
    add_library(grpc++ UNKNOWN IMPORTED)
    set_target_properties(
      grpc++ PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${gRPC_INCLUDE_DIRS}"
    )
    set_target_properties(
      grpc++ PROPERTIES INTERFACE_LINK_LIBRARIES "${gRPC_LINK_LIBRARIES}"
    )
    set_property(
      TARGET grpc++
      APPEND
      PROPERTY IMPORTED_LOCATION "${CPP_BINDINGS}"
    )
  endif()

  message(DEBUG "gRPC_VERSION: ${gRPC_VERSION}")
endif()

if(gRPC_FOUND)
  set(gRPC_FOUND TRUE)
endif()

if(NOT TARGET gRPC::grpc++_reflection)
  message(DEBUG "add namespaced alias for grpc++_reflection")
  add_library(gRPC::grpc++_reflection ALIAS grpc++_reflection)
endif()

if(NOT TARGET gRPC::grpc++)
  message(DEBUG "add namespaced alias for grpc++")
  add_library(gRPC::grpc++ ALIAS grpc++)
endif()

if(TARGET gRPC::grpc_cpp_plugin)
  set(gRPC_CPP_PROTO_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
elseif(TARGET grpc_cpp_plugin)
  set(gRPC_CPP_PROTO_PLUGIN $<TARGET_FILE:grpc_cpp_plugin>)
else()
  find_program(gRPC_CPP_PROTO_PLUGIN grpc_cpp_plugin REQUIRED)
endif()
message(DEBUG "plugin = ${gRPC_CPP_PROTO_PLUGIN}")

find_package_handle_standard_args(
  gRPC
  REQUIRED_VARS gRPC_CPP_PROTO_PLUGIN
  VERSION_VAR gRPC_VERSION
)

mark_as_advanced(gRPC_CPP_PROTO_PLUGIN)
