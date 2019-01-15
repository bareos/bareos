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

cmake_minimum_required(VERSION 3.0)
project(bareos)

# switch on CXX 11 Support
#
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
   set(CMAKE_CXX_STANDARD 11)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# cmake does not understand how to enforce CMAKE_CXX_STANDARD 11
# on Solaris  so we do it manually.
if (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wsuggest-override compiler_will_suggest_override)

if (${compiler_will_suggest_override})
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-override")
endif()

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
include_directories(/usr/include)
endif()

set(CMAKE_MODULE_PATH
         ${CMAKE_MODULE_PATH}
         ${PROJECT_SOURCE_DIR}/cmake)

SET(host ${CMAKE_SYSTEM})

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)





# enable "make check"
enable_testing()
set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})


# run git-info to collect sourcecode info
file(MAKE_DIRECTORY build)
execute_process(
    COMMAND ${PROJECT_SOURCE_DIR}/scripts/git-info.sh
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/build
)

INCLUDE(BareosExtractVersionInfo)

INCLUDE(BareosFindAllLibraries)

IF (NOT ${OPENSSL_FOUND})
  MESSAGE(FATAL_ERROR "FATAL ERROR: OpenSSL is required but was not found.")
ENDIF()


IF (coverage)
   set(COVERAGE_COMPILER_FLAGS "-g -O0 --coverage -fprofile-arcs -ftest-coverage" CACHE INTERNAL "")
   MESSAGE(STATUS "coverage requested, adding COVERAGE_COMPILER_FLAGS : ${COVERAGE_COMPILER_FLAGS}")
   set(CMAKE_BUILD_TYPE Debug)
   if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      link_libraries(gcov)
   else()
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
   endif()
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
ENDIF()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   set(HAVE_LINUX_OS 1)
   set(PSCMD "ps -e")
   include_directories(/usr/include/tirpc)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
   set(HAVE_SUN_OS 1)
   set(PSCMD "ps -e -o pid,comm")
   link_libraries(nsl socket curses)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
   set(HAVE_FREEBSD_OS 1)
   set(PSCMD "ps -ax -o pid,command")
   include_directories(/usr/local/include)
   link_directories(/usr/local/lib)
   link_libraries(intl)
endif()

# hpux
if (${CMAKE_SYSTEM_NAME} MATCHES "HP-UX")
   set(HAVE_HPUX_OS 1)
   set(PSCMD "UNIX95=1; ps -e -o pid,comm")
   add_definitions(-D_XOPEN_SOURCE_EXTENDED=1)
   add_definitions(-D_INCLUDE_XOPEN_SOURCE_EXTENDED=1)
   add_definitions(-D_INCLUDE_LONGLONG=1)
   include_directories(/usr/local/include/)
   link_directories(/usr/lib/hpux32 /usr/local/lib/hpux32 /usr/local/lib )
   link_libraries(dld intl)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "AIX")
   set(HAVE_AIX_OS 1)
   set(lmdb 0FF)
   link_libraries(intl)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
   include_directories(
      ${PROJECT_SOURCE_DIR}/src/win32/include
      ${PROJECT_SOURCE_DIR}/src/win32/compat/include
      )

   set(HAVE_WIN32 1)

   set(WINDOWS_LIBRARIES ws2_32)

   SET(PYTHON_LIBRARIES ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS}/python27.dll )
   SET(PYTHON_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include)
   SET(HAVE_PYTHON 1)

   SET(PostgreSQL_LIBRARY ${PROJECT_SOURCE_DIR}/src/win32/cats/pgsql/lib/${WINDOWS_BITS}/libpq.dll)
   SET(PostgreSQL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/win32/cats/pgsql/include)
   SET(PostgreSQL_TYPE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include)


else()
   add_definitions("-D_FILE_OFFSET_BITS=64")
endif()

IF(developer)
   add_definitions("-DDEVELOPER=1")
ENDIF()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-register")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-register")
endif()

INCLUDE(BareosFindPrograms)

INCLUDE(GNUInstallDirs)

INCLUDE(BareosSetVariableDefaults)

if (${CMAKE_SYSTEM_NAME} MATCHES "AIX")
   SET(ENABLE_NLS 0)
