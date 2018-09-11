#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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

set(INCLUDE_FILES ../include/baconfig.h ../include/bareos.h
   ../include/bc_types.h ../include/config.h
   ../include/jcr.h ../include/version.h
   address_conf.h alist.h attr.h base64.h berrno.h
   bits.h bpipe.h breg.h bregex.h bsock.h
   bsock_tcp.h btime.h btimers.h cbuf.h
   crypto.h crypto_cache.h devlock.h dlist.h fnmatch.h
   guid_to_name.h htable.h ini.h lex.h lib.h lockmgr.h
   md5.h mem_pool.h message.h mntent_cache.h ordered_cbuf.h parse_conf.h
   plugins.h qualified_resource_name_type_converter.h queue.h rblist.h
   runscript.h rwlock.h scsi_crypto.h scsi_lli.h scsi_tapealert.h sellist.h
   serial.h sha1.h smartall.h status.h tls.h tls_conf.h tree.h var.h
   watchdog.h workq.h)

INSTALL(FILES ${INCLUDE_FILES} DESTINATION ${includedir})

add_subdirectory(bareos)

IF(NOT HAVE_WIN32 AND GTEST_FOUND)
   add_subdirectory(unittests)
ENDIF()
