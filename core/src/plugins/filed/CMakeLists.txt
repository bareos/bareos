#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2024 Bareos GmbH & Co. KG
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

  link_directories(
    ${PROJECT_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS}
  )
  if(WINDOWS_BITS EQUAL 64)
    # work around https://bugs.python.org/issue4709
    add_definitions(-DMS_WIN64)
  endif()
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

if(ENABLE_PYTHON)
  add_subdirectory(python)
endif()

add_library(bpipe-fd MODULE bpipe/bpipe-fd.cc)
set_target_properties(bpipe-fd PROPERTIES PREFIX "")
install(
  TARGETS bpipe-fd
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)
if(HAVE_WIN32 OR HAVE_DARWIN_OS)
  target_link_libraries(bpipe-fd bareos)
endif()

if(HAVE_WIN32)
  add_library(mssqlvdi-fd MODULE ../../win32/plugins/filed/mssqlvdi-fd.cc)
  target_compile_options(mssqlvdi-fd PUBLIC -fpermissive)
  target_include_directories(
    mssqlvdi-fd PRIVATE ${PROJECT_SOURCE_DIR}/src/win32/vdi/include
  )
  if(MSVC)
    target_link_libraries(
      mssqlvdi-fd PRIVATE ${THREADS_THREADS} bareos comsupp.lib
    )
  else()
    target_link_libraries(mssqlvdi-fd bareos)
  endif()
  set_target_properties(
    mssqlvdi-fd PROPERTIES PREFIX "" DEFINE_SYMBOL "BUILDING_DLL"
  )

  install(
    TARGETS mssqlvdi-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
endif()

if(NOT HAVE_WIN32)
  add_library(example-plugin-fd MODULE example/example-plugin-fd.cc)
  set_target_properties(example-plugin-fd PROPERTIES PREFIX "")
endif()

if(NOT HAVE_WIN32)
  add_library(spam-plugin-fd MODULE spam/spam.cc)
  set_target_properties(spam-plugin-fd PROPERTIES PREFIX "")
endif()

option(ENABLE_GFAPI_FD "Build gfapi-fd plugin if possible" ON)
if(HAVE_GLUSTERFS AND ENABLE_GFAPI_FD)
  bareos_configure_file(GLOB_RECURSE "gfapi/gfapi-conf.d/*" COPY)
  add_library(gfapi-fd MODULE gfapi/gfapi-fd.cc)
  set_target_properties(gfapi-fd PROPERTIES PREFIX "")
  install(
    TARGETS gfapi-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(gfapi-fd gfapi)
  target_include_directories(gfapi-fd PRIVATE ${GFAPI_INCLUDE_DIRS})
endif()

add_subdirectory(grpc)

include(DebugEdit)
