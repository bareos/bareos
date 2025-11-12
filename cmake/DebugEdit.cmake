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

find_program(DEBUGEDIT_PROG debugedit HINTS /usr/lib/rpm)

option(DEBUGEDIT_REWRITE
       "Rewrite DW_AT_comp_dir in ELF objects to the real path" ON
)

if(DEBUGEDIT_REWRITE
   AND DEBUGEDIT_PROG
   AND NOT HAVE_WIN32
)
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/fix-debug-path.sh.in"
    "${CMAKE_BINARY_DIR}/cmake/fix-debug-path.sh" @ONLY
  )

  get_directory_property(directory_targets BUILDSYSTEM_TARGETS)
  foreach(target ${directory_targets})
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL EXECUTABLE
       OR target_type STREQUAL SHARED_LIBRARY
       OR target_type STREQUAL MODULE_LIBRARY
    )
      add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND "${CMAKE_BINARY_DIR}/cmake/fix-debug-path.sh"
                "$<TARGET_FILE:${target}>" "${CMAKE_SOURCE_DIR}"
      )
    endif()
  endforeach()
endif()