endif()

INCLUDE(BareosGetDistInfo)

INCLUDE(BareosGenerateDebianInfo)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
   add_definitions(-DWIN32_VSS -DMINGW64 -DHAVE_WIN32 -DHAVE_MINGW -D_WIN32_WINNT=${WINDOWS_VERSION})
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m${WINDOWS_BITS} -mwin32 -mthreads")
ENDIF()

# set INSTALL_LIB_DIR automatically to lib or lib64 automatically
# Taken from  https://cmake.org/pipermail/cmake/2013-July/055374.html

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if ("${LIB64}" STREQUAL "TRUE")
    set(LIBSUFFIX 64)
else()
    set(LIBSUFFIX "")
endif()
set(INSTALL_LIB_DIR  "usr/lib${LIBSUFFIX}" CACHE PATH "Installation directory for libraries")
mark_as_advanced(INSTALL_LIB_DIR)


# RPATH settings (from https://cmake.org/Wiki/CMake_RPATH_handling)
# use, i.e. don')t skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
#SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

#SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")
#SET(CMAKE_INSTALL_RPATH "/${INSTALL_LIB_DIR}")
SET(CMAKE_INSTALL_RPATH "${libdir}")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)




#cmake_host_system_information(RESULT hostname QUERY HOSTNAME)
site_name(hostname)

set(dbdriver sqlite3)

#manual definitions
set (BAREOS "Bareos")
#set(CONFDIR \"${CMAKE_INSTALL_FULL_SYSCONFDIR}\")
set(LOCALEDIR \"${CMAKE_INSTALL_FULL_LOCALEDIR}\")




set(HAVE_CRYPTO 1)

INCLUDE(BareosTypeSizes)



INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIGENDIAN)
IF(IS_BIGENDIAN)
   #SET(WORDS_BIGENDIAN 1)
   SET(HAVE_BIG_ENDIAN 1)
ELSE()
   SET(HAVE_LITTLE_ENDIAN 1)
ENDIF()



MESSAGE( STATUS "VERSION: " ${CMAKE_MATCH_1} )

MESSAGE( STATUS "PROJECT_SOURCE_DIR:         " ${PROJECT_SOURCE_DIR} )


# needed for check_include
set(CMAKE_REQUIRED_DEFINITIONS "-D_FILE_OFFSET_BITS=64")
INCLUDE(BareosCheckIncludes)

INCLUDE(BareosCheckFunctions)

INCLUDE(BareosCheckSymbols)

INCLUDE(BareosCheckStructHasMembers)

INCLUDE(BareosDetermineHaveLowLevelScsiInterface)

include_directories(${PYTHON_INCLUDE_DIRS})

include_directories(${PYTHON_INCLUDE_PATH})

include_directories(${PROJECT_SOURCE_DIR}/src)

# trick for socklen_t
set(CMAKE_EXTRA_INCLUDE_FILES "sys/socket.h")
check_type_size("socklen_t" HAVE_SOCKLEN_T)
set(CMAKE_EXTRA_INCLUDE_FILES)


set(CMAKE_EXTRA_INCLUDE_FILES sys/socket.h)




# ------------------------- Begin Generic CMake Variable Logging ------------------

# /*	C++ comment style not allowed	*/


