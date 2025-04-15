#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

function(bareos_windows_resource target in_file)
  if(HAVE_WIN32)
    if(IS_ABSOLUTE "${in_file}")
      file(RELATIVE_PATH rel_file "${CMAKE_CURRENT_SOURCE_DIR}" "${in_file}")
      set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${rel_file}")
    else()
      set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${in_file}")
    endif()
    string(REGEX REPLACE ".in\$" "" out_file "${out_file}")
    message(STATUS "creating file ${out_file}")
    configure_file(${in_file} ${out_file} @ONLY NEWLINE_STYLE UNIX)
    target_sources(${target} PRIVATE ${out_file})
  endif()
endfunction()
