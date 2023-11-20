#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2023 Bareos GmbH & Co. KG
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

# this will remove -DNDEBUG and /DNDEBUG from all build-types
foreach(
  flags_var_to_scrub
  CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO
  CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO
  CMAKE_C_FLAGS_MINSIZEREL
)
  string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " " "${flags_var_to_scrub}"
                       "${${flags_var_to_scrub}}"
  )
endforeach()
