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

# ---------------------------------------------------------------------------
# Vue / npm build
# ---------------------------------------------------------------------------
set(SETUP_VUE_SRC "${CMAKE_SOURCE_DIR}/bareos-setup-vue")
set(SETUP_VUE_DIST "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(SETUP_VUE_SOURCE_DIST "${SETUP_VUE_SRC}/dist")
set(SETUP_VUE_DIST_INDEX "${SETUP_VUE_DIST}/index.html")
set(SETUP_VUE_PACKAGE_JSON "${SETUP_VUE_SRC}/package.json")
set(SETUP_VUE_PACKAGE_LOCK "${SETUP_VUE_SRC}/package-lock.json")
set(SETUP_VUE_BUILD_SCRIPT "${SETUP_VUE_SRC}/build-dist.cmake")
set(SETUP_VUE_BUILD_STAMP
    "${CMAKE_CURRENT_BINARY_DIR}/bareos-setup-vue-build.stamp"
)

find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE AND NOT EXISTS "${SETUP_VUE_SOURCE_DIST}/index.html")
  message(
    STATUS
      "Skipping bareos-setup: npm not found and no prepared bareos-setup-vue/dist exists"
  )
  return()
endif()

file(GLOB_RECURSE SETUP_VUE_BUILD_INPUTS CONFIGURE_DEPENDS
     "${SETUP_VUE_SRC}/src/*" "${SETUP_VUE_SRC}/public/*"
)
list(APPEND SETUP_VUE_BUILD_INPUTS "${SETUP_VUE_SRC}/index.html"
     "${SETUP_VUE_SRC}/vite.config.js" "${SETUP_VUE_BUILD_SCRIPT}"
     "${SETUP_VUE_PACKAGE_JSON}" "${SETUP_VUE_PACKAGE_LOCK}"
)

add_custom_command(
  OUTPUT "${SETUP_VUE_BUILD_STAMP}" "${SETUP_VUE_DIST_INDEX}"
  COMMAND ${CMAKE_COMMAND} -DDIST_DIR=${SETUP_VUE_DIST} -P
          "${SETUP_VUE_BUILD_SCRIPT}"
  COMMAND ${CMAKE_COMMAND} -E touch "${SETUP_VUE_BUILD_STAMP}"
  DEPENDS ${SETUP_VUE_BUILD_INPUTS}
  COMMENT "Building bareos-setup-vue dist bundle"
  USES_TERMINAL VERBATIM
)
add_custom_target(bareos-setup-vue-build DEPENDS "${SETUP_VUE_BUILD_STAMP}")

# ---------------------------------------------------------------------------
# Asset embedding
# ---------------------------------------------------------------------------
set(EMBEDDED_ASSETS_CC "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cc")

add_custom_command(
  OUTPUT "${EMBEDDED_ASSETS_CC}"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/embed_assets.py"
          "${SETUP_VUE_DIST}" "${EMBEDDED_ASSETS_CC}"
  DEPENDS "${SETUP_VUE_BUILD_STAMP}"
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
    setup_steps.cc
    command_runner.cc
    os_detector.cc
    tui_wizard.cc
    "${EMBEDDED_ASSETS_CC}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ws_codec.cc"
)

add_executable(bareos-setup ${SETUP_SOURCES})

# Make sure assets are embedded before compiling
add_dependencies(bareos-setup bareos-setup-vue-build)

target_compile_features(bareos-setup PRIVATE cxx_std_20)
target_compile_options(
  bareos-setup PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
)

target_compile_definitions(
  bareos-setup PRIVATE BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
)

target_include_directories(
  bareos-setup
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
)

