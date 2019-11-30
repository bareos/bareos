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

include(CheckFunctionExists)

check_function_exists(strtoll HAVE_STRTOLL)
check_function_exists(backtrace HAVE_BACKTRACE)
check_function_exists(backtrace_symbols HAVE_BACKTRACE_SYMBOLS)
check_function_exists(bcopy HAVE_BCOPY)
check_function_exists(fchdir HAVE_FCHDIR)
check_function_exists(fchmod HAVE_FCHMOD)
check_function_exists(fchown HAVE_FCHOWN)

check_function_exists(add_proplist_entry HAVE_ADD_PROPLIST_ENTRY)
# CHECK_FUNCTION_EXISTS(chflags HAVE_CHFLAGS)
check_function_exists(closefrom HAVE_CLOSEFROM)
check_function_exists(extattr_get_file HAVE_EXTATTR_GET_FILE)
check_function_exists(extattr_get_link HAVE_EXTATTR_GET_LINK)
check_function_exists(extattr_list_file HAVE_EXTATTR_LIST_FILE)
check_function_exists(extattr_list_link HAVE_EXTATTR_LIST_LINK)
check_function_exists(
  extattr_namespace_to_string HAVE_EXTATTR_NAMESPACE_TO_STRING
)
check_function_exists(extattr_set_file HAVE_EXTATTR_SET_FILE)
check_function_exists(extattr_set_link HAVE_EXTATTR_SET_LINK)
check_function_exists(
  extattr_string_to_namespace HAVE_EXTATTR_STRING_TO_NAMESPACE
)
check_function_exists(fchownat HAVE_FCHOWNAT)
check_function_exists(fdatasync HAVE_FDATASYNC)
check_function_exists(fseeko HAVE_FSEEKO)
check_function_exists(futimens HAVE_FUTIMENS)
check_function_exists(futimes HAVE_FUTIMES)
check_function_exists(futimesat HAVE_FUTIMESAT)
check_function_exists(getaddrinfo HAVE_GETADDRINFO)
check_function_exists(getcwd HAVE_GETCWD)
check_function_exists(getea HAVE_GETEA)
check_function_exists(gethostbyname2 HAVE_GETHOSTBYNAME2)
check_function_exists(gethostid HAVE_GETHOSTID)
check_function_exists(gethostname HAVE_GETHOSTNAME)
check_function_exists(getmntent HAVE_GETMNTENT)
check_function_exists(getmntinfo HAVE_GETMNTINFO)
check_function_exists(getpagesize HAVE_GETPAGESIZE)
check_function_exists(getpid HAVE_GETPID)
check_function_exists(getproplist HAVE_GETPROPLIST)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(getxattr HAVE_GETXATTR)
check_function_exists(get_proplist_entry HAVE_GET_PROPLIST_ENTRY)
check_function_exists(glfs_readdirplus HAVE_GLFS_READDIRPLUS)
check_function_exists(glob HAVE_GLOB)
check_function_exists(inet_ntop HAVE_INET_NTOP)
check_function_exists(inet_pton HAVE_INET_PTON)
check_function_exists(lchmod HAVE_LCHMOD)
check_function_exists(lchown HAVE_LCHOWN)
check_function_exists(lgetea HAVE_LGETEA)
check_function_exists(lgetxattr HAVE_LGETXATTR)
check_function_exists(listea HAVE_LISTEA)
check_function_exists(listxattr HAVE_LISTXATTR)
check_function_exists(llistea HAVE_LLISTEA)
check_function_exists(llistxattr HAVE_LLISTXATTR)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(lsetea HAVE_LSETEA)
check_function_exists(lsetxattr HAVE_LSETXATTR)
check_function_exists(lstat HAVE_LSTAT)
check_function_exists(lutimes HAVE_LUTIMES)
check_function_exists(malloc_trim HAVE_MALLOC_TRIM)
check_function_exists(nanosleep HAVE_NANOSLEEP)
check_function_exists(nl_langinfo HAVE_NL_LANGINFO)
check_function_exists(openat HAVE_OPENAT)
check_function_exists(poll HAVE_POLL)
check_function_exists(posix_fadvise HAVE_POSIX_FADVISE)
check_function_exists(prctl HAVE_PRCTL)
check_function_exists(putenv HAVE_PUTENV)
check_function_exists(readdir_r HAVE_READDIR_R)
check_function_exists(select HAVE_SELECT)
check_function_exists(setea HAVE_SETEA)
check_function_exists(setenv HAVE_SETENV)
check_function_exists(setlocale HAVE_SETLOCALE)
check_function_exists(setpgid HAVE_SETPGID)
check_function_exists(setpgrp HAVE_SETPGRP)
check_function_exists(setproplist HAVE_SETPROPLIST)
check_function_exists(setreuid HAVE_SETREUID)
check_function_exists(setsid HAVE_SETSID)
check_function_exists(setxattr HAVE_SETXATTR)
check_function_exists(hl_loa HAVE_HL_LOA)
check_function_exists(signal HAVE_SIGNAL)
check_function_exists(sizeof_proplist_entry HAVE_SIZEOF_PROPLIST_ENTRY)
check_function_exists(sqlite3_threadsafe HAVE_SQLITE3_THREADSAFE)
check_function_exists(stpcpy HAVE_STPCPY)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strerror_r HAVE_STRERROR_R)
check_function_exists(strftime HAVE_STRFTIME)
check_function_exists(strncmp HAVE_STRNCMP)
check_function_exists(strncpy HAVE_STRNCPY)
check_function_exists(tcgetattr HAVE_TCGETATTR)
check_function_exists(unlinkat HAVE_UNLINKAT)
check_function_exists(utimes HAVE_UTIMES)
check_function_exists(vfprintf HAVE_VFPRINTF)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(vprintf HAVE_VPRINTF)

check_function_exists(dcgettext HAVE_DCGETTEXT)
check_function_exists(fork HAVE_FORK)
check_function_exists(gai_strerror HAVE_GAI_STRERROR)
check_function_exists(gethostbyname_r HAVE_GETHOSTBYNAME_R)
check_function_exists(gettext HAVE_GETTEXT)
check_function_exists(glfs_readdirplus HAVE_GLFS_READDIRPLUS)
