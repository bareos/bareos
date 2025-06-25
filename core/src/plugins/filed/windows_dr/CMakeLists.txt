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

add_library(parser OBJECT)
target_sources(parser PRIVATE parser.cc)
target_link_libraries(parser PUBLIC fmt::fmt-header-only)

if(HAVE_WIN32)
  add_executable(barri data_dumper.cc restore.cc common.cc)

  target_link_libraries(
    barri VssApi.lib virtdisk.lib CLI11::CLI11 ${ZLIB_LIBRARIES} parser
    Microsoft.GSL::GSL
  )

  add_executable(copy copy.cc)
else()
  add_executable(barri restore_linux.cc parser.cc)
  add_library(progress-bar-logger OBJECT logger.cc)
  target_compile_options(progress-bar-logger PRIVATE "-Wno-shadow")
  target_link_libraries(
    progress-bar-logger PRIVATE indicators::indicators fmt::fmt-header-only
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

include(DebugEdit)
