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

include_directories(
   ${OPENSSL_INCLUDE_DIRS}
   ${PTHREAD_INCLUDE_DIRS}
   ${FASTLZ_INCLUDE_DIRS}
   ${ZLIB_INCLUDE_DIRS}
   ${ACL_INCLUDE_DIRS}
   ${LZO2_INCLUDE_DIRS}
   ${CAP_INCLUDE_DIRS}
   ${WRAP_INCLUDE_DIRS}
   ..
   )

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
   sellist.cc serial.cc sha1.cc signal.cc  smartall.cc tls.cc
   tls_gnutls.cc tls_conf_base.cc tls_conf_psk.cc tls_conf_cert.cc tls_openssl.cc tls_openssl_crl.cc
   tls_openssl_private.cc tree.cc util.cc var.cc
   watchdog.cc workq.cc)

IF(HAVE_WIN32)
   LIST(APPEND BAREOS_SRCS
      ../../win32/compat/compat.cc
      ../../win32/compat/glob.cc
      ../../win32/compat/print.cc
      ../../win32/compat/winapi.cc
      )
ELSE()
   LIST(APPEND BAREOS_SRCS
     scsi_tapealert.cc)
ENDIF()

#link_directories(${PROJECT_BINARY_DIR}/src/lib/bareos)
add_library(bareos SHARED ${BAREOS_SRCS})

target_link_libraries(bareos
   ${OPENSSL_LIBRARIES} ${PTHREAD_LIBRARIES} ${FASTLZ_LIBRARIES} ${ZLIB_LIBRARIES}
   ${ACL_LIBRARIES} ${LZO2_LIBRARIES} ${CAP_LIBRARIES} ${WRAP_LIBRARIES} ${CAM_LIBRARIES}
   ${WINDOWS_LIBRARIES} ${JANSSON_LIBRARIES} bareoscfg_import_a)

INSTALL(TARGETS bareos DESTINATION ${libdir})

set_target_properties(bareos PROPERTIES VERSION "${VERSION}"
                                      SOVERSION "${SOVERSION}")

ADD_CUSTOM_COMMAND(
   PRE_LINK
   TARGET bareos
   DEPENDS libbareoscfg.def
   COMMAND x86_64-w64-mingw32-dlltool -d ${CMAKE_CURRENT_SOURCE_DIR}/libbareoscfg.def -l libbareoscfg_import_a 
   COMMAND echo ${CMAKE_CURRENT_BINARY_DIR}
   COMMAND cp libbareoscfg_import_a  ${WINDOWS_IMPORT_LIB_DIRECTORY}/libbareoscfg_import_a.a
)

IF(HAVE_WIN32)
   set_target_properties(bareos PROPERTIES DEFINE_SYMBOL "BUILDING_BAREOS_DLL")
ENDIF()

