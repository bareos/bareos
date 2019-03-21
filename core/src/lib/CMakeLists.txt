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

IF(NOT client-only)
set(INCLUDE_FILES ../include/baconfig.h ../include/bareos.h
   ../include/bc_types.h ../include/config.h
   ../include/jcr.h ../include/version.h
   address_conf.h alist.h attr.h base64.h berrno.h
   bits.h bpipe.h breg.h bregex.h bstringlist.h bsock.h
   bsock_tcp.h btime.h btimers.h cbuf.h
   crypto.h crypto_cache.h devlock.h dlist.h fnmatch.h
   guid_to_name.h htable.h ini.h lex.h lib.h lockmgr.h
   md5.h mem_pool.h message.h messages_resource.h mntent_cache.h parse_conf.h
   plugins.h qualified_resource_name_type_converter.h queue.h rblist.h
   runscript.h rwlock.h scsi_crypto.h scsi_lli.h scsi_tapealert.h
   serial.h sha1.h smartall.h status.h tls.h tls_conf.h tree.h try_tls_handshake_as_a_server.h
   var.h watchdog.h workq.h)

INSTALL(FILES ${INCLUDE_FILES} DESTINATION ${includedir})
ENDIF()

include_directories(
   ${OPENSSL_INCLUDE_DIR}
   ${PTHREAD_INCLUDE_DIRS}
   ${FASTLZ_INCLUDE_DIRS}
   ${ZLIB_INCLUDE_DIRS}
   ${ACL_INCLUDE_DIRS}
   ${LZO2_INCLUDE_DIRS}
   ${CAP_INCLUDE_DIRS}
   ${WRAP_INCLUDE_DIRS})

set (BAREOS_SRCS  address_conf.cc alist.cc attr.cc attribs.cc base64.cc
   berrno.cc bget_msg.cc binflate.cc bnet_server_tcp.cc bnet.cc  bpipe.cc
   breg.cc bregex.cc bsnprintf.cc bsock.cc bsock_tcp.cc bstringlist.cc
   bsys.cc btime.cc btimers.cc  cbuf.cc
   connection_pool.cc cram_md5.cc crypto.cc  crypto_cache.cc
   crypto_none.cc crypto_nss.cc  crypto_openssl.cc crypto_wrap.cc daemon.cc
   devlock.cc dlist.cc  edit.cc fnmatch.cc guid_to_name.cc hmac.cc htable.cc
   jcr.cc json.cc  lockmgr.cc md5.cc mem_pool.cc message.cc messages_resource.cc
   mntent_cache.cc output_formatter.cc passphrase.cc path_list.cc plugins.cc
   bpoll.cc  priv.cc
   queue.cc rblist.cc runscript.cc rwlock.cc scan.cc scsi_crypto.cc  scsi_lli.cc
   serial.cc sha1.cc signal.cc  smartall.cc tls.cc
   tls_conf.cc tls_conf_cert.cc tls_openssl.cc
   tls_openssl_crl.cc tls_openssl_private.cc tree.cc  try_tls_handshake_as_a_server.cc
   compression.cc util.cc var.cc watchdog.cc workq.cc)

IF(HAVE_WIN32)
   LIST(APPEND BAREOS_SRCS
      ../win32/compat/compat.cc
      ../win32/compat/glob.cc
      ../win32/compat/print.cc
      ../win32/compat/winapi.cc)
ELSE()
   LIST(APPEND BAREOS_SRCS
     scsi_tapealert.cc)
ENDIF()

set (BAREOSCFG_SRCS configured_tls_policy_getter.cc ini.cc lex.cc
     parse_bsr.cc res.cc parse_conf.cc
     res.cc qualified_resource_name_type_converter.cc)
LIST(APPEND BAREOS_SRCS ${BAREOSCFG_SRCS})

add_library(bareos SHARED ${BAREOS_SRCS})

target_link_libraries(bareos
   ${OPENSSL_LIBRARIES} ${PTHREAD_LIBRARIES} ${FASTLZ_LIBRARIES} ${ZLIB_LIBRARIES}
   ${ACL_LIBRARIES} ${LZO2_LIBRARIES} ${CAP_LIBRARIES} ${WRAP_LIBRARIES}
   ${CAM_LIBRARIES} ${WINDOWS_LIBRARIES} ${JANSSON_LIBRARIES})

INSTALL(TARGETS bareos DESTINATION ${libdir})

set_target_properties(bareos PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                                      SOVERSION "${SOVERSION}")

IF(HAVE_WIN32)
   set_target_properties(bareos PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
ENDIF()

