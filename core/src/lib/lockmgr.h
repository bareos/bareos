/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_LOCKMGR_H_
#define BAREOS_LIB_LOCKMGR_H_

#define PRIO_SD_VOL_LIST 0    /* vol_list_lock */
#define PRIO_SD_ACH_ACCESS 16 /* autochanger lock mutex */

#include <mutex>
#include <cstddef>

#define lock_mutex(m) lock_mutex_impl((m), __FILE__, __LINE__)
#define unlock_mutex(m) unlock_mutex_impl((m), __FILE__, __LINE__)

void lock_mutex_impl(pthread_mutex_t& m, const char* file, std::size_t line);
void unlock_mutex_impl(pthread_mutex_t& m, const char* file, std::size_t line);

#endif  // BAREOS_LIB_LOCKMGR_H_
