#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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
cmake_minimum_required(VERSION 3.0)
project(bareosfastlz LANGUAGES C)

add_library(bareosfastlz SHARED src/fastlzlib.c src/fastlz.c src/lz4.c src/lz4hc.c)
target_include_directories(bareosfastlz PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)
target_compile_definitions(bareosfastlz PRIVATE "-DDLL_EXPORT")
install(TARGETS bareosfastlz DESTINATION ${libdir})
set_target_properties(bareosfastlz PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                                              SOVERSION "${SOVERSION}"
                                              )