# ---------------------------------------------------------------------------
# Portable single-binary build
#
# bareos-setup uses no NSS-dependent libc functions (no getpwnam, gethostbyname,
# getaddrinfo, ...) and performs all actual HTTPS traffic by shelling out to the
# system "curl" instead of using OpenSSL for TLS in-process. This makes it safe
# to link it fully statically, producing a single binary with no runtime library
# dependencies at all, capable of running unmodified on any supported distro
# regardless of its glibc/ OpenSSL/Jansson version. This is intended to be built
# inside the musl/ Alpine container in devtools/bareos-setup-static-build/ (see
# devtools/build-bareos-setup-static.sh), not as part of normal distro
# packaging, which should keep linking dynamically against the distro's own
# libraries.
option(
  BAREOS_SETUP_STATIC
  "Link bareos-setup fully statically for a portable, distro-independent single binary"
  OFF
)
if(BAREOS_SETUP_STATIC)
  # -static requires actual .a archives: the OpenSSL::SSL/OpenSSL::Crypto/
  # Jansson::Jansson imported targets normally resolve to shared objects, so
  # look up their static archives explicitly instead of linking those imported
  # targets. A static executable also can't be position-independent (-pie), so
  # PIE is turned off for this target too.
  set(_bareos_setup_saved_lib_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  find_library(BAREOS_SETUP_STATIC_SSL_LIBRARY NAMES ssl)
  find_library(BAREOS_SETUP_STATIC_CRYPTO_LIBRARY NAMES crypto)
  find_library(BAREOS_SETUP_STATIC_JANSSON_LIBRARY NAMES jansson)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_bareos_setup_saved_lib_suffixes})
  if(NOT BAREOS_SETUP_STATIC_SSL_LIBRARY
     OR NOT BAREOS_SETUP_STATIC_CRYPTO_LIBRARY
     OR NOT BAREOS_SETUP_STATIC_JANSSON_LIBRARY
  )
    message(
      FATAL_ERROR
        "BAREOS_SETUP_STATIC requires static .a archives for OpenSSL and Jansson"
    )
  endif()
  target_link_options(bareos-setup PRIVATE -static -no-pie)
  set_target_properties(bareos-setup PROPERTIES POSITION_INDEPENDENT_CODE OFF)

  # Strip debug info at link time and drop unused code/data (each
  # function/variable in its own section, so the linker's --gc-sections can
  # discard ones that are never referenced). Debug symbols alone account for
  # roughly 3/4 of the unstripped binary's size, and this static build is meant
  # to be a distributed, portable artifact rather than a debug build.
  target_compile_options(
    bareos-setup PRIVATE -ffunction-sections -fdata-sections
  )
  target_link_options(bareos-setup PRIVATE -s -Wl,--gc-sections)
endif()

if(BAREOS_SETUP_STATIC)
  target_link_libraries(
    bareos-setup
    PRIVATE ${BAREOS_SETUP_STATIC_SSL_LIBRARY}
            ${BAREOS_SETUP_STATIC_CRYPTO_LIBRARY}
            ${BAREOS_SETUP_STATIC_JANSSON_LIBRARY}
            Threads::Threads
            CLI11::CLI11
  )
else()
  target_link_libraries(
    bareos-setup PRIVATE OpenSSL::SSL OpenSSL::Crypto Jansson::Jansson
                         Threads::Threads CLI11::CLI11
  )
endif()

install(
  TARGETS bareos-setup
  DESTINATION "${sbindir}"
  COMPONENT setup
)

if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)

  add_executable(
    test_bareos_setup_steps
    tests/test_setup_steps_shared.cc setup_steps.cc command_runner.cc
    os_detector.cc setup_session.cc http_server.cc
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ws_codec.cc" "${EMBEDDED_ASSETS_CC}"
  )
  add_dependencies(test_bareos_setup_steps bareos-setup-vue-build)
  target_compile_features(test_bareos_setup_steps PRIVATE cxx_std_20)
  target_compile_options(
    test_bareos_setup_steps
    PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
  )
  target_compile_definitions(
    test_bareos_setup_steps
    PRIVATE BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
  )
  target_include_directories(
    test_bareos_setup_steps
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
  )
  target_link_libraries(
    test_bareos_setup_steps
    PRIVATE GTest::gtest_main Threads::Threads OpenSSL::SSL OpenSSL::Crypto
            Jansson::Jansson
  )
  gtest_discover_tests(test_bareos_setup_steps)
endif()