# if you are building in-source, this is the same as PROJECT_SOURCE_DIR, otherwise
# this is the top level directory of your build tree
MESSAGE( STATUS "CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR} )

# if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR, otherwise this
# is the directory where the compiled or generated files from the current CMakeLists.txt will go to
MESSAGE( STATUS "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR} )

# this is the directory, from which cmake was started, i.e. the top level source directory
MESSAGE( STATUS "PROJECT_SOURCE_DIR:         " ${PROJECT_SOURCE_DIR} )

# this is the directory where the currently processed CMakeLists.txt is located in
MESSAGE( STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR} )

# contains the full path to the top level directory of your build tree
MESSAGE( STATUS "PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR} )

# contains the full path to the root of your project source directory,
# i.e. to the nearest directory where CMakeLists.txt contains the PROJECT() command
MESSAGE( STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR} )

# set this variable to specify a common place where CMake should put all executable files
# (instead of CMAKE_CURRENT_BINARY_DIR)
MESSAGE( STATUS "EXECUTABLE_OUTPUT_PATH: " ${EXECUTABLE_OUTPUT_PATH} )

# set this variable to specify a common place where CMake should put all libraries
# (instead of CMAKE_CURRENT_BINARY_DIR)
MESSAGE( STATUS "LIBRARY_OUTPUT_PATH:     " ${LIBRARY_OUTPUT_PATH} )

# tell CMake to search first in directories listed in CMAKE_MODULE_PATH
# when you use FIND_PACKAGE() or INCLUDE()
MESSAGE( STATUS "CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH} )

# this is the complete path of the cmake which runs currently (e.g. /usr/local/bin/cmake)
MESSAGE( STATUS "CMAKE_COMMAND: " ${CMAKE_COMMAND} )

# this is the CMake installation directory
MESSAGE( STATUS "CMAKE_ROOT: " ${CMAKE_ROOT} )

# this is the filename including the complete path of the file where this variable is used.
MESSAGE( STATUS "CMAKE_CURRENT_LIST_FILE: " ${CMAKE_CURRENT_LIST_FILE} )

# this is linenumber where the variable is used
MESSAGE( STATUS "CMAKE_CURRENT_LIST_LINE: " ${CMAKE_CURRENT_LIST_LINE} )

# this is used when searching for include files e.g. using the FIND_PATH() command.
MESSAGE( STATUS "CMAKE_INCLUDE_PATH: " ${CMAKE_INCLUDE_PATH} )

# this is used when searching for libraries e.g. using the FIND_LIBRARY() command.
MESSAGE( STATUS "CMAKE_LIBRARY_PATH: " ${CMAKE_LIBRARY_PATH} )

# the complete system name, e.g. "Linux-2.4.22", "FreeBSD-5.4-RELEASE" or "Windows 5.1"
MESSAGE( STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM} )

# the short system name, e.g. "Linux", "FreeBSD" or "Windows"
MESSAGE( STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME} )

# only the version part of CMAKE_SYSTEM
MESSAGE( STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION} )

# the processor name (e.g. "Intel(R) Pentium(R) M processor 2.00GHz")
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )

# is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
MESSAGE( STATUS "UNIX: " ${UNIX} )

# is TRUE on Windows, including CygWin
MESSAGE( STATUS "WIN32: " ${WIN32} )

# is TRUE on Apple OS X
MESSAGE( STATUS "APPLE: " ${APPLE} )

# is TRUE when using the MinGW compiler in Windows
MESSAGE( STATUS "MINGW: " ${MINGW} )

# is TRUE on Windows when using the CygWin version of cmake
MESSAGE( STATUS "CYGWIN: " ${CYGWIN} )

# is TRUE on Windows when using a Borland compiler
MESSAGE( STATUS "BORLAND: " ${BORLAND} )

# Microsoft compiler
MESSAGE( STATUS "MSVC: " ${MSVC} )
MESSAGE( STATUS "MSVC_IDE: " ${MSVC_IDE} )
MESSAGE( STATUS "MSVC60: " ${MSVC60} )
MESSAGE( STATUS "MSVC70: " ${MSVC70} )
MESSAGE( STATUS "MSVC71: " ${MSVC71} )
MESSAGE( STATUS "MSVC80: " ${MSVC80} )
MESSAGE( STATUS "CMAKE_COMPILER_2005: " ${CMAKE_COMPILER_2005} )


# set this to true if you don't want to rebuild the object files if the rules have changed,
# but not the actual source files or headers (e.g. if you changed the some compiler switches)
MESSAGE( STATUS "CMAKE_SKIP_RULE_DEPENDENCY: " ${CMAKE_SKIP_RULE_DEPENDENCY} )

# since CMake 2.1 the install rule depends on all, i.e. everything will be built before installing.
# If you don't like this, set this one to true.
MESSAGE( STATUS "CMAKE_SKIP_INSTALL_ALL_DEPENDENCY: " ${CMAKE_SKIP_INSTALL_ALL_DEPENDENCY} )

