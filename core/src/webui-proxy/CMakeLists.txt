#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

cmake_minimum_required(VERSION 3.17)

find_package(Threads REQUIRED)
find_package(Jansson REQUIRED)

set(PROXY_SOURCES main.cc bareos_base64.cc director_connection.cc ws_codec.cc
                  proxy_session.cc proxy_server.cc
)

add_executable(bareos-webui-proxy ${PROXY_SOURCES})

target_compile_features(bareos-webui-proxy PRIVATE cxx_std_20)

target_compile_definitions(
  bareos-webui-proxy PRIVATE BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
)

target_include_directories(
  bareos-webui-proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
  bareos-webui-proxy PRIVATE OpenSSL::SSL OpenSSL::Crypto Jansson::Jansson
                             Threads::Threads CLI11::CLI11
)

install(
  TARGETS bareos-webui-proxy
  DESTINATION "${sbindir}"
  COMPONENT webui
)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
  include(GoogleTest)

  set(PROXY_LIB_SOURCES bareos_base64.cc ws_codec.cc)

  add_executable(
    test_webui_proxy tests/test_bareos_base64.cc tests/test_ws_codec.cc
                     ${PROXY_LIB_SOURCES}
  )
  target_compile_features(test_webui_proxy PRIVATE cxx_std_20)
  target_include_directories(
    test_webui_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_link_libraries(
    test_webui_proxy PRIVATE OpenSSL::SSL OpenSSL::Crypto GTest::gtest
                             GTest::gtest_main Threads::Threads
  )
  gtest_discover_tests(test_webui_proxy)
endif()
