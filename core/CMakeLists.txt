#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2021 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

cmake_minimum_required(VERSION 3.12)
project(bareos C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(
    FATAL_ERROR
      "Building from bareos/core/ is not supported anymore. Use bareos/ instead"
  )
endif()

if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif(POLICY CMP0054)

if(POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif(POLICY CMP0057)

# disable in-source builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME common)

include(GNUInstallDirs)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

# switch on CXX 17 Support
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wsuggest-override compiler_will_suggest_override)

if(${compiler_will_suggest_override})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-override")
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  # make format-security issues a compiler-error
  check_cxx_compiler_flag(-Wformat compiler_format_security)
  if(${compiler_format_security})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat")
  endif()
  check_cxx_compiler_flag(
    -Werror=format-security compiler_error_format_security
  )
  if(${compiler_error_format_security})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format-security")
  endif()
endif()

option(DEBUG_PREFIX_MAP
       "remap absolute debug paths to relative if compiler supports it" ON
)
check_c_compiler_flag(
  -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=. c_compiler_debug_prefix_map
)
check_cxx_compiler_flag(
  -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.
  cxx_compiler_debug_prefix_map
)
if(DEBUG_PREFIX_MAP
   AND c_compiler_debug_prefix_map
   AND cxx_compiler_debug_prefix_map
)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
  )
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
  )
endif()

check_c_compiler_flag(
  -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=. c_compiler_macro_prefix_map
)
check_cxx_compiler_flag(
  -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.
  cxx_compiler_macro_prefix_map
)
if(c_compiler_macro_prefix_map)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
  )
endif()
if(cxx_compiler_macro_prefix_map)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
  )
endif()

check_cxx_compiler_flag(-Wno-unknown-pragmas compiler_has_no_unknown_pragmas)
if(${compiler_has_no_unknown_pragmas})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()

# warn on sign-conversion include(CheckCCompilerFlag)
# CHECK_C_COMPILER_FLAG(-Wsign-conversion c_compiler_will_warn_sign_conversion)
#
# if (${c_compiler_will_warn_sign_conversion}) set (CMAKE_C_FLAGS
# "${CMAKE_C_FLAGS} -Wsign-conversion") endif()
#
# CHECK_CXX_COMPILER_FLAG(-Wsign-conversion
# cxx_compiler_will_warn_sign_conversion)
#
# if (${cxx_compiler_will_warn_sign_conversion}) set (CMAKE_CXX_FLAGS
# "${CMAKE_CXX_FLAGS} -Wsign-conversion") endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  include_directories(/usr/include)
endif()

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/../../SOURCES
    ${PROJECT_SOURCE_DIR}/../SOURCES ${PROJECT_SOURCE_DIR}/cmake
)

set(host ${CMAKE_SYSTEM})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(BareosExtractVersionInfo)

include(BareosGetDistInfo)

set(BUILDNAME
    "${BAREOS_NUMERIC_VERSION}"
    CACHE STRING "site name variable for CDash"
)
set(SITE
    "${CMAKE_SYSTEM_NAME}-${DISTVER}-${CMAKE_HOST_SYSTEM_PROCESSOR}"
    CACHE STRING "build name variable for CDash"
)
# enable "make test"
enable_testing()
set(CMAKE_CTEST_COMMAND ctest -V)
include(CTest)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
  set(Readline_ROOT_DIR /usr/local/opt/readline)
  set(Intl_ROOT_DIR /usr/local/opt/gettext)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(MINGW_PREFIX ${CMAKE_INSTALL_PREFIX})
  set(MINGW_BINDIR "${MINGW_PREFIX}/bin")
  if(${WINDOWS_BITS} MATCHES "32")
    set(CMAKE_CROSSCOMPILING_EMULATOR wine)
    set(SITE
        "${CMAKE_SYSTEM}-32"
        CACHE STRING "build name variable for CDash"
    )
  else()
    set(CMAKE_CROSSCOMPILING_EMULATOR wine64)
    set(SITE
        "${CMAKE_SYSTEM}-64"
        CACHE STRING "build name variable for CDash"
    )
  endif()

  # search for dll.a files before .dll
  set(MINGW_LIBDIR "${MINGW_PREFIX}/lib")
  set(CMAKE_LIBRARY_PATH ${MINGW_LIBDIR})

  # set WINEPATH so that cross-compiled .exe tests find their dlls
  set(WINEPATH "Z:${MINGW_BINDIR}\\\\;")
  string(APPEND WINEPATH "Z:${CMAKE_CURRENT_BINARY_DIR}/src/lib\\\\;")
  string(APPEND WINEPATH "Z:${CMAKE_CURRENT_BINARY_DIR}/src/fastlz\\\\;")
  string(APPEND WINEPATH "Z:${CMAKE_CURRENT_BINARY_DIR}/src/findlib\\\\;")
  string(APPEND WINEPATH "Z:${CMAKE_CURRENT_BINARY_DIR}/src/stored\\\\;")
  string(APPEND WINEPATH "Z:${CMAKE_CURRENT_BINARY_DIR}/src/cats\\\\;")

  message(${WINEPATH})
  # replace slash with backslash everywhere
  string(REPLACE "/" "\\" WINEPATH ${WINEPATH})
  # MESSAGE(FATAL_ERROR ${WINEPATH})

  set(ENV{WINEPATH} ${WINEPATH})