# If set, runtime paths are not added when using shared libraries. Default it is set to OFF
MESSAGE( STATUS "CMAKE_SKIP_RPATH: " ${CMAKE_SKIP_RPATH} )

# set this to true if you are using makefiles and want to see the full compile and link
# commands instead of only the shortened ones
MESSAGE( STATUS "CMAKE_VERBOSE_MAKEFILE: " ${CMAKE_VERBOSE_MAKEFILE} )

# this will cause CMake to not put in the rules that re-run CMake. This might be useful if
# you want to use the generated build files on another machine.
MESSAGE( STATUS "CMAKE_SUPPRESS_REGENERATION: " ${CMAKE_SUPPRESS_REGENERATION} )


# A simple way to get switches to the compiler is to use ADD_DEFINITIONS().
# But there are also two variables exactly for this purpose:

# the compiler flags for compiling C sources
MESSAGE( STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} )

# the compiler flags for compiling C++ sources
MESSAGE( STATUS "CMAKE_CXX_FLAGS: " ${CMAKE_CXX_FLAGS} )


# Choose the type of build.  Example: SET(CMAKE_BUILD_TYPE Debug)
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

# if this is set to ON, then all libraries are built as shared libraries by default.
MESSAGE( STATUS "BUILD_SHARED_LIBS: " ${BUILD_SHARED_LIBS} )

# the compiler used for C files
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} )

# the compiler used for C++ files
MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )

# if the compiler is a variant of gcc, this should be set to 1
MESSAGE( STATUS "CMAKE_COMPILER_IS_GNUCC: " ${CMAKE_COMPILER_IS_GNUCC} )

# if the compiler is a variant of g++, this should be set to 1
MESSAGE( STATUS "CMAKE_COMPILER_IS_GNUCXX : " ${CMAKE_COMPILER_IS_GNUCXX} )

# the tools for creating libraries
MESSAGE( STATUS "CMAKE_AR: " ${CMAKE_AR} )
MESSAGE( STATUS "CMAKE_RANLIB: " ${CMAKE_RANLIB} )

MESSAGE( STATUS "CMAKE_INSTALL_PREFIX:         " ${CMAKE_INSTALL_PREFIX} )


set(uncomment_dbi "#")

set(srcdir ${PROJECT_SOURCE_DIR})

set(CONFDIR "\"${confdir}\"")

set(CC ${CMAKE_C_COMPILER})
set(CCVERSION ${CMAKE_CXX_COMPILER_VERSION})
set(CXX ${CMAKE_CXX_COMPILER})
set(CXXVERSION ${CMAKE_CXX_COMPILER_VERSION})

IF( "${HAVE_OPENSSL}" )
   set(HAVE_TLS "1")
ENDIF()

IF(NOT openssl)
   unset(HAVE_OPENSSL)
   unset(HAVE_TLS)
ENDIF()

set(largefile_support "" )
set(got_conio "" )

set(got_readline "${READLINE_FOUND}" )
if ("${READLINE_FOUND}")
   set(HAVE_READLINE 1)
endif()

if ("${PAM_FOUND}")
   set(HAVE_PAM 1)
endif()


set(have_tls "${HAVE_TLS}" )
set(have_crypto "${HAVE_CRYPTO}" )
set(have_openssl "${HAVE_OPENSSL}" )
set(have_zlib "${ZLIB_FOUND}" )
set(have_fastlz "${HAVE_FASTLZLIB_H}" )




set(HAVE_CAPABILITY "${HAVE_CAPABILITY_H}" CACHE BOOL "compile with capability support")
if ("${HAVE_CAPABILITY}")
   set(CAPABILITY_LIBS cap)
   message( STATUS "CAPABILITY set")
endif()




set(ndmp "${ndmp}" )
set(lmdb "${lmdb}" )
set(build_client_only "${client-only}" )
set(build_dird "${build-dird}" )
set(build_stored "${build-stored}" )
set(have_plugins "${have_plugins}" )
set(have_afs "" )

if(${HAVE_SYS_ACL_H})
   set(HAVE_ACL 1)
   Message(STATUS "acl found, libs: ${ACL_LIBS}")
