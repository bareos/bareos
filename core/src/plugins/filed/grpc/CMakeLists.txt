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

include(BareosDisableWarnings)
include(BareosConfigureFile)

if(ENABLE_GRPC)
  if(HAVE_WIN32)
    message(
      FATAL_ERROR "The grpc plugin is currently not compatible with windows"
    )
  endif()

  set(PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")
  make_directory(${PROTO_OUTPUT_DIR})

  # this is needed to make the output directory into a python module so that we
  # can load the files inside of it
  file(TOUCH "${PROTO_OUTPUT_DIR}/__init__.py")

  # Proto file
  function(target_add_proto_sources target)
    set(protosrcs)
    set(protohdrs)

    set(abs_files)

    foreach(file ${ARGN})
      get_filename_component(absolute "${file}" ABSOLUTE)
      get_filename_component(path "${absolute}" PATH)
      get_filename_component(stem "${file}" NAME_WE)

      set(proto_src "${PROTO_OUTPUT_DIR}/${stem}.pb.cc")
      set(proto_hdr "${PROTO_OUTPUT_DIR}/${stem}.pb.h")

      list(APPEND protosrcs "${proto_src}")
      list(APPEND protohdrs "${proto_hdr}")
      list(APPEND abs_files "${absolute}")
    endforeach()

    if(Protobuf_VERSION VERSION_LESS 3.15.0)
      set(_PROTOBUF_PROTOC_EXTRA_PARAMS
        "--experimental_allow_proto3_optional"
      )
    else()
      set(_PROTOBUF_PROTOC_EXTRA_PARAMS "")
    endif()

    add_custom_command(
      OUTPUT ${protosrcs} ${protohdrs}
      COMMAND
      $<TARGET_FILE:protobuf::protoc> ARGS --cpp_out=lite:"${PROTO_OUTPUT_DIR}"
      -I "${Protobuf_INCLUDE_DIRS}"
      -I "${path}" --python_out "${PROTO_OUTPUT_DIR}"
      ${_PROTOBUF_PROTOC_EXTRA_PARAMS} ${abs_files}
      DEPENDS ${abs_files} protobuf::protoc
      COMMENT "Compiling proto files"
    )

    message(DEBUG "Generated proto files:")
    message(DEBUG " proto: ${protosrcs} / ${protohdrs}")

    target_sources(${target} PRIVATE ${protosrcs})
    target_include_directories(${target} PUBLIC ${PROTO_OUTPUT_DIR})
  endfunction()

  add_library(grpc-proto OBJECT)
  target_add_proto_sources(
    grpc-proto "proto/plugin.proto" "proto/events.proto" "proto/bareos.proto"
    "proto/common.proto"
  )
  set_target_properties(
    grpc-proto PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN
                                                       ON
  )
  target_link_libraries(grpc-proto PUBLIC protobuf::libprotobuf)

  add_library(grpc-util OBJECT)
  target_sources(grpc-util PRIVATE util.cc)
  target_link_libraries(grpc-util grpc-proto)

  bareos_add_plugin(grpc-fd)

  include(GenerateExportHeader)
  generate_export_header(grpc-fd)

  target_sources(
    grpc-fd
    PRIVATE grpc.cc grpc_impl.cc bareos_api.cc
    PUBLIC FILE_SET generated_headers TYPE HEADERS BASE_DIRS
           $<TARGET_PROPERTY:grpc-fd,BINARY_DIR> FILES
           ${CMAKE_CURRENT_BINARY_DIR}/grpc-fd_export.h
  )
  target_link_libraries(grpc-fd Bareos::Lib fmt::fmt grpc-proto grpc-util)
  set_target_properties(
    grpc-fd PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN
                                                    ON
  )

  install(
    TARGETS grpc-fd
    DESTINATION ${plugindir}
    COMPONENT filedaemon
  )

  # add_executable( grpc-test-module test_module/test_module.cc
  # test_module/plugin_service.cc ) target_link_libraries(grpc-test-module
  # fmt::fmt grpc-proto) install( TARGETS grpc-test-module DESTINATION
  # ${plugindir}/grpc COMPONENT filedaemon )

  add_executable(
    bareos-grpc-fd-plugin-bridge bridge_module/bridge_module.cc
                                 bridge_module/plugin_service.cc
  )
  target_link_libraries(
    # we need to link to python3, because otherwise we will not be able to load
    # python-fd.so
    bareos-grpc-fd-plugin-bridge fmt::fmt grpc-proto Python3::Module
    ${CMAKE_DL_LIBS} grpc-util
  )
  set_target_properties(
    bareos-grpc-fd-plugin-bridge PROPERTIES CXX_VISIBILITY_PRESET hidden
                                            VISIBILITY_INLINES_HIDDEN ON
  )
  install(
    TARGETS bareos-grpc-fd-plugin-bridge
    DESTINATION ${plugindir}/grpc
    COMPONENT filedaemon
  )

  include(DebugEdit)

  bareos_add_test(
    protobuf_socket_test LINK_LIBRARIES GTest::gtest_main grpc-proto
  )

  bareos_configure_file(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/bareosfd.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/bareosfd_types.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/bareosfd_type_conv.py" COPY
  )
endif()
