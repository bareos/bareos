/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2009 Free Software Foundation Europe e.V.

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

#ifndef MUTEX_LIST_H
#define MUTEX_LIST_H 1

/*
 * Use this list to manage lock order and protect the BAREOS from
 * race conditions and dead locks
 */

#define PRIO_SD_DEV_ACQUIRE   4            /* dev.acquire_mutex */
#define PRIO_SD_DEV_ACCESS    5            /* dev.mutex_ */
#define PRIO_SD_VOL_LIST      0            /* vol_list_lock */
#define PRIO_SD_READ_VOL_LIST 12           /* read_vol_list */
#define PRIO_SD_DEV_SPOOL     14           /* dev.spool_mutex */
#define PRIO_SD_ACH_ACCESS    16           /* autochanger lock mutex */

#endif
