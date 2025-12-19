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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

add_library(bareos SHARED)
add_library(Bareos::Lib ALIAS bareos)
target_sources(
  bareos
  PRIVATE address_conf.cc
          alist.cc
          attr.cc
          attribs.cc
          bareos_universal_initialiser.cc
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
          tls.cc
          tls_conf.cc
          tls_openssl.cc
          tls_openssl_private.cc
          tree.cc
          try_tls_handshake_as_a_server.cc
          compression.cc
          util.cc
          var.cc
          watchdog.cc
          xxhash.cc
)

if(HAVE_WIN32)
  target_sources(bareos PRIVATE bpipe_win32.cc)
  target_sources(
    bareos PRIVATE ../win32/compat/compat.cc ../win32/compat/glob.cc
                   ../win32/compat/winapi.cc
  )
  target_sources(bareos PRIVATE osinfo_win32.cc)
  target_link_libraries(bareos PUBLIC ws2_32)

  find_package(pcre2 CONFIG REQUIRED)
  target_link_libraries(bareos PUBLIC PCRE2::POSIX)
else()
  target_sources(
    bareos PRIVATE bpipe_unix.cc scsi_tapealert.cc osinfo_unix.cc priv.cc
  )
endif()

target_sources(
  bareos
  PRIVATE bareos_resource.cc
          configured_tls_policy_getter.cc
          lex.cc
          parse_bsr.cc
          res.cc
          parse_conf.cc
          parse_conf_init_resource.cc
          parse_conf_state_machine.cc
          res.cc
          qualified_resource_name_type_converter.cc
)

# this is done to ensure every other object can be cached
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
  PRIVATE version-obj Bareos::Fastlz OpenSSL::SSL ZLIB::ZLIB CLI11::CLI11
          xxHash::xxhash utf8cpp::utf8cpp ${CMAKE_DL_LIBS}
  PUBLIC ${THREADS_THREADS}
)

if(TARGET Bareos::LowLevelScsi)
  target_link_libraries(bareos PRIVATE Bareos::LowLevelScsi)
endif()

if(ENABLE_LZO)
  target_link_libraries(bareos PRIVATE LZO::LZO)
endif()

if(XXHASH_ENABLE_DISPATCH)
  set_source_files_properties(
    xxhash.cc PROPERTIES COMPILE_FLAGS "-DXXHASH_ENABLE_DISPATCH"
  )
endif()

if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=address"
   AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION MATCHES "^1[234]"
)
  # work around compiler/stdlib bug see
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105562#c17
  set_source_files_properties(
    cli.cc PROPERTIES COMPILE_FLAGS "-Wno-maybe-uninitialized"
  )
endif()

if(ENABLE_CAPABILITY)
  target_link_libraries(bareos PRIVATE cap)
endif()

if(TARGET Jansson::Jansson)
  target_link_libraries(bareos PUBLIC Jansson::Jansson)
endif()

install(TARGETS bareos DESTINATION ${libdir})

set_target_properties(
  bareos PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                    SOVERSION "${BAREOS_VERSION_MAJOR}"
)

include(DebugEdit)
