#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2026 Bareos GmbH & Co. KG
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
project(bareos C CXX)

if(DEBUG_TARGET_DEPENDENCIES)
  set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(
    FATAL_ERROR
      "Building from bareos/core/ is not supported anymore. Use bareos/ instead"
  )
endif()

# disable in-source builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME common)

include(BareosHelper)

# switch on CXX 20 Support
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# enable PIC/PIE by default
include(CheckPIESupported)
check_pie_supported()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ENABLE_SANITIZERS "Build with ASan/LSan/UBSan enabled" OFF)
if(ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=address -fno-sanitize-recover"
    )
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=address -fno-sanitize-recover"
    )

    add_compile_definitions(BAREOS_UB_SANITIZER=1 BAREOS_ADDRESS_SANITIZER=1)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=address -fno-sanitize-recover"
    )
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=address -fno-sanitize-recover"
    )
    add_compile_definitions(BAREOS_UB_SANITIZER=1 BAREOS_ADDRESS_SANITIZER=1)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
    add_compile_definitions(BAREOS_ADDRESS_SANITIZER=1)
  else()
    message(
      FATAL_ERROR
        "Cannot (yet) compile with sanitizers on ${CMAKE_C_COMPILER_ID}"
    )
  endif()
endif()

option(ENABLE_THREAD_SANITIZERS "Build with TSan enabled" OFF)
if(ENABLE_THREAD_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-sanitize-recover"
    )
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=thread -fno-sanitize-recover"
    )

    add_compile_definitions(BAREOS_THREAD_SANITIZER=1)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-sanitize-recover"
    )
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=thread -fno-sanitize-recover"
    )
    add_compile_definitions(BAREOS_THREAD_SANITIZER=1)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=thread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=thread")
    add_compile_definitions(BAREOS_THREAD_SANITIZER=1)
  else()
    message(
      FATAL_ERROR
        "Cannot (yet) compile with sanitizers on ${CMAKE_C_COMPILER_ID}"
    )
  endif()
endif()

if(ENABLE_THREAD_SANITIZERS AND ENABLE_SANITIZERS)
  message(
    WARNING
      "enabling both thread sanitizer and address sanitizer is not supported"
  )
endif()

option(ENABLE_STATIC_RUNTIME_LIBS "Link C and C++ runtime libraries statically"
       OFF
)
if(ENABLE_STATIC_RUNTIME_LIBS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
  else()
    message(
      FATAL_ERROR
        "Cannot (yet) do static runtime-libraries on ${CMAKE_C_COMPILER_ID}"
    )
  endif()
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18.0)
  include(CheckLinkerFlag)
  check_linker_flag(CXX "-Wl,--as-needed" LD_SUPPORTS_AS_NEEDED)
  if(LD_SUPPORTS_AS_NEEDED AND NOT CMAKE_LINK_WHAT_YOU_USE)
    add_link_options("-Wl,--as-needed")
  endif()
endif()

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

check_cxx_compiler_flag(-Winvalid-offsetof compiler_has_invalid_offsetof)
if(compiler_has_invalid_offsetof)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
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
    ${PROJECT_SOURCE_DIR}/../../../SOURCES ${PROJECT_SOURCE_DIR}/../SOURCES
    ${PROJECT_SOURCE_DIR}/cmake
)

set(host ${CMAKE_SYSTEM})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  set(HAVE_DARWIN_OS 1)
  if(DEFINED ENV{HOMEBREW_PREFIX})
    set(HOMEBREW_PREFIX "$ENV{HOMEBREW_PREFIX}")
  else()
    set(HOMEBREW_PREFIX "/usr/local")
  endif()

  set(OPENSSL_ROOT_DIR ${HOMEBREW_PREFIX}/opt/openssl@3)
  set(Readline_ROOT_DIR ${HOMEBREW_PREFIX}/opt/readline)
  set(Intl_ROOT_DIR ${HOMEBREW_PREFIX}/opt/gettext)
endif()

include(BareosFindAllLibraries)

if(MSVC)
  set(THREADS_THREADS "PThreads4W::PThreads4W_CXXEXC")
else()
  set(THREADS_THREADS "Threads::Threads")
endif()

