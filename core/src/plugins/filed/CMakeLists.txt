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

include(BareosConfigureFile)
include(BareosPlugins)

if(HAVE_WIN32)
  include_directories(
    ${PROJECT_SOURCE_DIR}/src/win32
    ${PROJECT_SOURCE_DIR}/src/win32/include
    ${PROJECT_SOURCE_DIR}/src/win32/filed
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/filed
    ${PROJECT_SOURCE_DIR}/src/win32/compat/include
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/include
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/filed
    ${PROJECT_SOURCE_DIR}/src/plugins/filed
  )

  # work around https://bugs.python.org/issue4709
  add_definitions(-DMS_WIN64)
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

if(ENABLE_PYTHON)
  add_subdirectory(python)
endif()

bareos_add_plugin(bpipe-fd)
target_sources(bpipe-fd PRIVATE bpipe/bpipe-fd.cc)
install(
  TARGETS bpipe-fd
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
if(HAVE_WIN32 OR HAVE_DARWIN_OS)
  target_link_libraries(bpipe-fd Bareos::Lib)
endif()

if(HAVE_WIN32)
  bareos_add_plugin(mssqlvdi-fd)
  target_sources(mssqlvdi-fd PRIVATE ../../win32/plugins/filed/mssqlvdi-fd.cc)
  target_include_directories(
    mssqlvdi-fd PRIVATE ${PROJECT_SOURCE_DIR}/src/win32/vdi/include
  )
  if(MSVC)
    target_link_libraries(mssqlvdi-fd PRIVATE Bareos::Lib comsupp.lib)
  else()
    target_compile_options(mssqlvdi-fd PUBLIC -fpermissive)
    target_link_libraries(mssqlvdi-fd Bareos::Lib)
  endif()
  set_target_properties(mssqlvdi-fd PROPERTIES PREFIX "")

  install(
    TARGETS mssqlvdi-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )

  bareos_add_plugin(hyper-v-fd)
  target_sources(hyper-v-fd PRIVATE ../../win32/plugins/filed/hyper-v.cc)
  target_link_libraries(hyper-v-fd PRIVATE ${THREADS_THREADS} bareos)
  target_link_libraries(
    hyper-v-fd PRIVATE wbemuuid.lib virtdisk.lib Shell32.lib
  )
  target_link_libraries(hyper-v-fd PRIVATE Jansson::Jansson)

  install(
    TARGETS hyper-v-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

if(NOT HAVE_WIN32)
  bareos_add_plugin(example-plugin-fd)
  target_sources(example-plugin-fd PRIVATE example/example-plugin-fd.cc)
endif()

if(NOT HAVE_WIN32)
  bareos_add_plugin(spam-plugin-fd)
  target_sources(spam-plugin-fd PRIVATE spam/spam.cc)
endif()

add_subdirectory(grpc)

if(ENABLE_BARRI)
  add_subdirectory(windows_dr)
endif()

include(DebugEdit)
