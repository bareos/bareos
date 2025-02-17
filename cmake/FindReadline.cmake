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

if(WIN32)
  # get the unofficial target from vcpkg port and add an alias for that
  find_package(unofficial-readline-win32 CONFIG REQUIRED)
  if(unofficial-readline-win32_FOUND)
    add_library(Readline::Readline ALIAS unofficial::readline-win32::readline)
    set(Readline_FOUND "unofficial-readline-win32")
  endif()
else()

  # Search for the path containing library's headers
  find_path(Readline_ROOT_DIR NAMES include/readline/readline.h)

  # Search for include directory
  find_path(
    Readline_INCLUDE_DIR
    NAMES readline/readline.h
    HINTS ${Readline_ROOT_DIR}/include
  )

  # Search for library
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(Readline_LIBRARY ${HOMEBREW_PREFIX}/opt/readline/lib/libreadline.a)
  else()
    find_library(
      Readline_LIBRARY
      NAMES readline
      HINTS ${Readline_ROOT_DIR}/lib
    )
  endif()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    Readline REQUIRED_VARS Readline_INCLUDE_DIR Readline_LIBRARY
  )

  if(Readline_FOUND AND NOT TARGET Readline::Readline)
    add_library(Readline::Readline SHARED IMPORTED)
    set_target_properties(
      Readline::Readline
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${Readline_INCLUDE_DIR}"
                 IMPORTED_LOCATION "${Readline_LIBRARY}"
    )
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      set_target_properties(
        Readline::Readline PROPERTIES INTERFACE_LINK_LIBRARIES "ncurses"
      )
    endif()
  endif()

  mark_as_advanced(Readline_ROOT_DIR Readline_INCLUDE_DIR Readline_LIBRARY)
endif()
