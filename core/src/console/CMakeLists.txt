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

set(BCONSSRCS connect_to_director.cc console_conf.cc console_globals.cc
              console_output.cc
)
if(HAVE_WIN32)
  list(APPEND BCONSSRCS ../win32/console/consoleres.rc)
endif()

include_directories(${Readline_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

if(HAVE_PAM)
  include_directories(${PAM_INCLUDE_DIRS})
  list(APPEND BCONSSRCS auth_pam.cc)
endif()

add_executable(bconsole console.cc)

add_library(console_objects STATIC ${BCONSSRCS})

if(HAVE_PAM)
  set(CONSOLE_LINK_LIBRARIES console_objects bareos ${Readline_LIBRARY}
                             ${PAM_LIBRARIES}
  )
else()
  set(CONSOLE_LINK_LIBRARIES console_objects bareos ${Readline_LIBRARY})
endif()

target_link_libraries(bconsole ${CONSOLE_LINK_LIBRARIES})

install(TARGETS bconsole DESTINATION "${bindir}" COMPONENT bconsole)
install(TARGETS bconsole DESTINATION "${sbindir}" COMPONENT bconsole)
install(
  FILES bconsole.conf
  DESTINATION "${configtemplatedir}"
  COMPONENT bconsole
)
