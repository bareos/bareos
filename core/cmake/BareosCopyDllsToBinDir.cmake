#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2025 Bareos GmbH & Co. KG
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

# Windows: copy over all dlls our binaries depend on into the binary dir Also
# create a file containing all dlls required

macro(BareosCopyDllsToBinDir)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND MSVC)
    get_property(
      current_targets
      DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      PROPERTY BUILDSYSTEM_TARGETS
    )
    foreach(tgt ${current_targets})
      get_target_property(targettype ${tgt} TYPE)
      if(${targettype} MATCHES "EXECUTABLE" OR ${targettype} MATCHES
                                               "SHARED_LIBRARY"
      )
        add_custom_command(
          TARGET ${tgt}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${tgt}>
                  $<TARGET_RUNTIME_DLLS:${tgt}>
          COMMAND_EXPAND_LISTS USES_TERMINAL
        )
      endif()
    endforeach()
  endif()
endmacro()
