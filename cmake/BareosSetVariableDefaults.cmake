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


# check if variables are set via cmdline else set them to default values

## configure variables
##
## strings - directories

# prefix
IF(NOT DEFINED prefix)
   set (prefix ${CMAKE_DEFAULT_PREFIX})
ENDIF()

# libdir
IF(NOT DEFINED libdir)
   set(libdir ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME})
ENDIF()

# includedir
IF(NOT DEFINED includedir)
   set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${CMAKE_PROJECT_NAME})
ENDIF()

# bindir
IF(NOT DEFINED bindir)
   set(bindir ${CMAKE_INSTALL_FULL_BINDIR})
   MESSAGE(STATUS "set bindir to default ${bindir}")
ENDIF()


# sbindir
IF(NOT DEFINED sbindir)
   set(sbindir ${CMAKE_INSTALL_FULL_SBINDIR})
   MESSAGE(STATUS "set sbindir to default ${sbindir}")
ENDIF()

# sysconfdir
IF(NOT DEFINED sysconfdir)
   set(sysconfdir ${CMAKE_INSTALL_FULL_SYSCONFDIR})
ENDIF()
set(SYSCONFDIR "\"${sysconfdir}\"")

# confdir
IF(NOT DEFINED confdir)
   set(confdir "${sysconfdir}/${CMAKE_PROJECT_NAME}")
ENDIF()

# configtemplatedir
IF(NOT DEFINED configtemplatedir)
   set(configtemplatedir "${confdir}")
ENDIF()

# mandir
IF(NOT DEFINED mandir)
   set(mandir ${CMAKE_INSTALL_FULL_MANDIR})
ENDIF()

# docdir
IF(NOT DEFINED docdir)
   SET(docdir default_for_docdir)
ENDIF()

# htmldir
IF(NOT DEFINED htmldir)
   SET(htmldir default_for_htmldir)
ENDIF()

# archivedir
IF(NOT DEFINED archivedir)
   set(archivedir "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}/storage")
ENDIF()

# backenddir
IF(NOT DEFINED backenddir)
   set(backenddir ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}/backends)
ENDIF()

# scriptdir
IF(NOT DEFINED scriptdir)
   set(scriptdir "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_PROJECT_NAME}/scripts")
ENDIF()

# workingdir
IF(NOT DEFINED workingdir)
   set(workingdir "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME}")
ENDIF()
set(working_dir "${workingdir}")

# plugindir
IF(NOT DEFINED plugindir)
   set(plugindir ${CMAKE_INSTALL_FULL_LIBDIR}/${CMAKE_PROJECT_NAME}/plugins)
ENDIF()

# piddir
IF(NOT DEFINED piddir)
   SET(piddir ${workingdir})
ENDIF()

# bsrdir
IF(NOT DEFINED bsrdir)
   SET(bsrdir ${workingdir})
ENDIF()

# logdir
IF(NOT DEFINED logdir)
   set(logdir "${CMAKE_INSTALL_LOCALSTATEDIR}/log/${CMAKE_PROJECT_NAME}")
ENDIF()

# datarootdir
IF(NOT DEFINED datarootdir)
   set(datarootdir "${CMAKE_INSTALL_FULL_DATAROOTDIR}")
ENDIF()

# subsysdir
IF(NOT DEFINED subsysdir)
   set(subsysdir "${CMAKE_INSTALL_PREFIX}/var/lock/subsys")
ENDIF()

# db_name
IF(NOT DEFINED db_name)
   SET(db_name "bareos")
ENDIF()

# db_user
IF(NOT DEFINED db_user)
   SET(db_user "bareos")
ENDIF()

# db_password
IF(NOT DEFINED db_password)
   SET(db_password "")
ENDIF()

# dir-user
IF(NOT DEFINED dir-user)
   SET(dir-user "")
ENDIF()
SET(dir_user "${dir-user}")

# dir-group
IF(NOT DEFINED dir-group)
   SET(dir-group "")
ENDIF()
set(dir_group ${dir-group})

# sd-user
IF(NOT DEFINED sd-user)
   SET(sd-user "")
ENDIF()
SET(sd_user ${sd-user})

# sd-group
IF(NOT DEFINED sd-group)
   SET(sd-group "")
ENDIF()
SET(sd_group ${sd-group})

# fd-user
IF(NOT DEFINED fd-user)
   SET(fd-user "")
ENDIF()
SET(fd_user ${fd-user})

# fd-group
IF(NOT DEFINED fd-group)
   SET(fd-group "")
ENDIF()
SET(fd_group ${fd-group})


# dir-password
IF(NOT DEFINED dir-password)
   SET(dir-password "bareos")
ENDIF()
SET(dir_password ${dir-password})