endif()

include(BareosFindAllLibraries)
include(BareosFindPrograms)

if(NOT ${OPENSSL_FOUND})
  message(FATAL_ERROR "FATAL ERROR: OpenSSL is required but was not found.")
endif()

if(coverage)
  set(COVERAGE_COMPILER_FLAGS
      "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
      CACHE INTERNAL ""
  )
  message(
    STATUS
      "coverage requested, adding COVERAGE_COMPILER_FLAGS : ${COVERAGE_COMPILER_FLAGS}"
  )
  set(CMAKE_BUILD_TYPE Debug)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    link_libraries(gcov)
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(HAVE_LINUX_OS 1)
  include_directories(/usr/include/tirpc)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(HAVE_SUN_OS 1)
  link_libraries(nsl socket curses sec)

  if(${CMAKE_C_COMPILER_ID} MATCHES SunPro)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
  endif()

  if(${CMAKE_CXX_COMPILER_ID} MATCHES SunPro)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  set(HAVE_FREEBSD_OS 1)
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
  link_libraries(intl)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(HAVE_DARWIN_OS 1)
  # set(CMAKE_INCLUDE_PATH  "${OPENSSL_INCLUDE_DIR}:${CMAKE_INCLUDE_PATH}")
  set(CMAKE_INCLUDE_PATH "/usr/local/opt/:${CMAKE_INCLUDE_PATH}")
  link_directories(/usr/local/lib)

  include_directories(/usr/local/opt/gettext/include)
  link_libraries(
    /usr/local/opt/gettext/lib/libintl.a iconv "-framework CoreFoundation"
  )
  set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}")
endif()

# hpux
if(${CMAKE_SYSTEM_NAME} MATCHES "HP-UX")
  set(HAVE_HPUX_OS 1)
  add_definitions(-D_XOPEN_SOURCE_EXTENDED=1)
  add_definitions(-D_INCLUDE_XOPEN_SOURCE_EXTENDED=1)
  add_definitions(-D_INCLUDE_LONGLONG=1)
  include_directories(/usr/local/include/)
  link_directories(/usr/lib/hpux32 /usr/local/lib/hpux32 /usr/local/lib)
  link_libraries(dld intl)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX")
  set(HAVE_AIX_OS 1)
  set(lmdb 0FF)
  link_libraries(intl)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  include_directories(
    SYSTEM ${PROJECT_SOURCE_DIR}/src/win32/include
    ${PROJECT_SOURCE_DIR}/src/win32/compat/include
  )

  set(HAVE_WIN32 1)

  set(Python2_LIBRARIES python27.dll)
  set(Python3_LIBRARIES python38.dll)

  if(${WINDOWS_BITS} MATCHES "32")
    set(cross-prefix i686-w64-mingw32)
  else()
    set(cross-prefix x86_64-w64-mingw32)
  endif()

  set(Python2_INCLUDE_DIRS /usr/${cross-prefix}/include/python2/)
  set(Python3_INCLUDE_DIRS /usr/${cross-prefix}/include/python3/)

  set(PostgreSQL_LIBRARY libpq.dll)
  set(PostgreSQL_INCLUDE_DIR "")
  set(PostgreSQL_TYPE_INCLUDE_DIR "")

  set(dynamic-storage-backends OFF)
  set(HAVE_DYNAMIC_SD_BACKENDS 0)

