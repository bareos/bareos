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

#ifndef BAREOS_LIB_CRYPTO_CACHE_H_
#define BAREOS_LIB_CRYPTO_CACHE_H_ 1

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

DLL_IMP_EXP void ReadCryptoCache(const char *dir, const char *progname, int port);
DLL_IMP_EXP void ReadCryptoCache(const char *cache_file);
DLL_IMP_EXP void WriteCryptoCache(const char *dir, const char *progname, int port);
DLL_IMP_EXP void WriteCryptoCache(const char *cache_file);
DLL_IMP_EXP bool UpdateCryptoCache(const char *VolumeName, const char *EncryptionKey);
DLL_IMP_EXP char *lookup_crypto_cache_entry(const char *VolumeName);
DLL_IMP_EXP void DumpCryptoCache(int fd);
DLL_IMP_EXP void ResetCryptoCache(void);
DLL_IMP_EXP void FlushCryptoCache(void);

#endif /* BAREOS_LIB_CRYPTO_CACHE_H_ */
