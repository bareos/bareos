if(TARGET python-dir)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-dir")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-dir" DISABLED COMMENT "no python-dir"
  )
endif()

if(TARGET python3-dir)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-dir")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-dir" DISABLED COMMENT "no python3-dir"
  )
endif()