else()
  add_definitions("-D_FILE_OFFSET_BITS=64")
endif()

include(FindIntl)

if(developer)
  add_definitions("-DDEVELOPER=1")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

include(BareosSetVariableDefaults)

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX")
  set(ENABLE_NLS 0)
endif()
include(BareosFindStaticCodeAnalysisTools)

if(NOT client-only)
  # droplet does not build on solaris because of sys/cdefs.h: No such file or
  # directory
  if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/droplet/CMakeLists.txt)
      message(
        FATAL_ERROR
          "Please checkout the libdroplet submodule via \n \"git submodule update --init --recursive\""
      )
    endif()
    add_subdirectory(src/droplet)
    if(TARGET droplet)
      set_target_properties(
        droplet
        PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                   SOVERSION "${BAREOS_VERSION_MAJOR}"
                   PREFIX "libbareos"
      )
    endif()
  endif()
endif()

include(BareosGenerateDebianInfo)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  add_definitions(
    -DWIN32_VSS -DMINGW64 -DHAVE_WIN32 -DHAVE_MINGW
    -D_WIN32_WINNT=${WINDOWS_VERSION}
  )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m${WINDOWS_BITS} -mwin32 -mthreads")

  # c++17 removed register keyword
  set(Python2_CCSHARED -Wno-register)

  set(Python2_SITELIB ${plugindir})
  set(Python3_SITELIB ${plugindir})
endif()

# set INSTALL_LIB_DIR automatically to lib or lib64 automatically Taken from
# https://cmake.org/pipermail/cmake/2013-July/055374.html

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if("${LIB64}" STREQUAL "TRUE")
  set(LIBSUFFIX 64)
else()
  set(LIBSUFFIX "")
endif()
set(INSTALL_LIB_DIR
    "usr/lib${LIBSUFFIX}"
    CACHE PATH "Installation directory for libraries"
)
mark_as_advanced(INSTALL_LIB_DIR)

# RPATH settings (from https://cmake.org/Wiki/CMake_RPATH_handling)
set(CMAKE_SKIP_BUILD_RPATH FALSE)

set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "${libdir}")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(dbdriver sqlite3)

# manual definitions
set(BAREOS "Bareos")
set(LOCALEDIR \"${CMAKE_INSTALL_LOCALEDIR}\")

set(HAVE_CRYPTO 1)

include(TestBigEndian)
test_big_endian(IS_BIGENDIAN)
if(IS_BIGENDIAN)
  set(HAVE_BIG_ENDIAN 1)
endif()

message(STATUS "VERSION: " ${CMAKE_MATCH_1})
message(STATUS "PROJECT_SOURCE_DIR:         " ${PROJECT_SOURCE_DIR})

# needed for check_include
set(CMAKE_REQUIRED_DEFINITIONS "-D_FILE_OFFSET_BITS=64")
include(BareosCheckIncludes)

include(BareosCheckFunctions)

include(BareosCheckSymbols)

include(BareosDetermineHaveLowLevelScsiInterface)

include(acltypes)

# include_directories(SYSTEM ${Python_INCLUDE_DIRS})

# include_directories(SYSTEM ${Python_INCLUDE_PATH})

include_directories(${PROJECT_SOURCE_DIR}/src)

# ------------------------- Begin Generic CMake Variable Logging
# ------------------

# /*      C++ comment style not allowed   */

# if you are building in-source, this is the same as PROJECT_SOURCE_DIR,
# otherwise this is the top level directory of your build tree
message(STATUS "CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR})

# if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR,
# otherwise this is the directory where the compiled or generated files from the
# current CMakeLists.txt will go to
message(STATUS "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR})

# this is the directory, from which cmake was started, i.e. the top level source
# directory
message(STATUS "PROJECT_SOURCE_DIR:         " ${PROJECT_SOURCE_DIR})

# this is the directory where the currently processed CMakeLists.txt is located
# in
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR})

# contains the full path to the top level directory of your build tree
message(STATUS "PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR})

# contains the full path to the root of your project source directory, i.e. to
# the nearest directory where CMakeLists.txt contains the PROJECT() command
message(STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR})

# set this variable to specify a common place where CMake should put all
# executable files (instead of CMAKE_CURRENT_BINARY_DIR)
message(STATUS "EXECUTABLE_OUTPUT_PATH: " ${EXECUTABLE_OUTPUT_PATH})

# set this variable to specify a common place where CMake should put all
# libraries (instead of CMAKE_CURRENT_BINARY_DIR)
message(STATUS "LIBRARY_OUTPUT_PATH:     " ${LIBRARY_OUTPUT_PATH})

# tell CMake to search first in directories listed in CMAKE_MODULE_PATH when you
# use FIND_PACKAGE() or INCLUDE()
message(STATUS "CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH})

# this is the complete path of the cmake which runs currently (e.g.
# /usr/local/bin/cmake)
message(STATUS "CMAKE_COMMAND: " ${CMAKE_COMMAND})
message(STATUS "CMAKE_VERSION: " ${CMAKE_VERSION})

# this is the CMake installation directory
message(STATUS "CMAKE_ROOT: " ${CMAKE_ROOT})

# this is the filename including the complete path of the file where this
# variable is used.
message(STATUS "CMAKE_CURRENT_LIST_FILE: " ${CMAKE_CURRENT_LIST_FILE})

# this is linenumber where the variable is used
message(STATUS "CMAKE_CURRENT_LIST_LINE: " ${CMAKE_CURRENT_LIST_LINE})

# this is used when searching for include files e.g. using the FIND_PATH()
# command.
message(STATUS "CMAKE_INCLUDE_PATH: " ${CMAKE_INCLUDE_PATH})

# this is used when searching for libraries e.g. using the FIND_LIBRARY()
# command.
message(STATUS "CMAKE_LIBRARY_PATH: " ${CMAKE_LIBRARY_PATH})

# the complete system name, e.g. "Linux-2.4.22", "FreeBSD-5.4-RELEASE" or
# "Windows 5.1"
message(STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM})

# the short system name, e.g. "Linux", "FreeBSD" or "Windows"
message(STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})

