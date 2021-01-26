/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
 * Written by Kern Sibbald, February 2008
 */

#ifndef BAREOS_WIN32_COMPAT_INCLUDE_DLFCN_H_
#define BAREOS_WIN32_COMPAT_INCLUDE_DLFCN_H_

#define RTLD_LAZY 0x00001   /* Deferred function binding */
#define RTLD_NOW 0x00002    /* Immediate function binding */
#define RTLD_NOLOAD 0x00004 /* Don't load object */

#define RTLD_GLOBAL 0x00100 /* Export symbols to others */
#define RTLD_LOCAL 0x00000  /* Symbols are only available to group members */

void* dlopen(const char* file, int mode);
void* dlsym(void* handle, const char* name);
int dlclose(void* handle);
char* dlerror(void);

#endif  // BAREOS_WIN32_COMPAT_INCLUDE_DLFCN_H_
