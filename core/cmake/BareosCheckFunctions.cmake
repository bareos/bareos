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

  # FreeBSD extended attributes
  try_compile(
    HAVE_FREEBSD_EXTATTR ${CMAKE_BINARY_DIR}/compile_tests
    ${PROJECT_SOURCE_DIR}/src/compile_tests/freebsd_extattr.c
  )

  # AIX extended attributes
  try_compile(
    HAVE_AIX_EA ${CMAKE_BINARY_DIR}/compile_tests
    ${PROJECT_SOURCE_DIR}/src/compile_tests/aix_ea.c
  )

  # Linux extended attributes
  try_compile(
    HAVE_LINUX_XATTR ${CMAKE_BINARY_DIR}/compile_tests
    ${PROJECT_SOURCE_DIR}/src/compile_tests/linux_xattr.c
  )

  # MacOS extended attributes
  try_compile(
    HAVE_DARWIN_XATTR ${CMAKE_BINARY_DIR}/compile_tests
    ${PROJECT_SOURCE_DIR}/src/compile_tests/darwin_xattr.c
  )

  include(CheckFunctionExists)

  # FreeBSD & MacOS
  check_function_exists(chflags HAVE_CHFLAGS)
  check_function_exists(getmntinfo HAVE_GETMNTINFO)

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
