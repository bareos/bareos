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

function(find_dll_for_lib out_var implib)
  find_program(
    DLLTOOL_EXECUTABLE
    NAMES dlltool dlltool.exe
    DOC "The path to the DLLTOOL utility"
  )

  if(NOT DLLTOOL_EXECUTABLE)
    message(FATAL_ERROR "Required DLLTOOL to resolve DLL names not available")
  endif()
  execute_process(
    COMMAND "${DLLTOOL_EXECUTABLE}" -I "${implib}"
    OUTPUT_VARIABLE dll_name
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  cmake_path(GET implib PARENT_PATH implib_dir)
  if(EXISTS "${implib_dir}/${dll_name}")
    set(dll_path "${implib_dir}/${dll_name}")
  elseif(EXISTS "${implib_dir}/../bin/${dll_name}")
    set(dll_path "${implib_dir}/../bin/${dll_name}")
  else()
    message(FATAL_ERROR "No DLL for ${implib} found")
  endif()
  cmake_path(NORMAL_PATH dll_path)
  set(${out_var}
      ${dll_path}
      PARENT_SCOPE
  )
endfunction()

# Fixup for targets created by broken find modules.
# This will try to find the matching .dll for the .lib or .dll.a, set up a
# proper imported shared_library target and make that new target an interface
# requirement of the original one.

function(fixup_target_dlls target)
  if(WIN32)
    if(NOT TARGET "${target}")
      message(FATAL_ERROR "Target ${target} not found.")
    endif()
    get_target_property(type "${target}" TYPE)
    get_target_property(imported ${target} IMPORTED)
    if(imported AND type STREQUAL "UNKNOWN_LIBRARY")
      add_library(dll_fixup_${target} SHARED IMPORTED)
      target_link_libraries(${target} INTERFACE dll_fixup_${target})

      get_property(
        loc_release_set
        TARGET ${target}
        PROPERTY IMPORTED_LOCATION_RELEASE
        SET
      )
      if(loc_release_set)
        get_target_property(loc_release ${target} IMPORTED_LOCATION_RELEASE)
        find_dll_for_lib(dll_release ${loc_release})
        set_target_properties(
          dll_fixup_${target}
          PROPERTIES IMPORTED_LOCATION ${dll_release}
                     IMPORTED_IMPLIB ${loc_release}
                     IMPORTED_LOCATION_RELEASE ${dll_release}
                     IMPORTED_IMPLIB_RELEASE ${loc_release}
        )
      endif()

      get_property(
        loc_set
        TARGET ${target}
        PROPERTY IMPORTED_LOCATION
        SET
      )
      if(loc_set)
        get_target_property(loc ${target} IMPORTED_LOCATION)
        find_dll_for_lib(dll ${loc})
        set_target_properties(
          dll_fixup_${target}
          PROPERTIES IMPORTED_LOCATION ${dll}
                     IMPORTED_IMPLIB ${loc}
        )
      endif()

      get_property(
        loc_debug_set
        TARGET ${target}
        PROPERTY IMPORTED_LOCATION_DEBUG
        SET
      )
      if(loc_debug_set)
        get_target_property(loc_debug ${target} IMPORTED_LOCATION_DEBUG)
        find_dll_for_lib(dll_debug ${loc_debug})
        set_target_properties(
          dll_fixup_${target} PROPERTIES IMPORTED_LOCATION_DEBUG ${dll_debug}
                                         IMPORTED_IMPLIB_DEBUG ${loc_debug}
        )
      endif()
    else()
      message(
        FATAL_ERROR "Target ${target} with type ${type} cannot be fixed up"
      )
    endif()

  endif()
endfunction()
