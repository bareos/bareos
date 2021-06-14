if(TARGET gfapi-fd
   AND gfapi_fd_host
   AND SUDO
)
  list(APPEND SYSTEM_TESTS gfapi-fd)
  message(
    "SUDO: ${SUDO},  gfapi-fd test settings: gfapi_fd_host=${gfapi_fd_host}, gfapi_fd_testvolume=${gfapi_fd_testvolume}"
  )
  create_systemtest(${SYSTEMTEST_PREFIX} gfapi-fd)
else()
  create_systemtest(${SYSTEMTEST_PREFIX} gfapi-fd DISABLED)
endif()
