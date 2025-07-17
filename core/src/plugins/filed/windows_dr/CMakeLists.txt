#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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
if(0)
  target_compile_options(format INTERFACE USE_STD_FMT)
else()
  target_link_libraries(format INTERFACE fmt::fmt-header-only)
endif()

add_library(progress-bar-logger OBJECT logger.cc)
target_link_libraries(progress-bar-logger PRIVATE indicators::indicators format)

bareos_add_plugin(wdr-fd)
target_link_libraries(
  wdr-fd
  PUBLIC Bareos::Lib
  PRIVATE format
)
install(
  TARGETS wdr-fd
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)

if(HAVE_WIN32)
  add_library(dump OBJECT)
  target_sources(
    dump PRIVATE dump.cc common.cc com.cc restore.cc parser.cc remove_holes.cc
  )
  target_link_libraries(
    dump PRIVATE VssApi.lib virtdisk.lib format Microsoft.GSL::GSL
                 ${ZLIB_LIBRARIES}
  )

  target_sources(wdr-fd PRIVATE plugin.cc bareos_api.cc)
  target_link_libraries(wdr-fd PRIVATE dump)

  add_executable(barri barri.cc)
  target_link_libraries(barri CLI11::CLI11 progress-bar-logger dump format)
  target_compile_definitions(
    barri PRIVATE BARRI_VERSION="${BAREOS_FULL_VERSION}" BARRI_DATE="${DATE}"
  )

  add_executable(copy copy.cc)
  add_executable(count count.cc)
else()

  target_sources(wdr-fd PRIVATE plugin_linux.cc)
  target_compile_definitions(wdr-fd PRIVATE PLUGIN_NAME="wdr")
  target_link_libraries(wdr-fd PRIVATE tl::expected)
  # target_link_libraries(wdr-fd PRIVATE dump)

  add_executable(barri restore_linux.cc parser.cc)
  target_compile_options(progress-bar-logger PRIVATE "-Wno-shadow")
  target_compile_definitions(
    barri PRIVATE BARRI_VERSION="${BAREOS_FULL_VERSION}" BARRI_DATE="${DATE}"
  )

  add_executable(test-progress progress.cc)
  target_link_libraries(test-progress PUBLIC indicators::indicators)
  target_compile_options(test-progress PRIVATE "-Wno-shadow")

  target_link_libraries(
    barri CLI11::CLI11 Microsoft.GSL::GSL ${ZLIB_LIBRARIES}
    fmt::fmt-header-only progress-bar-logger
  )

  install(
    TARGETS barri
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

bareos_add_test(
  find_holes
  LINK_LIBRARIES GTest::gtest_main
  ADDITIONAL_SOURCES remove_holes.cc
)

include(DebugEdit)
