#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2021 Bareos GmbH & Co. KG
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

if(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(HAVE_EXTENDED_ACL 1)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

  # libdir
  if(NOT DEFINED libdir)
    set(libdir ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME})
  endif()

  # includedir
  if(NOT DEFINED includedir)
    set(includedir ${CMAKE_INSTALL_INCLUDEDIR}/${CMAKE_PROJECT_NAME})
  endif()

  # bindir
  if(NOT DEFINED bindir)
    set(bindir ${CMAKE_INSTALL_BINDIR})
    message(STATUS "set bindir to default ${bindir}")
  endif()

  # sbindir
  if(NOT DEFINED sbindir)
    set(sbindir ${CMAKE_INSTALL_SBINDIR})
    message(STATUS "set sbindir to default ${sbindir}")
  endif()

  # sysconfdir
  if(NOT DEFINED sysconfdir)
    set(sysconfdir ${CMAKE_INSTALL_SYSCONFDIR})
  endif()
  set(SYSCONFDIR "\"${sysconfdir}\"")

  # confdir
  if(NOT DEFINED confdir)
    set(confdir "${sysconfdir}/${CMAKE_PROJECT_NAME}")
  endif()

  # configtemplatedir
  if(NOT DEFINED configtemplatedir)
    set(configtemplatedir "lib/${CMAKE_PROJECT_NAME}/defaultconfigs")
  endif()

  # mandir
  if(NOT DEFINED mandir)
    set(mandir ${CMAKE_INSTALL_MANDIR})
  endif()

  # docdir
  if(NOT DEFINED docdir)
    set(docdir default_for_docdir)
  endif()

  # archivedir
  if(NOT DEFINED archivedir)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
      # windows install scripts replace the string "/var/lib/bareos/storage"
      set(archivedir "/var/lib/${CMAKE_PROJECT_NAME}/storage")
    else()
      set(archivedir
          "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}/storage"
      )
    endif()
  endif()

  # backenddir
  if(NOT DEFINED backenddir)
    set(backenddir ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}/backends)
  endif()

  # scriptdir
  if(NOT DEFINED scriptdir)
    set(scriptdir "lib/${CMAKE_PROJECT_NAME}/scripts")
  endif()

  # workingdir
  if(NOT DEFINED workingdir)
    set(workingdir
        "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}"
    )
  endif()
  set(working_dir "${workingdir}")

  # plugindir
  if(NOT DEFINED plugindir)
    set(plugindir ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}/plugins)
  endif()

  # piddir
  if(NOT DEFINED piddir)
    set(piddir ${workingdir})
  endif()

  # bsrdir
  if(NOT DEFINED bsrdir)
    set(bsrdir ${workingdir})
  endif()

  # logdir
  if(NOT DEFINED logdir)
    set(logdir "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/${CMAKE_PROJECT_NAME}")
  endif()

  # datarootdir
  if(NOT DEFINED datarootdir)
    set(datarootdir "${CMAKE_INSTALL_DATAROOTDIR}")
  endif()

  # subsysdir
  if(NOT DEFINED subsysdir)
    set(subsysdir "${workingdir}")
  endif()

else() # IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

  # libdir
  if(NOT DEFINED libdir)
    set(libdir ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME})
  endif()

  # includedir
  if(NOT DEFINED includedir)
    set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${CMAKE_PROJECT_NAME})
  endif()

  # bindir
  if(NOT DEFINED bindir)
    set(bindir ${CMAKE_INSTALL_FULL_BINDIR})
    message(STATUS "set bindir to default ${bindir}")
  endif()

  # sbindir
  if(NOT DEFINED sbindir)
    set(sbindir ${CMAKE_INSTALL_FULL_SBINDIR})
    message(STATUS "set sbindir to default ${sbindir}")
  endif()

  # sysconfdir
  if(NOT DEFINED sysconfdir)
    set(sysconfdir ${CMAKE_INSTALL_FULL_SYSCONFDIR})
  endif()
  set(SYSCONFDIR "\"${sysconfdir}\"")

  # confdir
  if(NOT DEFINED confdir)
    set(confdir "${sysconfdir}/${CMAKE_PROJECT_NAME}")
  endif()

  # configtemplatedir
  if(NOT DEFINED configtemplatedir)
    set(configtemplatedir "${confdir}")
  endif()

  # mandir
  if(NOT DEFINED mandir)
    set(mandir ${CMAKE_INSTALL_FULL_MANDIR})
  endif()

  # docdir
  if(NOT DEFINED docdir)
    set(docdir default_for_docdir)
  endif()

  # archivedir
  if(NOT DEFINED archivedir)
    set(archivedir
        "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}/storage"
    )
  endif()

  # backenddir
  if(NOT DEFINED backenddir)
    set(backenddir ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}/backends)
  endif()

  # scriptdir
  if(NOT DEFINED scriptdir)
    set(scriptdir "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_PROJECT_NAME}/scripts")
  endif()

  # workingdir
  if(NOT DEFINED workingdir)
    set(workingdir
        "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}"
    )
  endif()
  set(working_dir "${workingdir}")

  # plugindir
  if(NOT DEFINED plugindir)
    set(plugindir ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}/plugins)
  endif()

  # piddir
  if(NOT DEFINED piddir)
    set(piddir ${workingdir})
  endif()

  # bsrdir
  if(NOT DEFINED bsrdir)
    set(bsrdir ${workingdir})
  endif()

  # logdir
  if(NOT DEFINED logdir)
    set(logdir "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/${CMAKE_PROJECT_NAME}")
  endif()

  # datarootdir
  if(NOT DEFINED datarootdir)
    set(datarootdir "${CMAKE_INSTALL_FULL_DATAROOTDIR}")
  endif()

  # subsysdir
  if(NOT DEFINED subsysdir)
    set(subsysdir "${workingdir}")
  endif()

endif() # IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

# db_name
if(NOT DEFINED db_name)
  set(db_name "bareos")
endif()

# db_user
if(NOT DEFINED db_user)
  set(db_user "bareos")
endif()

# db_password
if(NOT DEFINED db_password)
  set(db_password "")
endif()

# dir-user
if(NOT DEFINED dir-user)
  set(dir-user "")
endif()
set(dir_user "${dir-user}")

# dir-group
if(NOT DEFINED dir-group)
  set(dir-group "")
endif()
set(dir_group ${dir-group})

# sd-user
if(NOT DEFINED sd-user)
  set(sd-user "")
endif()
set(sd_user ${sd-user})

# sd-group
if(NOT DEFINED sd-group)
  set(sd-group "")
endif()
set(sd_group ${sd-group})

# fd-user
if(NOT DEFINED fd-user)
  set(fd-user "")
endif()
set(fd_user ${fd-user})

# fd-group
if(NOT DEFINED fd-group)
  set(fd-group "")
endif()
set(fd_group ${fd-group})

# dir-password
if(NOT DEFINED dir-password)
  set(dir-password "bareos")
endif()
set(dir_password ${dir-password})

# fd-password
if(NOT DEFINED fd-password)
  set(fd-password "")
endif()
set(fd_password ${fd-password})

# sd-password
if(NOT DEFINED sd-password)
  set(sd-password "")
endif()
set(sd_password ${sd-password})

# mon-dir-password
if(NOT DEFINED mon-dir-password)
  set(mon-dir-password "")
endif()
set(mon_dir_password ${mon-dir-password})

# mon-fd-password
if(NOT DEFINED mon-fd-password)
  set(mon-fd-password "")
endif()
set(mon_fd_password ${mon-fd-password})

# mon-sd-password
if(NOT DEFINED mon-sd-password)
  set(mon-sd-password "")
endif()
set(mon_sd_password ${mon-sd-password})

# basename
if(NOT DEFINED basename)
  set(basename localhost)
endif()

# hostname
if(NOT DEFINED hostname)
  set(hostname localhost)
endif()

# ##############################################################################
# rights
# ##############################################################################
# sbin-perm
if(NOT DEFINED sbin-perm)
  set(sbin-perm 755)
endif()

# ##############################################################################
# bool
# ##############################################################################
# python
if(NOT DEFINED python)
  set(python ON)
endif()

# lockmgr
if(NOT DEFINED lockmgr)
  set(lockmgr OFF)
  set(LOCKMGR 0)
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

# dynamic-cats-backends
option(dynamic-cats-backends "enable dynamic catalog backends" ON)
if(dynamic-cats-backends)
  set(HAVE_DYNAMIC_CATS_BACKENDS 1)
