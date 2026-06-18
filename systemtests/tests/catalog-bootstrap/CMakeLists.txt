get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
configurefilestosystemtest(
  "core/src" "tests/${BASENAME}/etc/bareos" "*.conf" @ONLY "defaultconfigs"
)
configurefilestosystemtest(
  "core/src" "tests/${BASENAME}/etc/bareos" "*.in" @ONLY "defaultconfigs"
)
configurefilestosystemtest(
  "core/src" "tests/${BASENAME}/etc/bareos" "*.conf" @ONLY "console"
)
configurefilestosystemtest(
  "core/src" "tests/${BASENAME}/etc/bareos" "*.in" @ONLY "console"
)
