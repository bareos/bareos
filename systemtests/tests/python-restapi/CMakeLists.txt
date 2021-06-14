get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
check_pymodule_available(3 fastapi)
check_pymodule_available(3 uvicorn)
find_program(CURL curl)

# python-restapi-test does not work on installed files and needs some modules
if(PYTHON_EXECUTABLE
   AND CURL
   AND PYMODULE_3_FASTAPI_FOUND
   AND PYMODULE_3_UVICORN_FOUND
   AND NOT RUN_SYSTEMTESTS_ON_INSTALLED_FILES
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  set_tests_properties(
    ${SYSTEMTEST_PREFIX}${BASENAME} PROPERTIES LABELS "broken"
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