endif()

if("${HAVE_SYS_XATTR_H}" OR "${HAVE_SYS_EXTATTR_H}")
   set(HAVE_XATTR 1)
   set(have_xattr YES)
endif()

if(${HAVE_RADOS_LIBRADOS_H})
   set(HAVE_CEPH_RADOS 1)
   set(have_ceph_rados ${HAVE_CEPH_RADOS} )
endif()

if(${HAVE_RADOSSTRIPER_LIBRADOSSTRIPER_H})
   set(HAVE_CEPH_RADOS_STRIPER 1)
   set(have_ceph_rados_striper ${HAVE_CEPH_RADOS_STRIPER})
endif()


if(${HAVE_GLUSTERFS_API_GLFS_H})
   set(HAVE_GLUSTERFS 1)
endif()
set(have_glusterfs ${HAVE_GLUSTERFS_API_GLFS_H} )
if (${HAVE_GLUSTERFS})
include_directories(/usr/include/glusterfs)
endif()



if (NOT HAVE_WIN32)
   if (${ndmp})
      set(HAVE_NDMP 1)
      set(NDMP_LIBS bareosndmp)
   endif()
ENDIF()

if (${lmdb})
   set(HAVE_LMDB 1)
   set(LMDB_LIBS bareoslmdb)
endif()



# info what the config files need to be installed
#####  PLUGINS  ############
SET(PLUGINS python-ldap-conf.d)
if(${HAVE_CEPHFS})
LIST(APPEND PLUGINS cephfs-conf.d)
endif()
if(${HAVE_CEPH_RADOS})
LIST(APPEND PLUGINS rados-conf.d)
endif()

if(${HAVE_GLUSTERFS})
LIST(APPEND PLUGINS gfapi-conf.d)
endif()

##### BACKENDS  ####

IF(build_client_only)
   SET(BACKENDS "")
ELSE()
   SET(BACKENDS unix_tape_device.d)
   LIST(APPEND BACKENDS unix_fifo_device.d)
   if(${HAVE_CEPHFS})
      LIST(APPEND BACKENDS rados_device.d)
   endif()
   if(${HAVE_GLUSTERFS})
      LIST(APPEND BACKENDS gfapi_device.d)
   endif()
   if(${HAVE_DROPLET})
      LIST(APPEND BACKENDS droplet_device.d)
   endif()
ENDIF()



set(have_elasto "" )
set(support_systemd "" )
set(batch_insert_db_backends "" )
set(have_cmocka "" )

