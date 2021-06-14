get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

set(glusterfs_uri
    ""
    CACHE STRING "URI for GlusterFS backend test"
)
mark_as_advanced(glusterfs_uri)

if(SD_GFAPI_DIR_TO_TEST AND glusterfs_uri)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
