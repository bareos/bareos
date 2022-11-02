#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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

# This functions disables compiler warnings on targets, if they're supported.

function(bareos_disable_warnings)
  set(options C_ONLY CXX_ONLY ADD_FOR_LTO)
  set(oneValueArgs TARGET)
  set(multiValueArgs TARGETS WARNINGS)
  cmake_parse_arguments(
    ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
  )
  if(ARG_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "Unparsed arguments to bareos_disable_warnings(): ${ARG_UNPARSED_ARGUMENTS}"
    )
  endif()
  if(ARG_TARGET AND ARG_TARGETS)
    message(
      FATAL_ERROR
        "Don't mix TARGET and TARGETS. Just add '${ARG_TARGET}' to TARGETS"
    )
  endif()
  if(ARG_TARGET)
    set(ARG_TARGETS ${ARG_TARGET})
  endif()
  foreach(target ${ARG_TARGETS})
    if(NOT TARGET ${target})
      message(FATAL_ERROR "No such target ${target}")
    endif()
  endforeach()

  foreach(warning ${ARG_WARNINGS})
    string(REGEX REPLACE "^-W" "" plain_warning ${warning})
    string(REGEX REPLACE "-" "_" plain_warning_sym ${plain_warning})

    if(NOT ARG_CXX_ONLY)
      check_c_compiler_flag(
        "-W${plain_warning}" "CC_SUPPORTS_W${plain_warning_sym}"
      )
    endif()
    if(NOT ARG_C_ONLY)
      check_cxx_compiler_flag(
        "-W${plain_warning}" "CXX_SUPPORTS_W${plain_warning_sym}"
      )
    endif()

    if((ARG_CXX_ONLY OR CC_SUPPORTS_W${plain_warning_sym})
       AND (ARG_C_ONLY OR CXX_SUPPORTS_W${plain_warning_sym})
    )
      foreach(target ${ARG_TARGETS})
        target_compile_options("${target}" PRIVATE "-Wno-${plain_warning}")
        if(ARG_ADD_FOR_LTO)
          target_link_options("${target}" PRIVATE "-Wno-${plain_warning}")
        endif()
      endforeach()
    endif()
  endforeach()
endfunction()
