#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2023 Bareos GmbH & Co. KG
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
  set(Readline_LIBRARY ${HOMEBREW_PREFIX}/opt/readline/lib/libreadline.a
                       ncurses
  )
else()
  find_library(
    Readline_LIBRARY
    NAMES readline
    HINTS ${Readline_ROOT_DIR}/lib
  )
endif()
# Conditionally set READLINE_FOUND value
if(Readline_INCLUDE_DIR
   AND Readline_LIBRARY
   AND Ncurses_LIBRARY
)
  set(READLINE_FOUND TRUE)
else(
  Readline_INCLUDE_DIR
  AND Readline_LIBRARY
  AND Ncurses_LIBRARY
)
  find_library(Readline_LIBRARY NAMES readline)
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    Readline DEFAULT_MSG Readline_INCLUDE_DIR Readline_LIBRARY
  )
  mark_as_advanced(Readline_INCLUDE_DIR Readline_LIBRARY)
endif(
  Readline_INCLUDE_DIR
  AND Readline_LIBRARY
  AND Ncurses_LIBRARY
)

# Hide these variables in cmake GUIs
mark_as_advanced(Readline_ROOT_DIR Readline_INCLUDE_DIR Readline_LIBRARY)
