#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2024 Bareos GmbH & Co. KG
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

# configure a .in file, either IN_TREE which will put the configured file next
# to its template or into the corresponding CMAKE_CURRENT_BINARY_DIR

function(bareos_configure_file)
  cmake_parse_arguments(ARG "IN_TREE;COPY" "" "FILES;GLOB;GLOB_RECURSE" ${ARGN})
  set(file_list "")
  if(ARG_FILES)
    list(APPEND file_list ${ARG_FILES})
  endif()

  if(ARG_GLOB_RECURSE)
    file(GLOB_RECURSE glob_lst ${ARG_GLOB_RECURSE})
    list(APPEND file_list ${glob_lst})
  endif()
  if(ARG_GLOB)
    file(GLOB glob_lst ${ARG_GLOB})
    list(APPEND file_list ${glob_lst})
  endif()
  foreach(in_file ${file_list})
    if(ARG_IN_TREE)
      set(out_file "${in_file}")
    else()
      if(IS_ABSOLUTE "${in_file}")
        file(RELATIVE_PATH rel_file "${CMAKE_CURRENT_SOURCE_DIR}" "${in_file}")
        set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${rel_file}")
      else()
        set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${in_file}")
      endif()
    endif()
    if(${out_file} MATCHES ".*\.in$")
      string(REGEX REPLACE ".in\$" "" out_file "${out_file}")
      message(STATUS "creating file ${out_file}")
      configure_file(${in_file} ${out_file} @ONLY)
    else()
      get_filename_component(out_dir ${out_file} DIRECTORY)
      if(NOT EXISTS "${out_dir}")
        message(STATUS "creating directory ${out_dir}")
        file(MAKE_DIRECTORY "${out_dir}")
      endif()
      if(ARG_COPY)
        message(STATUS "creating file ${out_file}")
        configure_file("${in_file}" "${out_file}" COPYONLY)
      else()
        message(STATUS "creating symlinking ${out_file}")
        file(CREATE_LINK "${in_file}" "${out_file}" SYMBOLIC)
      endif()
    endif()
  endforeach()
endfunction()
