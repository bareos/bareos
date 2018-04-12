/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2012-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * wrapper around config.h from autoconf or the windows
 *                 config headers so its never included more then once.
 */

#ifndef _HOSTCONFIG_H
#define _HOSTCONFIG_H 1

#if defined(HAVE_WIN32)
#if defined(HAVE_MINGW)
#include "mingwconfig.h"
#else
#include "winconfig.h"
#endif
#else
#include "config.h"
#endif

#endif
