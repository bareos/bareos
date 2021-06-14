if(TARGET python-fd AND XTRABACKUP)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-percona-xtrabackup")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-percona-xtrabackup" DISABLED COMMENT
    "no python-fd or XTRABACKUP not set"
  )
endif()

if(TARGET python3-fd AND XTRABACKUP)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-percona-xtrabackup")
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-percona-xtrabackup" DISABLED COMMENT
    "no python3-fd or XTRABACKUP not set"
  )
endif()
