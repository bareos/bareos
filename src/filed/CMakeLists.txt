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


IF(HAVE_WIN32)
   include_directories(
      ${CMAKE_SOURCE_DIR}/src/win32/generic
      ${CMAKE_SOURCE_DIR}/src/win32/filed
      ${CMAKE_SOURCE_DIR}/src/win32/vss/include  ../include . .. ../lmdb/)
ELSE()
   include_directories(. .. ../include ../lmdb)
ENDIF()

include_directories(
   ${PTHREAD_INCLUDE_DIRS}
   ${FASTLZ_INCLUDE_DIRS}
   ${ZLIB_INCLUDE_DIRS}
   ${ACL_INCLUDE_DIRS}
   ${LZO2_INCLUDE_DIRS}
   ${CAP_INCLUDE_DIRS}
   ${WRAP_INCLUDE_DIRS}
)
include_directories(BEFORE ${OPENSSL_INCLUDE_DIRS} ${Readline_INCLUDE_DIR} )

MESSAGE( STATUS "src/filed build: " )
get_property(got_d_include_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
foreach(_dir IN LISTS got_d_include_dirs)
   message(STATUS "  -I ${_dir}")
endforeach()


set(FDSRCS accurate.cc authenticate.cc crypto.cc fd_plugins.cc fileset.cc sd_cmds.cc verify.cc
    accurate_htable.cc backup.cc dir_cmd.cc filed.cc heartbeat.cc socket_server.cc verify_vol.cc
    accurate_lmdb.cc compression.cc estimate.cc filed_conf.cc restore.cc status.cc)

IF(HAVE_WIN32)
   LIST(APPEND FDSRCS
      ../win32/filed/vss.cc
      ../win32/filed/vss_XP.cc
      ../win32/filed/vss_W2K3.cc
      ../win32/filed/vss_Vista.cc
      ../win32/generic/service.cc
      ../win32/generic/main.cc
      )
ENDIF()

IF(HAVE_WIN32)
   LIST(APPEND FDSRCS ../win32/filed/filedres.rc)
ENDIF()



add_executable(bareos-fd ${FDSRCS})

SET(BAREOS_FD_LIBRARIES
         bareosfind bareoscfg bareos
         pthread
         ${LMDB_LIBS}
         )
IF(HAVE_WIN32)
   LIST(APPEND BAREOS_FD_LIBRARIES
      comctl32 bareosstatic
      )

   SET (FD_COMPILE_DEFINITIONS -DWIN32_VSS)

   IF ("${WINDOWS_BITS}" STREQUAL "64")
      LIST(APPEND FD_COMPILE_DEFINITIONS -DHAVE_VSS64)
   ENDIF()

   target_compile_definitions(bareos-fd PRIVATE ${FD_COMPILE_DEFINITIONS})
ENDIF()

target_link_libraries(bareos-fd
   ${BAREOS_FD_LIBRARIES}
         )


INSTALL(TARGETS bareos-fd DESTINATION "${sbindir}")

install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")
install(SCRIPT ${CMAKE_SOURCE_DIR}/cmake/install-filed-configfiles.cmake)
