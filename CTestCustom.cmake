
SET(CTEST_CUSTOM_ERROR_EXCEPTION
  ${CTEST_CUSTOM_ERROR_EXCEPTION}
  "ERROR: *database \".*\" already exists"
  "ERROR: *table \".*\" does not exist"
  "NOTICE: *table \".*\" does not exist, skipping"
  "NOTICE: .*will create implicit sequence"
  "NOTICE: .*will create implicit index"
  "ERROR: *role \".*\" already exists"
  "libtool: install: warning: relinking .*"
  )

SET(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 100000)
SET(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 1048576)
SET (CTEST_NIGHTLY_START_TIME "23:00:00 CET")

SET(CTEST_CUSTOM_PRE_TEST scripts/pretest)
