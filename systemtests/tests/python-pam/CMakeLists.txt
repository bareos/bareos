get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
# python-bareos does not work on installed files and is used here
if(PYTHON_EXECUTABLE
   AND ENABLE_BCONSOLE_PAM_TEST
   AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
