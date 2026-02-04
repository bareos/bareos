#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2026 Bareos GmbH & Co. KG
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
list(APPEND pymod_include_dirs ${PROJECT_BINARY_DIR}/src)

# FreeBSD
if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  list(APPEND pymod_include_dirs ${Intl_INCLUDE_DIRS})
endif()

# Darwin
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  list(APPEND pymod_include_dirs ${HOMEBREW_PREFIX}/opt/gettext/include
       ${HOMEBREW_PREFIX}/opt/openssl/include
  )
endif()

# Windows
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  list(APPEND pymod_include_dirs ${PROJECT_SOURCE_DIR}/src/win32/compat/include
       ${PROJECT_SOURCE_DIR}/src/win32/include
  )
endif()

set(pymod3_include_dirs ${Python3_INCLUDE_DIRS} ${pymod_include_dirs})

if(ENABLE_PYTHON)
  add_library(python-common python/common.cc)
  target_link_libraries(python-common PUBLIC Python3::Python Bareos::Lib)
  target_include_directories(
    python-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/python
  )
endif()

if(NOT client-only)
  add_subdirectory(dird)
  add_subdirectory(stored)
endif()
add_subdirectory(filed)