# only the version part of CMAKE_SYSTEM
message(STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION})

# the processor name (e.g. "Intel(R) Pentium(R) M processor 2.00GHz")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

# is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
message(STATUS "UNIX: " ${UNIX})

# is TRUE on Windows, including CygWin
message(STATUS "WIN32: " ${WIN32})

# is TRUE on Apple OS X
message(STATUS "APPLE: " ${APPLE})

# is TRUE when using the MinGW compiler in Windows
message(STATUS "MINGW: " ${MINGW})

# is TRUE on Windows when using the CygWin version of cmake
message(STATUS "CYGWIN: " ${CYGWIN})

# is TRUE on Windows when using a Borland compiler
message(STATUS "BORLAND: " ${BORLAND})

# Microsoft compiler
message(STATUS "MSVC: " ${MSVC})
message(STATUS "MSVC_IDE: " ${MSVC_IDE})
message(STATUS "MSVC60: " ${MSVC60})
message(STATUS "MSVC70: " ${MSVC70})
message(STATUS "MSVC71: " ${MSVC71})
message(STATUS "MSVC80: " ${MSVC80})
message(STATUS "CMAKE_COMPILER_2005: " ${CMAKE_COMPILER_2005})

# set this to true if you don't want to rebuild the object files if the rules
# have changed, but not the actual source files or headers (e.g. if you changed
# the some compiler switches)
message(STATUS "CMAKE_SKIP_RULE_DEPENDENCY: " ${CMAKE_SKIP_RULE_DEPENDENCY})

# since CMake 2.1 the install rule depends on all, i.e. everything will be built
# before installing. If you don't like this, set this one to true.
message(STATUS "CMAKE_SKIP_INSTALL_ALL_DEPENDENCY: "
               ${CMAKE_SKIP_INSTALL_ALL_DEPENDENCY}
)

# If set, runtime paths are not added when using shared libraries. Default it is
# set to OFF
message(STATUS "CMAKE_SKIP_RPATH: " ${CMAKE_SKIP_RPATH})

# set this to true if you are using makefiles and want to see the full compile
# and link commands instead of only the shortened ones
message(STATUS "CMAKE_VERBOSE_MAKEFILE: " ${CMAKE_VERBOSE_MAKEFILE})

# this will cause CMake to not put in the rules that re-run CMake. This might be
# useful if you want to use the generated build files on another machine.
message(STATUS "CMAKE_SUPPRESS_REGENERATION: " ${CMAKE_SUPPRESS_REGENERATION})

