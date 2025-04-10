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

  check_include_files(alloca.h HAVE_ALLOCA_H)
  check_include_files(afs/afsint.h HAVE_AFS_AFSINT_H)
  check_include_files(afs/venus.h HAVE_AFS_VENUS_H)
  check_include_files(arpa/nameser.h HAVE_ARPA_NAMESER_H)
  check_include_files(attr.h HAVE_ATTR_H)
  check_include_files(demangle.h HAVE_DEMANGLE_H)
  check_include_files(execinfo.h HAVE_EXECINFO_H)
  check_include_files(grp.h HAVE_GRP_H)
  check_include_files(libutil.h HAVE_LIBUTIL_H)
  check_include_files(mtio.h HAVE_MTIO_H)
  check_include_files(pwd.h HAVE_PWD_H)
  check_include_files(regex.h HAVE_REGEX_H)
  check_include_files("sys/types.h;sys/acl.h" HAVE_SYS_ACL_H)
  check_include_files(sys/attr.h HAVE_SYS_ATTR_H)
  check_include_files(sys/capability.h HAVE_SYS_CAPABILITY_H)
  check_include_files(sys/ea.h HAVE_SYS_EA_H)
  check_include_files("sys/types.h;sys/extattr.h" HAVE_SYS_EXTATTR_H)
  check_include_files(sys/mtio.h HAVE_SYS_MTIO_H)
  check_include_files(sys/nvpair.h HAVE_SYS_NVPAIR_H)

  check_include_files("sys/types.h;sys/tape.h" HAVE_SYS_TAPE_H)

  check_include_files(sys/time.h HAVE_SYS_TIME_H)

  check_include_file_cxx(cxxabi.h HAVE_CXXABI_H)
  check_include_files(curses.h HAVE_CURSES_H)
  check_include_files(poll.h HAVE_POLL_H)
  check_include_files(sys/poll.h HAVE_SYS_POLL_H)
  check_include_files(sys/statvfs.h HAVE_SYS_STATVFS_H)
  check_include_files(umem.h HAVE_UMEM_H)
  check_include_files(ucontext.h HAVE_UCONTEXT_H)
  check_include_files(demangle.h HAVE_DEMANGLE_H)

  check_include_files(sys/extattr.h HAVE_SYS_EXTATTR_H)
  check_include_files(libutil.h HAVE_LIBUTIL_H)
  check_include_files(sys/ea.h HAVE_SYS_EA_H)
  check_include_files(sys/proplist.h HAVE_SYS_PROPLIST_H)
  check_include_files(sys/xattr.h HAVE_SYS_XATTR_H)

  include(CheckSymbolExists)

  check_include_files(glusterfs/api/glfs.h HAVE_GLUSTERFS_API_GLFS_H)

  check_include_files(sys/prctl.h HAVE_SYS_PRCTL_H)

  check_include_files(scsi/scsi.h HAVE_SCSI_SCSI_H)

  check_include_files("stddef.h;scsi/sg.h" HAVE_SCSI_SG_H)

  check_include_files(
    "sys/types.h;sys/scsi/impl/uscsi.h" HAVE_SYS_SCSI_IMPL_USCSI_H
  )
  check_include_files("stdio.h;camlib.h" HAVE_CAMLIB_H)
  check_include_files(cam/scsi/scsi_message.h HAVE_CAM_SCSI_SCSI_MESSAGE_H)
  check_include_files(dev/scsipi/scsipi_all.h HAVE_DEV_SCSIPI_SCSIPI_ALL_H)

  check_include_files(scsi/uscsi_all.h HAVE_USCSI_ALL_H)
else()
  # we provide tho following headers
  set(HAVE_SYS_MTIO_H 1)
  set(HAVE_SYS_TIME_H 1)
endif()
