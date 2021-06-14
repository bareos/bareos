if(TARGET python-sd)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-sd")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-sd" DISABLED COMMENT "no python-sd"
  )
endif()

if(TARGET python3-sd)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-sd")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-sd" DISABLED COMMENT "no python3-sd"
  )
endif()
