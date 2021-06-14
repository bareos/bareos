get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
find_package(GTest 1.8 CONFIG)
if(GTest_FOUND AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
    "gtest not found or RUN_SYSTEMTESTS_ON_INSTALLED_FILES"
  )
endif()
