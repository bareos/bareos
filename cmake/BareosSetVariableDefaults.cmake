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

# check if variables are set via cmdline else set them to default values

# configure variables
#
# strings - directories

# prefix
if(NOT DEFINED prefix)
  set(prefix ${CMAKE_DEFAULT_PREFIX})
endif()

option(USE_RELATIVE_PATHS
       "Compile with relatives path, required for relocatable binaries." OFF
)

if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(HAVE_EXTENDED_ACL 1)
endif()

if(USE_RELATIVE_PATHS)

  set(bindir
      "${CMAKE_INSTALL_BINDIR}"
      CACHE STRING "bin directory"
  )
  set(sbindir
      "${CMAKE_INSTALL_SBINDIR}"
      CACHE STRING "sbin directory"
  )
  set(libdir
      "${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}"
      CACHE STRING "lib directory"
  )
  set(backenddir
      "${libdir}/backends"
      CACHE STRING "directory for Bareos backends"
  )
  set(plugindir
      "${libdir}/plugins"
      CACHE STRING "directory for Bareos plugins"
  )
  set(scriptdir
      "lib/${CMAKE_PROJECT_NAME}/scripts"
      CACHE STRING "directory for Bareos helper scripts"
  )
  set(sysconfdir
      "${CMAKE_INSTALL_SYSCONFDIR}"
      CACHE STRING "system configuration directory"
  )
  set(SYSCONFDIR "\"${sysconfdir}\"")
  set(confdir
      "${sysconfdir}/${CMAKE_PROJECT_NAME}"
      CACHE STRING "Bareos configuration directory"
  )
  set(configtemplatedir
      "${confdir}"
      CACHE STRING "directory for Bareos configuration templates (optional)"
  )
  set(includedir
      "${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PROJECT_NAME}"
      CACHE STRING "include directory"
  )
  set(mandir
      ${CMAKE_INSTALL_MANDIR}
      CACHE STRING "man(uals) directory"
  )
  set(workingdir
      "${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}"
      CACHE STRING "Bareos working directory"
  )
  set(working_dir "${workingdir}")
  set(archivedir
      "${workingdir}/storage"
      CACHE STRING "Bareos archive directory"
  )
  set(subsysdir
      "${workingdir}"
      CACHE STRING "subsys directory"
  )
  set(logdir
      "${CMAKE_INSTALL_LOCALSTATEDIR}/log/${CMAKE_PROJECT_NAME}"
      CACHE STRING "log directory"
  )
  set(datarootdir
      "${CMAKE_INSTALL_DATAROOTDIR}"
      CACHE STRING "data root directory"
  )

else() # if(USE_RELATIVE_PATHS)

  set(bindir
      "${CMAKE_INSTALL_FULL_BINDIR}"
      CACHE STRING "bin directory"
  )
  set(sbindir
      "${CMAKE_INSTALL_FULL_SBINDIR}"
      CACHE STRING "sbin directory"
  )
  set(libdir
      "${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}"
      CACHE STRING "lib directory"
  )
  set(backenddir
      "${libdir}/backends"
      CACHE STRING "directory for Bareos backends"
  )
  set(plugindir
      "${libdir}/plugins"
      CACHE STRING "directory for Bareos plugins"
  )
  set(scriptdir
      "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_PROJECT_NAME}/scripts"
      CACHE STRING "directory for Bareos helper scripts"
  )
  set(sysconfdir
      "${CMAKE_INSTALL_FULL_SYSCONFDIR}"
      CACHE STRING "system configuration directory"
  )
  set(SYSCONFDIR "\"${sysconfdir}\"")
  set(confdir
      "${sysconfdir}/${CMAKE_PROJECT_NAME}"
      CACHE STRING "Bareos configuration directory"
  )
  set(configtemplatedir
      "${confdir}"
      CACHE STRING "directory for Bareos configuration templates (optional)"
  )
  set(includedir
      "${CMAKE_INSTALL_FULL_INCLUDEDIR}/${CMAKE_PROJECT_NAME}"
      CACHE STRING "include directory"
  )
  set(mandir
      ${CMAKE_INSTALL_FULL_MANDIR}
      CACHE STRING "man(uals) directory"
  )
  set(workingdir
      "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}"
      CACHE STRING "Bareos working directory"
  )
  set(working_dir "${workingdir}")
  set(archivedir
      "${workingdir}/storage"
      CACHE STRING "Bareos archive directory"
  )
  set(subsysdir
      "${workingdir}"
      CACHE STRING "subsys directory"
  )
  set(logdir
      "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/${CMAKE_PROJECT_NAME}"
      CACHE STRING "log directory"
  )
  set(datarootdir
      "${CMAKE_INSTALL_FULL_DATAROOTDIR}"
      CACHE STRING "data root directory"
  )

endif() # if(USE_RELATIVE_PATHS)

set(PYTHON_MODULE_PATH
    "${plugindir}"
    CACHE STRING "Default path for Bareos Python modules"
)

# db_name
set(db_name "bareos" CACHE STRING "Bareos database name")

# db_user
set(db_user "bareos" CACHE STRING "Bareos database username")

