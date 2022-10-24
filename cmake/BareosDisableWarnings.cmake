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

function(bareos_target_disable_warnings)
  list(POP_FRONT ARGV target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "No such target ${target}")
  endif()
  foreach(warning ${ARGV})
    string(REGEX REPLACE "^-W" "" plain_warning ${warning})
    string(REGEX REPLACE "-" "_" plain_warning_sym ${plain_warning})
    check_c_compiler_flag(
      "-W${plain_warning}" "CC_SUPPORTS_W${plain_warning_sym}"
    )
    check_cxx_compiler_flag(
      "-W${plain_warning}" "CXX_SUPPORTS_W${plain_warning_sym}"
    )
    if(CC_SUPPORTS_W${plain_warning_sym} AND CXX_SUPPORTS_W${plain_warning_sym})
      target_compile_options("${target}" PRIVATE "-Wno-${plain_warning}")
      target_link_options("${target}" PRIVATE "-Wno-${plain_warning}")
    endif()
  endforeach()
endfunction()
