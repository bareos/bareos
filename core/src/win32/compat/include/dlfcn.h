/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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

#ifndef __DLFCN_H_
#define __DLFCN_H_

#define RTLD_LAZY 0x00001   /* Deferred function binding */
#define RTLD_NOW 0x00002    /* Immediate function binding */
#define RTLD_NOLOAD 0x00004 /* Don't load object */

#define RTLD_GLOBAL 0x00100 /* Export symbols to others */
#define RTLD_LOCAL 0x00000  /* Symbols are only available to group members */

void* dlopen(const char* file, int mode);
void* dlsym(void* handle, const char* name);
int dlclose(void* handle);
char* dlerror(void);

#endif /* __DLFCN_H_ */
