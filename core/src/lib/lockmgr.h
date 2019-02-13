/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.

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

#ifndef BAREOS_LIB_LOCKMGR_H_
#define BAREOS_LIB_LOCKMGR_H_ 1

#define PRIO_SD_VOL_LIST      0            /* vol_list_lock */
#define PRIO_SD_ACH_ACCESS    16           /* autochanger lock mutex */

#include <mutex>

void Lmgr_p(pthread_mutex_t *m);
void Lmgr_v(pthread_mutex_t *m);

#define P(x) Lmgr_p(&(x))
#define V(x) Lmgr_v(&(x))
#endif /* BAREOS_LIB_LOCKMGR_H_ */
