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

IF(${HAVE_WIN32})
   include_directories(${CMAKE_SOURCE_DIR}/src/win32/generic ${CMAKE_SOURCE_DIR}/src/win32/stored  ${CMAKE_SOURCE_DIR}/src/stored/backends ../include .. .)
ELSE()
   include_directories(. .. ../lib ../include ../lmdb ../ndmp)
ENDIF()

if(NOT ${HAVE_DYNAMIC_SD_BACKENDS})

   set (AVAILABLE_DEVICE_API_SRCS
         backends/generic_tape_device.cc
       )

   IF(${HAVE_WIN32})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         ${CMAKE_SOURCE_DIR}/src/win32/stored/backends/win32_fifo_device.cc
         ${CMAKE_SOURCE_DIR}/src/win32/stored/backends/win32_file_device.cc
         ${CMAKE_SOURCE_DIR}/src/win32/stored/backends/win32_tape_device.cc
         )
   ELSE()
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/unix_fifo_device.cc
         backends/object_store_device.cc
         backends/unix_tape_device.cc
         backends/unix_file_device.cc
         )
   ENDIF()

   if(${HAVE_CEPHFS})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/cephfs_device.cc)
   endif()

   if(${HAVE_GFAPI})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/gfapi_device.cc)
   endif()

   if(${HAVE_ELASTO})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/elasto_device.cc)
   endif()

   if(${HAVE_RADOS})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/rados_device.cc)
   endif()

ENDIF()



set (LIBBAREOSSD_SRCS acquire.cc ansi_label.cc askdir.cc autochanger.cc
         block.cc bsr.cc
		   butil.cc crc32.cc dev.cc device.cc ebcdic.cc label.cc lock.cc
		   mount.cc read_record.cc record.cc reserve.cc scan.cc
		   sd_backends.cc sd_plugins.cc sd_stats.cc spool.cc
		   stored_conf.cc vol_mgr.cc wait.cc
         ${AVAILABLE_DEVICE_API_SRCS}
    )


set(SDSRCS append.cc askdir.cc authenticate.cc dir_cmd.cc fd_cmds.cc
         job.cc mac.cc ndmp_tape.cc read.cc sd_cmds.cc sd_stats.cc
         socket_server.cc status.cc stored.cc
)
IF(${HAVE_WIN32})
   LIST(APPEND SDSRCS
      ../win32/generic/main.cc
      ../win32/generic/service.cc
      )
ENDIF()

set(BTAPESRCS btape.cc)
set(BLSSRCS bls.cc)
set(BEXTRACTSRS bextract.cc)
set(BSCANSRCS bscan.cc)
set(BCOPYSRCS bcopy.cc)

set(STORED_RESTYPES autochanger device director ndmp messages storage)

SET(BACKEND_OBJECTS "")   # backend shared objects

if(RADOS_LIBRARIES)
   LIST(APPEND BACKEND_OBJECTS libbareossd-rados)
endif()
if(CEPHFS_LIBRARIES)
   LIST(APPEND BACKEND_OBJECTS libbareossd-cephfs)
endif()
if(GFAPI_LIBRARIES)
   LIST(APPEND BACKEND_OBJECTS libbareossd-gfapi)
endif()
MESSAGE(STATUS "BACKEND_OBJECTS ARE ${BACKEND_OBJECTS}")

add_library(bareossd SHARED ${LIBBAREOSSD_SRCS})

if(NOT ${HAVE_DYNAMIC_SD_BACKENDS})
   target_link_libraries(bareossd bareos bareosstatic ${BACKEND_OBJECTS})
endif()

add_executable(bareos-sd ${SDSRCS})

SET(BAREOS_SD_LIBRARIES
   bareos bareossd bareosfind bareoscfg
   ${NDMP_LIBS}
   )

IF(${HAVE_WIN32})
   LIST(APPEND BAREOS_SD_LIBRARIES
      comctl32
      )
ENDIF()

target_link_libraries(bareos-sd
   ${BAREOS_SD_LIBRARIES}
   )

add_executable(bls ${BLSSRCS})
target_link_libraries(bls
   bareos bareossd bareosfind bareoscfg
   )

add_executable(bextract ${BEXTRACTSRS})
target_link_libraries(bextract
   bareossd bareosfind bareos bareoscfg
    )

add_executable(bscan ${BSCANSRCS})
target_link_libraries(bscan
   bareos bareossd bareosfind bareoscfg bareossql bareoscats
    )

add_executable(btape ${BTAPESRCS})
target_link_libraries(btape
   bareossd bareos bareoscfg
    )

add_executable(bcopy ${BCOPYSRCS})


target_link_libraries(bcopy
   bareossd bareos bareoscfg
    )

INSTALL(TARGETS bareossd DESTINATION ${libdir})

INSTALL(TARGETS bareos-sd bls bextract bscan btape bcopy DESTINATION "${sbindir}")

install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${CMAKE_SOURCE_DIR}\")")
install(SCRIPT ${CMAKE_SOURCE_DIR}/cmake/install-stored-configfiles.cmake)

INSTALL(DIRECTORY DESTINATION "../${archivedir}")

IF( ${HAVE_DYNAMIC_SD_BACKENDS})
add_subdirectory(backends)
ENDIF()