# fd-password
IF(NOT DEFINED fd-password)
   SET(fd-password "")
ENDIF()
SET(fd_password ${fd-password})

# sd-password
IF(NOT DEFINED sd-password)
   SET(sd-password "")
ENDIF()
SET(sd_password ${sd-password})

# mon-dir-password
IF(NOT DEFINED mon-dir-password)
   SET(mon-dir-password "")
ENDIF()
SET(mon_dir_password ${mon-dir-password})

# mon-fd-password
IF(NOT DEFINED mon-fd-password)
   SET(mon-fd-password "")
ENDIF()
SET(mon_fd_password ${mon-fd-password})

# mon-sd-password
IF(NOT DEFINED mon-sd-password)
   SET(mon-sd-password "")
ENDIF()
SET(mon_sd_password ${mon-sd-password} )

# basename
IF(NOT DEFINED basename)
   SET(basename localhost)
ENDIF()

# hostname
IF(NOT DEFINED hostname)
   SET(hostname localhost)
ENDIF()

##########
## rights
##########
# sbin-perm
IF(NOT DEFINED sbin-perm)
   SET(sbin-perm 755)
ENDIF()



###########
## bool
###########
# python
IF(NOT DEFINED python)
   SET(python ON)
ENDIF()

# lockmgr
IF(NOT DEFINED lockmgr)
   SET(lockmgr ON)
   set(LOCKMGR 1)
ENDIF()

# smartalloc
IF(NOT DEFINED smartalloc)
   SET(smartalloc ON)
   set(SMARTALLOC 1)
ENDIF()

# smartalloc
IF(NOT smartalloc)
   SET(smartalloc OFF)
   set(SMARTALLOC 0)
ELSE()
   SET(smartalloc ON)
   set(SMARTALLOC 1)
ENDIF()

# disable-conio
IF(NOT DEFINED disable-conio)
   SET(disable-conio ON)
ENDIF()

# readline
IF(NOT DEFINED readline)
   SET(readline ON)
ENDIF()

# batch-insert
IF((NOT DEFINED batch-insert) OR (${batch-insert}))
   SET(batch-insert ON)
   SET(HAVE_POSTGRESQL_BATCH_FILE_INSERT 1)
   SET(USE_BATCH_FILE_INSERT 1)
ENDIF()

# dynamic-cats-backends
IF(NOT DEFINED dynamic-cats-backends)
   SET(dynamic-cats-backends OFF)
   SET(HAVE_DYNAMIC_CATS_BACKENDS 0)
ELSE()
   SET(dynamic-cats-backends ON)
   SET(HAVE_DYNAMIC_CATS_BACKENDS 1)
ENDIF()

# dynamic-storage-backends
IF(NOT DEFINED dynamic-storage-backends)
   # OR (${dynamic-storage-backends}))
   SET(dynamic-storage-backends OFF)
   SET(HAVE_DYNAMIC_SD_BACKENDS 0)
ELSE()
   SET(dynamic-storage-backends ON)
   SET(HAVE_DYNAMIC_SD_BACKENDS 1)
ENDIF()

# scsi-crypto
IF(NOT DEFINED scsi-crypto)
   SET(scsi-crypto OFF)
ENDIF()

# lmdb
IF(NOT DEFINED lmdb)
   SET(lmdb ON)
ENDIF()

# ndmp
IF(NOT DEFINED ndmp)
   SET(ndmp ON)
ENDIF()

# ipv6
IF((NOT DEFINED ipv6) OR (${ipv6}))
   SET(ipv6 ON)
   SET(HAVE_IPV6 1)
ENDIF()

# acl
IF(NOT DEFINED acl)
   SET(acl ON)
ENDIF()

# xattr
IF(NOT DEFINED xattr)
   SET(xattr ON)
ENDIF()

# bat
IF(NOT DEFINED bat)
   SET(bat OFF)
ENDIF()

# traymonitor
IF(NOT DEFINED traymonitor)
   SET(HAVE_TRAYMONITOR 0)
ENDIF()

# client-only
IF(NOT DEFINED client-only)
   SET(client-only OFF)
   SET(build_client_only OFF)
ELSE()
   SET(client-only ON)
   SET(build_client_only ON)
ENDIF()

# postgresql
IF(NOT DEFINED postgresql)
   SET(postgresql OFF)
ENDIF()

# mysql
IF(NOT DEFINED mysql)
   SET(mysql OFF)
ENDIF()

# sqlite3
IF(NOT DEFINED sqlite3)
   SET(sqlite3 OFF)
ENDIF()