else()
  set(HAVE_DYNAMIC_CATS_BACKENDS 0)
endif()

# dynamic-storage-backends
if(NOT DEFINED dynamic-storage-backends)
  set(dynamic-storage-backends ON)
  set(HAVE_DYNAMIC_SD_BACKENDS 1)
else()
  if(${dynamic-storage-backends})
    set(HAVE_DYNAMIC_SD_BACKENDS 1)
  else()
    set(HAVE_DYNAMIC_SD_BACKENDS 0)
  endif()
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

# ipv6
if((NOT DEFINED ipv6) OR (${ipv6}))
  set(ipv6 ON)
  set(HAVE_IPV6 1)
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

# bat
if(NOT DEFINED bat)
  set(bat OFF)
endif()

# traymonitor
if(NOT DEFINED traymonitor)
  set(HAVE_TRAYMONITOR 0)
endif()

# client-only
if(NOT DEFINED client-only)
  set(client-only OFF)
  set(build_client_only OFF)
else()
  set(client-only ON)
  set(build_client_only ON)
endif()

# postgresql
if(NOT DEFINED postgresql)
  set(postgresql OFF)
endif()

# mysql
if(NOT DEFINED mysql)
  set(mysql OFF)
endif()

# sqlite3
if(NOT DEFINED sqlite3)
  set(sqlite3 OFF)
endif()

if(NOT sqlite3)
  if(NOT mysql)
    if(NOT postgresql)
      if(NOT client-only)
        message(
          FATAL_ERROR
            "No database backend was chosen, please choose at least one from \n"
            " - postgresql (-Dpostgresql=yes)\n"
            " - mysqli     (-Dmysql=yes)     \n"
            " - sqlite3    (-Dsqlite3=yes)   \n"
            " or select client only build     (-Dclient-only=yes)   \n"
        )
      endif()
    endif()
  endif()
endif()

if(NOT mysql)
  set(MYSQL_INCLUDE_DIR "")
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

set(db_backends "")

if(NOT client-only)

  if(${postgresql})
    set(HAVE_POSTGRESQL 1)
    list(APPEND db_backends postgresql)
  endif()

  if(${sqlite3})
    set(HAVE_SQLITE3 1)
    list(APPEND db_backends sqlite3)
  endif()

  if(${mysql})
    set(HAVE_MYSQL 1)
    list(APPEND db_backends mysql)
  endif()

  if(NOT DEFINED default_db_backend)
    # set first entry as default db backend if not already defined
    list(GET db_backends 0 default_db_backend)
  endif()
  # set first backend to be tested by systemtests
  list(GET db_backends 0 db_backend_to_test)
  get_directory_property(hasParent PARENT_DIRECTORY)
  if(hasParent)
    set(db_backend_to_test
        ${db_backend_to_test}
        PARENT_SCOPE
    )
    set(DEFAULT_DB_TYPE
        ${default_db_backend}
        PARENT_SCOPE
    )
  endif()
  set(DEFAULT_DB_TYPE ${default_db_backend})
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

if(DEFINED test-plugin)
  set(HAVE_TEST_PLUGIN 1)
endif()

if(NOT DEFINED developer)
  set(developer OFF)
endif()

if(DEFINED dynamic-debian-package-list)
  set(GENERATE_DEBIAN_CONTROL ON)
else()
  set(GENERATE_DEBIAN_CONTROL OFF)
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

set(HAVE_MYSQL_THREAD_SAFE 1)
set(HAVE_SHA2 1)
set(HAVE_PQISTHREADSAFE 1)

set(_LARGEFILE_SOURCE 1)
set(_LARGE_FILES 1)
set(_FILE_OFFSET_BITS 64)
set(HAVE_COMPRESS_BOUND 1)
set(HAVE_SQLITE3_THREADSAFE 1)

set(PACKAGE_NAME "\"${CMAKE_PROJECT_NAME}\"")
set(PACKAGE_STRING "\"${CMAKE_PROJECT_NAME} ${BAREOS_NUMERIC_VERSION}\"")
set(PACKAGE_VERSION "\"${BAREOS_NUMERIC_VERSION}\"")

set(ENABLE_NLS 1)

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
  set(gfapi_fd_testvolume testvol PARENT_SCOPE)
endif()
