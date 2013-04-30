/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2012-2012 Free Software Foundation Europe e.V.

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
 *  Marco van Wieringen, April 2012
 */

#ifndef _CRYPTO_CACHE_H
#define _CRYPTO_CACHE_H 1

#define CRYPTO_CACHE_MAX_AGE	60 * 60 * 24 * 60 /* 60 Days */

struct s_crypto_cache_hdr {
   char id[21];
   int32_t version;
   int32_t nr_entries;
};

struct crypto_cache_entry_t {
   dlink link;
   char VolumeName[MAX_NAME_LENGTH];
   char EncryptionKey[MAX_NAME_LENGTH];
   utime_t added;
};

void read_crypto_cache(const char *dir, const char *progname, int port);
void read_crypto_cache(const char *cache_file);
void write_crypto_cache(const char *dir, const char *progname, int port);
void write_crypto_cache(const char *cache_file);
bool update_crypto_cache(const char *VolumeName, const char *EncryptionKey);
char *lookup_crypto_cache_entry(const char *VolumeName);
void flush_crypto_cache(void);

#endif /* _CRYPTO_CACHE_H */
