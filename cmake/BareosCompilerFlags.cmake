#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2026 Bareos GmbH & Co. KG
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

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Check compiler flags and register standardized variable names
function(bareos_check_compiler_flags)
  set(options C_ONLY CXX_ONLY)
  cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})
  foreach(flag IN LISTS ARG_UNPARSED_ARGUMENTS)
    string(REGEX REPLACE "^-" "" plain_flag ${flag})
    string(REGEX REPLACE "[-=]" "_" plain_flag_sym ${plain_flag})
    if(NOT ARG_CXX_ONLY)
      check_c_compiler_flag("${flag}" "C_SUPPORTS_${plain_flag_sym}")
    endif()
    if(NOT ARG_C_ONLY)
      check_cxx_compiler_flag("${flag}" "CXX_SUPPORTS_${plain_flag_sym}")
    endif()
  endforeach()
endfunction()

# add s list of compile flags, but only when supported by the compiler
function(bareos_add_compile_flags)
  set(options C_ONLY CXX_ONLY)
  cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})
  foreach(flag IN LISTS ARG_UNPARSED_ARGUMENTS)
    string(REGEX REPLACE "^-" "" tmp1 ${flag})

    # Strip prefixes from warning flags. When we want to add -Wno-xyz or
    # -Wno-error=xyz, we check if -Wxyz is supported instead of checking for
    # -Wno-xyz or -Wno-error=xyz to avoid duplicated checks
    string(REGEX REPLACE "^W(no-)?(error=)?" "W" plain_flag ${tmp1})
    string(REGEX REPLACE "[-=]" "_" plain_flag_sym ${plain_flag})

    if(ARG_C_ONLY)
      bareos_check_compiler_flags("-${plain_flag}" C_ONLY)
      if(C_SUPPORTS_${plain_flag_sym})
        add_compile_options("$<$<COMPILE_LANGUAGE:C>:${flag}>")
      endif()
    elseif(ARG_CXX_ONLY)
      bareos_check_compiler_flags("-${plain_flag}" CXX_ONLY)
      if(CXX_SUPPORTS_${plain_flag_sym})
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${flag}>")
      endif()
    else()
      bareos_check_compiler_flags("-${plain_flag}")
      if(C_SUPPORTS_${plain_flag_sym} AND CXX_SUPPORTS_${plain_flag_sym})
        add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:${flag}>")
      elseif(C_SUPPORTS_${plain_flag_sym})
        add_compile_options("$<$<COMPILE_LANGUAGE:C>:${flag}>")
      elseif(CXX_SUPPORTS_${plain_flag_sym})
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${flag}>")
      endif()
    endif()
  endforeach()
endfunction()

# This functions disables compiler warnings on targets, if they're supported.
function(bareos_disable_warnings)
  set(options C_ONLY CXX_ONLY ADD_FOR_LTO)
  set(oneValueArgs "")
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
  foreach(target ${ARG_TARGETS})
    if(NOT TARGET ${target})
      message(FATAL_ERROR "No such target ${target}")
    endif()
  endforeach()

  foreach(warning ${ARG_WARNINGS})
    string(REGEX REPLACE "^-W" "" plain_warning ${warning})
    string(REGEX REPLACE "[-=]" "_" plain_warning_sym ${plain_warning})

    if(ARG_C_ONLY)
      bareos_check_compiler_flags("-W${plain_warning}" C_ONLY)
    elseif(ARG_CXX_ONLY)
      bareos_check_compiler_flags("-W${plain_warning}" CXX_ONLY)
    else()
      bareos_check_compiler_flags("-W${plain_warning}")
    endif()

    if((ARG_CXX_ONLY OR C_SUPPORTS_W${plain_warning_sym})
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