set(_PATH_BAREOS_BACKENDDIR \"${backenddir}\")
set(_PATH_BAREOS_PIDDIR \"${piddir}\")
set(_PATH_BAREOS_WORKINGDIR \"${workingdir}\")




IF(HAVE_WIN32)
  INSTALL(DIRECTORY DESTINATION var/log)
ENDIF()

INSTALL(DIRECTORY DESTINATION ${logdir})


INCLUDE(BareosConfigureFile)


add_subdirectory(scripts)
add_subdirectory(manpages)
add_subdirectory(platforms)
add_subdirectory(src)

STRING(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S")

# give configure output
MESSAGE(" ")
MESSAGE("Configuration on ${NOW} : ")
MESSAGE(" ")

MESSAGE("   Host:                         ${host} -- ${DISTNAME} ${DISTVER} ")
MESSAGE("   Bareos version:               ${BAREOS} ${VERSION} (${DATE}) ")
MESSAGE("   Distribution:                 ${DISTNAME} OBS PROJECT:${OBS_PROJECT} OBS DIST:${OBS_DISTRIBUTION} OBS ARCH:${OBS_ARCH}")
MESSAGE("   Source code location:         ${srcdir} ")
MESSAGE("   Modify Debian Control file:   ${GENERATE_DEBIAN_CONTROL} ")
MESSAGE("   Install binaries:             ${bindir} ")
MESSAGE("   Install system binaries:      ${sbindir} ")
MESSAGE("   Install libraries:            ${libdir} ")
MESSAGE("   Install system config files:  ${sysconfdir} ")
MESSAGE("   Install Bareos config dir:    ${confdir} ")
MESSAGE("   Install Bareos config files:  ${configtemplatedir} ")
MESSAGE("   Log directory:                ${logdir} ")
MESSAGE("   Scripts directory:            ${scriptdir} ")
MESSAGE("   Archive directory:            ${archivedir} ")
MESSAGE("   Working directory:            ${workingdir} ")
MESSAGE("   BSR directory:                ${bsrdir} ")
MESSAGE("   PID directory:                ${piddir} ")
MESSAGE("   Subsys directory:             ${subsysdir} ")
MESSAGE("   Man directory:                ${mandir} ")
MESSAGE("   Data directory:               ${datarootdir} ")
MESSAGE("   Backend directory:            ${backenddir} ")
MESSAGE("   Plugin directory:             ${plugindir} ")
MESSAGE("   C Compiler:                   ${CC} ${CCVERSION} ")
MESSAGE("   C++ Compiler:                 ${CXX} ${CXXVERSION} ")
MESSAGE("   C Compiler flags:             ${CMAKE_C_FLAGS} ")
MESSAGE("   C++ Compiler flags:           ${CMAKE_CXX_FLAGS} ")
MESSAGE("   Linker flags:                 ${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_MODULE_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_STATIC_LINKER_FLAGS} ")
MESSAGE("   Libraries:                    ${LIBS} ")
MESSAGE("   Statically Linked Tools:      ${support_static_tools} ")
MESSAGE("   Statically Linked FD:         ${support_static_fd} ")
MESSAGE("   Statically Linked SD:         ${support_static_sd} ")
MESSAGE("   Statically Linked DIR:        ${support_static_dir} ")
MESSAGE("   Statically Linked CONS:       ${support_static_cons} ")
MESSAGE("   Database backends:            ${db_backends} ")
MESSAGE("   Database port:                ${db_port} ")
MESSAGE("   Database name:                ${db_name} ")
MESSAGE("   Database user:                ${db_user} ")
MESSAGE("   Database version:             ${BDB_VERSION} ")
MESSAGE(" ")
MESSAGE("   Job Output Email:             ${job_email} ")
MESSAGE("   Traceback Email:              ${dump_email} ")
MESSAGE("   SMTP Host Address:            ${smtp_host} ")
MESSAGE(" ")
MESSAGE("   Director Port:                ${dir_port} ")
MESSAGE("   File daemon Port:             ${fd_port} ")
MESSAGE("   Storage daemon Port:          ${sd_port} ")
MESSAGE(" ")
MESSAGE("   Director User:                ${dir_user} ")
MESSAGE("   Director Group:               ${dir_group} ")
MESSAGE("   Storage Daemon User:          ${sd_user} ")
MESSAGE("   Storage DaemonGroup:          ${sd_group} ")
MESSAGE("   File Daemon User:             ${fd_user} ")
MESSAGE("   File Daemon Group:            ${fd_group} ")
MESSAGE(" ")
MESSAGE("   Large file support:           ${largefile_support}")
MESSAGE("   Bareos conio support:         ${got_conio} ${CONS_LIBS} ")
MESSAGE("   readline support:             ${got_readline} ${Readline_ROOT_DIR} ${Readline_INCLUDE_DIR} ${Readline_LIBRARY}
 ")
MESSAGE("   TCP Wrappers support:         ${WRAP_FOUND} ${WRAP_LIBRARIES} ${WRAP_INCLUDE_DIRS} ")
MESSAGE("   TLS support:                  ${have_tls} ")
MESSAGE("   Encryption support:           ${have_crypto} ")
MESSAGE("   OpenSSL support:              ${OPENSSL_FOUND} ${OPENSSL_VERSION} ${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES} ")
MESSAGE("   PAM support:                  ${PAM_FOUND} ${PAM_LIBRARIES} ${PAM_INCLUDE_DIRS} ")
MESSAGE("   ZLIB support:                 ${ZLIB_FOUND} ${ZLIB_LIBRARIES} ${ZLIB_INCLUDE_DIRS} ")
MESSAGE("   LZO2 support:                 ${LZO2_FOUND} ${LZO2_LIBRARIES} ${LZO2_INCLUDE_DIRS} ")
MESSAGE("   FASTLZ support:               ${FASTLZ_FOUND} ${FASTLZ_LIBRARIES} ${FASTLZ_INCLUDE_DIRS} ")
MESSAGE("   JANSSON support:              ${JANSSON_FOUND} ${JANSSON_LIBRARIES} ${JANSSON_INCLUDE_DIRS} ")
MESSAGE("   LMDB support:                 ${lmdb} ")
MESSAGE("   NDMP support:                 ${ndmp} ")
MESSAGE("   Build ndmjob binary:          ${build_ndmjob} ")
MESSAGE("   enable-smartalloc:            ${smartalloc} ")
MESSAGE("   enable-lockmgr:               ${lockmgr} ")
MESSAGE("   bat support:                  ${support_bat} ")
MESSAGE("   tray-monitor support:         ${HAVE_TRAYMONITOR} ")
MESSAGE("   test-plugin support:          ${HAVE_TEST_PLUGIN} ")
MESSAGE("   client-only:                  ${build_client_only} ")
MESSAGE("   build-dird:                   ${build_dird} ")
MESSAGE("   build-stored:                 ${build_stored} ")
MESSAGE("   Plugin support:               ${have_plugins} ")
MESSAGE("   AFS support:                  ${have_afs} ")
MESSAGE("   ACL support:                  ${HAVE_ACL} ${ACL_LIBS}")
MESSAGE("   XATTR support:                ${have_xattr} ")
MESSAGE("   SCSI Crypto support:          ${scsi-crypto} ${HAVE_LOWLEVEL_SCSI_INTERFACE} ")
MESSAGE("   GFAPI(GLUSTERFS) support:     ${GFAPI_FOUND} ${GFAPI_LIBRARIES} ${GFAPI_INCLUDE_DIRS} ")
MESSAGE("   DROPLET support:              ${DROPLET_FOUND} ${DROPLET_LIBRARIES} ${DROPLET_INCLUDE_DIRS} ")
MESSAGE("   CEPH RADOS support:           ${RADOS_FOUND} ${RADOS_LIBRARIES} ${RADOS_INCLUDE_DIRS} ")
MESSAGE("   RADOS striping support:       ${RADOSSTRIPER_FOUND} ${RADOSSTRIPER_LIBRARIES} ${RADOSSTRIPER_INCLUDE_DIRS} ")
MESSAGE("   CEPHFS support:               ${CEPHFS_FOUND} ${CEPHFS_LIBRARIES} ${CEPHFS_INCLUDE_DIRS} ")
MESSAGE("   ELASTO support:               ${have_elasto} ")
MESSAGE("   Python support:               ${PYTHONLIBS_FOUND} ${PYTHONLIBS_VERSION_STRING} ${PYTHON_INCLUDE_PATH}")
MESSAGE("   systemd support:              ${WITH_SYSTEMD} ${SYSTEMD_UNITDIR}")
MESSAGE("   Batch insert enabled:         ${USE_BATCH_FILE_INSERT}")
MESSAGE("   gtest support:                ${GTEST_FOUND} ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ")

MESSAGE("   Dynamic cats backends:        ${dynamic-cats-backends} ${HAVE_DYNAMIC_CATS_BACKENDS} ")
MESSAGE("   Dynamic storage backends:     ${dynamic-storage-backends} ${HAVE_DYNAMIC_SD_BACKENDS} ")
MESSAGE("   Build for Test Coverage :     ${coverage} ")

message("   PSCMD:                        ${PSCMD}")
message("   PS:                           ${PS}")
message("   PIDOF:                        ${PIDOF}")
message("   PGREP:                        ${PGREP}")
message("   AWK:                          ${AWK}")
message("   GAWK:                         ${GAWK}")
message("   RPCGEN:                       ${RPCGEN}")
message("   MTX:                          ${MTX}")
message("   DEVELOPER:                    ${developer}")

IF(HAVE_WIN32)
   message("   WINDOWS_VERSION:              ${WINDOWS_VERSION}")
   message("   WINDOWS_BITS:                 ${WINDOWS_BITS}")
ENDIF()
