/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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

/*
 * Define Host machine
 */
#ifdef HAVE_MINGW
extern char win_os[];
#define HOST_OS  "Linux"
#define DISTNAME "Cross-compile"
#ifndef BAREOS
#define BAREOS "Bareos"
#endif
#ifdef _WIN64
# define DISTVER "Win64"
#else
# define DISTVER "Win32"
#endif

#else

extern char WIN_VERSION_LONG[];
extern char WIN_VERSION[];

#define HOST_OS  WIN_VERSION_LONG
#define DISTNAME "MVS"
#define DISTVER  WIN_VERSION

#endif

#define HELPDIR "c://Program Files//Bareos//help"

#define FD_DEFAULT_PORT "9102"
#define SD_DEFAULT_PORT "9103"
#define DIR_DEFAULT_PORT "9101"
#define NDMP_DEFAULT_PORT "10000"
