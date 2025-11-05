#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2025 Bareos GmbH & Co. KG
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

set(ndmp_data_agent_address
    ""
    CACHE STRING "address of the ndmp data agent"
)
set(ndmp_data_agent_user
    "ndmpadmin"
    CACHE STRING "user to access the ndmp data agent"
)
set(ndmp_data_agent_password
    "test"
    CACHE STRING "password to access the ndmp data agent"
)
set(ndmp_tape_and_robot_agent_address
    CACHE STRING "address of the ndmp robot and tape agents"
)
set(ndmp_tape_and_robot_agent_user
    "ndmpadmin"
    CACHE STRING "user of the ndmp robot and tape agents"
)
set(ndmp_tape_and_robot_agent_password
    "test"
    CACHE STRING "password of the ndmp robot and tape agents"
)
set(ndmp_tape_and_robot_agent_changer_device
    ""
    CACHE STRING "changer device of the ndmp robot agent"
)
set(ndmp_tape_and_robot_agent_tape_device
    ""
    CACHE STRING "list of tape device of the ndmp robot agent"
)
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

      if(ndmp_tape_and_robot_agent_address)
        set(ndmp_tapes_devices
            ""
            CACHE STRING "builded string of Devices for ndmp storage"
        )
        foreach(device ${ndmp_tape_and_robot_agent_tape_device})
          string(APPEND ndmp_tapes_devices "Device = \"${device}\"\n")
        endforeach(device)
        create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
        # add_alphabetic_requirements(${SYSTEMTEST_PREFIX} ${BASENAME}) Testing
        # needs exclusive access to an autochanger from the ndmp host by iscsi
        set_tests_properties(
          system:ndmp-native:03-restore-single-file PROPERTIES DISABLED TRUE
        )
        set_tests_properties(
          system:ndmp-native:07-restore-incremental PROPERTIES DISABLED TRUE
        )
      else()
        create_systemtest(
          ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
          "ndmp_tape_and_robot_agent_address not given"
        )
      endif()
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
