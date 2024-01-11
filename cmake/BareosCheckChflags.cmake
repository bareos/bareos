# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2020-2024 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

# Extract version information and commit timestamp if run in a git checkout

find_program(CHFLAGS_PROG chflags)

set(CHFLAGS_WORKS NO)
if(CHFLAGS_PROG)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/chflags-test-file.txt"
       "Just a testfile"
  )
  execute_process(
    COMMAND ${CHFLAGS_PROG} nosunlink chflags-test-file.txt
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RESULT_VARIABLE CHFLAGS_RETURN
  )
  if(CHFLAGS_RETURN EQUAL 0)
    set(CHFLAGS_WORKS YES)
  endif()
endif()
