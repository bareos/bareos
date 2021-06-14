if(TARGET python-fd)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-local-fileset-restoreobject"
  )
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-local-fileset-restoreobject" DISABLED
  )
endif()

if(TARGET python3-fd)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-local-fileset-restoreobject"
  )
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-local-fileset-restoreobject" DISABLED
  )
endif()
