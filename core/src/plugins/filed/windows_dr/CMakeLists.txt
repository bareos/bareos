#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2026 Bareos GmbH & Co. KG
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

include(BareosDisableWarnings)
include(BareosConfigureFile)

add_library(format INTERFACE)
if(HAVE_WIN32)
  target_compile_options(format INTERFACE "-DUSE_STD_FMT=1")
else()
  target_link_libraries(format INTERFACE fmt::fmt)
endif()

add_library(progress-bar-logger OBJECT logger.cc)
target_link_libraries(progress-bar-logger PRIVATE indicators::indicators format)

bareos_add_plugin(barri-fd)
target_link_libraries(
  barri-fd
  PUBLIC Bareos::Lib
  PRIVATE format
)
target_compile_definitions(barri-fd PRIVATE PLUGIN_NAME="barri")
install(
  TARGETS barri-fd
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)

add_library(barri-version OBJECT)
target_sources(barri-version PRIVATE version.cc)
target_compile_definitions(
  barri-version PRIVATE BARRI_VERSION="${BAREOS_FULL_VERSION}"
                        BARRI_DATE="${DATE}" BARRI_YEAR="${BAREOS_YEAR}"
)

add_library(parser OBJECT)
add_library(Bareos::BarriParser ALIAS parser)
target_sources(parser PRIVATE parser.cc)
target_include_directories(parser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(parser PUBLIC format)

if(HAVE_WIN32)
  add_library(dump OBJECT)
  target_sources(
    dump PRIVATE dump.cc common.cc com.cc restore_win.cc parser.cc
                 remove_holes.cc
  )
  target_include_directories(dump PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
  add_library(Bareos::BarriDump ALIAS dump)
  target_link_libraries(
    dump
    PRIVATE VssApi.lib virtdisk.lib ZLIB::ZLIB
    PUBLIC format Microsoft.GSL::GSL
  )

  target_sources(barri-fd PRIVATE plugin.cc bareos_api.cc)
  target_link_libraries(barri-fd PRIVATE dump)

  add_executable(barri-cli barri.cc)
  target_link_libraries(
    barri-cli CLI11::CLI11 progress-bar-logger dump format barri-version
  )
else()
  bareos_disable_warnings(TARGETS progress-bar-logger WARNINGS "-Wshadow")

  add_library(windows-dr-restore OBJECT)
  target_sources(windows-dr-restore PRIVATE restore_linux.cc parser.cc)
  target_link_libraries(windows-dr-restore PUBLIC Microsoft.GSL::GSL format)

  target_sources(barri-fd PRIVATE plugin_linux.cc bareos_api.cc)
  target_link_libraries(barri-fd PRIVATE tl::expected windows-dr-restore)

  add_executable(barri-cli barri_linux.cc)
  target_link_libraries(
    barri-cli PRIVATE CLI11::CLI11 ZLIB::ZLIB progress-bar-logger
                      windows-dr-restore barri-version
  )

  install(
    TARGETS barri-cli
    DESTINATION ${bindir}
    COMPONENT filedaemon
  )
endif()

add_executable(logger_test)
target_sources(logger_test PRIVATE logger_test.cc)
target_link_libraries(logger_test PRIVATE progress-bar-logger format)

include(DebugEdit)
