#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2023 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

include_directories(
  ${OPENSSL_INCLUDE_DIR} ${PTHREAD_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS}
  ${ACL_INCLUDE_DIRS} ${LZO2_INCLUDE_DIRS} ${CAP_INCLUDE_DIRS}
)

set(BAREOS_SRCS
    address_conf.cc
    alist.cc
    attr.cc
    attribs.cc
    backtrace.cc
    base64.cc
    berrno.cc
    bget_msg.cc
    binflate.cc
    bnet_server_tcp.cc
    bnet.cc
    bnet_network_dump.cc
    bnet_network_dump_private.cc
    bpipe.cc
    breg.cc
    bregex.cc
    bsnprintf.cc
    bsock.cc
    bsock_tcp.cc
    bstringlist.cc
    bsys.cc
    btime.cc
    btimers.cc
    cbuf.cc
    cli.cc
    connection_pool.cc
    cram_md5.cc
    crypto.cc
    crypto_cache.cc
    crypto_openssl.cc
    crypto_wrap.cc
    daemon.cc
    devlock.cc
    dlist_string.cc
    edit.cc
    fnmatch.cc
    guid_to_name.cc
    hmac.cc
    htable.cc
    jcr.cc
    lockmgr.cc
    mem_pool.cc
    message.cc
    messages_resource.cc
    mntent_cache.cc
    monotonic_buffer.cc
    output_formatter.cc
    output_formatter_resource.cc
    passphrase.cc
    path_list.cc
    plugins.cc
    bpoll.cc
    priv.cc
    recent_job_results_list.cc
    rblist.cc
    runscript.cc
    rwlock.cc
    scan.cc
    scsi_crypto.cc
    scsi_lli.cc
    serial.cc
    signal.cc
    status_packet.cc
    thread_list.cc
    thread_specific_data.cc
    timer_thread.cc
    tls.cc
    tls_conf.cc
    tls_openssl.cc
    tls_openssl_crl.cc
    tls_openssl_private.cc
    tree.cc
    try_tls_handshake_as_a_server.cc
    compression.cc
    util.cc
    var.cc
    watchdog.cc
    watchdog_timer.cc
    xxhash.cc
)

if(HAVE_WIN32)
  list(APPEND BAREOS_SRCS ../win32/compat/compat.cc ../win32/compat/glob.cc
       ../win32/compat/winapi.cc
  )
  set_source_files_properties(
    ../win32/compat/winapi.cc ../win32/compat/service.cc
    ../win32/generic/service.cc osinfo_win32.cc
  )

else()
  list(APPEND BAREOS_SRCS scsi_tapealert.cc)
endif()

set(BAREOSCFG_SRCS
    bareos_resource.cc
    configured_tls_policy_getter.cc
    ini.cc
    lex.cc
    parse_bsr.cc
    res.cc
    parse_conf.cc
    parse_conf_init_resource.cc
    parse_conf_state_machine.cc
    res.cc
    qualified_resource_name_type_converter.cc
)
list(APPEND BAREOS_SRCS ${BAREOSCFG_SRCS})

if(HAVE_WIN32)
  list(APPEND BAREOS_SRCS osinfo_win32.cc)
else()
  list(APPEND BAREOS_SRCS osinfo.cc)
endif()

add_library(bareos SHARED ${BAREOS_SRCS} $<TARGET_OBJECTS:version-obj>)

add_library(version-obj OBJECT version.cc)
target_compile_definitions(
  version-obj
  PRIVATE -DBAREOS_VERSION="${BAREOS_FULL_VERSION}"
          -DBAREOS_DATE="${DATE}"
          -DBAREOS_SHORT_DATE="${BAREOS_SHORT_DATE}"
          -DBAREOS_PROG_DATE_TIME="${BAREOS_PROG_DATE_TIME}"
          -DBAREOS_YEAR="${BAREOS_YEAR}"
)

target_link_libraries(
  bareos
  PRIVATE bareosfastlz ${OPENSSL_LIBRARIES} Threads::Threads ${ZLIB_LIBRARIES}
          ${LZO2_LIBRARIES} ${CAM_LIBRARIES} CLI11::CLI11 xxHash::xxhash
)

if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=address"
   AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION MATCHES "^12"
)
  # work around compiler/stdlib bug see
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105562#c17
  set_source_files_properties(
    cli.cc PROPERTIES COMPILE_FLAGS "-Wno-maybe-uninitialized"
  )
endif()

if(ENABLE_CAPABILITY)
  target_link_libraries(bareos PRIVATE ${CAP_LIBRARIES})
endif()

if(TARGET Jansson::Jansson)
  target_link_libraries(bareos PUBLIC Jansson::Jansson)
endif()

install(TARGETS bareos DESTINATION ${libdir})

set_target_properties(
  bareos PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                    SOVERSION "${BAREOS_VERSION_MAJOR}"
)

if(HAVE_WIN32)
  set_target_properties(bareos PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
  target_link_libraries(bareos PUBLIC ws2_32)
endif()
