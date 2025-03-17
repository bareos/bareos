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

# Create and preconfigure a plugin-target for one of the daemons
function(bareos_add_plugin target)
  if(target MATCHES "-dir$")
    set(suffix "dird")
  elseif(target MATCHES "-fd$")
    set(suffix "filed")
  elseif(target MATCHES "-sd$")
    set(suffix "stored")
  else()
    message(
      FATAL_ERROR "plugin target name ${target} must end in -dir, -fd or -sd"
    )
  endif()
  if(PLUGIN_OUTPUT_DIRECTORY)
    set(output_dir "${PLUGIN_OUTPUT_DIRECTORY}")
  else()
    set(output_dir "${CMAKE_BINARY_DIR}/core/src/plugins/${suffix}")
  endif()
  add_library(${target} MODULE)
  set_target_properties(
    ${target} PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${output_dir}"
  )
endfunction()
