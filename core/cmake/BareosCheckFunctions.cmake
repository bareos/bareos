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

if(NOT MSVC)
  include(CheckFunctionExists)

  try_compile(
    HAVE_SYSTEM_INTERFACES ${CMAKE_BINARY_DIR}/compile_tests
    ${PROJECT_SOURCE_DIR}/src/compile_tests/core_system_interfaces.c
  )
  if(HAVE_SYSTEM_INTERFACES)
    set(HAVE_FCHMOD 1)
    set(HAVE_FCHOWN 1)
    set(HAVE_FCHOWNAT 1)
    set(HAVE_FDATASYNC 1)
    set(HAVE_FSEEKO 1)
    set(HAVE_FUTIMENS 1)
    set(HAVE_FUTIMES 1)
    set(HAVE_GETADDRINFO 1)
    set(HAVE_GETHOSTBYNAME2 1)
    set(HAVE_GETPAGESIZE 1)
    set(HAVE_GLOB 1)
    set(HAVE_INET_NTOP 1)
    set(HAVE_LCHOWN 1)
    set(HAVE_LOCALTIME_R 1)
    set(HAVE_LUTIMES 1)
    set(HAVE_NANOSLEEP 1)
    set(HAVE_OPENAT 1)
    set(HAVE_POLL 1)
    set(HAVE_READDIR_R 1)
    set(HAVE_SETREUID 1)
    set(HAVE_UNLINKAT 1)
    set(HAVE_UTIMES 1)

    set(HAVE_SYS_TIME_H 1)
    set(HAVE_POLL_H 1)
  else()
    message(
      SEND_ERROR
        "core/src/compile_tests/core_system_interfaces.c failed to build"
    )
    message(
      FATAL_ERROR
        "Your system does not provide all required system interfaces. Cannot continue."
    )
  endif()

  # FreeBSD, but apparently not found during configure
  check_function_exists(add_proplist_entry HAVE_ADD_PROPLIST_ENTRY)
  check_function_exists(get_proplist_entry HAVE_GET_PROPLIST_ENTRY)
  check_function_exists(getproplist HAVE_GETPROPLIST)
  check_function_exists(setproplist HAVE_SETPROPLIST)
  check_function_exists(sizeof_proplist_entry HAVE_SIZEOF_PROPLIST_ENTRY)

  # FreeBSD extended attributes
  check_function_exists(extattr_get_file HAVE_EXTATTR_GET_FILE)
  check_function_exists(extattr_get_link HAVE_EXTATTR_GET_LINK)
  check_function_exists(extattr_list_file HAVE_EXTATTR_LIST_FILE)
  check_function_exists(extattr_list_link HAVE_EXTATTR_LIST_LINK)
  check_function_exists(
    extattr_namespace_to_string HAVE_EXTATTR_NAMESPACE_TO_STRING
  )
  check_function_exists(extattr_set_file HAVE_EXTATTR_SET_FILE)
  check_function_exists(extattr_set_link HAVE_EXTATTR_SET_LINK)
  check_function_exists(
    extattr_string_to_namespace HAVE_EXTATTR_STRING_TO_NAMESPACE
  )

  # FreeBSD & MacOS
  check_function_exists(chflags HAVE_CHFLAGS)
  check_function_exists(getmntinfo HAVE_GETMNTINFO)

  # AIX extended attributes
  check_function_exists(getea HAVE_GETEA)
  check_function_exists(lgetea HAVE_LGETEA)
  check_function_exists(listea HAVE_LISTEA)
  check_function_exists(llistea HAVE_LLISTEA)
  check_function_exists(lsetea HAVE_LSETEA)
  check_function_exists(setea HAVE_SETEA)

  # Linux extended attributes
  check_function_exists(getxattr HAVE_GETXATTR)
  check_function_exists(lgetxattr HAVE_LGETXATTR)
  check_function_exists(listxattr HAVE_LISTXATTR)
  check_function_exists(llistxattr HAVE_LLISTXATTR)
  check_function_exists(lsetxattr HAVE_LSETXATTR)
  check_function_exists(setxattr HAVE_SETXATTR)

  # Linux
  check_function_exists(getmntent HAVE_GETMNTENT)
  check_function_exists(prctl HAVE_PRCTL)

  # Missing on older Linux
  check_function_exists(lchmod HAVE_LCHMOD)

  # Missing on MacOS
  check_function_exists(posix_fadvise HAVE_POSIX_FADVISE)
  check_function_exists(futimesat HAVE_FUTIMESAT)

  # GNU extensions
  check_function_exists(backtrace HAVE_BACKTRACE)
  check_function_exists(backtrace_symbols HAVE_BACKTRACE_SYMBOLS)

  # Other
  check_function_exists(closefrom HAVE_CLOSEFROM)

  check_function_exists(
    glfs_readdirplus HAVE_GLFS_READDIRPLUS
  ) # in gfapi since 3.5, no check needed anymore

else()
  # windows provides these functions
  set(HAVE_GETADDRINFO 1)
  set(HAVE_INET_NTOP 1)
  # we provide implementations for these
  set(HAVE_GLOB 1)
  set(HAVE_LCHOWN 1)

  set(HAVE_SYS_TIME_H 1)
endif()

check_cxx_source_compiles(
  "
#include <source_location>
int main() {
 std::source_location l{};
 return 0;
}
"
  HAVE_SOURCE_LOCATION
)

check_cxx_source_compiles(
  "
int main() {
 constexpr auto function = __builtin_FUNCTION();
 constexpr auto file = __builtin_FILE();
 constexpr auto line = __builtin_LINE();

 (void) function;
 (void) file;
 (void) line;

 return 0;
}
"
  HAVE_BUILTIN_LOCATION
)