include(BareosFindPrograms)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(HAVE_LINUX_OS 1)
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
  include_directories(SYSTEM /usr/local/include)
  link_directories(/usr/local/lib)
  link_libraries(intl)
  check_cxx_compiler_flag(
    -Wunused-but-set-variable compiler_will_warn_of_unused_but_set_variable
  )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  # set(CMAKE_INCLUDE_PATH  "${OPENSSL_INCLUDE_DIR}:${CMAKE_INCLUDE_PATH}")
  set(CMAKE_INCLUDE_PATH "${HOMEBREW_PREFIX}/opt/:${CMAKE_INCLUDE_PATH}")
  link_directories(${HOMEBREW_PREFIX}/lib)

  include_directories(${HOMEBREW_PREFIX}/opt/gettext/include)
  link_libraries(
    ${HOMEBREW_PREFIX}/opt/gettext/lib/libintl.a iconv
    "-framework CoreFoundation" "-framework CoreServices"
  )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX")
  set(HAVE_AIX_OS 1)
  set(lmdb OFF)
  link_libraries(intl)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  include_directories(
    SYSTEM ${PROJECT_SOURCE_DIR}/src/win32/include
    ${PROJECT_SOURCE_DIR}/src/win32/compat/include
  )
  if(MSVC)
    link_libraries(Intl::Intl)
  endif()

  set(HAVE_WIN32 1)

  if(MSVC)
    set(PostgreSQL_INCLUDE_DIR "")
    set(PostgreSQL_TYPE_INCLUDE_DIR "")
  endif()
  set(dynamic-storage-backends
      OFF
      CACHE BOOL "Enable dynamic storage backends" FORCE
  )
  set(HAVE_DYNAMIC_SD_BACKENDS
      0
      CACHE INTERNAL "" FORCE
  )

else()
  add_compile_definitions("_FILE_OFFSET_BITS=64")
endif()

if(MSVC)
  set(BAREOS_WARNING_FLAGS
      /W4 # enable all reasonable warnings, but disable the following
      /wd4273 # inconsistent dll linkage
      /wd4127 # conditional expression is constant
      /wd4244 # conversion warning
      /wd4245 # conversion warning
      /wd4267 # conversion warning
      /wd4389 # sign mismatch
      /D_CRT_SECURE_NO_DEPRECATE # deprecation warning for stdlib functions
  )
else()
  set(BAREOS_WARNING_FLAGS -Werror -Wall -Wextra -Wshadow)
endif()
list(JOIN BAREOS_WARNING_FLAGS " " BAREOS_WARNING_FLAGS_STRING)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${BAREOS_WARNING_FLAGS_STRING}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${BAREOS_WARNING_FLAGS_STRING}")

option(ENABLE_BCONSOLE "Build bconsole binary" ON)

if(${CMAKE_SYSTEM_NAME} MATCHES "AIX")
  set(ENABLE_NLS 0)
endif()
include(BareosFindStaticCodeAnalysisTools)


if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  if(MSVC)
    add_definitions(
      -DWIN32_VSS -DMINGW64 -DHAVE_WIN32 -D_WIN32_WINNT=${WINDOWS_VERSION}
      -DWINVER=${WINDOWS_VERSION}
    )
    # add parallel builds (-MP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP")
    # export all symbols of dlls trick in cmake
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()

  set(Python3_SITEARCH ${plugindir})
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

