#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2021 Bareos GmbH & Co. KG
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

if(HAVE_FREEBSD_OS)
  add_definitions(-D_WANT_SEMUN=1)
endif()

add_library(bareoslmdb SHARED mdb.c midl.c)
target_compile_options(bareoslmdb PRIVATE -Wno-missing-braces -Wno-extra)

# set target_link_options so lto doesn't complain either
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_link_options(bareoslmdb PRIVATE -Wno-stringop-overflow)
  target_compile_options(
    bareoslmdb PRIVATE -Wno-stringop-overflow -Wno-return-local-addr
  )
endif()

# empty the include_directories
set_property(TARGET bareoslmdb PROPERTY INCLUDE_DIRECTORIES "")

set_target_properties(
  bareoslmdb PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                        SOVERSION "${BAREOS_VERSION_MAJOR}"
)
if(HAVE_WIN32)
  set_target_properties(bareoslmdb PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
  target_link_libraries(bareoslmdb -lntdll)
endif()

install(TARGETS bareoslmdb DESTINATION ${libdir})
