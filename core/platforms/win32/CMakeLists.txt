#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2026 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

# VCTOOLSREDISTDIR is set by the MSVC developer environment (e.g. vcvarsall.bat)
# and points at the VC++ redistributable tree matching the toolset Bareos was
# built with. The windows installer script locates vcruntime140.dll below this
# directory at NSIS-compile-time to determine the minimum VC++ runtime version
# it must require on the target system. Fail early here with a clear message
# instead of a hard to interpret NSIS compile error.
if(NOT DEFINED ENV{VCTOOLSREDISTDIR} OR "$ENV{VCTOOLSREDISTDIR}" STREQUAL "")
  message(
    FATAL_ERROR
      "VCTOOLSREDISTDIR is not set in the environment. It must point to the"
      " MSVC VC++ redistributable tree (normally set by vcvarsall.bat /"
      " a Visual Studio developer command prompt) so the windows installer"
      " can determine the required VC++ runtime version."
  )
endif()
if(NOT IS_DIRECTORY "$ENV{VCTOOLSREDISTDIR}")
  message(
    FATAL_ERROR
      "VCTOOLSREDISTDIR=$ENV{VCTOOLSREDISTDIR} does not exist or is not a"
      " directory."
  )
endif()

install(FILES winbareos-native.nsi DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES bareos.ico bareos-config-deploy.bat fillup.sed clientdialog.ini
              databasedialog.ini directordialog.ini storagedialog.ini
        DESTINATION "${CMAKE_INSTALL_PREFIX}/win32"
)
