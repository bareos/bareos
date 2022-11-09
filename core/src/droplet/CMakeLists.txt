#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2022 Bareos GmbH & Co. KG
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

message(STATUS "Try to configure LibDroplet")
list(APPEND CMAKE_MESSAGE_INDENT "   ")

project(droplet C)

set(default_build_type "Debug")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FindLibXml2)
include(Findjsonc)
include(FindOpenSSL)

if(${JSONC_FOUND}
   AND ${OPENSSL_FOUND}
   AND ${LIBXML2_FOUND}
)
  add_subdirectory(libdroplet)
  message(STATUS "LibDroplet ready to be built")
else()
  message(STATUS "LibDroplet not configured to be built")
endif(
  ${JSONC_FOUND}
  AND ${OPENSSL_FOUND}
  AND ${LIBXML2_FOUND}
)

list(REMOVE_AT CMAKE_MESSAGE_INDENT -1)
