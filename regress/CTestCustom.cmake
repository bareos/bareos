set(CTEST_CUSTOM_ERROR_EXCEPTION
    ${CTEST_CUSTOM_ERROR_EXCEPTION}
    "ERROR: *role \".*\" already exists"
    "ERROR: *database \".*\" already exists"
    "ERROR: *table \".*\" does not exist"
    "NOTICE: *table \".*\" does not exist, skipping"
    "NOTICE: .*will create implicit sequence"
    "NOTICE: .*will create implicit index"
)

set(CTEST_CUSTOM_WARNING_EXCEPTION
    ${CTEST_CUSTOM_WARNING_EXCEPTION}
    "libtool: install: warning: relinking .*"
    "libtool: link: warning: .* seems to be moved"
    "libtool: relink: warning: .* seems to be moved"
    "libtool: warning: relinking .*"
)

set(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 100000)
set(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 1048576)
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 500)

set(CTEST_NIGHTLY_START_TIME "23:00:00 CET")

set(CTEST_CUSTOM_PRE_TEST scripts/pretest)
