if(ndmp_data_agent_address)

  find_program(IP_COMMAND ip)
  if(IP_COMMAND)
    execute_process(
      COMMAND ${IP_COMMAND} route get ${ndmp_data_agent_address}
      OUTPUT_VARIABLE ROUTE_INFO_TO_ACCESS_NDMP_DATA_AGENT
    )

    # cmake-format: off
    #
    # Example:
    # 192.168.76.10 via 192.168.77.1 dev eth0 src 192.168.77.101 uid 25031
    string(
      REGEX REPLACE ".* src ([^ ]+).*"
      \\1
      IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT
      ${ROUTE_INFO_TO_ACCESS_NDMP_DATA_AGENT}
    )
    # cmake-format: on

    message(DEBUG "NDMP ROUTE INFO: ${ROUTE_INFO_TO_ACCESS_NDMP_DATA_AGENT}")

    message(
      STATUS
        "IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT (${ndmp_data_agent_address})=${IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT}"
    )

    if(IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT)
      if(NOT ndmp_data_agent_user)
        set(ndmp_data_agent_user ndmpadmin)
      endif()
      if(NOT ndmp_data_agent_password)
        set(ndmp_data_agent_password test)
      endif()

      create_systemtest(${SYSTEMTEST_PREFIX} "ndmp")
    else()
      create_systemtest(
        ${SYSTEMTEST_PREFIX} "ndmp" DISABLED COMMENT
        "failed to get IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT"
      )
    endif()
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} "ndmp" DISABLED COMMENT "ip command not found"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} "ndmp" DISABLED COMMENT
    "ndmp_data_agent_address not given"
  )
endif()
