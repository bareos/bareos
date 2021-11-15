#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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

if(${scsi-crypto})

  # LINUX: check if HAVE_SCSI_SG_H and HAVE_SCSI_SCSI_H are true
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if("${HAVE_SCSI_SG_H}" AND "${HAVE_SCSI_SCSI_H}")
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 1)
    else()
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 0)
      message(
        FATAL_ERROR
          "scsi crypto support selected but required headers are missing"
      )
    endif()
  endif()

  # SUN: check if HAVE_SYS_SCSI_IMPL_USCSI_H
  if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
    if("${HAVE_SYS_SCSI_IMPL_USCSI_H}")
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 1)
    else()
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 0)
      message(
        FATAL_ERROR
          "scsi crypto support selected but required headers are missing"
      )
    endif()
  endif()

  # FREEBSD: HAVE_CAMLIB_H and HAVE_CAM_SCSI_SCSI_MESSAGE_H also add
  # CAM_LIBS="-lcam"
  if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    if("${HAVE_CAMLIB_H}" AND "${HAVE_CAM_SCSI_SCSI_MESSAGE_H}")
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 1)
      set(CAM_LIBRARIES cam)
    else()
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 0)
      message(
        FATAL_ERROR
          "scsi crypto support selected but required headers are missing"
      )
    endif()
  endif()

  # NETBSD: HAVE_DEV_SCSIPI_SCSIPI_ALL_H
  if(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    if("${HAVE_DEV_SCSIPI_SCSIPI_ALL_H}")
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 1)
    else()
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 0)
      message(
        FATAL_ERROR
          "scsi crypto support selected but required headers are missing"
      )
    endif()
  endif()

  # OPENBSD: HAVE_USCSI_ALL_H
  if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    if("${HAVE_USCSI_ALL_H}")
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 1)
    else()
      set(HAVE_LOWLEVEL_SCSI_INTERFACE 0)
      message(
        FATAL_ERROR
          "scsi crypto support selected but required headers are missing"
      )
    endif()
  endif()

endif()
