#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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

# generic macro to find libraries

macro(BareosFindLibrary LIBNAME)
  message(STATUS "checking for library ${LIBNAME}...")

  string(TOUPPER ${LIBNAME} LIBNAME_UPCASE)
  set(INCLUDE_VAR_NAME ${LIBNAME_UPCASE}_INCLUDE_DIRS)
  set(LIB_VAR_NAME ${LIBNAME_UPCASE}_LIBRARIES)
  set(FOUND_VAR_NAME ${LIBNAME_UPCASE}_FOUND)
  set(HAVE_VAR_NAME HAVE_LIB${LIBNAME_UPCASE})

  find_library(${LIB_VAR_NAME} NAMES ${LIBNAME})

  set(LIBFOUND ${${LIB_VAR_NAME}})

  string(
    REGEX
      MATCH
      "-NOTFOUND"
      LIBNOTFOUND
      ${LIBFOUND}
  )
  # MESSAGE(STATUS  "LIB REGEX MATCH: ${LIBNOTFOUND}")
  string(LENGTH "${LIBNOTFOUND}" LIBNOTFOUND)

  if(NOT ${LIBNOTFOUND})
    set(${FOUND_VAR_NAME} TRUE)
    set(${HAVE_VAR_NAME} 1)
  else()
    set(${FOUND_VAR_NAME} FALSE)
    set(${HAVE_VAR_NAME} 0)
  endif()

  set(QUIETVALUE ${${QUIET_VAR_NAME}})
  set(FOUNDVALUE ${${FOUND_VAR_NAME}})
  if(${FOUNDVALUE})
    message(STATUS "  ${FOUND_VAR_NAME}=${${FOUND_VAR_NAME}}")
    message(STATUS "  ${LIB_VAR_NAME}=${${LIB_VAR_NAME}}")
    message(STATUS "  ${HAVE_VAR_NAME}=${${HAVE_VAR_NAME}}")
  else()
    message(STATUS "              ERROR:  ${LIBNAME} libraries NOT found. ")

    set("${LIB_VAR_NAME}" "")
    set("${INCLUDE_VAR_NAME}" "")

    message(STATUS "  ${FOUND_VAR_NAME}=${${FOUND_VAR_NAME}}")
    message(STATUS "  ${LIB_VAR_NAME}=${${LIB_VAR_NAME}}")
    message(STATUS "  ${HAVE_VAR_NAME}=${${HAVE_VAR_NAME}}")
  endif()

  mark_as_advanced(${LIB_VAR_NAME} ${FOUND_VAR_NAME})
endmacro()