IF(NOT sqlite3)
   IF(NOT mysql)
      IF(NOT postgresql)
         IF(NOT client-only)
         MESSAGE(FATAL_ERROR "No database backend was chosen, please choose at least one from \n"
          " - postgresql (-Dpostgresql=yes)\n"
          " - mysqli     (-Dmysql=yes)     \n"
          " - sqlite3    (-Dsqlite3=yes)   \n"
          " or select client only build     (-Dclient-only=yes)   \n"
          )
         ENDIF()
      ENDIF()
   ENDIF()
ENDIF()

IF(NOT mysql)
   set(MYSQL_INCLUDE_DIR "")
endif()

IF(NOT postgresql)
 set(PostgreSQL_INCLUDE_DIR "")
endif()

if(NOT Readline_LIBRARY)
 set(Readline_LIBRARY "")
endif()

if(NOT LIBZ_FOUND)
  set(ZLIB_LIBRARY "")
endif()

set(db_backends "")

IF(NOT client-only)
   if(${sqlite3})
      set(HAVE_SQLITE3 1)
      list(APPEND db_backends sqlite3)
   endif()

   if(${postgresql})
      set(HAVE_POSTGRESQL 1)
      list(APPEND db_backends postgresql)
   endif()

   if(${mysql})
      set(HAVE_MYSQL 1)
      list(APPEND db_backends mysql)
   endif()

   # set first entry as default db backend
   LIST(GET db_backends 0 default_db_backend)
endif()

# tcp-wrappers
IF(NOT DEFINED tcp-wrappers)
   SET(tcp-wrappers OFF)
ENDIF()

# systemd
IF(NOT DEFINED systemd)
   SET(systemd OFF)
ENDIF()

# includes
IF(NOT DEFINED includes)
   SET(includes ON)
ENDIF()

# openssl
IF(NOT DEFINED openssl)
   SET(openssl ON)
ENDIF()



# ports
IF(NOT DEFINED dir_port)
   SET(dir_port "9101")
ENDIF()

IF(NOT DEFINED fd_port)
   SET(fd_port "9102")
ENDIF()

IF(NOT DEFINED sd_port)
   SET(sd_port "9103")
ENDIF()

IF(DEFINED baseport)
   MATH(EXPR dir_port "${baseport}+0")
   MATH(EXPR fd_port "${baseport}+1")
   MATH(EXPR sd_port "${baseport}+2")
ENDIF()


IF(NOT DEFINED job_email)
   SET(job_email "root@localhost")
ENDIF()

IF(NOT DEFINED dump_email)
   SET(dump_email "root@localhost")
ENDIF()

IF(NOT DEFINED smtp_host)
   SET(smtp_host "root@localhost")
ENDIF()

IF(DEFINED traymonitor)
   SET(HAVE_TRAYMONITOR 1)
ENDIF()

IF(DEFINED test-plugin)
   SET(HAVE_TEST_PLUGIN 1)
ENDIF()

# do not destroy bareos-config-lib.sh
SET(DB_NAME "@DB_NAME@")
SET(DB_USER "@DB_USER@")
SET(DB_PASS "@DB_PASS@")
SET(DB_VERSION "@DB_VERSION@")


IF (${CMAKE_COMPILER_IS_GNUCC})
   SET(HAVE_GCC 1)
ENDIF()

set(lld "\"lld\"")
set(llu "\"llu\"")

SET(HAVE_MYSQL_THREAD_SAFE 1)
SET(HAVE_POSIX_SIGNALS 1)
SET(HAVE_SHA2 1)
SET(HAVE_PQISTHREADSAFE 1)
SET(HAVE_PQ_COPY 1)

SET(PROTOTYPES 1)
SET(RETSIGTYPE void)
SET(SETPGRP_VOID 1)
SET(SSTDC_HEADERS 1)


SET(_LARGEFILE_SOURCE 1)
SET(_LARGE_FILES 1)
SET(_FILE_OFFSET_BITS 64)
SET(HAVE_COMPRESS_BOUND 1)
SET(STDC_HEADERS 1)
SET(HAVE_SQLITE3_THREADSAFE 1)
SET(FSTYPE_MNTENT 1)

SET(PACKAGE_BUGREPORT "\"\"")
SET(PACKAGE_NAME "\"${CMAKE_PROJECT_NAME}\"")
SET(PACKAGE_STRING "\"${CMAKE_PROJECT_NAME} ${VERSION}\"")
SET(PACKAGE_TARNAME "\"\"" )
SET(PACKAGE_URL "\"\"")
SET(PACKAGE_VERSION "\"${VERSION}\"")

set(ENABLE_NLS 1)

IF(HAVE_WIN32)

   IF(NOT DEFINED WINDOWS_VERSION)
      SET(WINDOWS_VERSION 0x600)
   ENDIF()

   IF(NOT DEFINED WINDOWS_BITS)
      SET(WINDOWS_BITS 64)
   ENDIF()

ENDIF()
