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

# do a conditional try-compile
function(bareos_compile_test result_var source_file)
  if(NOT DEFINED ${result_var})
    try_compile(
      ${result_var} ${CMAKE_BINARY_DIR}/compile_tests
      ${PROJECT_SOURCE_DIR}/src/compile_tests/${source_file}
    )
  endif()
endfunction()

if(NOT MSVC)
  bareos_compile_test(HAVE_CORE_SYSTEM_INTERFACES core_system_interfaces.c)
  if(HAVE_CORE_SYSTEM_INTERFACES)
    set(HAVE_GETPAGESIZE 1)
    set(HAVE_POLL 1)
    set(HAVE_READDIR_R 1)
  else()
    message(
      FATAL_ERROR
        "Your system does not provide all required system interfaces. Cannot continue."
    )
  endif()

  # FreeBSD extended attributes
  if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    bareos_compile_test(HAVE_FREEBSD_EXTATTR freebsd_extattr.c)
  endif()

  # AIX extended attributes
  if(CMAKE_SYSTEM_NAME MATCHES "AIX")
    message(WARNING "AIX compile test for EA is untested.")
    bareos_compile_test(HAVE_AIX_EA aix_ea.c)
  endif()

  # Linux extended attributes
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    bareos_compile_test(HAVE_LINUX_XATTR linux_xattr.c)
  endif()

  # MacOS extended attributes
  if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    bareos_compile_test(HAVE_DARWIN_XATTR darwin_xattr.c)
  endif()

  include(CheckFunctionExists)

  # FreeBSD & MacOS
  check_function_exists(chflags HAVE_CHFLAGS)
  check_function_exists(getmntinfo HAVE_GETMNTINFO)

  # Linux
  check_function_exists(getmntent HAVE_GETMNTENT)

  # Missing on older Linux
  check_function_exists(lchmod HAVE_LCHMOD)

  # Missing on MacOS
  check_function_exists(posix_fadvise HAVE_POSIX_FADVISE)

  # GNU extensions
  check_function_exists(backtrace HAVE_BACKTRACE)
  check_function_exists(backtrace_symbols HAVE_BACKTRACE_SYMBOLS)

  # Other
  check_function_exists(closefrom HAVE_CLOSEFROM)
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
