#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2024 Bareos GmbH & Co. KG
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

target_link_libraries(console_objects PRIVATE bareos)
target_link_libraries(
  bconsole PRIVATE console_objects ${Readline_LIBRARY} CLI11::CLI11
)

include(BareosConfigureFile)
bareos_configure_file(FILES bconsole.conf.in)

install(
  TARGETS bconsole
  DESTINATION "${bindir}"
  COMPONENT bconsole
)
install(
  TARGETS bconsole
  DESTINATION "${sbindir}"
  COMPONENT bconsole
)

install(
  CODE "
  include(\"${PROJECT_SOURCE_DIR}/cmake/BareosInstallConfigFiles.cmake\")
  bareos_install_single_config_file(\"${CMAKE_CURRENT_BINARY_DIR}/bconsole.conf\" \"${configtemplatedir}\" \".\")
"
)

include(DebugEdit)
