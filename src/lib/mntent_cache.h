/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2009-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

/*
 *  Marco van Wieringen, August 2009
 */

#ifndef _MNTENT_CACHE_H
#define _MNTENT_CACHE_H 1

/*
 * Don't use the mountlist data when its older then this amount
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
   hlink link;
   uint32_t dev;
   char *special;
   char *mountpoint;
   char *fstype;
   char *mntopts;
};

mntent_cache_entry_t *find_mntent_mapping(uint32_t dev);
void flush_mntent_cache(void);

#endif /* _MNTENT_CACHE_H */