# manual definitions
set(BAREOS "Bareos")
if(MSVC)
  set(LOCALEDIR \".\")
else()
  set(LOCALEDIR \"${CMAKE_INSTALL_LOCALEDIR}\")
endif()

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

# whether or not
message(STATUS "CCACHE: " ${CCACHE_PROGRAM})

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

if("${PAM_FOUND}")
  set(HAVE_PAM 1)
endif()

option(ENABLE_CAPABILITY "Enable keep readall capability in filedaemon"
       "${HAVE_CAPABILITY_H}"
)

try_compile(
  HAVE_IS_TRIVIALLY_COPYABLE ${CMAKE_BINARY_DIR}/compile_tests
  ${PROJECT_SOURCE_DIR}/src/compile_tests/trivially_copyable.cc
)

option(BUILD_BENCHMARKS "Build benchmark binaries" OFF)
mark_as_advanced(BUILD_BENCHMARKS)

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

if(NOT HAVE_WIN32)
  if(${ndmp})
    set(HAVE_NDMP 1)
  endif()
endif()

if(${lmdb})
  set(HAVE_LMDB 1)
endif()

# info what the config files need to be installed PLUGINS  ############
if(ENABLE_PYTHON)
  set(PLUGINS python-ldap)
else()
  set(PLUGINS "")
endif()

if(VIXDISKLIB_FOUND)
  list(APPEND PLUGINS python-vmware)
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
  if(TARGET droplet)
    list(APPEND BACKENDS droplet_device.d)
  endif()
  list(APPEND BACKENDS dplcompat_device.d)
endif()

if(HAVE_MSVC)
  set(PATH_BAREOS_WORKINGDIR \"%TEMP%\")
  set(PATH_BAREOS_BACKENDDIR \".\")
  set(PATH_BAREOS_SCRIPTDIR \".\")
else()
  set(PATH_BAREOS_BACKENDDIR \"${backenddir}\")
  set(PATH_BAREOS_WORKINGDIR \"${workingdir}\")
  set(PATH_BAREOS_SCRIPTDIR \"${scriptdir}\")
endif()

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
bareos_configure_file(
  GLOB_RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/defaultconfigs/*" COPY
)

add_subdirectory(platforms)
add_subdirectory(src)
add_subdirectory(scripts)
add_subdirectory(manpages)

if(NOT HAVE_WIN32)
  include(BareosGenerateDebianInfo)
endif()

include(BareosLocalBuildDefinitions OPTIONAL
        RESULT_VARIABLE BareosLocalBuildDefinitionsFile
)

string(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S")

# give configure output
message(" ")
message("Configuration on ${NOW}: ")
message(" ")

message(
  "   Host:                         ${host} -- ${BAREOS_PLATFORM} ${DISTVER} "
)
message("   Hostname:                     ${hostname} ")
message(
  "   Bareos version:               ${BAREOS} ${BAREOS_FULL_VERSION} (${DATE}) "
)
message("   Build platform:               ${BAREOS_PLATFORM}")
message("   Source code location:         ${srcdir} ")
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
message("   Enable systemtest suite:      ${ENABLE_SYSTEMTESTS} ")
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
message(
  "   readline support:             ROOT_DIR:${Readline_ROOT_DIR} INCLUDE_DIR:${Readline_INCLUDE_DIR} LIBRARY:${Readline_LIBRARY}
 "
)
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
message("   Build tray-monitor:           ${traymonitor} ")
message("   webui:                        ${ENABLE_WEBUI} ")
message("   client-only:                  ${build_client_only} ")
message("   Plugin support:               ${have_plugins} ")
message("   AFS support:                  ${have_afs} ")
message("   ACL support:                  ${HAVE_ACL} ${ACL_LIBRARIES}")
message("   XATTR support:                ${have_xattr} ")
message(
  "   SCSI Crypto support:          ${scsi-crypto} ${HAVE_LOWLEVEL_SCSI_INTERFACE} "
)
message(
  "   Python3 support:              ${Python3_FOUND} ${Python3_VERSION} ${Python3_INCLUDE_DIRS} ${Python3_EXECUTABLE}"
)
message(
  "   Protobuf support:             ${Protobuf_FOUND} ${Protobuf_VERSION}"
)
message("   GRPC support:                 ${gRPC_FOUND} ${gRPC_VERSION}")
message("   systemd support:              ${WITH_SYSTEMD} ${SYSTEMD_UNITDIR}")
message("   Batch insert enabled:         ${USE_BATCH_FILE_INSERT}")
message(
  "   PostgreSQL Version:           ${PostgreSQL_VERSION_STRING} ${PostgreSQL_LIBRARY}"
)
message("   GTest enabled:                ${GTest_FOUND}")
message(
  "   Intl support:                 ${Intl_FOUND} ${INTLINCLUDE_DIRS} ${INTL_LIBRARIES}"
)
message(
  "   Dynamic storage backends:     ${dynamic-storage-backends} ${HAVE_DYNAMIC_SD_BACKENDS} ${BACKENDS} "
)
message("   PLUGINS:                      ${PLUGINS} ")
message("   Build for Test Coverage :     ${coverage} ")

message("   PSCMD:                        ${PSCMD}")
message("   PERL:                         ${PERL}")
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
  message("   WINEPATH environment:         $ENV{WINEPATH}")
endif()

print_variables("${DUMP_VARS}")

if(ENABLE_SANITIZERS)
  # sanitizer-build needs more resources, so run fewer tests in parallel
  set(CTEST_PARALLEL_LEVEL "PARALLEL_LEVEL 5")
elseif(NOT DEFINED ENV{CTEST_PARALLEL_LEVEL})
  # default to 10 parallel tests
  set(CTEST_PARALLEL_LEVEL "PARALLEL_LEVEL 10")
else()
  # use the user's environment setting
  set(CTEST_PARALLEL_LEVEL "")
endif()

configure_file(
  "CTestScript.cmake.in" "${CMAKE_BINARY_DIR}/CTestScript.cmake" @ONLY
)

if(DEBUG_TARGET_DEPENDENCIES)
  include(BareosTargetTools)
  get_all_targets(targets)

  # resolve dependencies for all targets and write a list-file
  set(ALL_DEPS)
  foreach(tgt IN LISTS targets)
    get_target_dependencies(
      deps ${tgt} "${PROJECT_BINARY_DIR}/dep-debug/${tgt}.txt"
    )
    list(APPEND ALL_DEPS ${deps})
  endforeach()
  list(SORT ALL_DEPS)
  list(REMOVE_DUPLICATES ALL_DEPS)

  # dump interesting properties of all imported targets that are used
  debug_deps(
    "${ALL_DEPS}" ${PROJECT_BINARY_DIR}/dep-debug/imported-libraries.txt
  )
endif()
