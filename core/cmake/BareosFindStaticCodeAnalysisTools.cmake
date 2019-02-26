#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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


IF(DO_STATIC_CODE_CHECKS)
# analyse the code with clang-tidy, cppcheck, cpplint and iwyu
find_program(IWYU iwyu)
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU})
set(CMAKE_C_INCLUDE_WHAT_YOU_USE ${IWYU})

set(CMAKE_LINK_WHAT_YOU_USE true)

find_program(CLANGTIDY clang-tidy)
set(CMAKE_CXX_CLANG_TIDY "${CLANGTIDY};-checks=*")

find_program(CPPCHECK cppcheck)
set(CMAKE_CXX_CPPCHECK "${CPPCHECK};--std=c++11")

find_program(CPPLINT cpplint.py)
set(CMAKE_CXX_CPPLINT "${CPPLINT};--linelength=80")

ENDIF()


