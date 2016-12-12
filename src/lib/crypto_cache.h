/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
/*
 *  Marco van Wieringen, April 2012
 */
/**
 * @file
 * crypto cache definition
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
void dump_crypto_cache(int fd);
void reset_crypto_cache(void);
void flush_crypto_cache(void);

#endif /* _CRYPTO_CACHE_H */
