#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2025 Bareos GmbH & Co. KG
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

# determine value of HAVE_LOWLEVEL_SCSI_INTERFACE

if(NOT HAVE_WIN32 AND NOT HAVE_DARWIN_OS AND ${scsi-crypto})
  include(CheckIncludeFiles)
  add_library(bareos-low-level-scsi INTERFACE)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    check_include_files(
      "stddef.h;scsi/scsi.h;scsi/sg.h" HAVE_LOWLEVEL_SCSI_INTERFACE
    )
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    check_include_files(
      "sys/types.h;sys/scsi/impl/uscsi.h" HAVE_LOWLEVEL_SCSI_INTERFACE
    )
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    check_include_files(
      "stdio.h;camlib.h;cam/scsi/scsi_message.h" HAVE_LOWLEVEL_SCSI_INTERFACE
    )
    target_link_libraries(bareos-low-level-scsi PRIVATE cam)
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    check_include_files("dev/scsipi/scsipi_all.h" HAVE_LOWLEVEL_SCSI_INTERFACE)
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    check_include_files("scsi/uscsi_all.h" HAVE_LOWLEVEL_SCSI_INTERFACE)
  endif()

  if(NOT HAVE_LOWLEVEL_SCSI_INTERFACE)
    message(
      FATAL_ERROR
        "scsi crypto support selected but required headers are missing"
    )
  endif()

  add_library(Bareos::LowLevelScsi ALIAS bareos-low-level-scsi)
endif()
