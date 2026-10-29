# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

add_library(bareos-http-protocol STATIC http_protocol.cc)
add_library(Bareos::HttpProtocol ALIAS bareos-http-protocol)
target_compile_features(bareos-http-protocol PUBLIC cxx_std_20)
target_include_directories(
  bareos-http-protocol PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_library(bareos-http-server STATIC http_server.cc)
add_library(Bareos::HttpServer ALIAS bareos-http-server)
target_compile_features(bareos-http-server PUBLIC cxx_std_20)
target_include_directories(
  bareos-http-server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
