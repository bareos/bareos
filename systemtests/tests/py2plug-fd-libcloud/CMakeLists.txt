if(TARGET python-fd)
  check_pymodule_available(2 libcloud)
  if(S3CMD
     AND MINIO
     AND PYMODULE_2_LIBCLOUD_FOUND
  )
    create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-libcloud")
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} "py2plug-fd-libcloud" DISABLED COMMENT
      "S3CMD, MINIO or LIBCLOUD MODULE not found"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py2plug-fd-libcloud" DISABLED COMMENT
    "requires python-fd (Python2)"
  )
endif()

if(TARGET python3-fd)
  check_pymodule_available(3 libcloud)
  if(S3CMD
     AND MINIO
     AND PYMODULE_3_LIBCLOUD_FOUND
     AND (${Python3_VERSION_MAJOR} EQUAL 3)
     AND (${Python3_VERSION_MINOR} LESS 10)
  )
    create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud")
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud" DISABLED COMMENT
      "S3CMD, MINIO or LIBCLOUD MODULE not found or Python > 3.7"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "py3plug-fd-libcloud" DISABLED COMMENT
    "requires python3-fd"
  )
endif()
