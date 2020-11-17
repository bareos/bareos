#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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

#
# configure file all *.in files
#

# if we're not the toplevel project, then we want to glob in core and debian
# subdirectories only
get_directory_property(have_parent PARENT_DIRECTORY)
if(have_parent)
  file(GLOB_RECURSE IN_FILES "${CMAKE_SOURCE_DIR}/core/*.in"
       "${CMAKE_SOURCE_DIR}/debian/*.in"
  )
else()
  file(GLOB_RECURSE IN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
endif()

foreach(in_file ${IN_FILES})
  string(REGEX REPLACE ".in\$" "" file ${in_file})
  message(STATUS "creating file ${file}")
  configure_file(${in_file} ${file} @ONLY)
endforeach()
