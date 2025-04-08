#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2025 Bareos GmbH & Co. KG
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

include(BareosConfigureFile)
include(BareosWindowsResource)

find_package(Readline REQUIRED)

add_library(console_objects STATIC)
add_library(Bareos::Console ALIAS console_objects)
target_sources(
  console_objects PRIVATE connect_to_director.cc console_conf.cc
                          console_globals.cc console_output.cc
)
if(HAVE_PAM)
  target_sources(console_objects PRIVATE auth_pam.cc)
endif()
target_link_libraries(console_objects PRIVATE Bareos::Lib)

add_executable(bconsole)
target_sources(bconsole PRIVATE console.cc)

add_executable(bconsole-unsafe)
target_compile_definitions(bconsole-unsafe PRIVATE "DISABLE_SAFETY_CHECK")
target_sources(bconsole-unsafe PRIVATE console.cc)

bareos_windows_resource(bconsole consoleres.rc.in)
bareos_windows_resource(bconsole-unsafe consoleres.rc.in)

target_link_libraries(
  bconsole PRIVATE console_objects Readline::Readline CLI11::CLI11
)
target_link_libraries(
  bconsole-unsafe PRIVATE console_objects Readline::Readline CLI11::CLI11
)

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
