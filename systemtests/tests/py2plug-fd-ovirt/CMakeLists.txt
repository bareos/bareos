if(TARGET python-fd AND ovirt_server)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-ovirt")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-ovirt" DISABLED COMMENT
    "no python-fd or ovirt_server not set"
  )
endif()

if(TARGET python3-fd AND ovirt_server)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-ovirt")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-ovirt" DISABLED COMMENT
    "no python3-fd or ovirt_server not set"
  )
endif()
