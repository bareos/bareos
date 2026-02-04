#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2026 Bareos GmbH & Co. KG
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
include(BareosPlugins)

if(Python3_FOUND)
  bareos_add_plugin(python3-fd)
  target_sources(python3-fd PRIVATE python-fd.cc)
  install(
    TARGETS python3-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )
  target_link_libraries(python3-fd ${Python3_LIBRARIES} Bareos::Lib)
  target_include_directories(python3-fd PUBLIC ${Python3_INCLUDE_DIRS})
  target_compile_definitions(
    python3-fd PUBLIC PY3MODDIR=\"${Python3_SITEARCH}\"
  )
  add_dependencies(bareos-fd python3-fd)
  add_dependencies(python3-fd bareosfd-python3-module)
endif()

if(Python3_FOUND AND NOT HAVE_WIN32)
  add_executable(bareosfd-python3-module-tester test/python-fd-module-tester.cc)
  target_link_libraries(
    bareosfd-python3-module-tester ${Python3_LIBRARIES} bareos
  )
  target_include_directories(
    bareosfd-python3-module-tester PUBLIC ${Python3_INCLUDE_DIRS}
  )

  add_test(NAME bareosfd-python3-module-tester
           COMMAND $<TARGET_FILE:bareosfd-python3-module-tester>
  )
  set_property(
    TEST bareosfd-python3-module-tester
    PROPERTY
      ENVIRONMENT
      PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/test/:${CMAKE_CURRENT_BINARY_DIR}/python3modules/${CMAKE_CONFIG_TYPE}:${CMAKE_CURRENT_SOURCE_DIR}/pyfiles
      "LSAN_OPTIONS=fast_unwind_on_malloc=0 suppressions=${CMAKE_CURRENT_SOURCE_DIR}/test/lsan-suppressions.txt"
  )
endif()

if(Python3_FOUND)
  add_library(bareosfd-python3-module MODULE module/bareosfd.cc)
  install(
    TARGETS bareosfd-python3-module
    DESTINATION ${Python3_SITEARCH}
    COMPONENT filedaemon
  )
  set_target_properties(
    bareosfd-python3-module
    PROPERTIES PREFIX ""
               C_STANDARD 11
               SUFFIX ${Python3_EXT_SUFFIX}
               LIBRARY_OUTPUT_DIRECTORY python3modules
               LIBRARY_OUTPUT_NAME bareosfd
               LINK_FLAGS "${Python3_BLDSHARED_FLAGS}"
               LINKER_LANGUAGE CXX
               INCLUDE_DIRECTORIES "${pymod3_include_dirs}"
               CXX_STANDARD 20
  )
  if(NOT MSVC)
    target_compile_options(bareosfd-python3-module PUBLIC -fno-strict-aliasing)
  endif()
  if(NOT "${Python3_CCSHARED}" STREQUAL "")
    set_property(
      TARGET bareosfd-python3-module PROPERTY COMPILE_OPTIONS
                                              ${Python3_CCSHARED}
    )
  endif()
  bareos_disable_warnings(
    TARGET bareosfd-python3-module WARNINGS -Wc99-designator
  )
  target_link_libraries(
    bareosfd-python3-module Bareos::Lib ${Python3_LIBRARIES}
  )

  if(NOT MSVC) # TODO: fix on MSVC
    add_test(
      NAME bareosfd-python3-module
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_CURRENT_SOURCE_DIR}/test/bareosfd_test.py "--verbose"
    )

    set_property(
      TEST bareosfd-python3-module
      PROPERTY
        ENVIRONMENT
        "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/python3modules:${CMAKE_CURRENT_SOURCE_DIR}/test"
        LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/../../../lib
        "ASAN_OPTIONS=detect_leaks=0 verify_asan_link_order=0"
    )
    if(CMAKE_SYSTEM_NAME MATCHES FreeBSD)
      # the testscript randomly segfaults on FreeBSD during the CI-build, so we
      # disable it for now.
      set_property(TEST bareosfd-python3-module PROPERTY DISABLED true)
    endif()
    add_test(
      NAME python-simple-test-example
      COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/test/simple-test-example.py
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    )
    set_property(
      # add current directory for documentation only
      TEST python-simple-test-example PROPERTY ENVIRONMENT PYTHONPATH=./
    )
    set_property(TEST python-simple-test-example PROPERTY DISABLED true)
  endif()
endif()

bareos_configure_file(GLOB_RECURSE "ldap/python-ldap-conf.d/*" COPY)

set(PYFILES
    pyfiles/BareosFdPluginBaseclass.py
    pyfiles/BareosFdWrapper.py
    pyfiles/BareosFdPluginLocalFilesBaseclass.py
    pyfiles/bareos-fd-local-fileset.py
    ldap/bareos-fd-ldap.py
    mariabackup/bareos-fd-mariabackup.py
    percona-xtrabackup/bareos-fd-percona-xtrabackup.py
    postgresql/bareos-fd-postgresql.py
    vmware/bareos-fd-vmware.py
    libcloud/BareosLibcloudApi.py
    libcloud/bareos-fd-libcloud.py
)

include(CMakeDependentOption)
cmake_dependent_option(
  ENABLE_PROXMOX_PLUGIN
  "Enable the filedaemon plugin for Proxmox VE"
  ON
  "ENABLE_PYTHON;BAREOS_PLATFORM STREQUAL debian;DIST_VERSION_ID GREATER_EQUAL 12"
  OFF
)

if(ENABLE_PROXMOX_PLUGIN)
  list(APPEND PYFILES proxmox/bareos-fd-proxmox.py)
endif()

install(
  FILES ${PYFILES}
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)

install(
  DIRECTORY libcloud/bareos_libcloud_api
  DESTINATION ${plugindir}
  COMPONENT filedaemon
)

include(DebugEdit)
