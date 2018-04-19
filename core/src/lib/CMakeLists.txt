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

include_directories(../include ..
   ${OPENSSL_INCLUDE_DIRS}
   ${PTHREAD_INCLUDE_DIRS}
   ${FASTLZ_INCLUDE_DIRS}
   ${ZLIB_INCLUDE_DIRS}
   ${ACL_INCLUDE_DIRS}
   ${LZO2_INCLUDE_DIRS}
   ${CAP_INCLUDE_DIRS}
   ${WRAP_INCLUDE_DIRS}
   )

set(INCLUDE_FILES ../include/baconfig.h ../include/bareos.h
		../include/bc_types.h ../include/config.h
		../include/jcr.h ../include/version.h
		address_conf.h alist.h attr.h base64.h berrno.h
		bits.h bpipe.h breg.h bregex.h bsock.h
		bsock_tcp.h bsr.h btime.h btimers.h cbuf.h
		crypto.h crypto_cache.h devlock.h dlist.h fnmatch.h
		guid_to_name.h htable.h ini.h lex.h lib.h lockmgr.h
		md5.h mem_pool.h message.h mntent_cache.h ordered_cbuf.h parse_conf.h
		plugins.h protos.h queue.h rblist.h runscript.h rwlock.h
		scsi_crypto.h scsi_lli.h scsi_tapealert.h sellist.h
		serial.h sha1.h smartall.h status.h tls.h tls_conf.h tree.h var.h
		watchdog.h workq.h)

set (BAREOS_SRCS  address_conf.cc alist.cc attr.cc attribs.cc base64.cc
   berrno.cc bget_msg.cc binflate.cc bnet_server_tcp.cc bnet.cc  bpipe.cc
   breg.cc bregex.cc bsnprintf.cc bsock.cc bsock_tcp.cc
   bsys.cc btime.cc btimers.cc  cbuf.cc compression.cc
   connection_pool.cc cram-md5.cc crypto.cc  crypto_cache.cc crypto_gnutls.cc
   crypto_none.cc crypto_nss.cc  crypto_openssl.cc crypto_wrap.cc daemon.cc
   devlock.cc dlist.cc  edit.cc fnmatch.cc guid_to_name.cc hmac.cc htable.cc
   jcr.cc json.cc  lockmgr.cc md5.cc mem_pool.cc message.cc mntent_cache.cc
   output_formatter.cc ordered_cbuf.cc passphrase.cc path_list.cc plugins.cc
   bpoll.cc  priv.cc
   queue.cc rblist.cc runscript.cc rwlock.cc scan.cc scsi_crypto.cc  scsi_lli.cc
   sellist.cc serial.cc sha1.cc signal.cc  smartall.cc
   tls_gnutls.cc tls_none.cc tls_nss.cc tls_conf.cc tls_openssl.cc tree.cc util.cc var.cc
   watchdog.cc workq.cc)

IF(HAVE_WIN32)
   LIST(APPEND BAREOS_SRCS
      ../win32/compat/compat.cc
      ../win32/compat/glob.cc
      ../win32/compat/print.cc
      ../win32/compat/winapi.cc
      )
ELSE()
   LIST(APPEND BAREOS_SRCS
     scsi_tapealert.cc)
ENDIF()

add_library(bareos SHARED ${BAREOS_SRCS})

target_link_libraries(bareos
   ${OPENSSL_LIBRARIES} ${PTHREAD_LIBRARIES} ${FASTLZ_LIBRARIES} ${ZLIB_LIBRARIES}
   ${ACL_LIBRARIES} ${LZO2_LIBRARIES} ${CAP_LIBRARIES} ${WRAP_LIBRARIES} ${CAM_LIBRARIES}
   ${WINDOWS_LIBRARIES} ${JANSSON_LIBRARIES})

set (BAREOSCFG_SRCS ini.cc lex.cc parse_bsr.cc )



set (BAREOS_STATIC_SRCS parse_conf.cc res.cc)

IF(HAVE_WIN32)
   add_library(bareosstatic STATIC ${BAREOS_STATIC_SRCS})
   target_link_libraries(bareosstatic bareoscfg)
ELSE()
   LIST(APPEND BAREOSCFG_SRCS ${BAREOS_STATIC_SRCS})
ENDIF()

add_library(bareoscfg SHARED ${BAREOSCFG_SRCS})
target_link_libraries(bareoscfg bareos ${JANSSON_LIBRARIES})

INSTALL(TARGETS bareos DESTINATION ${libdir})
INSTALL(TARGETS bareoscfg DESTINATION ${libdir})

set_target_properties(bareoscfg PROPERTIES VERSION "${VERSION}"
                                          SOVERSION "${SOVERSION}"
                                          )

set_target_properties(bareos PROPERTIES VERSION "${VERSION}"
                                          SOVERSION "${SOVERSION}"
                                          )
IF(HAVE_WIN32)
   set_target_properties(bareoscfg PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
   set_target_properties(bareos PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
ENDIF()

INSTALL(FILES ${INCLUDE_FILES} DESTINATION ${includedir})


IF(NOT HAVE_WIN32 AND GTEST_FOUND)
   add_subdirectory(unittests)
ENDIF()