# A simple way to get switches to the compiler is to use ADD_DEFINITIONS(). But
# there are also two variables exactly for this purpose:

# wheter or not
message(STATUS "CCACHE_FOUND: " ${CCACHE_FOUND})

# Choose the type of build.  Example: SET(CMAKE_BUILD_TYPE Debug)
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# if this is set to ON, then all libraries are built as shared libraries by
# default.
message(STATUS "BUILD_SHARED_LIBS: " ${BUILD_SHARED_LIBS})

# the compiler used for C files
message(STATUS "CMAKE_C_COMPILER:           " ${CMAKE_C_COMPILER})
message(STATUS "CMAKE_C_FLAGS:              " ${CMAKE_C_FLAGS})
message(STATUS "CMAKE_C_COMPILER_ID:        " ${CMAKE_C_COMPILER_ID})
message(STATUS "CMAKE_C_COMPILER_VERSION:   " ${CMAKE_C_COMPILER_VERSION})

# the compiler used for C++ files
message(STATUS "CMAKE_CXX_COMPILER:         " ${CMAKE_CXX_COMPILER})
message(STATUS "CMAKE_CXX_FLAGS:            " ${CMAKE_CXX_FLAGS})
message(STATUS "CMAKE_CXX_COMPILER_ID:      " ${CMAKE_CXX_COMPILER_ID})
message(STATUS "CMAKE_CXX_COMPILER_VERSION: " ${CMAKE_CXX_COMPILER_VERSION})

# the tools for creating libraries
message(STATUS "CMAKE_AR: " ${CMAKE_AR})
message(STATUS "CMAKE_RANLIB: " ${CMAKE_RANLIB})

message(STATUS "CMAKE_INSTALL_PREFIX:         " ${CMAKE_INSTALL_PREFIX})

if(HAVE_DYNAMIC_SD_BACKENDS)
  set(UNCOMMENT_SD_BACKEND_DIRECTORY "")
else()
  set(UNCOMMENT_SD_BACKEND_DIRECTORY "# ")
endif()

set(uncomment_dbi "#")

set(srcdir ${PROJECT_SOURCE_DIR})

set(CONFDIR "\"${confdir}\"")

set(CC ${CMAKE_C_COMPILER})
set(CCVERSION ${CMAKE_CXX_COMPILER_VERSION})
set(CXX ${CMAKE_CXX_COMPILER})
set(CXXVERSION ${CMAKE_CXX_COMPILER_VERSION})

if("${HAVE_OPENSSL}")
  set(HAVE_TLS "1")
endif()

if(NOT openssl)
  unset(HAVE_OPENSSL)
  unset(HAVE_TLS)
endif()

set(largefile_support "")

if("${PAM_FOUND}")
  set(HAVE_PAM 1)
endif()

set(have_tls "${HAVE_TLS}")
set(have_crypto "${HAVE_CRYPTO}")
set(have_openssl "${HAVE_OPENSSL}")
set(have_zlib "${ZLIB_FOUND}")

set(HAVE_CAPABILITY
    "${HAVE_CAPABILITY_H}"
    CACHE BOOL "compile with capability support"
)
if("${HAVE_CAPABILITY}")
  set(CAPABILITY_LIBS cap)
  message(STATUS "CAPABILITY set")
endif()

try_compile(
  HAVE_IS_TRIVIALLY_COPYABLE ${CMAKE_BINARY_DIR}/compile_tests
  ${PROJECT_SOURCE_DIR}/src/compile_tests/trivially_copyable.cc
)

set(ndmp "${ndmp}")
set(lmdb "${lmdb}")
set(build_client_only "${client-only}")
set(build_dird "${build-dird}")
set(build_stored "${build-stored}")
set(have_plugins "${have_plugins}")
set(have_afs "")

if(${acl})
  if(NOT HAVE_WIN32)
    if(${HAVE_SYS_ACL_H})
      set(HAVE_ACL 1)
      message(STATUS "acl found, libs: ${ACL_LIBRARIES}")
    else()
      message(FATAL_ERROR "build with acl requested, but lib not found")
    endif()
  endif()
endif()

if("${HAVE_SYS_XATTR_H}" OR "${HAVE_SYS_EXTATTR_H}")
  set(HAVE_XATTR 1)
  set(have_xattr YES)
endif()

if(${HAVE_RADOS_LIBRADOS_H})
  set(HAVE_CEPH_RADOS 1)
  set(have_ceph_rados ${HAVE_CEPH_RADOS})
