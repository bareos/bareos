#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2025-2025 Bareos GmbH & Co. KG
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

function(get_target_dependencies result_var target outfile)
  set(required_targets "")

  get_target_property(targettype ${target} TYPE)
  set(msg "${target} ${targettype}\n")
  get_target_property(link_libraries ${target} LINK_LIBRARIES)
  if(NOT link_libraries)
    return()
  endif()
  foreach(library IN LISTS link_libraries)
    strip_link_only(library)
    if(TARGET ${library})
      get_target_property(library_type ${library} TYPE)
      string(APPEND msg "  ${library} ${library_type}\n")
      list(APPEND required_targets ${library})
      get_target_property(
        interface_link_libraries ${library} INTERFACE_LINK_LIBRARIES
      )
      if(interface_link_libraries)
        foreach(interface_library IN LISTS interface_link_libraries)
          strip_link_only(interface_library)
          if(TARGET ${interface_library})
            get_target_property(library_type ${interface_library} TYPE)
            string(APPEND msg "    ${interface_library} ${library_type}\n")
            list(APPEND required_targets ${interface_library})
          else()
            string(APPEND msg "    ${interface_library} (not a target)\n")
          endif()
        endforeach()
      endif()
    else()
      string(APPEND msg "  ${library} (not a target)\n")
    endif()
  endforeach()
  list(SORT required_targets)
  list(REMOVE_DUPLICATES required_targets)
  set(${result_var}
      ${required_targets}
      PARENT_SCOPE
  )
  if(outfile)
    file(WRITE "${outfile}" "${msg}")
  endif()
endfunction()

function(debug_deps targets outfile)
  set(msg)
  set(wanted_types "SHARED_LIBRARY;UNKNOWN_LIBRARY")
  set(properties
      "TYPE;INCLUDE_DIRECTORIES;INTERFACE_INCLUDE_DIRECTORIES;LOCATION"
  )
  foreach(suffix IN ITEMS "" "_RELEASE" "_DEBUG")
    foreach(propname IN ITEMS LOCATION IMPLIB OBJECTS LIBNAME)
      list(APPEND properties "IMPORTED_${propname}${suffix}")
    endforeach()
  endforeach()
  foreach(target IN LISTS targets)
    get_target_property(type ${target} TYPE)
    if(type IN_LIST wanted_types)
      get_target_property(imported ${target} IMPORTED)
      if(imported)
        string(APPEND msg "${target}:\n")
        foreach(property IN LISTS properties)
          string(CONCAT _tmp ${property} " ..................... ")
          string(SUBSTRING "${_tmp}" 0 30 prop_display)
          get_property(
            propset
            TARGET ${target}
            PROPERTY ${property}
            SET
          )
          if(propset)
            get_target_property(prop ${target} ${property})
            string(APPEND msg "  ${prop_display} ${prop}\n")
          else()
            string(APPEND msg "  ${prop_display} (not set)\n")
          endif()
        endforeach()
        string(APPEND msg "\n")
      endif()
    endif()
  endforeach()
  file(WRITE ${outfile} "${msg}")
endfunction()

function(strip_link_only target_var)
  set(target_name "${${target_var}}")
  string(FIND "${target_name}" "$<LINK_ONLY:" pos)
  if(pos EQUAL 0)
    string(LENGTH "${target_name}" target_name_len)
    math(EXPR len "${target_name_len} - 12 - 1")
    string(SUBSTRING "${target_name}" 12 ${len} stripped)
    set(${target_var}
        ${stripped}
        PARENT_SCOPE
    )
  endif()
endfunction()

function(get_all_targets var)
  set(targets)
  get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
  set(${var}
      ${targets}
      PARENT_SCOPE
  )
endfunction()

macro(get_all_targets_recursive targets dir)
  get_property(
    subdirectories
    DIRECTORY ${dir}
    PROPERTY SUBDIRECTORIES
  )
  foreach(subdir ${subdirectories})
    get_all_targets_recursive(${targets} ${subdir})
  endforeach()

  get_property(
    current_targets
    DIRECTORY ${dir}
    PROPERTY BUILDSYSTEM_TARGETS
  )
  list(APPEND ${targets} ${current_targets})
endmacro()
