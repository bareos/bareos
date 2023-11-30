#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

include(CMakePrintHelpers)

function(print_variables regex)
  if(NOT "${regex}" STREQUAL "")
    get_cmake_property(_variableNames VARIABLES)
    message("Variables matching '${regex}':")
    foreach(_variableName ${_variableNames})
      if(_variableName MATCHES "${regex}")
        message("   ${_variableName}=${${_variableName}}")
      endif()
    endforeach()
  endif()
endfunction()