endif()

if(${HAVE_RADOSSTRIPER_LIBRADOSSTRIPER_H})
  set(HAVE_CEPH_RADOS_STRIPER 1)
  set(have_ceph_rados_striper ${HAVE_CEPH_RADOS_STRIPER})
endif()

if(${HAVE_GLUSTERFS_API_GLFS_H})
  set(HAVE_GLUSTERFS 1)
endif()
set(have_glusterfs ${HAVE_GLUSTERFS_API_GLFS_H})

if(NOT HAVE_WIN32)
  if(${ndmp})
    set(HAVE_NDMP 1)
  endif()
endif()

if(${lmdb})
  set(HAVE_LMDB 1)
endif()

# info what the config files need to be installed PLUGINS  ############
set(PLUGINS python-ldap)

list(APPEND PLUGINS python-ovirt)

if(VIXDISKLIB_FOUND)
  list(APPEND PLUGINS python-vmware)
endif()

if(${HAVE_CEPHFS})
  list(APPEND PLUGINS cephfs)
endif()
if(${HAVE_CEPH_RADOS})
  list(APPEND PLUGINS rados)
endif()

if(${HAVE_GLUSTERFS})
  list(APPEND PLUGINS gfapi)
endif()

set(PLUGINS
    ${PLUGINS}
    PARENT_SCOPE
)

# BACKENDS  ####
if(build_client_only)
  set(BACKENDS "")
else()
  set(BACKENDS unix_tape_device.d)
  list(APPEND BACKENDS unix_fifo_device.d)
  if(${HAVE_CEPHFS})
    list(APPEND BACKENDS rados_device.d)
  endif()
  if(${HAVE_GLUSTERFS})
    list(APPEND BACKENDS gfapi_device.d)
  endif()
  if(TARGET droplet)
    list(APPEND BACKENDS droplet_device.d)
  endif()
endif()

set(support_systemd "")
set(batch_insert_db_backends "")
set(have_cmocka "")

