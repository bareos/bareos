#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.

cmake_minimum_required(VERSION 3.17)

find_package(Threads REQUIRED)

add_executable(
  bareos-config
  main.cc
  http_server.cc
  config_service.cc
  config_model.cc
)

target_compile_features(bareos-config PRIVATE cxx_std_20)
target_compile_definitions(
  bareos-config PRIVATE BAREOS_FULL_VERSION="${BAREOS_FULL_VERSION}"
)
target_include_directories(bareos-config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
  bareos-config
  PRIVATE Threads::Threads
          CLI11::CLI11
          Bareos::Lib
          Bareos::Dir
          Bareos::FD
          Bareos::SD
          Bareos::LibSD
          Bareos::Console
)

install(
  TARGETS bareos-config
  DESTINATION "${sbindir}"
  COMPONENT config
)

if(BUILD_TESTING AND GTest_FOUND)
  include(GoogleTest)

  add_executable(test_bareos_config_model tests/test_config_model.cc
                                         config_model.cc)
  target_compile_features(test_bareos_config_model PRIVATE cxx_std_20)
  target_include_directories(
    test_bareos_config_model PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(
    test_bareos_config_model
    PRIVATE GTest::gtest_main
            Bareos::Lib
            Bareos::Dir
            Bareos::FD
            Bareos::SD
            Bareos::LibSD
            Bareos::Console
  )
  gtest_discover_tests(test_bareos_config_model)

  add_executable(test_bareos_config_service tests/test_config_service.cc
                                            config_service.cc config_model.cc)
  target_compile_features(test_bareos_config_service PRIVATE cxx_std_20)
  target_include_directories(
    test_bareos_config_service PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(
    test_bareos_config_service
    PRIVATE GTest::gtest_main
            Bareos::Lib
            Bareos::Dir
            Bareos::FD
            Bareos::SD
            Bareos::LibSD
            Bareos::Console
  )
  gtest_discover_tests(test_bareos_config_service)
endif()
