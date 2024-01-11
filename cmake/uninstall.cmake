#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

# taken from https://gist.github.com/royvandam
set(MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

if(NOT EXISTS ${MANIFEST})
  message(FATAL_ERROR "Cannot find install manifest: '${MANIFEST}'")
endif()

file(STRINGS ${MANIFEST} files)
foreach(file ${files})
  if(EXISTS ${file})
    message(STATUS "Removing file: '${file}'")

    execute_process(
      COMMAND ${CMAKE_COMMAND} -E remove ${file} COMMAND_ECHO STDOUT
      RESULT_VARIABLE result
    )

    if(NOT "${result}" STREQUAL 0)
      message(FATAL_ERROR "Failed to remove file: '${file}'.")
    endif()
  else()
    message(STATUS "File '${file}' does not exist.")
  endif()
endforeach(file)
