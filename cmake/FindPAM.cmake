#[=======================================================================[.rst:
FindPAM
-----------

Finds PAM.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``PAM::pam``
  Main PAM library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``PAM_FOUND``
  true if PAM headers and library were found
``PAM_LIBRARY``
  PAM library to be linked
``PAM_INCLUDE_DIR``
  the directory containing PAM headers

#]=======================================================================]

find_path(PAM_INCLUDE_DIR NAMES "security/pam_appl.h" "pam/pam_appl.h")
find_library(PAM_LIBRARY NAMES "pam")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PAM REQUIRED_VARS PAM_INCLUDE_DIR PAM_LIBRARY)
mark_as_advanced(PAM_INCLUDE_DIR PAM_LIBRARY)

if(PAM_FOUND)
  if(NOT TARGET PAM::pam)
    add_library(PAM::pam SHARED IMPORTED)
    set_target_properties(
      PAM::pam PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${PAM_INCLUDE_DIR}"
    )
    set_target_properties(
      PAM::pam PROPERTIES IMPORTED_LOCATION "${PAM_LIBRARY}"
    )
  endif()
endif()