set(PATH_BAREOS_BACKENDDIR \"${backenddir}\")
set(PATH_BAREOS_PIDDIR \"${piddir}\")
set(PATH_BAREOS_WORKINGDIR \"${workingdir}\")

if(HAVE_WIN32)
  install(DIRECTORY DESTINATION var/log)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  message(STATUS "installing var/log/bareos")
  install(DIRECTORY DESTINATION var/log/bareos)

  message(STATUS "installing var/lib/bareos")
  install(DIRECTORY DESTINATION var/lib/bareos)
else()
  install(DIRECTORY DESTINATION ${logdir})
  message(STATUS "installing ${logdir}")

  install(DIRECTORY DESTINATION ${workingdir})
  message(STATUS "installing ${woringdir}")
endif()

include(BareosConfigureFile)

add_subdirectory(scripts)
add_subdirectory(manpages)
add_subdirectory(platforms)
add_subdirectory(src)

include(BareosLocalBuildDefinitions OPTIONAL
        RESULT_VARIABLE BareosLocalBuildDefinitionsFile
)

string(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S")

# give configure output
message(" ")
message("Configuration on ${NOW} : ")
message(" ")

message("   Host:                         ${host} -- ${PLATFORM} ${DISTVER} ")
message("   Hostname:                     ${hostname} ")
message(
  "   Bareos version:               ${BAREOS} ${BAREOS_FULL_VERSION} (${DATE}) "
)
message("   Build platform:               ${PLATFORM}")
message("   Source code location:         ${srcdir} ")
message("   Modify Debian Control file:   ${GENERATE_DEBIAN_CONTROL} ")
message("   Install binaries:             ${bindir} ")
message("   Install system binaries:      ${sbindir} ")
message("   Install libraries:            ${libdir} ")
message("   Install system config files:  ${sysconfdir} ")
message("   Install Bareos config dir:    ${confdir} ")
message("   Install Bareos config files:  ${configtemplatedir} ")
message("   Log directory:                ${logdir} ")
message("   Scripts directory:            ${scriptdir} ")
message("   Archive directory:            ${archivedir} ")
message("   Working directory:            ${workingdir} ")
message("   BSR directory:                ${bsrdir} ")
message("   PID directory:                ${piddir} ")
message("   Subsys directory:             ${subsysdir} ")
message("   Man directory:                ${mandir} ")
message("   Data directory:               ${datarootdir} ")
message("   Backend directory:            ${backenddir} ")
message("   Plugin directory:             ${plugindir} ")
message("   C Compiler:                   ${CC} ${CCVERSION} ")
message("   C++ Compiler:                 ${CXX} ${CXXVERSION} ")
message("   C Compiler flags:             ${CMAKE_C_FLAGS} ")
message("   C++ Compiler flags:           ${CMAKE_CXX_FLAGS} ")
message(
  "   Linker flags:                 ${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_MODULE_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_STATIC_LINKER_FLAGS} "
)
message("   Libraries:                    ${LIBS} ")
message("   Statically Linked Tools:      ${support_static_tools} ")
message("   Statically Linked FD:         ${support_static_fd} ")
message("   Statically Linked SD:         ${support_static_sd} ")
message("   Statically Linked DIR:        ${support_static_dir} ")
message("   Statically Linked CONS:       ${support_static_cons} ")
message("   Database backends:            ${db_backends} ")
message("   default_db_backend:           ${default_db_backend} ")
message("   db_backend_to_test:           ${db_backend_to_test} ")
message("   Database port:                ${db_port} ")
message("   Database name:                ${db_name} ")
message("   Database user:                ${db_user} ")
message("   Database version:             ${BDB_VERSION} ")
message(" ")
message("   Job Output Email:             ${job_email} ")
message("   Traceback Email:              ${dump_email} ")
message("   SMTP Host Address:            ${smtp_host} ")
message(" ")
message("   Director Port:                ${dir_port} ")
message("   File daemon Port:             ${fd_port} ")
message("   Storage daemon Port:          ${sd_port} ")
message(" ")
message("   Director User:                ${dir_user} ")
message("   Director Group:               ${dir_group} ")
message("   Storage Daemon User:          ${sd_user} ")
message("   Storage DaemonGroup:          ${sd_group} ")
message("   File Daemon User:             ${fd_user} ")
message("   File Daemon Group:            ${fd_group} ")
message(" ")
message("   Large file support:           ${largefile_support}")
message(
  "   readline support:             ROOT_DIR:${Readline_ROOT_DIR} INCLUDE_DIR:${Readline_INCLUDE_DIR} LIBRARY:${Readline_LIBRARY}
 "
)
message("   TLS support:                  ${have_tls} ")
message("   Encryption support:           ${have_crypto} ")
message(
  "   OpenSSL support:              ${OPENSSL_FOUND} ${OPENSSL_VERSION} ${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES} "
)
message(
  "   PAM support:                  ${PAM_FOUND} ${PAM_INCLUDE_DIRS} ${PAM_LIBRARIES} "
)
message(
  "   ZLIB support:                 ${ZLIB_FOUND} ${ZLIB_INCLUDE_DIRS} ${ZLIB_LIBRARIES} "
)
message(
  "   LZO2 support:                 ${LZO2_FOUND} ${LZO2_INCLUDE_DIRS} ${LZO2_LIBRARIES} "
)
message(
  "   JANSSON support:              ${JANSSON_FOUND} ${JANSSON_VERSION_STRING} ${JANSSON_INCLUDE_DIRS} ${JANSSON_LIBRARIES}"
)
message(
  "   VIXDISKLIB support:           ${VIXDISKLIB_FOUND} ${VIXDISKLIB_LIBRARIES} ${VIXDISKLIB_INCLUDE_DIRS} "
)
message("   LMDB support:                 ${lmdb} ")
message("   NDMP support:                 ${ndmp} ")
message("   Build ndmjob binary:          ${build_ndmjob} ")
message("   enable-lockmgr:               ${lockmgr} ")
message("   tray-monitor support:         ${HAVE_TRAYMONITOR} ")
message("   test-plugin support:          ${HAVE_TEST_PLUGIN} ")
message("   client-only:                  ${build_client_only} ")
message("   build-dird:                   ${build_dird} ")
message("   build-stored:                 ${build_stored} ")
message("   Plugin support:               ${have_plugins} ")
message("   AFS support:                  ${have_afs} ")
message("   ACL support:                  ${HAVE_ACL} ${ACL_LIBRARIES}")
message("   XATTR support:                ${have_xattr} ")
message(
  "   SCSI Crypto support:          ${scsi-crypto} ${HAVE_LOWLEVEL_SCSI_INTERFACE} "
)
message(
  "   GFAPI(GLUSTERFS) support:     ${GFAPI_FOUND} ${GFAPI_LIBRARIES} ${GFAPI_INCLUDE_DIRS} "
)
message(
  "   CEPH RADOS support:           ${RADOS_FOUND} ${RADOS_LIBRARIES} ${RADOS_INCLUDE_DIRS} "
)
message(
  "   RADOS striping support:       ${RADOSSTRIPER_FOUND} ${RADOSSTRIPER_LIBRARIES} ${RADOSSTRIPER_INCLUDE_DIRS} "
)
message(
  "   CEPHFS support:               ${CEPHFS_FOUND} ${CEPHFS_LIBRARIES} ${CEPHFS_INCLUDE_DIRS} "
)
message(
  "   Python2 support:              ${Python2_FOUND} ${Python2_VERSION} ${Python2_INCLUDE_DIRS} ${Python2_EXECUTABLE}"
)
message(
  "   Python3 support:              ${Python3_FOUND} ${Python3_VERSION} ${Python3_INCLUDE_DIRS} ${Python3_EXECUTABLE}"
)
message("   systemd support:              ${WITH_SYSTEMD} ${SYSTEMD_UNITDIR}")
message("   Batch insert enabled:         ${USE_BATCH_FILE_INSERT}")
message("   PostgreSQL Version:           ${PostgreSQL_VERSION_STRING} ")
message("   GTest enabled:                ${GTest_FOUND}")
message(
  "   Intl support:                 ${Intl_FOUND} ${INTLINCLUDE_DIRS} ${INTL_LIBRARIES}"
)

message(
  "   Dynamic cats backends:        ${dynamic-cats-backends} ${HAVE_DYNAMIC_CATS_BACKENDS} "
)
message(
  "   Dynamic storage backends:     ${dynamic-storage-backends} ${HAVE_DYNAMIC_SD_BACKENDS} ${BACKENDS} "
)
message("   PLUGINS:                      ${PLUGINS} ")
message("   Build for Test Coverage :     ${coverage} ")

message("   PSCMD:                        ${PSCMD}")
message("   PS:                           ${PS}")
message("   PIDOF:                        ${PIDOF}")
message("   PGREP:                        ${PGREP}")
message("   AWK:                          ${AWK}")
message("   GAWK:                         ${GAWK}")
message("   GDB:                          ${GDB}")
message("   RPCGEN:                       ${RPCGEN}")
message("   MTX:                          ${MTX}")
message("   MT:                           ${MT}")
message("   MINIO:                        ${MINIO}")
message("   S3CMD:                        ${S3CMD}")
message("   XTRABACKUP:                   ${XTRABACKUP}")
message("   DEVELOPER:                    ${developer}")
message("   LocalBuildDefinitionsFile:    ${BareosLocalBuildDefinitionsFile}")
message("   HAVE_IS_TRIVIALLY_COPYABLE:   ${HAVE_IS_TRIVIALLY_COPYABLE}")

message("   do-static-code-checks:        ${DO_STATIC_CODE_CHECKS}")

if(DO_STATIC_CODE_CHECKS)
  message("    static code check tools:")
  message(
    "    CMAKE_CXX_INCLUDE_WHAT_YOU_USE: ${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}"
  )
  message("    CMAKE_CXX_CLANG_TIDY:           ${CMAKE_CXX_CLANG_TIDY}")
  message("    CMAKE_CXX_CPPCHECK:             ${CMAKE_CXX_CPPCHECK}")
  message("    CMAKE_CXX_CPPLINT:              ${CMAKE_CXX_CPPLINT}")
  message("    CMAKE_LINK_WHAT_YOU_USE:        ${CMAKE_LINK_WHAT_YOU_USE}")
endif()

if(HAVE_WIN32)
  message("   WINDOWS_VERSION:              ${WINDOWS_VERSION}")
  message("   WINDOWS_BITS:                 ${WINDOWS_BITS}")
  message("   WINEPATH environment:         $ENV{WINEPATH}")
endif()
# REPEAT_UNTIL requires cmake 3.17+
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0")
  set(REPEAT_UNTIL "REPEAT UNTIL_PASS:2")
endif()

configure_file(
  "CTestScript.cmake.in" "${CMAKE_BINARY_DIR}/CTestScript.cmake" @ONLY
)
