# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2020-2022 Bareos GmbH & Co. KG
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

find_program(SETFACL_PROG setfacl)
find_program(GETFACL_PROG getfacl)

set(SETFACL_WORKS NO)
if(SETFACL_PROG AND GETFACL_PROG)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/acl-test-file.txt" "Just a testfile")
  execute_process(
    COMMAND ${SETFACL_PROG} -m user:0:rw- acl-test-file.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    RESULT_VARIABLE SETFACL_RETURN
  )
  if(SETFACL_RETURN EQUAL 0)
    set(SETFACL_WORKS YES)
  endif()
endif()
