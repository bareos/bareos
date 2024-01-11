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

find_program(SETFATTR_PROG setfattr)
find_program(GETFATTR_PROG getfattr)

set(SETFATTR_WORKS NO)
if(SETFATTR_PROG AND GETFATTR_PROG)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/xattr-test-file.txt"
       "Just a testfile"
  )
  execute_process(
    COMMAND ${SETFATTR_PROG} --name=user.cmake-check --value=xattr-value
            xattr-test-file.txt
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RESULT_VARIABLE SETFATTR_RETURN
  )
  if(SETFATTR_RETURN EQUAL 0)
    set(SETFATTR_WORKS YES)
  endif()
endif()
