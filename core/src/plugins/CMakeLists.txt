#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

# create include dirs needed when compiling python modules

set(pymod_include_dirs ${PROJECT_SOURCE_DIR}/src)

# FreeBSD
if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  list(APPEND pymod_include_dirs ${Intl_INCLUDE_DIRS})
endif()

# Darwin
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  list(APPEND pymod_include_dirs /usr/local/opt/gettext/include
       /usr/local/opt/openssl/include
  )
endif()

# Windows
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  list(APPEND pymod_include_dirs ${PROJECT_SOURCE_DIR}/src/win32/compat/include
       ${PROJECT_SOURCE_DIR}/src/win32/include
  )
endif()

set(pymod2_include_dirs ${Python2_INCLUDE_DIRS} ${pymod_include_dirs})
set(pymod3_include_dirs ${Python3_INCLUDE_DIRS} ${pymod_include_dirs})

if(NOT client-only)
  add_subdirectory(dird)
  add_subdirectory(stored)
endif()
add_subdirectory(filed)
