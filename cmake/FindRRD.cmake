# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_RRD QUIET librrd)

find_path(
  RRD_INCLUDE_DIR
  NAMES rrd.h
  HINTS ${PC_RRD_INCLUDEDIR} ${PC_RRD_INCLUDE_DIRS}
)

find_library(
  RRD_LIBRARY
  NAMES rrd librrd
  HINTS ${PC_RRD_LIBDIR} ${PC_RRD_LIBRARY_DIRS}
)

if(PC_RRD_VERSION)
  set(RRD_VERSION_STRING ${PC_RRD_VERSION})
endif()

if(NOT RRD_LIBRARY OR NOT RRD_INCLUDE_DIR)
  include(CPM)
  include(ExternalProject)
  include(ProcessorCount)

  set(RRD_FALLBACK_VERSION "1.9.0")
  set(RRD_FALLBACK_URL
      "https://github.com/oetiker/rrdtool-1.x/releases/download/v${RRD_FALLBACK_VERSION}/rrdtool-${RRD_FALLBACK_VERSION}.tar.gz"
  )
  set(RRD_FALLBACK_PREFIX "${CMAKE_BINARY_DIR}/rrdtool")
  set(RRD_FALLBACK_LIBRARY
      "${RRD_FALLBACK_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}rrd${CMAKE_SHARED_LIBRARY_SUFFIX}"
  )

  find_program(RRD_MAKE_EXECUTABLE NAMES gmake make REQUIRED)
  pkg_check_modules(PC_GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
  ProcessorCount(RRD_FALLBACK_BUILD_JOBS)
  if(NOT RRD_FALLBACK_BUILD_JOBS OR RRD_FALLBACK_BUILD_JOBS EQUAL 0)
    set(RRD_FALLBACK_BUILD_JOBS 1)
  endif()

  CPMAddPackage(
    NAME rrdtool
    VERSION ${RRD_FALLBACK_VERSION}
    URL ${RRD_FALLBACK_URL}
    DOWNLOAD_ONLY YES
    EXCLUDE_FROM_ALL YES
  )
  set(RRD_FALLBACK_WARNING_FLAGS "-w -fno-lto")
  set(RRD_FALLBACK_CFLAGS "$ENV{CFLAGS} ${RRD_FALLBACK_WARNING_FLAGS}")
  string(STRIP "${RRD_FALLBACK_CFLAGS}" RRD_FALLBACK_CFLAGS)
  set(RRD_FALLBACK_CXXFLAGS "$ENV{CXXFLAGS} ${RRD_FALLBACK_WARNING_FLAGS}")
  string(STRIP "${RRD_FALLBACK_CXXFLAGS}" RRD_FALLBACK_CXXFLAGS)
  set(RRD_FALLBACK_LDFLAGS "$ENV{LDFLAGS} ${RRD_FALLBACK_WARNING_FLAGS}")
  string(STRIP "${RRD_FALLBACK_LDFLAGS}" RRD_FALLBACK_LDFLAGS)
  set(RRD_FALLBACK_MAKE_FLAGS
      AM_CFLAGS=${RRD_FALLBACK_WARNING_FLAGS}
      librrd_la_CFLAGS=${RRD_FALLBACK_WARNING_FLAGS}
      librrdupd_la_CFLAGS=${RRD_FALLBACK_WARNING_FLAGS}
  )
  set(RRD_FALLBACK_THREAD_LIBRARY
      "${CPM_PACKAGE_rrdtool_SOURCE_DIR}/src/.libs/librrdupd.a"
  )

  ExternalProject_Add(
    rrdtool_external
    PREFIX "${CMAKE_BINARY_DIR}/rrdtool-prefix"
    SOURCE_DIR "${CPM_PACKAGE_rrdtool_SOURCE_DIR}"
    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env CFLAGS=${RRD_FALLBACK_CFLAGS}
      CXXFLAGS=${RRD_FALLBACK_CXXFLAGS} LDFLAGS=${RRD_FALLBACK_LDFLAGS}
      <SOURCE_DIR>/configure --prefix=${RRD_FALLBACK_PREFIX} --disable-docs
      --disable-examples --disable-rrdcached --disable-rrdcgi
      --disable-rrd_graph --disable-rrd_restore --disable-perl --disable-ruby
      --disable-lua --disable-tcl --disable-python --disable-libdbi
      --disable-librados --disable-libwrap
    BUILD_COMMAND
      ${CMAKE_COMMAND} -E env CFLAGS=${RRD_FALLBACK_CFLAGS}
      CXXFLAGS=${RRD_FALLBACK_CXXFLAGS} LDFLAGS=${RRD_FALLBACK_LDFLAGS}
      ${RRD_MAKE_EXECUTABLE} -C src -j${RRD_FALLBACK_BUILD_JOBS}
      ${RRD_FALLBACK_MAKE_FLAGS}
    INSTALL_COMMAND
      ${CMAKE_COMMAND} -E env CFLAGS=${RRD_FALLBACK_CFLAGS}
      CXXFLAGS=${RRD_FALLBACK_CXXFLAGS} LDFLAGS=${RRD_FALLBACK_LDFLAGS}
      ${RRD_MAKE_EXECUTABLE} -C src install ${RRD_FALLBACK_MAKE_FLAGS}
    BUILD_IN_SOURCE YES
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_BYPRODUCTS "${RRD_FALLBACK_LIBRARY}" "${RRD_FALLBACK_THREAD_LIBRARY}"
  )

  add_library(rrd_fallback INTERFACE)
  add_dependencies(rrd_fallback rrdtool_external)
  target_include_directories(
    rrd_fallback INTERFACE "${RRD_FALLBACK_PREFIX}/include"
  )
  target_link_libraries(
    rrd_fallback
    INTERFACE "${RRD_FALLBACK_LIBRARY}" "${RRD_FALLBACK_THREAD_LIBRARY}"
              PkgConfig::PC_GLIB2 m
  )
  add_library(RRD::RRD ALIAS rrd_fallback)

  set(RRD_INCLUDE_DIR "${RRD_FALLBACK_PREFIX}/include")
  set(RRD_INCLUDE_DIRS "${RRD_INCLUDE_DIR}")
  set(RRD_LIBRARY "${RRD_FALLBACK_LIBRARY}")
  set(RRD_LIBRARIES "${RRD_LIBRARY}")
  set(RRD_VERSION_STRING "${RRD_FALLBACK_VERSION}")
  set(RRD_DEFINITIONS "")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  RRD
  REQUIRED_VARS RRD_LIBRARY RRD_INCLUDE_DIR
  VERSION_VAR RRD_VERSION_STRING
)

if(RRD_FOUND)
  set(RRD_LIBRARIES ${RRD_LIBRARY})
  set(RRD_INCLUDE_DIRS ${RRD_INCLUDE_DIR})
  set(HAVE_LIBRRD 1)
endif()

mark_as_advanced(RRD_INCLUDE_DIR RRD_LIBRARY)

if(RRD_FOUND AND NOT TARGET RRD::RRD)
  add_library(RRD::RRD UNKNOWN IMPORTED)
  set_target_properties(
    RRD::RRD PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${RRD_INCLUDE_DIRS}"
  )
  set_target_properties(
    RRD::RRD PROPERTIES INTERFACE_COMPILE_OPTIONS "${RRD_DEFINITIONS}"
  )
  set_property(
    TARGET RRD::RRD
    APPEND
    PROPERTY IMPORTED_LOCATION "${RRD_LIBRARY}"
  )
endif()
