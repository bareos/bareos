#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2018 Bareos GmbH & Co. KG
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

include_directories(..)

set (BAREOSCFG_SRCS ini.cc lex.cc parse_bsr.cc res.cc parse_conf.cc res.cc qualified_resource_name_type_converter.cc)
set (BAREOS_STATIC_SRCS parse_conf.cc res.cc qualified_resource_name_type_converter.cc)

IF(HAVE_WIN32)
   add_library(bareosstatic STATIC ${BAREOS_STATIC_SRCS})
   target_link_libraries(bareosstatic bareoscfg)
ELSE()
   LIST(APPEND BAREOSCFG_SRCS ${BAREOS_STATIC_SRCS})
ENDIF()

add_library(bareoscfg SHARED ${BAREOSCFG_SRCS})
target_link_libraries(bareoscfg bareos ${JANSSON_LIBRARIES})

INSTALL(TARGETS bareoscfg DESTINATION ${libdir})

set_target_properties(bareoscfg PROPERTIES VERSION "${VERSION}"
                                          SOVERSION "${SOVERSION}")

IF(HAVE_WIN32)
   set_target_properties(bareoscfg PROPERTIES DEFINE_SYMBOL "BUILDING_BAREOSCFG_DLL")
ENDIF()

