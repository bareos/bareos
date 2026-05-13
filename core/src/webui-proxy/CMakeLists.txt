#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

cmake_minimum_required(VERSION 3.17)

find_package(Threads REQUIRED)
find_package(Jansson REQUIRED)

set(PROXY_SOURCES
    main.cc bareos_base64.cc director_connection.cc proxy_config.cc
    proxy_log.cc proxy_session.cc proxy_server.cc ws_codec.cc
)

add_executable(bareos-webui-proxy ${PROXY_SOURCES})

target_compile_definitions(
  bareos-webui-proxy
  PRIVATE BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
          WEBUI_PROXY_DEFAULT_CONFIG_PATH="${confdir}/bareos-webui-proxy.ini"
)

target_link_libraries(
  bareos-webui-proxy PRIVATE Bareos::Lib OpenSSL::SSL OpenSSL::Crypto
                             Jansson::Jansson Threads::Threads CLI11::CLI11
)

configure_file(
  bareos-webui-proxy.ini.in ${CMAKE_CURRENT_BINARY_DIR}/bareos-webui-proxy.ini
  @ONLY
)

install(
  TARGETS bareos-webui-proxy
  DESTINATION "${sbindir}"
  COMPONENT webui
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/bareos-webui-proxy.ini
  DESTINATION "${confdir}"
  COMPONENT webui
)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)

  set(PROXY_LIB_SOURCES bareos_base64.cc director_connection.cc proxy_config.cc
                        proxy_log.cc
  )

  add_executable(
    test_webui_proxy
    tests/test_bareos_base64.cc
    tests/test_director_connection.cc
    tests/test_proxy_config.cc
    tests/test_proxy_log.cc
    tests/test_proxy_session.cc
    tests/test_ws_codec.cc
    proxy_session.cc
    ws_codec.cc
    ${PROXY_LIB_SOURCES}
  )
  target_compile_definitions(
    test_webui_proxy PRIVATE BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
  )
  target_link_libraries(
    test_webui_proxy
    PRIVATE Bareos::Lib OpenSSL::SSL OpenSSL::Crypto Jansson::Jansson
            GTest::gtest GTest::gtest_main Threads::Threads
  )
  gtest_discover_tests(test_webui_proxy)
endif()
