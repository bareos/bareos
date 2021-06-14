get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

set(SYSTEMTEST_LDAP_ADDRESS
    ""
    CACHE STRING "address of LDAP server for Plugin test"
)
set(SYSTEMTEST_LDAP_BASEDN
    "dc=example,dc=org"
    CACHE STRING "LDAP base DN for plugin test"
)
set(SYSTEMTEST_LDAP_BINDDN
    "cn=admin,dc=example,dc=org"
    CACHE STRING "LDAP bind DN for plugin test"
)
set(SYSTEMTEST_LDAP_PASSWORD
    "admin"
    CACHE STRING "LDAP bind password for plugin test"
)
check_pymodule_available(2 ldap)
if(TARGET python-fd
   AND PYMODULE_2_LDAP_FOUND
   AND Python2_EXECUTABLE
   AND SYSTEMTEST_LDAP_ADDRESS
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED)
endif()
