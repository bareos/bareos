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

IF(HAVE_WIN32)
   include_directories(${PROJECT_SOURCE_DIR}/src/win32/generic ${PROJECT_SOURCE_DIR}/src/win32/stored  ${PROJECT_SOURCE_DIR}/src/stored/backends)
ENDIF()

if(NOT ${HAVE_DYNAMIC_SD_BACKENDS})

   set (AVAILABLE_DEVICE_API_SRCS
         backends/generic_tape_device.cc
       )

   IF(HAVE_WIN32)
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         ${PROJECT_SOURCE_DIR}/src/win32/stored/backends/win32_fifo_device.cc
         ${PROJECT_SOURCE_DIR}/src/win32/stored/backends/win32_file_device.cc
         ${PROJECT_SOURCE_DIR}/src/win32/stored/backends/win32_tape_device.cc
         )
   ELSE()
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/unix_fifo_device.cc
         #         backends/droplet_device.cc
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
   if(${HAVE_DROPLET})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/droplet_device.cc)
   endif()
   if(${HAVE_DROPLET})
      LIST(APPEND AVAILABLE_DEVICE_API_SRCS
         backends/chunked_device.cc)
   endif()
ENDIF()



set (LIBBAREOSSD_SRCS acquire.cc ansi_label.cc askdir.cc autochanger.cc
         autochanger_resource.cc block.cc bsr.cc
         butil.cc crc32.cc dev.cc device.cc device_resource.cc
         ebcdic.cc label.cc lock.cc
         mount.cc read_record.cc record.cc reserve.cc scan.cc
         sd_backends.cc sd_plugins.cc sd_stats.cc spool.cc
         stored_globals.cc stored_conf.cc vol_mgr.cc wait.cc
         ${AVAILABLE_DEVICE_API_SRCS}
    )

set(SDSRCS append.cc askdir.cc authenticate.cc dir_cmd.cc fd_cmds.cc
         job.cc mac.cc ndmp_tape.cc read.cc sd_cmds.cc sd_stats.cc
         socket_server.cc status.cc )

IF(HAVE_WIN32)
   LIST(APPEND SDSRCS
      stored.cc
      ../win32/generic/main.cc
      ../win32/generic/service.cc
      ../win32/stored/storedres.rc
      )
ENDIF()

set(BTAPESRCS btape.cc)
IF(HAVE_WIN32)
   LIST(APPEND BTAPESRCS ../win32/stored/btaperes.rc)
ENDIF()

set(BLSSRCS bls.cc)
IF(HAVE_WIN32)
   LIST(APPEND BLSSRCS ../win32/stored/blsres.rc)
ENDIF()

set(BEXTRACTSRS bextract.cc)
IF(HAVE_WIN32)
   LIST(APPEND BEXTRACTSRS ../win32/stored/bextractres.rc)
ENDIF()


set(BSCANSRCS bscan.cc)
IF(HAVE_WIN32)
   LIST(APPEND BSCANSRCS ../win32/stored/bscanres.rc)
ENDIF()

set(BCOPYSRCS bcopy.cc)

set(STORED_RESTYPES autochanger device director ndmp messages storage)

SET(BACKEND_OBJECTS "")   # backend shared objects

if(${HAVE_DYNAMIC_SD_BACKENDS})
   if(${HAVE_RADOS})
      LIST(APPEND BACKEND_OBJECTS libbareossd-rados)
   endif()
   if(${HAVE_CEPHFS})
      LIST(APPEND BACKEND_OBJECTS libbareossd-cephfs)
   endif()
   if(GFAPI_LIBRARIES)
      LIST(APPEND BACKEND_OBJECTS libbareossd-gfapi)
   endif()
   if(${HAVE_DROPLET})
      LIST(APPEND BACKEND_OBJECTS libbareossd-droplet)
   endif()
   if(${HAVE_DROPLET})
      LIST(APPEND BACKEND_OBJECTS libbareossd-chunked)
   endif()
   SET(LIBBAREOSSD_LIBRARIES bareos ${BACKEND_OBJECTS})
   MESSAGE(STATUS "BACKEND_OBJECTS ARE ${BACKEND_OBJECTS}")
