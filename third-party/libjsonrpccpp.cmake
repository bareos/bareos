#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

include(ExternalProject)

set(RPC_BUILD_DIR ${CMAKE_BINARY_DIR}/third-party/libjson-rpc-cpp)
set(RPC_INSTALL_DIR ${RPC_BUILD_DIR}/install)
set(RPC_LIB_DIR ${RPC_INSTALL_DIR}/lib)

ExternalProject_Add(libjson-rpc-cpp-download
  SOURCE_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/libjson-rpc-cpp
  TMP_DIR       ${RPC_BUILD_DIR}/tmp
  STAMP_DIR     ${RPC_BUILD_DIR}/stamp
  BINARY_DIR    ${RPC_BUILD_DIR}/build
  INSTALL_DIR   ${RPC_INSTALL_DIR}
  TEST_EXCLUDE_FROM_MAIN TRUE
  STEP_TARGETS   build
  CMAKE_ARGS -DCOMPILE_STUBGEN=ON
    -DCOMPILE_EXAMPLES=OFF
    -DCOMPILE_TESTS=OFF
    -DHTTP_SERVER=OFF
    -DHTTP_CLIENT=OFF
    -DREDIS_SERVER=OFF
    -DREDIS_CLIENT=OFF
    -DUNIX_DOMAIN_SOCKET_SERVER=OFF
    -DUNIX_DOMAIN_SOCKET_CLIENT=OFF
    -DFILE_DESCRIPTOR_SERVER=OFF
    -DFILE_DESCRIPTOR_CLIENT=OFF
    -DTCP_SOCKET_SERVER=OFF
    -DTCP_SOCKET_CLIENT=OFF
    -DCMAKE_INSTALL_PREFIX=${RPC_INSTALL_DIR}
    -DCMAKE_INSTALL_LIBDIR=${RPC_LIB_DIR}
)

# Create imported target libjson-rpc-cpp::jsonrpccommon
add_library(libjson-rpc-cpp::jsonrpccommon SHARED IMPORTED)
set_target_properties(libjson-rpc-cpp::jsonrpccommon PROPERTIES
  IMPORTED_LOCATION "${RPC_LIB_DIR}/libjsonrpccpp-common.so"
)
set_target_properties(libjson-rpc-cpp::jsonrpccommon PROPERTIES
  INTERFACE_LINK_LIBRARIES "libjsoncpp.so"
)
target_include_directories(libjson-rpc-cpp::jsonrpccommon
  SYSTEM INTERFACE ${JSONCPP_INCLUDE_DIR})

# Create imported target libjson-rpc-cpp::jsonrpcserver
add_library(libjson-rpc-cpp::jsonrpcserver SHARED IMPORTED GLOBAL)
set_target_properties(libjson-rpc-cpp::jsonrpcserver PROPERTIES
  IMPORTED_LOCATION "${RPC_LIB_DIR}/libjsonrpccpp-server.so"
)
set_target_properties(libjson-rpc-cpp::jsonrpcserver PROPERTIES
  INTERFACE_LINK_LIBRARIES "libjson-rpc-cpp::jsonrpccommon"
)

# Create imported target libjson-rpc-cpp::jsonrpcclient
add_library(libjson-rpc-cpp::jsonrpcclient SHARED IMPORTED GLOBAL)
set_target_properties(libjson-rpc-cpp::jsonrpcclient PROPERTIES
  IMPORTED_LOCATION "${RPC_LIB_DIR}/libjsonrpccpp-client.so"
)
set_target_properties(libjson-rpc-cpp::jsonrpcserver PROPERTIES
  INTERFACE_LINK_LIBRARIES "libjson-rpc-cpp::jsonrpccommon"
)

file(MAKE_DIRECTORY ${RPC_INSTALL_DIR}/include)
target_include_directories(libjson-rpc-cpp::jsonrpcserver SYSTEM INTERFACE ${RPC_INSTALL_DIR}/include)
