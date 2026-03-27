#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2025 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

cmake_minimum_required(VERSION 3.17)

find_package(Jansson REQUIRED)

# ---------------------------------------------------------------------------
# Vue / npm build
# ---------------------------------------------------------------------------
set(SETUP_VUE_SRC "${CMAKE_SOURCE_DIR}/bareos-setup-vue")
set(SETUP_VUE_DIST "${SETUP_VUE_SRC}/dist")

# Collect Vue source files so CMake knows when to retrigger the build
file(GLOB_RECURSE SETUP_VUE_SOURCES
    "${SETUP_VUE_SRC}/src/*.vue"
    "${SETUP_VUE_SRC}/src/*.js"
    "${SETUP_VUE_SRC}/src/*.scss"
    "${SETUP_VUE_SRC}/index.html"
    "${SETUP_VUE_SRC}/package.json"
    "${SETUP_VUE_SRC}/vite.config.js"
)

# Stamp file tracks when dist/ was last rebuilt
set(SETUP_VUE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/bareos-setup-vue.stamp")

add_custom_command(
    OUTPUT "${SETUP_VUE_STAMP}"
    COMMAND npm ci --silent
    COMMAND npm run build
    COMMAND ${CMAKE_COMMAND} -E touch "${SETUP_VUE_STAMP}"
    WORKING_DIRECTORY "${SETUP_VUE_SRC}"
    DEPENDS ${SETUP_VUE_SOURCES}
    COMMENT "Building bareos-setup-vue frontend"
    VERBATIM
)

add_custom_target(bareos-setup-vue-build
    DEPENDS "${SETUP_VUE_STAMP}"
)

# ---------------------------------------------------------------------------
# Asset embedding
# ---------------------------------------------------------------------------
set(EMBEDDED_ASSETS_CC "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cc")

add_custom_command(
    OUTPUT "${EMBEDDED_ASSETS_CC}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/embed_assets.py"
            "${SETUP_VUE_DIST}"
            "${EMBEDDED_ASSETS_CC}"
    DEPENDS "${SETUP_VUE_STAMP}"
            "${CMAKE_CURRENT_SOURCE_DIR}/embed_assets.py"
    COMMENT "Embedding bareos-setup-vue assets"
    VERBATIM
)

# ---------------------------------------------------------------------------
# bareos-setup executable
# ---------------------------------------------------------------------------
set(SETUP_SOURCES
    main.cc
    http_server.cc
    setup_session.cc
    command_runner.cc
    os_detector.cc
    "${EMBEDDED_ASSETS_CC}"
    # Reuse WsCodec + base64 from bareos-webui-proxy
    "${CMAKE_CURRENT_SOURCE_DIR}/../webui-proxy/ws_codec.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../webui-proxy/bareos_base64.cc"
)

add_executable(bareos-setup ${SETUP_SOURCES})

# Make sure assets are embedded before compiling
add_dependencies(bareos-setup bareos-setup-vue-build)

target_compile_features(bareos-setup PRIVATE cxx_std_20)

target_compile_definitions(
    bareos-setup PRIVATE
    BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
)

target_include_directories(bareos-setup PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    # For ws_codec.h and bareos_base64.h
    "${CMAKE_CURRENT_SOURCE_DIR}/../webui-proxy"
)

target_link_libraries(
    bareos-setup
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Jansson::Jansson
        Threads::Threads
        CLI11::CLI11
)

install(
    TARGETS bareos-setup
    DESTINATION ${CMAKE_INSTALL_SBINDIR}
    COMPONENT setup
)
