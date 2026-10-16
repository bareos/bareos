# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

add_library(bareos-websocket STATIC ws_codec.cc)
add_library(Bareos::WebSocket ALIAS bareos-websocket)
target_compile_features(bareos-websocket PUBLIC cxx_std_20)
target_include_directories(
  bareos-websocket
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE "${OPENSSL_INCLUDE_DIR}"
)
target_link_libraries(bareos-websocket PUBLIC Bareos::HttpProtocol)

if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)

  add_executable(test_websocket tests/test_ws_codec.cc)
  target_link_libraries(
    test_websocket PRIVATE Bareos::WebSocket OpenSSL::Crypto GTest::gtest_main
  )
  gtest_discover_tests(test_websocket)
endif()
