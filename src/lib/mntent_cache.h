/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2010 Free Software Foundation Europe e.V.
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
 *  Marco van Wieringen, August 2009
 */

#ifndef _MNTENT_CACHE_H
#define _MNTENT_CACHE_H 1

/*
 * Don't use the mountlist data when its older than this amount
 * of seconds but perform a rescan of the mountlist.
 */
#define MNTENT_RESCAN_INTERVAL		1800

/*
 * Initial size of number of hash entries we expect in the cache.
 * If more are needed the hash table will grow as needed.
 */
#define NR_MNTENT_CACHE_ENTRIES		256

/*
 * Number of pages to allocate for the big_buffer used by htable.
 */
#define NR_MNTENT_HTABLE_PAGES		32

struct mntent_cache_entry_t {
   dlink link;
   uint32_t dev;
   char *special;
   char *mountpoint;
   char *fstype;
   char *mntopts;
   int reference_count;
   bool validated;
   bool destroyed;
};

mntent_cache_entry_t *find_mntent_mapping(uint32_t dev);
void release_mntent_mapping(mntent_cache_entry_t *mce);
DLL_IMP_EXP void flush_mntent_cache(void);

#endif /* _MNTENT_CACHE_H */
