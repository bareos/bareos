#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

# ndmjob emulates the NDMP DATA, TAPE and ROBOT agents
if(TARGET ndmjob AND TARGET wrap_tar)
  get_target_output_dir(ndmjob_output_dir ndmjob)
  set(NDMJOB_BINARY "${ndmjob_output_dir}/ndmjob")
  get_target_output_dir(WRAP_TAR_DIR wrap_tar)

  # the director limits the tape device and changer names to 127 chars
  set(NDMJOB_ROBOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/tmp/ndmjob/robot)
  string(LENGTH "${NDMJOB_ROBOT_DIR}/drive1" ndmjob_drive_length)
  if(ndmjob_drive_length GREATER 127)
    set(NDMJOB_ROBOT_DIR /tmp/${BASENAME}-ndmjob-robot)
  endif()

  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
  add_alphabetic_requirements(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "ndmjob is not built"
  )
endif()
