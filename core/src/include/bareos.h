/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/**
 * @file
 * main header file to include in all Bareos source
 *
 */
/**
 * @brief BAREOS® - Backup Archiving REcovery Open Sourced
 * @mainpage BAREOS® - Backup Archiving REcovery Open Sourced
 * @section intro_sec Introduction
 *
 * Bareos is open source network backup software licensed AGPLv3.
 *
 * The sourcecode is available on <A HREF="https://github.com/bareos/bareos">
 * github </A>
 *
 * Bareos is being developed and maintained by Bareos GmbH & Co. KG.
 *
 * There is true open source software and as such there is only one version of
 * Bareos. Other than other "open core" software, there is no "enterprise" or
 * "professional" version that contain parts that are licensed proprietary and
 * by doing that lose all advantages that open source software gives you.
 *
 * @section binaries
 * In addition to the source code, Bareos GmbH & Co. KG releases binary packages
 * of every major release. Usually, a major release is released once a year.
 *
 * The binary packages can be downloaded on
 * https://download.bareos.org/
 * and
 * https://download.bareos.com/
 *
 * Maintenance releases are available in sourcecode.
 *
 * Binary packages of maintenance releases are available in the Bareos
 * Subscription repositories.
 *
 * For more information about subscriptions, consulting, funded development and
 * trainings please see http://www.bareos.com
 *
 * @section developer documentation
 * @author Bareos GmbH & Co. KG and others, see
 * <A HREF="https://github.com/bareos/bareos/blob/master/AUTHORS"> AUTHORS </A>
 * file
 *
 * @copyright 2012-2016 Bareos GmbH & Co. KG
 *
 */

#ifndef BAREOS_INCLUDE_BAREOS_H_
#define BAREOS_INCLUDE_BAREOS_H_

#include "include/config.h"

#if defined(HAVE_AIX_OS)
#  define _LINUX_SOURCE_COMPAT 1
#endif

#define _REENTRANT 1
#define _THREAD_SAFE 1
#define _POSIX_PTHREAD_SEMANTICS 1

/* System includes */
#if __has_include(<umem.h>)
#  include <umem.h>
#endif
#if __has_include(<alloca.h>)
#  include <alloca.h>
#endif
#if defined(HAVE_MSVC)
#  include <io.h>
#  include <direct.h>
#  include <process.h>
#  define NOMINMAX  // suppress definition of min() and max() macros on windows
#endif

#include <sys/socket.h>
#if defined(HAVE_WIN32) & !defined(HAVE_MINGW)
#  include <winsock2.h>
#endif

#if defined(HAVE_WIN32)
/* we must include winsock2.h before windows.h, because that would implicitly
 * include winsock.h which is incompatible to winsock2.h
 */
#  include <winsock2.h>
#  include <windows.h>
#  include <ws2tcpip.h>
#endif


// Local Bareos includes. Be sure to put all the system includes before these.
#include "bc_types.h"

#if defined(HAVE_WIN32)
#  include "compat.h"
#endif

#ifndef _GLIBCXX_GTHREAD_USE_WEAK
#  define _GLIBCXX_GTHREAD_USE_WEAK 0
#endif

// Libraries widely used throughout the code
#include "baconfig.h"
#include "lib/lockmgr.h"
#include "lib/mem_pool.h"
#include "lib/btime.h"

#if defined(HAVE_WIN32)
#  include "winapi.h"
#endif

#endif  // BAREOS_INCLUDE_BAREOS_H_
