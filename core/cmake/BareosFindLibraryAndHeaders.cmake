#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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

# generic macro to find libraries and headers

MACRO(BareosFindLibraryAndHeaders LIBNAME HEADERFILE)
   MESSAGE(STATUS  "checking for library ${LIBNAME} and ${HEADERFILE} header ...")

   STRING(TOUPPER ${LIBNAME} LIBNAME_UPCASE)
   SET(INCLUDE_VAR_NAME ${LIBNAME_UPCASE}_INCLUDE_DIRS)
   SET(LIB_VAR_NAME ${LIBNAME_UPCASE}_LIBRARIES)
   SET(FOUND_VAR_NAME ${LIBNAME_UPCASE}_FOUND)
   SET(HAVE_VAR_NAME HAVE_${LIBNAME_UPCASE})


   find_path(${INCLUDE_VAR_NAME} NAMES ${HEADERFILE})
   find_library(${LIB_VAR_NAME} NAMES ${LIBNAME})

   #MESSAGE(STATUS "find path result for ${INCLUDE_VAR_NAME} is ${${INCLUDE_VAR_NAME}}")
   #MESSAGE(STATUS "find library result for ${LIB_VAR_NAME} is ${${LIB_VAR_NAME}}")

   #STRING(LENGTH ${${LIB_VAR_NAME}} LIBFOUND )
   #STRING(LENGTH ${${INCLUDE_VAR_NAME}} INCLUDEFOUND )

   SET(LIBFOUND ${${LIB_VAR_NAME}})
   #MESSAGE(STATUS  "LIBFOUND: ${LIBFOUND}")
   SET(INCLUDEFOUND ${${INCLUDE_VAR_NAME}})
   #MESSAGE(STATUS  "INCLUDEFOUND: ${INCLUDEFOUND}")

   STRING(REGEX MATCH "-NOTFOUND" LIBNOTFOUND ${LIBFOUND})
   #MESSAGE(STATUS  "LIB REGEX MATCH: ${LIBNOTFOUND}")
   STRING(LENGTH  "${LIBNOTFOUND}" LIBNOTFOUND )

   STRING(REGEX MATCH "-NOTFOUND" INCLUDENOTFOUND ${INCLUDEFOUND})
   #MESSAGE(STATUS  "INC REGEX MATCH: ${INCLUDENOTFOUND}")
   STRING(LENGTH "${INCLUDENOTFOUND}" INCLUDENOTFOUND )


   if (NOT ${LIBNOTFOUND})
      #MESSAGE(STATUS  "LIBFOUND is true: ${LIBFOUND}")
      if (NOT ${INCLUDENOTFOUND})
         #MESSAGE(STATUS  "INCLUDEFOUND is true: ${INCLUDEFOUND}")
         #MESSAGE(STATUS  "setting ${FOUND_VAR_NAME} to true")
         set(${FOUND_VAR_NAME} TRUE)
         set(${HAVE_VAR_NAME} 1)
      else()
         #MESSAGE(STATUS  "INCLUDEFOUND is true: ${INCLUDEFOUND}")
         set(${FOUND_VAR_NAME} FALSE)
         set(${HAVE_VAR_NAME} 0)
      endif()
   else()
      #MESSAGE(STATUS  "LIBFOUND is false: ${LIBFOUND}")
      set(${FOUND_VAR_NAME} FALSE)
      set(${HAVE_VAR_NAME} 0)
   endif ()



   SET(FOUNDVALUE ${${FOUND_VAR_NAME}})
   if (${FOUNDVALUE})
      message(STATUS "  ${FOUND_VAR_NAME}=${${FOUND_VAR_NAME}}")
      message(STATUS "  ${LIB_VAR_NAME}=${${LIB_VAR_NAME}}")
      message(STATUS "  ${INCLUDE_VAR_NAME}=${${INCLUDE_VAR_NAME}}")
      message(STATUS "  ${HAVE_VAR_NAME}=${${HAVE_VAR_NAME}}")
   else ()
      message(STATUS "  ${HAVE_VAR_NAME}=${${HAVE_VAR_NAME}}")
      if (${LIBNOTFOUND})
         message(STATUS     "              ERROR:  ${LIBNAME} libraries NOT found. ")
         set("${LIB_VAR_NAME}" "")
      endif()
      if (${INCLUDENOTFOUND})
         message(STATUS     "              ERROR:  ${LIBNAME} includes  NOT found. ")
         set("${INCLUDE_VAR_NAME}" "")
      endif()
      message(STATUS "  ${FOUND_VAR_NAME}=${${FOUND_VAR_NAME}}")
      message(STATUS "  ${LIB_VAR_NAME}=${${LIB_VAR_NAME}}")
      message(STATUS "  ${INCLUDE_VAR_NAME}=${${INCLUDE_VAR_NAME}}")
      message(STATUS "  ${HAVE_VAR_NAME}=${${HAVE_VAR_NAME}}")
   endif ()

   mark_as_advanced(
      ${INCLUDE_VAR_NAME}
      ${LIB_VAR_NAME}
      ${FOUND_VAR_NAME}
      )
ENDMACRO()
