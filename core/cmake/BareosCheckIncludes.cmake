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

if(NOT MSVC)
  include(CheckIncludeFiles)
  include(CheckIncludeFileCXX)

  check_include_files("sys/types.h;sys/acl.h" HAVE_SYS_ACL_H)
  check_include_files(sys/capability.h HAVE_SYS_CAPABILITY_H)

  include(CheckSymbolExists)

  check_include_files(scsi/scsi.h HAVE_SCSI_SCSI_H)

  check_include_files("stddef.h;scsi/sg.h" HAVE_SCSI_SG_H)

  check_include_files(
    "sys/types.h;sys/scsi/impl/uscsi.h" HAVE_SYS_SCSI_IMPL_USCSI_H
  )
  check_include_files("stdio.h;camlib.h" HAVE_CAMLIB_H)
  check_include_files(cam/scsi/scsi_message.h HAVE_CAM_SCSI_SCSI_MESSAGE_H)
  check_include_files(dev/scsipi/scsipi_all.h HAVE_DEV_SCSIPI_SCSIPI_ALL_H)

  check_include_files(scsi/uscsi_all.h HAVE_USCSI_ALL_H)
endif()
