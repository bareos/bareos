/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * crypto_cache.c Encryption key caching functions
 *
 * Marco van Wieringen, April 2012
 */

#include "include/bareos.h"
#include "crypto_cache.h"

static pthread_mutex_t crypto_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static dlist *cached_crypto_keys = NULL;

static s_crypto_cache_hdr crypto_cache_hdr = {
   "BAREOS Crypto Cache\n",
   1,
   0
};

/*
 * Read the content of the crypto cache from the filesystem.
 */
void ReadCryptoCache(const char *cache_file)
{
   int fd, cnt;
   ssize_t status;
   bool ok = false;
   s_crypto_cache_hdr hdr;
   int hdr_size = sizeof(hdr);
   crypto_cache_entry_t *cce = NULL;

   if ((fd = open(cache_file, O_RDONLY|O_BINARY)) < 0) {
      berrno be;

      Dmsg2(010, "Could not open crypto cache file. %s ERR=%s\n", cache_file, be.bstrerror());
      goto bail_out;
   }

   if ((status = read(fd, &hdr, hdr_size)) != hdr_size) {
      berrno be;

      Dmsg4(010, "Could not read crypto cache file. fd=%d status=%d size=%d: ERR=%s\n",
            fd, (int)status, hdr_size, be.bstrerror());
      goto bail_out;
   }

   if (hdr.version != crypto_cache_hdr.version) {
      Dmsg2(010, "Crypto cache bad hdr version. Wanted %d got %d\n",
            crypto_cache_hdr.version, hdr.version);
      goto bail_out;
   }

   hdr.id[20] = 0;
   if (!bstrcmp(hdr.id, crypto_cache_hdr.id)) {
      Dmsg0(000, "Crypto cache file header id invalid.\n");
      goto bail_out;
   }

   if (!cached_crypto_keys) {
      cached_crypto_keys = New(dlist(cce, &cce->link));
   }

   /*
    * Read as many crypto cache entries as available.
    */
   cnt = 0;
   cce = (crypto_cache_entry_t *)malloc(sizeof(crypto_cache_entry_t));
   while (read(fd, cce, sizeof(crypto_cache_entry_t)) == sizeof(crypto_cache_entry_t)) {
      cnt++;
      cached_crypto_keys->append(cce);
      cce = (crypto_cache_entry_t *)malloc(sizeof(crypto_cache_entry_t));
   }

   /*
    * We always allocate a dangling crypto_cache_entry_t structure in
    * the way that we malloc before the loop and in the loop. So drop
    * the last unused entry.
    */
   free(cce);

   /*
    * Check if we read the number of entries the header said are in the file.
    */
   if (cnt == hdr.nr_entries) {
      ok = true;
      Dmsg2(010, "Crypto cache read %d entries in file %s\n", cnt, cache_file);
   } else {
      Dmsg3(000, "Crypto cache read %d entries while %d entries should be in file %s\n",
            cnt, hdr.nr_entries, cache_file);
   }

bail_out:
   if (fd >= 0) {
      close(fd);
   }

   if (!ok) {
      SecureErase(NULL, cache_file);
      if (cached_crypto_keys) {
         cached_crypto_keys->destroy();
         delete cached_crypto_keys;
         cached_crypto_keys = NULL;
      }
   }

}

void ReadCryptoCache(const char *dir, const char *progname, int port)
{
   POOLMEM *fname = GetPoolMemory(PM_FNAME);

   Mmsg(fname, "%s/%s.%d.cryptoc", dir, progname, port);
   ReadCryptoCache(fname);
   FreePoolMemory(fname);
}

/*
 * Write the content of the crypto cache to the filesystem.
 */
void WriteCryptoCache(const char *cache_file)
{
   int fd;
   bool ok = false;
   crypto_cache_entry_t *cce;

   if (!cached_crypto_keys) {
      return;
   }

   /*
    * Lock the cache.
    */
   P(crypto_cache_lock);

   SecureErase(NULL, cache_file);
   if ((fd = open(cache_file, O_CREAT | O_WRONLY | O_BINARY, 0640)) < 0) {
      berrno be;

      Emsg2(M_ERROR, 0, _("Could not create crypto cache file. %s ERR=%s\n"), cache_file, be.bstrerror());
      goto bail_out;
   }

   crypto_cache_hdr.nr_entries = cached_crypto_keys->size();
   if (write(fd, &crypto_cache_hdr, sizeof(crypto_cache_hdr)) != sizeof(crypto_cache_hdr)) {
      berrno be;

      Dmsg1(000, "Write hdr error: ERR=%s\n", be.bstrerror());
      goto bail_out;
   }

   foreach_dlist(cce, cached_crypto_keys) {
      if (write(fd, cce, sizeof(crypto_cache_entry_t)) != sizeof(crypto_cache_entry_t)) {
         berrno be;

         Dmsg1(000, "Write record error: ERR=%s\n", be.bstrerror());
         goto bail_out;
      }
   }

   ok = true;

bail_out:
   if (fd >= 0) {
      close(fd);
   }

   if (!ok) {
      SecureErase(NULL, cache_file);
   }

   V(crypto_cache_lock);
}

void WriteCryptoCache(const char *dir, const char *progname, int port)
{
   POOLMEM *fname = GetPoolMemory(PM_FNAME);

   Mmsg(fname, "%s/%s.%d.cryptoc", dir, progname, port);
   WriteCryptoCache(fname);
   FreePoolMemory(fname);
}

/*
 * Update the internal cache with new data. When the cache gets
 * modified by a new entry or by expiring old data the return
 * value gives an indication of that.
 * Returns: true - cache was updated with new data.
 *          false - cache was not updated with new data.
 */