# db_password
set(db_password "" CACHE STRING "Bareos database password")

set(systemtest_db_user
    "regress"
    CACHE STRING "Database user for the systemtests"
)

set(systemtest_db_password
    ""
    CACHE STRING "Database password for the systemtests"
)

# dir-user
set(dir-user "" CACHE STRING "Bareos Director user")
set(dir_user "${dir-user}")

# dir-group
set(dir-group "" CACHE STRING "Bareos Director group")
set(dir_group ${dir-group})

# sd-user
set(sd-user "" CACHE STRING "Bareos Storage Daemon user")
set(sd_user ${sd-user})

# sd-group
set(sd-group "" CACHE STRING "Bareos Storage Daemon group")
set(sd_group ${sd-group})

# fd-user
set(fd-user "" CACHE STRING "Bareos File Daemon user")
set(fd_user ${fd-user})

# fd-group
set(fd-group "" CACHE STRING "Bareos File Daemon group")
set(fd_group ${fd-group})

# dir-password
set(dir-password "" CACHE STRING "Bareos Director password")
set(dir_password ${dir-password})

# sd-password
set(sd-password "" CACHE STRING "Bareos Storage Daemon password")
set(sd_password ${sd-password})

# fd-password
set(fd-password "" CACHE STRING "Bareos File Daemon password")
set(fd_password ${fd-password})

# mon-dir-password
set(mon-dir-password "" CACHE STRING "Bareos Director monitor password")
set(mon_dir_password ${mon-dir-password})

# mon-fd-password
set(mon-fd-password "" CACHE STRING "Bareos File Daemon monitor password")
set(mon_fd_password ${mon-fd-password})

# mon-sd-password
set(mon-sd-password "" CACHE STRING "Bareos Storage Daemon monitor password")
set(mon_sd_password ${mon-sd-password})

# basename
set(basename "localhost" CACHE STRING "basename")

# hostname
set(hostname "localhost" CACHE STRING "hostname")

# ##############################################################################
# bool
# ##############################################################################
# python
if(NOT DEFINED python)
  set(python ON)
endif()

# readline
if(NOT DEFINED readline)
  set(readline ON)
endif()

# batch-insert
if((NOT DEFINED batch-insert) OR (${batch-insert}))
  set(batch-insert ON)
  set(HAVE_POSTGRESQL_BATCH_FILE_INSERT 1)
  set(USE_BATCH_FILE_INSERT 1)
endif()

# dynamic-storage-backends
if(NOT DEFINED dynamic-storage-backends OR dynamic-storage-backends)
  set(dynamic-storage-backends ON)
  set(HAVE_DYNAMIC_SD_BACKENDS
      1
      CACHE INTERNAL ""
  )
else()
  set(HAVE_DYNAMIC_SD_BACKENDS
      0
      CACHE INTERNAL ""
  )
endif()

# scsi-crypto
if(NOT DEFINED scsi-crypto)
  set(scsi-crypto OFF)
endif()

# lmdb
if(NOT DEFINED lmdb)
  set(lmdb ON)
endif()

# ndmp
if(NOT DEFINED ndmp)
  set(ndmp ON)
endif()

# acl
if(NOT DEFINED acl)
  set(acl ON)
endif()

# xattr
if(NOT DEFINED xattr)
  set(xattr ON)
endif()

# build_ndmjob
if(NOT DEFINED build_ndmjob)
  set(build_ndmjob OFF)
endif()

# traymonitor
if(NOT DEFINED traymonitor)
  set(HAVE_TRAYMONITOR 0)
endif()

# client-only
if(NOT DEFINED client-only)
  set(client-only OFF)
  set(build_client_only OFF)
  set(postgresql ON)
else()
  set(client-only ON)
  set(build_client_only ON)
  set(postgresql OFF)
endif()

if(NOT postgresql)
  set(PostgreSQL_INCLUDE_DIR "")
endif()

if(NOT Readline_LIBRARY)
  set(Readline_LIBRARY "")
endif()

if(NOT LIBZ_FOUND)
  set(ZLIB_LIBRARY "")
endif()

if(NOT client-only)
  if(${postgresql})
    set(HAVE_POSTGRESQL 1)
    set(DEFAULT_DB_TYPE postgresql)
  endif()
endif()

# systemd
if(NOT DEFINED systemd)
  set(systemd OFF)
endif()

# includes
if(NOT DEFINED includes)
  set(includes ON)
endif()

# openssl
if(NOT DEFINED openssl)
  set(openssl ON)
endif()

# ports
if(NOT DEFINED dir_port)
  set(dir_port "9101")
endif()

if(NOT DEFINED fd_port)
  set(fd_port "9102")
endif()

if(NOT DEFINED sd_port)
  set(sd_port "9103")
endif()

if(DEFINED baseport)
  math(EXPR dir_port "${baseport}+0")
  math(EXPR fd_port "${baseport}+1")
  math(EXPR sd_port "${baseport}+2")
endif()

if(NOT DEFINED job_email)
  set(job_email "root")
endif()

if(NOT DEFINED dump_email)
  set(dump_email "root")
