SET(CTEST_CUSTOM_ERROR_EXCEPTION
  ${CTEST_CUSTOM_ERROR_EXCEPTION}
  "ERROR: *role \".*\" already exists"
  "ERROR: *database \".*\" already exists"
  "ERROR: *table \".*\" does not exist"
  "NOTICE: *table \".*\" does not exist, skipping"
  "NOTICE: .*will create implicit sequence"
  "NOTICE: .*will create implicit index"
  )

SET(CTEST_CUSTOM_WARNING_EXCEPTION
  ${CTEST_CUSTOM_WARNING_EXCEPTION}
  "libtool: install: warning: relinking .*"
  "libtool: link: warning: .* seems to be moved"
  "libtool: relink: warning: .* seems to be moved"
  "libtool: warning: relinking .*"
  )

SET(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 100000)
SET(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 1048576)
SET (CTEST_NIGHTLY_START_TIME "23:00:00 CET")

SET(CTEST_CUSTOM_PRE_TEST scripts/pretest)
