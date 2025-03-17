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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/generic
    ${PROJECT_SOURCE_DIR}/src/win32/stored
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include
  )
  link_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS}
  )
  if(WINDOWS_BITS EQUAL 64)
    # work around https://bugs.python.org/issue4709
    add_definitions(-DMS_WIN64)
  endif()
endif()

if(ENABLE_PYTHON)
  add_subdirectory(python)
endif()

bareos_add_plugin(autoxflate-sd)
target_sources(autoxflate-sd PRIVATE autoxflate/autoxflate-sd.cc)

if(MSVC)
  target_link_libraries(autoxflate-sd Bareos::Lib Bareos::Fastlz ZLIB::ZLIB)
else()
  target_link_libraries(
    autoxflate-sd Bareos::Lib Bareos::Fastlz
    $<$<PLATFORM_ID:Windows>:ZLIB::ZLIB>
  )
endif()
install(TARGETS autoxflate-sd DESTINATION ${plugindir})

if(NOT HAVE_WIN32 AND NOT HAVE_DARWIN_OS)
  bareos_add_plugin(scsicrypto-sd)
  target_sources(scsicrypto-sd PRIVATE scsicrypto/scsicrypto-sd.cc)
  install(TARGETS scsicrypto-sd DESTINATION ${plugindir})

  bareos_add_plugin(scsitapealert-sd)
  target_sources(scsitapealert-sd PRIVATE scsitapealert/scsitapealert-sd.cc)
  install(TARGETS scsitapealert-sd DESTINATION ${plugindir})
  if(HAVE_DARWIN_OS)
    target_link_libraries(scsitapealert-sd bareossd)
  endif()
endif()

include(DebugEdit)
