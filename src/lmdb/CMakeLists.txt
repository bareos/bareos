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


add_library(bareoslmdb SHARED mdb.c midl.c)

# empty the include_directories
set_property(TARGET bareoslmdb PROPERTY INCLUDE_DIRECTORIES "")

IF(${HAVE_WIN32})
   add_definitions(-Wall -m64 -mwin32 -mthreads)
ENDIF()

set_target_properties(bareoslmdb PROPERTIES VERSION "${VERSION}"
                                          SOVERSION "${SOVERSION}"
                                          DEFINE_SYMBOL "BUILDING_DLL"
                                          )
IF(${HAVE_WIN32})
target_link_libraries(bareoslmdb -lntdll)
ENDIF()

INSTALL(TARGETS bareoslmdb DESTINATION ${libdir})
