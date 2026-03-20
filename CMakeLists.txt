#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2025 Bareos GmbH & Co. KG
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

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  cmake_minimum_required(VERSION 3.25...3.31)
else()
  cmake_minimum_required(VERSION 3.17...3.31)
endif()

get_property(HAVE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(HAVE_MULTI_CONFIG)
  message(
    FATAL_ERROR "Multi-config generators are not supported. "
                "While they may build fine, most tests won't work currently."
  )
endif()

project(bareos NONE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/core/cmake")

option(coverage "Enable test coverage analysis" OFF)
if(coverage)
  set(CMAKE_BUILD_TYPE Debug)
  include(BareosTestCoverage)
endif()

option(ENABLE_SYSTEMTESTS "Enable the systemtest suite" ON)
option(CPM_ONLY "Add CPM packages and exit" OFF)
if(CPM_ONLY)
  include(BareosCpmPackages)
  return()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
  string(REPLACE "Program Files (x86)" "Program Files" CMAKE_INSTALL_PREFIX
                 "${CMAKE_INSTALL_PREFIX}"
  )
  set(TESTS_THAT_REQUIRE_SH system:filesets system:ignoreduplicatecheck)
  # these tests do not work on windows
  set(TESTS_TO_SKIP
      system:always-incremental-consolidate
      system:autoxflate:setup
      system:autoxflate:copy-to-local
      system:autoxflate:copy-to-offsite
      system:autoxflate:create-backup
      system:autoxflate:replication-local
      system:autoxflate:replication-offsite
      system:autoxflate:cleanup
      system:bareos-basic:hardlinks
      system:bareos-basic:test-make-catalog-backup
      system:bareos-concurrency
      system:catalog
      system:checkpoints:checkpoints-on-kill
      system:checkpoints:checkpoints-on-stop
      system:commandline-options
      system:dedupestimate:do-estimation
      system:dedupable
      system:fileset-multiple-blocks:include-blocks
      system:fileset-multiple-blocks:options-blocks
      system:heartbeat-interval:check
      system:messages
      system:py3plug-dir
      system:py3plug-fd-local-fileset-restoreobject
      system:py3plug-sd
      system:py3plug-fd-local-fileset-basic:basic
      system:py3plug-fd-local-fileset-basic:accurate
      system:py3plug-fd-local-fileset-basic:io_in_core
      system:py3plug-fd-basic:cancel-check
      system:py3plug-fd-basic:cancel-check-no-wait
      system:py3plug-fd-basic:config
      system:py3plug-fd-basic:forcebackup
      system:py3plug-fd-basic:jobmsg
      system:python-bareos:acl
      system:python-bareos:bvfs
      system:python-bareos:delete
      system:python-bareos:filedaemon
      system:python-bareos:json
      system:python-bareos:json_config
      system:python-bareos:list_command
      system:python-bareos:plaintext
      system:python-bareos:protocol_124
      system:python-bareos:python_bareos_module
      system:python-bareos:python_fd_plugins
      system:python-bareos:runscript
      system:python-bareos:show
      system:python-bareos:tlspsk
      system:python-bareos:update_all_volumes_all_pools
      system:scheduler:scheduler-backup
      system:sparse-file
      system:strippath:02-restore
      system:strippath:03-verify
  )
  list(APPEND TESTS_TO_SKIP ${TESTS_THAT_REQUIRE_SH})
else()
  set(TESTS_TO_SKIP)
endif()

add_custom_target(
  uninstall "${CMAKE_COMMAND}" -P
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake"
)
set(default_build_type "Debug")

include(CMakePrintHelpers)
find_package(Git)
include(BareosVersionFromGit)
include(BareosExtractVersionInfo)
include(BareosGetDistInfo)
include(RemoveNDebugFlag)
include(ForceOption)

set(BUILDNAME
    "${BAREOS_FULL_VERSION}"
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

option(BUILD_UNIVERSAL_CLIENT
       "Override build-options to produce a limited, but universal client" OFF
)
if(BUILD_UNIVERSAL_CLIENT)
  if(CMAKE_VERSION VERSION_LESS 3.18.0)
    message(
      FATAL_ERROR "Building the universal client requires at least CMake 3.18"
    )
  endif()
  force_option(FETCHCONTENT_FULLY_DISCONNECTED OFF)
  force_option(CPM_USE_LOCAL_PACKAGES OFF)
  force_option(ENABLE_PYTHON OFF)
  force_option(ENABLE_GFAPI_FD OFF)
  force_option(ENABLE_JANSSON OFF)
  force_option(ENABLE_LZO OFF)
  force_option(ENABLE_CAPABILITY OFF)
  force_option(ENABLE_BCONSOLE OFF)
  force_option(ENABLE_STATIC_RUNTIME_LIBS ON)
  set(client-only ON)
endif()

option(ENABLE_WEBUI "Build with WEBUI" ON)
option(docs-only "Build only the documentation" OFF)
option(docs-build-json "Update the configuration schema JSON files" OFF)

set(ENABLE_BARRI_INIT ON)
if(client-only OR "${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
  set(ENABLE_BARRI_INIT OFF)
endif()

option(ENABLE_BARRI "enable building of barri" "${ENABLE_BARRI_INIT}")

if(docs-build-json)
  # check for wrong combinations of build flags
  if(docs-only)
    message(
      FATAL_ERROR
        "\n\"docs-build-json\" needs bareos-executables and cannot be combined with \"docs-only\""
    )
  endif()
  if(NOT traymonitor)
    message(
      FATAL_ERROR
        "\n\"docs-build-json\" needs the traymonitor executable: add \"-Dtraymonitor=on\""
    )
  endif()
endif()

if(docs-only)
  set(BUILD_BAREOS_BINARIES no)
else()
  set(BUILD_BAREOS_BINARIES yes)
endif()

if(NOT hostname)
  site_name(hostname)
endif()

if(BUILD_BAREOS_BINARIES)
  project(bareos C CXX)
  if(NOT CMAKE_BUILD_TYPE
     AND NOT CMAKE_C_FLAGS
     AND NOT CMAKE_CXX_FLAGS
  )
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
  endif()
  include(BareosKnownCompilerFlags)
  include(BareosOutputDirectories)
  include(BareosCpmPackages)
  # include PrefixMap before BareosCcache so hash_dir is set correctly
  include(PrefixMap)
  include(BareosCcache)
  include(BareosSetVariableDefaults)
  add_subdirectory(core)
  if(ENABLE_WEBUI)
    add_subdirectory(webui)
  endif()
  if(NOT client-only)
    if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
      add_subdirectory(contrib)
    endif()
    if(ENABLE_SYSTEMTESTS)
      add_subdirectory(systemtests)
    endif()
  endif()
endif()

if(docs
   OR docs-only
   OR docs-build-json
)
  add_subdirectory(docs/manuals)
endif()
set(SITE ${BAREOS_PLATFORM}-${DISTVER})
