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

# pre-seed the CC_SUPPORT/CXX_SUPPORTS variables if we alrady know to avoid
# running the compiler

function(bareos_known_compiler_flags)
  set(options C_ONLY CC_ONLY CXX_ONLY UNSUPPORTED)
  set(oneValueArgs ID SINCE)
  set(multiValueArgs FLAGS)
  cmake_parse_arguments(
    ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
  )

  if(ARG_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "Unparsed arguments to bareos_known_compiler_flags(): ${ARG_UNPARSED_ARGUMENTS}"
    )
  endif()
  if(ARG_SINCE AND ARG_UNSUPPORTED)
    message(FATAL_ERROR "SINCE and UNSUPPORTED cannot be used together")
  endif()
  if(ARG_CXX_ONLY AND (ARG_C_ONLY OR ARG_CC_ONLY))
    message(FATAL_ERROR "C_ONLY and CXX_ONLY cannot be used together")
  endif()
  if(NOT ARG_ID)
    message(FATAL_ERROR "argument ID is missing")
  endif()

  if(ARG_C_ONLY OR ARG_CC_ONLY)
    set(langs C)
  elseif(ARG_CXX_ONLY)
    set(langs CXX)
  else()
    set(langs C CXX)
  endif()

  foreach(lang IN LISTS langs)
    if("${CMAKE_${lang}_COMPILER_ID}" STREQUAL ARG_ID)
      if(ARG_UNSUPPORTED)
        set(supported "")
      elseif("${CMAKE_${lang}_COMPILER_VERSION}" VERSION_GREATER_EQUAL
             ARG_SINCE
      )
        set(supported "1")
      else()
        set(supported "")
      endif()

      # fixup poorly chosen name: while the CMake LANG is "C", our
      # cache-variables start with "CC"
      if(lang STREQUAL "C")
        set(lang CC)
      endif()

      foreach(flag IN LISTS ARG_FLAGS)
        string(REGEX REPLACE "^-" "" raw_flag "${flag}")
        string(REPLACE "-" "_" mangled_flag "${raw_flag}")
        set("${lang}_SUPPORTS_${mangled_flag}"
            "${supported}"
            CACHE INTERNAL "From KnownCompilerFlags.cmake"
        )
      endforeach()
    endif()
  endforeach()

endfunction()

bareos_known_compiler_flags(
  ID GNU
  SINCE 8.5.0
  FLAGS -Wcast-function-type
        -Wdeprecated-declarations
        -Wformat
        -Wformat-security
        -Wimplicit-fallthrough
        -Wincompatible-pointer-types
        -Winvalid-offsetof
        -Wmissing-field-initializers
        -Wold-style-definition
        -Wshadow
        -Wsign-compare
        -Wstringop-overflow
        -Wsuggest-override
        -Wunused-but-set-variable
        -Wunused-parameter
        -fdebug-prefix-map
        -fmacro-prefix-map
)
bareos_known_compiler_flags(
  ID GNU
  SINCE 10.0.0
  FLAGS -Wenum-conversion
)
bareos_known_compiler_flags(
  ID GNU
  UNSUPPORTED
  FLAGS -Wdeprecated-non-prototype -Wnon-literal-null-conversion
        -Wc99-designator
)

bareos_known_compiler_flags(
  ID Clang
  SINCE 18.1.8
  FLAGS -Wc99-designator
        -Wdeprecated-non-prototype
        -Wnon-literal-null-conversion
        -Wenum-conversion
        -Wcast-function-type
        -Wdeprecated-declarations
        -Wformat
        -Wformat-security
        -Wimplicit-fallthrough
        -Wincompatible-pointer-types
        -Winvalid-offsetof
        -Wmissing-field-initializers
        -Wold-style-definition
        -Wshadow
        -Wsign-compare
        -Wsuggest-override
        -Wunused-but-set-variable
        -Wunused-parameter
        -fdebug-prefix-map
        -fmacro-prefix-map
)

bareos_known_compiler_flags(
  ID Clang
  UNSUPPORTED
  FLAGS -Wstringop-overflow
)