endif()

if(NOT DEFINED smtp_host)
  set(smtp_host "localhost")
endif()

if(DEFINED traymonitor)
  set(HAVE_TRAYMONITOR 1)
endif()

if(NOT DEFINED coverage)
  set(coverage OFF)
endif()

# do not destroy bareos-config-lib.sh
set(DB_NAME "@DB_NAME@")
set(DB_USER "@DB_USER@")
set(DB_PASS "@DB_PASS@")
set(DB_VERSION "@DB_VERSION@")

if(${CMAKE_COMPILER_IS_GNUCC})
  set(HAVE_GCC 1)
endif()

set(HAVE_SHA2 1)
set(HAVE_PQISTHREADSAFE 1)

set(_LARGEFILE_SOURCE 1)
set(_LARGE_FILES 1)
set(_FILE_OFFSET_BITS 64)
set(HAVE_COMPRESS_BOUND 1)

set(PACKAGE_NAME "\"${CMAKE_PROJECT_NAME}\"")
set(PACKAGE_STRING "\"${CMAKE_PROJECT_NAME} ${BAREOS_NUMERIC_VERSION}\"")
set(PACKAGE_VERSION "\"${BAREOS_NUMERIC_VERSION}\"")

if(NOT DEFINED ENABLE_NLS)
  set(ENABLE_NLS 1)
endif()

if(HAVE_WIN32)

  if(NOT DEFINED WINDOWS_VERSION)
    set(WINDOWS_VERSION 0x600)
  endif()

  if(NOT DEFINED WINDOWS_BITS)
    set(WINDOWS_BITS 64)
  endif()

endif()

if(DEFINED do-static-code-checks)
  set(DO_STATIC_CODE_CHECKS ${do-static-code-checks})
else()
  set(DO_STATIC_CODE_CHECKS OFF)
endif()

if(DEFINED changer-device AND NOT DEFINED tape-devices)
  message(STATUS "Error: Changer device defined but no tape device defined")
  set(error_odd_devices TRUE)
elseif(NOT DEFINED changer-device AND DEFINED tape-devices)
  message(STATUS "Error: Tape device defined but no changer device defined")
  set(error_odd_devices TRUE)
endif()

if(DEFINED changer-device)
  execute_process(
    COMMAND ls "${changer-device}"
    RESULT_VARIABLE changer_device0_exists
    OUTPUT_QUIET ERROR_QUIET
  )
endif()

if(NOT ${changer_device0_exists} EQUAL 0)
  message(STATUS "Error: Could not find changer-device \"${changer-device}\"")
  set(error_changer0 TRUE)
endif()

if(DEFINED tape-devices)
  list(LENGTH tape-devices number_of_tape_devices)
  if(number_of_tape_devices EQUAL 0)
    message(STATUS "Error: list of tape-devices is empty")
    set(error_tape_devices0 TRUE)
  else()
    foreach(device ${tape-devices})
      execute_process(
        COMMAND ls "${device}"
        RESULT_VARIABLE tape_device_exists
        OUTPUT_QUIET ERROR_QUIET
      )
      if(NOT ${tape_device_exists} EQUAL 0)
        message(STATUS "Error: Could not find tape-device \"${device}\"")
        set(error_tape_devices0 TRUE)
      endif()
    endforeach()
  endif()
endif()

if(error_changer0
   OR error_tape_devices0
   OR error_odd_devices
)
  set(changer_example
      "-D changer-device=\"/dev/tape/by-id/scsi-SSTK_L700_XYZZY_A\""
  )
  set(tape_example
      "-D tape-devices=\"/dev/tape/by-id/scsi-XYZZY_A1-nst;/dev/tape/by-id/scsi-XYZZY_A2-nst;/dev/tape/by-id/scsi-XYZZY_A3-nst;/dev/tape/by-id/scsi-XYZZY_A4-nst\""
  )
  message(
    FATAL_ERROR
      "Errors occurred during cmake run for autochanger test (see above).\n\nUse this Example as guideline:
    ${changer_example} ${tape_example}\n"
  )
else() # no error
  if(DEFINED changer-device AND DEFINED tape-devices)
    set(autochanger_devices_found TRUE)
  endif()
endif()
if(autochanger_devices_found)
  set(AUTOCHANGER_DEVICES_FOUND
      TRUE
      PARENT_SCOPE
  )
  set(CHANGER_DEVICE0
      ${changer-device}
      PARENT_SCOPE
  )

  list(JOIN tape-devices "\" \"" joined_tape_devices_0)
  set(TAPE_DEVICES0
      "\"${joined_tape_devices_0}\""
      PARENT_SCOPE
  )

  message(
    STATUS
      "Found these devices for autochanger test: \"${changer-device}\" \"${joined_tape_devices_0}\""
  )
endif()

# gfapi-fd
if(NOT DEFINED gfapi_fd_testvolume)
  set(gfapi_fd_testvolume
      testvol
      PARENT_SCOPE
  )
endif()

set(DUMP_VARS
    ""
    CACHE STRING "Dump all variables that matches this regex."
)
