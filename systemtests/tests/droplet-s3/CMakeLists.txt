get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
if(TARGET droplet
   AND S3CMD
   AND MINIO
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
