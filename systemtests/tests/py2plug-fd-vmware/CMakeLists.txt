if(TARGET python-fd AND enable_vmware_test)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-vmware")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-vmware" DISABLED COMMENT
    "no python-fd or vmware_server not set"
  )
endif()

if(TARGET python3-fd AND enable_vmware_test)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-vmware")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-vmware" DISABLED COMMENT
    "no python3-fd or vmware_server not set"
  )
endif()
