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
    CACHE STRING "tape device of the ndmp robot agent"
)

if(ndmp_tape_and_robot_agent_address)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
    "ndmp_tape_and_robot_agent_address not given"
  )
endif()
