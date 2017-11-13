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

include_directories(. .. ../include )

add_executable(bconsole console.cc console_conf.cc)

set(CONSOLE_LINK_LIBRARIES bareos bareoscfg ${Readline_LIBRARY})

IF(HAVE_WIN32)
  LIST(APPEND CONSOLE_LINK_LIBRARIES bareosstatic)
ENDIF()

target_link_libraries(bconsole ${CONSOLE_LINK_LIBRARIES})

INSTALL(TARGETS bconsole DESTINATION "${bindir}")
INSTALL(TARGETS bconsole DESTINATION "${sbindir}")
INSTALL(FILES bconsole.conf DESTINATION "${configtemplatedir}")
