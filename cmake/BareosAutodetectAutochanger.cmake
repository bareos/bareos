#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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

# bareos_autodetect_autochanger()
#
# Detects connected SCSI tape changers and their associated tape drives by
# running core/scripts/bareos-detect-autochanger, which uses the Linux sysfs
# interface.  Prefers stable /dev/tape/by-id/ symlinks over raw /dev/sgX /
# /dev/nstX device nodes.  Multiple autochangers are supported.
#
# The same script can be run standalone from the source tree:
#   ./core/scripts/bareos-detect-autochanger
#
# Sets in the caller's scope:
#   DETECTED_CHANGER_COUNT   – number of changers found (0 if none)
#   DETECTED_CHANGER_DEVICE  – path to the first SCSI medium changer found
#   DETECTED_TAPE_DEVICES    – CMake list of tape drive paths for the first
#                              changer (semicolon-separated)
#
function(bareos_autodetect_autochanger)
  if(NOT UNIX)
    set(DETECTED_CHANGER_COUNT
        0
        PARENT_SCOPE
    )
    set(DETECTED_CHANGER_DEVICE
        ""
        PARENT_SCOPE
    )
    set(DETECTED_TAPE_DEVICES
        ""
        PARENT_SCOPE
    )
    return()
  endif()

  set(_detect_script
      "${CMAKE_SOURCE_DIR}/core/scripts/bareos-detect-autochanger"
  )

  execute_process(
    COMMAND bash "${_detect_script}"
    OUTPUT_VARIABLE _detect_output
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _detect_result
  )

  set(_changer_count 0)
  set(_changer_path "")
  set(_tape_paths "")

  if(_detect_result EQUAL 0)
    # Output format:
    #   CHANGER_COUNT=<n>
    #   CHANGER_DEVICE_0=<path>
    #   TAPE_DEVICES_0=<path1>;<path2>;...
    #   CHANGER_DEVICE_1=<path>
    #   ...
    string(REGEX MATCH "CHANGER_COUNT=([0-9]+)" _ "${_detect_output}")
    set(_changer_count "${CMAKE_MATCH_1}")

    string(REGEX MATCH "CHANGER_DEVICE_0=([^\n]+)" _ "${_detect_output}")
    set(_changer_path "${CMAKE_MATCH_1}")

    string(REGEX MATCH "TAPE_DEVICES_0=([^\n]+)" _ "${_detect_output}")
    # The value is already semicolon-separated — CMake treats it as a list.
    set(_tape_paths "${CMAKE_MATCH_1}")
  endif()

  set(DETECTED_CHANGER_COUNT
      "${_changer_count}"
      PARENT_SCOPE
  )
  set(DETECTED_CHANGER_DEVICE
      "${_changer_path}"
      PARENT_SCOPE
  )
  set(DETECTED_TAPE_DEVICES
      "${_tape_paths}"
      PARENT_SCOPE
  )
endfunction()
