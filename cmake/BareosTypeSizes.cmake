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

# type size checks
INCLUDE(CheckTypeSize)
CHECK_TYPE_SIZE("short int" SIZEOF_SHORT_INT)
CHECK_TYPE_SIZE("char" SIZEOF_CHAR)
CHECK_TYPE_SIZE("int" SIZEOF_INT)
CHECK_TYPE_SIZE("int *" SIZEOF_INT_P)
CHECK_TYPE_SIZE("long int" SIZEOF_LONG_INT)
CHECK_TYPE_SIZE("long long int" SIZEOF_LONG_LONG_INT)

CHECK_TYPE_SIZE(int8_t INT8_T)
CHECK_TYPE_SIZE(int16_t INT16_T)
CHECK_TYPE_SIZE(int32_t INT32_T)
CHECK_TYPE_SIZE(int64_t INT64_T)
CHECK_TYPE_SIZE(intmax_t INTMAX_T)

CHECK_TYPE_SIZE(u_int8_t U_INT8_T)
CHECK_TYPE_SIZE(u_int16_t U_INT16_T)
CHECK_TYPE_SIZE(u_int32_t U_INT32_T)
CHECK_TYPE_SIZE(u_int64_t U_INT64_T)

CHECK_TYPE_SIZE(intptr_t INTPTR_T)
CHECK_TYPE_SIZE(uintptr_t UINTPTR_T)
CHECK_TYPE_SIZE(u_int HAVE_U_INT)

CHECK_TYPE_SIZE(major_t MAJOR_T)
CHECK_TYPE_SIZE(minor_t MINOR_T)

IF (NOT ${MAJOR_T})
   set(major_t int)
ENDIF()
IF (NOT ${MINOR_T})
   set(minor_t int)
ENDIF()

# config.h requires  1 instead of TRUE
IF(${HAVE_U_INT})
   SET(HAVE_U_INT 1)
ENDIF()

IF(${HAVE_INT64_T})
   SET(HAVE_INT64_T 1)
ENDIF()

IF(${HAVE_U_INT64_T})
   SET(HAVE_U_INT64_T 1)
ENDIF()

IF(${HAVE_INTMAX_T})
   SET(HAVE_INTMAX_T 1)
ENDIF()

IF(${HAVE_INTPTR_T})
   SET(HAVE_INTPTR_T 1)
ENDIF()

IF(${HAVE_UINTPTR_T})
   SET(HAVE_UINTPTR_T 1)
ENDIF()


if (${HAVE_INT8_T} AND
    ${HAVE_INT16_T} AND
    ${HAVE_INT32_T} AND
    ${HAVE_INT32_T})
   set(HAVE_INTXX_T 1)
endif()

if (${HAVE_U_INT8_T} AND
    ${HAVE_U_INT16_T} AND
    ${HAVE_U_INT32_T} AND
    ${HAVE_U_INT32_T})
   set(HAVE_U_INTXX_T 1)
endif()
