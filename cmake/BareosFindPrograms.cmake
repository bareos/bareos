#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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

# find programs
find_program(AWK awk)
find_program(GAWK gawk)
IF(HAVE_SUN_OS)
   SET(AWK ${GAWK})
ENDIF()

find_program(PIDOF pidof)
IF(NOT ${PIDOF})
   SET(PIDOF "")
ENDIF()

find_program(PS ps)
find_program(PGREP pgrep)
find_program(RPCGEN rpcgen)
find_program(MTX mtx)

find_program(GCORE gcore)
find_program(GDB gdb)
find_program(DBX dbx)
find_program(MDB mdb)
