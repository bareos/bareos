# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2019 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

function(timestamp_at at result format)
  if(CMAKE_VERSION VERSION_GREATER 3.8.0)
    set(old_epoch "$ENV{SOURCE_DATE_EPOCH}")
    set(ENV{SOURCE_DATE_EPOCH} "${at}")
    string(TIMESTAMP out "${format}" UTC)
    set(ENV{SOURCE_DATE_EPOCH} "${old_epoch}")
  else()
    set(old_lang "$ENV{LC_ALL}")
    set(ENV{LC_ALL} "C")
    execute_process(
      COMMAND date --utc "--date=@${at}" "+${format}"
      OUTPUT_VARIABLE out
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(ENV{LC_ALL} "${old_lang}")
    if(out STREQUAL "")
      message(
        FATAL_ERROR
          "Cannot use SOURCE_DATE_EPOCH (cmake < 3.8) and your 'date' command is not compatible with Bareos' timestamp_at()."
      )
    endif()
  endif()
  set("${result}"
      "${out}"
      PARENT_SCOPE
  )
endfunction()
