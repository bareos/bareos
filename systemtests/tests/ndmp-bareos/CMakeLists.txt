#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2024 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
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

      create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
    else()
      create_systemtest(
        ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
        "failed to get IP_ADDRESS_TO_ACCESS_NDMP_DATA_AGENT"
      )
    endif()
  else()
    create_systemtest(
      ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "ip command not found"
    )
  endif()
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
    "ndmp_data_agent_address not given"
  )
endif()
