if(TARGET python-fd)
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-local-fileset")
else()
  create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-local-fileset" DISABLED)
endif()

if(TARGET python3-fd)
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-local-fileset")
  # Fails with UnicodeEncodeError: 'utf-8' codec can't encode characters in
  # position 133-142: surrogates not allowed
  set_tests_properties(
    "${SYSTEMTEST_PREFIX}py3plug-fd-local-fileset" PROPERTIES LABELS "broken"
  )
else()
  create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-local-fileset" DISABLED)
endif()
