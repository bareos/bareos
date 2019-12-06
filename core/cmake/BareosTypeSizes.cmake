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

# type size checks
include(CheckTypeSize)
check_type_size("short int" SIZEOF_SHORT_INT)
check_type_size("char" SIZEOF_CHAR)
check_type_size("int" SIZEOF_INT)
check_type_size("int *" SIZEOF_INT_P)
check_type_size("long int" SIZEOF_LONG_INT)
check_type_size("long long int" SIZEOF_LONG_LONG_INT)

check_type_size(int8_t INT8_T)
check_type_size(int16_t INT16_T)
check_type_size(int32_t INT32_T)
check_type_size(int64_t INT64_T)
check_type_size(intmax_t INTMAX_T)

check_type_size(u_int8_t U_INT8_T)
check_type_size(u_int16_t U_INT16_T)
check_type_size(u_int32_t U_INT32_T)
check_type_size(u_int64_t U_INT64_T)

check_type_size(intptr_t INTPTR_T)
check_type_size(uintptr_t UINTPTR_T)
check_type_size(u_int HAVE_U_INT)

check_type_size(major_t MAJOR_T)
check_type_size(minor_t MINOR_T)

if(NOT ${MAJOR_T})
  set(major_t int)
endif()
if(NOT ${MINOR_T})
  set(minor_t int)
endif()

# config.h requires  1 instead of TRUE
if(${HAVE_U_INT})
  set(HAVE_U_INT 1)
endif()

if(${HAVE_INT64_T})
  set(HAVE_INT64_T 1)
endif()

if(${HAVE_U_INT64_T})
  set(HAVE_U_INT64_T 1)
endif()

if(${HAVE_INTMAX_T})
  set(HAVE_INTMAX_T 1)
endif()

if(${HAVE_INTPTR_T})
  set(HAVE_INTPTR_T 1)
endif()

if(${HAVE_UINTPTR_T})
  set(HAVE_UINTPTR_T 1)
endif()

if(${HAVE_INT8_T}
   AND ${HAVE_INT16_T}
   AND ${HAVE_INT32_T}
   AND ${HAVE_INT32_T}
)
  set(HAVE_INTXX_T 1)
endif()

if(${HAVE_U_INT8_T}
   AND ${HAVE_U_INT16_T}
   AND ${HAVE_U_INT32_T}
   AND ${HAVE_U_INT32_T}
)
  set(HAVE_U_INTXX_T 1)
endif()