ELSE()
   if(${HAVE_DROPLET})
      LIST(APPEND LIBBAREOSSD_LIBRARIES ${DROPLET_LIBRARIES})
   endif()
   if(${HAVE_RADOS})
      LIST(APPEND LIBBAREOSSD_LIBRARIES ${RADOS_LIBRARIES})
   endif()
   if(${HAVE_CEPHFS})
      LIST(APPEND LIBBAREOSSD_LIBRARIES ${CEPHFS_LIBRARIES})
   endif()
   if(${HAVE_GFAPI})
      LIST(APPEND LIBBAREOSSD_LIBRARIES ${GFAPI_LIBRARIES})
   endif()
   if(${HAVE_DROPLET})
      LIST(APPEND LIBBAREOSSD_LIBRARIES ${DROPLET_LIBRARIES})
   endif()
   SET(LIBBAREOSSD_LIBRARIES bareos ${LIBBAREOSSD_LIBRARIES})
   MESSAGE(STATUS "LIBBAREOSSD_LIBRARIES ARE ${LIBBAREOSSD_LIBRARIES}")
ENDIF()

if(${HAVE_DYNAMIC_SD_BACKENDS})
   LIST(APPEND LIBBAREOSSD_SRCS backends/unix_file_device.cc)
ENDIF()

add_library(bareossd SHARED ${LIBBAREOSSD_SRCS})

if(NOT ${HAVE_DYNAMIC_SD_BACKENDS})
   target_link_libraries(bareossd ${LIBBAREOSSD_LIBRARIES})
endif()

IF(HAVE_DARWIN_OS)
  target_link_libraries(bareossd bareos)
ENDIF()

add_library(stored_objects STATIC ${SDSRCS})

add_executable(bareos-sd stored.cc)

SET(BAREOS_SD_LIBRARIES
   bareos bareossd bareosfind 
   ${NDMP_LIBS}
   )

IF(HAVE_WIN32)
   LIST(APPEND BAREOS_SD_LIBRARIES
      comctl32
      )
ENDIF()

target_link_libraries(bareos-sd
   stored_objects
   ${BAREOS_SD_LIBRARIES}
   )

add_executable(bls ${BLSSRCS})
target_link_libraries(bls
   bareos bareossd bareosfind 
   )

add_executable(bextract ${BEXTRACTSRS})
target_link_libraries(bextract
   bareossd bareosfind bareos 
    )

add_executable(bscan ${BSCANSRCS})
target_link_libraries(bscan
   bareos bareossd bareosfind bareossql bareoscats
    )

add_executable(btape ${BTAPESRCS})
target_link_libraries(btape
   bareossd bareos 
    )

add_executable(bcopy ${BCOPYSRCS})


target_link_libraries(bcopy
   bareossd bareos 
    )

INSTALL(TARGETS bareossd DESTINATION ${libdir})

INSTALL(TARGETS bareos-sd bls bextract bscan btape bcopy DESTINATION "${sbindir}")

install(CODE "set(PLUGINS \"${PLUGINS}\")")
install(CODE "set(BACKENDS \"${BACKENDS}\")")
install(CODE "set(configtemplatedir \"${configtemplatedir}\")")
install(CODE "set(SRC_DIR \"${PROJECT_SOURCE_DIR}\")")
install(SCRIPT ${PROJECT_SOURCE_DIR}/cmake/install-stored-configfiles.cmake)

INSTALL(DIRECTORY DESTINATION "../${archivedir}")

IF(${HAVE_DYNAMIC_SD_BACKENDS})
add_subdirectory(backends)
ENDIF()