bool UpdateCryptoCache(const char *VolumeName, const char *EncryptionKey)
{
   time_t now;
   bool found;
   bool retval = false;
   crypto_cache_entry_t *cce = NULL;
   crypto_cache_entry_t *next_cce;

   /*
    * Lock the cache.
    */
   P(crypto_cache_lock);

   /*
    * See if there are any cached encryption keys.
    */
   if (!cached_crypto_keys) {
      cached_crypto_keys = New(dlist(cce, &cce->link));

      cce = (crypto_cache_entry_t *)malloc(sizeof(crypto_cache_entry_t));
      bstrncpy(cce->VolumeName, VolumeName, sizeof(cce->VolumeName));
      bstrncpy(cce->EncryptionKey, EncryptionKey, sizeof(cce->EncryptionKey));
      cce->added = time(NULL);
      cached_crypto_keys->append(cce);
      retval = true;
   } else {
      found = false;
      now = time(NULL);
      cce = (crypto_cache_entry_t *)cached_crypto_keys->first();
      while (cce) {
         next_cce = (crypto_cache_entry_t *)cached_crypto_keys->next(cce);
         if (bstrcmp(cce->VolumeName, VolumeName)) {
            found = true;

            /*
             * If the key changed update the cached entry.
             */
            if (!bstrcmp(cce->EncryptionKey, EncryptionKey)) {
               bstrncpy(cce->EncryptionKey, EncryptionKey, sizeof(cce->EncryptionKey));
               retval = true;
            }

            cce->added = time(NULL);
            cce = next_cce;
            continue;
         }

         /*
          * Validate the entry.
          * Any entry older the CRYPTO_CACHE_MAX_AGE seconds is removed.
          */
         if ((cce->added + CRYPTO_CACHE_MAX_AGE) < now) {
            cached_crypto_keys->remove(cce);
            retval = true;
         }
         cce = next_cce;
      }

      /*
       * New entry.
       */
      if (!found) {
         cce = (crypto_cache_entry_t *)malloc(sizeof(crypto_cache_entry_t));
         bstrncpy(cce->VolumeName, VolumeName, sizeof(cce->VolumeName));
         bstrncpy(cce->EncryptionKey, EncryptionKey, sizeof(cce->EncryptionKey));
         cce->added = time(NULL);
         cached_crypto_keys->append(cce);
         retval = true;
      }
   }

   V(crypto_cache_lock);
   return retval;
}

/*
 * Lookup a cache entry for the given VolumeName.
 * Returns: string dupped encryption key must be freed by caller.
 */
char *lookup_crypto_cache_entry(const char *VolumeName)
{
   crypto_cache_entry_t *cce;

   if (!cached_crypto_keys) {
      return NULL;
   }

   /*
    * Lock the cache.
    */
   P(crypto_cache_lock);

   foreach_dlist(cce, cached_crypto_keys) {
      if (bstrcmp(cce->VolumeName, VolumeName)) {
         V(crypto_cache_lock);
         return bstrdup(cce->EncryptionKey);
      }
   }

   V(crypto_cache_lock);
   return NULL;
}

/*
 * Dump the content of the crypto cache to a filedescriptor.
 */
void DumpCryptoCache(int fd)
{
   int len;
   PoolMem msg(PM_MESSAGE);
   crypto_cache_entry_t *cce;
   char dt1[MAX_TIME_LENGTH],
        dt2[MAX_TIME_LENGTH];
   unsigned int max_vol_length,
                max_key_length;

   if (!cached_crypto_keys) {
      return;
   }

   /*
    * Lock the cache.
    */
   P(crypto_cache_lock);

   /*
    * See how long the biggest volumename and key are.
    */
   max_vol_length = strlen(_("Volumename"));
   max_key_length = strlen(_("EncryptionKey"));
   foreach_dlist(cce, cached_crypto_keys) {
      if (strlen(cce->VolumeName) > max_vol_length) {
         max_vol_length = strlen(cce->VolumeName);
      }

      if (strlen(cce->EncryptionKey) > max_key_length) {
         max_key_length = strlen(cce->EncryptionKey);
      }
   }

   len = Mmsg(msg, "%-*s %-*s %-20s %-20s\n",
              max_vol_length, _("Volumename"),
              max_key_length, _("EncryptionKey"),
              _("Added"), _("Expires"));
   write(fd, msg.c_str(), len);

   foreach_dlist(cce, cached_crypto_keys) {
      bstrutime(dt1, sizeof(dt1), cce->added);
      bstrutime(dt2, sizeof(dt2), cce->added + CRYPTO_CACHE_MAX_AGE);
      len = Mmsg(msg, "%-*s %-*s %-20s %-20s\n",
                 max_vol_length, cce->VolumeName,
                 max_key_length, cce->EncryptionKey,
                 dt1, dt2);

      write(fd, msg.c_str(), len);
   }

   V(crypto_cache_lock);
}

/*
 * Reset all entries in the cache to the current time.
 */
void ResetCryptoCache(void)
{
   time_t now;
   crypto_cache_entry_t *cce;

   if (!cached_crypto_keys) {
      return;
   }

   now = time(NULL);

   /*
    * Lock the cache.
    */
   P(crypto_cache_lock);

   foreach_dlist(cce, cached_crypto_keys) {
      cce->added = now;
   }

   V(crypto_cache_lock);
}

/*
 * Flush the date from the internal cache.
 */
void FlushCryptoCache(void)
{
   if (!cached_crypto_keys) {
      return;
   }

   /*
    * Lock the cache.
    */
   P(crypto_cache_lock);

   cached_crypto_keys->destroy();
   delete cached_crypto_keys;
   cached_crypto_keys = NULL;

   V(crypto_cache_lock);
}
