get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
if(AUTOCHANGER_TEST_ENABLED)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  # Make sure these tests run sequentially.
  set_tests_properties(
    ${SYSTEMTEST_PREFIX}autochanger PROPERTIES RESOURCE_LOCK
                                               autochanger_resource
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
