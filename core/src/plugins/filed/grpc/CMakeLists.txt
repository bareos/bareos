#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2025 Bareos GmbH & Co. KG
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

include(BareosCompilerFlags)
include(BareosConfigureFile)

if(ENABLE_GRPC)
  if(HAVE_WIN32)
    message(
      FATAL_ERROR "The grpc plugin is currently not compatible with windows"
    )
  endif()

  set(PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")
  make_directory(${PROTO_OUTPUT_DIR})
  # Proto file
  function(target_add_proto_sources target)
    set(protosrcs)
    set(protohdrs)
    set(grpcsrcs)
    set(grpchdrs)

    foreach(file ${ARGN})
      get_filename_component(absolute "${file}" ABSOLUTE)
      get_filename_component(path "${absolute}" PATH)
      get_filename_component(stem "${file}" NAME_WE)

      set(proto_src "${PROTO_OUTPUT_DIR}/${stem}.pb.cc")
      set(proto_hdr "${PROTO_OUTPUT_DIR}/${stem}.pb.h")
      set(grpc_src "${PROTO_OUTPUT_DIR}/${stem}.grpc.pb.cc")
      set(grpc_hdr "${PROTO_OUTPUT_DIR}/${stem}.grpc.pb.h")

      if(Protobuf_VERSION VERSION_LESS 3.15.0)
        set(_PROTOBUF_PROTOC_EXTRA_PARAMS
            "--experimental_allow_proto3_optional"
        )
      else()
        set(_PROTOBUF_PROTOC_EXTRA_PARAMS "")
      endif()
      add_custom_command(
        OUTPUT "${proto_src}" "${proto_hdr}" "${grpc_src}" "${grpc_hdr}"
        COMMAND
          $<TARGET_FILE:protobuf::protoc> ARGS --grpc_out "${PROTO_OUTPUT_DIR}"
          --cpp_out "${PROTO_OUTPUT_DIR}" -I "${path}"
          --plugin=protoc-gen-grpc="${gRPC_CPP_PROTO_PLUGIN}"
          ${_PROTOBUF_PROTOC_EXTRA_PARAMS} "${absolute}"
        DEPENDS "${absolute}" protobuf::protoc
        COMMENT "Compiling proto file ${absolute}"
      )

      list(APPEND protosrcs "${proto_src}")
      list(APPEND protohdrs "${proto_hdr}")
      list(APPEND grpcsrcs "${grpc_src}")
      list(APPEND grpchdrs "${grpc_hdr}")
    endforeach()

    message(DEBUG "Generated proto files:")
    message(DEBUG " proto: ${protosrcs} / ${protohdrs}")
    message(DEBUG " grpc: ${grpcsrcs} / ${grpchdrs}")

    target_sources(${target} PRIVATE ${protosrcs} ${grpcsrcs})
    target_include_directories(${target} PUBLIC ${PROTO_OUTPUT_DIR})
  endfunction()

  add_library(grpc-proto OBJECT)
  target_add_proto_sources(
    grpc-proto "proto/plugin.proto" "proto/events.proto" "proto/bareos.proto"
    "proto/common.proto"
  )
  target_link_libraries(
    grpc-proto PUBLIC gRPC::grpc++_reflection gRPC::grpc++
                      protobuf::libprotobuf
  )

  bareos_add_plugin(grpc-fd)
  target_sources(grpc-fd PRIVATE grpc.cc grpc_impl.cc bareos_api.cc)
  target_link_libraries(grpc-fd Bareos::Lib fmt::fmt grpc-proto)
  install(
    TARGETS grpc-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )

  add_executable(
    grpc-test-module test_module/test_module.cc test_module/plugin_service.cc
  )
  target_link_libraries(grpc-test-module fmt::fmt grpc-proto)
  install(
    TARGETS grpc-test-module
    DESTINATION ${plugindir}/grpc
    COMPONENT filedaemon
  )

  add_executable(
    bareos-grpc-fd-plugin-bridge bridge_module/bridge_module.cc
                                 bridge_module/plugin_service.cc
  )
  target_link_libraries(
    # we need to link to python3, because otherwise we will not be able to load
    # python-fd.so
    bareos-grpc-fd-plugin-bridge fmt::fmt grpc-proto Python3::Module
    ${CMAKE_DL_LIBS}
  )
  install(
    TARGETS bareos-grpc-fd-plugin-bridge
    DESTINATION ${plugindir}/grpc
    COMPONENT filedaemon
  )

  include(DebugEdit)
endif()
