/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.

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

/**
 * This code implements a cache with the current mounted filesystems for which
 * its uses the mostly in kernel mount information and export the different OS
 * specific interfaces using a generic interface. We use a hashed cache which is
 * accessed using a hash on the device id and we keep the previous cache hit as
 * most of the time we get called quite a lot with most of the time the same
 * device so keeping the previous cache hit we have a very optimized code path.
 *
 * This interface is implemented for the following OS-es:
 *
 * - Linux
 * - HPUX
 * - DARWIN (OSX)
 * - IRIX
 * - AIX
 * - OSF1 (Tru64)
 * - Solaris
 *
 * Currently we only use this code for Linux and OSF1 based fstype determination.
 * For the other OS-es we can use the fstype present in stat structure on those OS-es.
 *
 * This code replaces the big switch we used before based on SUPER_MAGIC present in
 * the statfs(2) structure but which need extra code for each new filesystem added to
 * the OS and for Linux that tends to be often as it has quite some different filesystems.
 * This new implementation should eliminate this as we use the Linux /proc/mounts in kernel
 * data which automatically adds any new filesystem when added to the kernel.
 */

/*
 *  Marco van Wieringen, August 2009
 */

#include "bacula.h"
#include "mntent_cache.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(HAVE_GETMNTENT)
#if defined(HAVE_LINUX_OS) || \
    defined(HAVE_HPUX_OS) || \
    defined(HAVE_AIX_OS)
#include <mntent.h>
#elif defined(HAVE_SUN_OS)
#include <sys/mnttab.h>
#elif defined(HAVE_HURD_OS)
#include <hurd/paths.h>
#include <mntent.h>
#endif /* HAVE_GETMNTENT */
#elif defined(HAVE_GETMNTINFO)
#if defined(HAVE_OPENBSD_OS)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(HAVE_NETBSD_OS)
#include <sys/types.h>
#include <sys/statvfs.h>
#else
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#endif
#elif defined(HAVE_AIX_OS)
#include <fshelp.h>
#include <sys/vfs.h>
#elif defined(HAVE_OSF1_OS)
#include <sys/mount.h>
#endif

/*
 * Protected data by mutex lock.
 */
static pthread_mutex_t mntent_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static mntent_cache_entry_t *previous_cache_hit = NULL;
static htable *mntent_cache_entry_hashtable = NULL;

/*
 * Last time a rescan of the mountlist took place.
 */
static time_t last_rescan = 0;

static const char *skipped_fs_types[] = {
#if defined(HAVE_LINUX_OS)
   "rootfs",
#endif
   NULL
};

/**
 * Add a new entry to the cache.
 * This function should be called with a write lock on the mntent_cache.
 */
static inline void add_mntent_mapping(uint32_t dev,
                                      const char *special,
                                      const char *mountpoint,
                                      const char *fstype,
                                      const char *mntopts)
{
   int len;
   mntent_cache_entry_t *mce;

   /*
    * Calculate the length of all strings so we can allocate the buffer
    * as one big chunk of memory using the hash_malloc method.
    */
   len = strlen(special) + 1;
   len += strlen(mountpoint) + 1;
   len += strlen(fstype) + 1;
   if (mntopts) {
      len += strlen(mntopts) + 1;
   }

   /*
    * We allocate all members of the hash entry in the same memory chunk.
    */
   mce = (mntent_cache_entry_t *)mntent_cache_entry_hashtable->hash_malloc(sizeof(mntent_cache_entry_t) + len);
   mce->dev = dev;

   mce->special = (char *)mce + sizeof(mntent_cache_entry_t);
   strcpy(mce->special, special);

   mce->mountpoint = mce->special + strlen(mce->special) + 1;
   strcpy(mce->mountpoint, mountpoint);

   mce->fstype = mce->mountpoint + strlen(mce->mountpoint) + 1;
   strcpy(mce->fstype, fstype);

   if (mntopts) {
      mce->mntopts = mce->fstype + strlen(mce->fstype) + 1;
      strcpy(mce->mntopts, mntopts);
   } else {
      mce->mntopts = NULL;
   }

   mntent_cache_entry_hashtable->insert(mce->dev, mce);
}

static inline bool skip_fstype(const char *fstype)
{
   int i;

   for (i = 0; skipped_fs_types[i]; i++) {
      if (bstrcmp(fstype, skipped_fs_types[i]))
         return true;
   }

   return false;
}

/**
 * OS specific function to load the different mntents into the cache.
 * This function should be called with a write lock on the mntent_cache.
 */
static void refresh_mount_cache(void)
{
#if defined(HAVE_GETMNTENT)
   FILE *fp;
   struct stat st;
#if defined(HAVE_LINUX_OS) || \
    defined(HAVE_HPUX_OS) || \
    defined(HAVE_IRIX_OS) || \
    defined(HAVE_AIX_OS) || \
    defined(HAVE_HURD_OS)
   struct mntent *mnt;

#if defined(HAVE_LINUX_OS)
   if ((fp = setmntent("/proc/mounts", "r")) == (FILE *)NULL) {
      if ((fp = setmntent(_PATH_MOUNTED, "r")) == (FILE *)NULL) {
         return;
      }
   }
#elif defined(HAVE_HPUX_OS)
   if ((fp = fopen(MNT_MNTTAB, "r")) == (FILE *)NULL) {
      return;
   }
#elif defined(HAVE_IRIX_OS)
   if ((fp = setmntent(MOUNTED, "r")) == (FILE *)NULL) {
      return;
   }
#elif defined(HAVE_AIX_OS)
   if ((fp = setmntent(MNTTAB, "r")) == (FILE *)NULL) {
      return;
   }
#elif defined(HAVE_HURD_OS)
   if ((fp = setmntent(_PATH_MNTTAB, "r")) == (FILE *)NULL) {
      return;
   }
#endif

   while ((mnt = getmntent(fp)) != (struct mntent *)NULL) {
      if (skip_fstype(mnt->mnt_type)) {
         continue;
      }

      if (stat(mnt->mnt_dir, &st) < 0) {
         continue;
      }

      add_mntent_mapping(st.st_dev, mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts);
   }

   endmntent(fp);
#elif defined(HAVE_SUN_OS)
   struct mnttab mnt;

   if ((fp = fopen(MNTTAB, "r")) == (FILE *)NULL)
      return;

   while (getmntent(fp, &mnt) == 0) {
      if (skip_fstype(mnt.mnt_fstype)) {
         continue;
      }

      if (stat(mnt.mnt_mountp, &st) < 0) {
         continue;
      }

      add_mntent_mapping(st.st_dev, mnt.mnt_special, mnt.mnt_mountp, mnt.mnt_fstype, mnt.mnt_mntopts);
   }

   fclose(fp);
#endif /* HAVE_SUN_OS */
#elif defined(HAVE_GETMNTINFO)
   int cnt;
   struct stat st;
#if defined(HAVE_NETBSD_OS)
   struct statvfs *mntinfo;
#else
   struct statfs *mntinfo;
#endif
#if defined(ST_NOWAIT)
   int flags = ST_NOWAIT;
#elif defined(MNT_NOWAIT)
   int flags = MNT_NOWAIT;
#else
   int flags = 0;
#endif

   if ((cnt = getmntinfo(&mntinfo, flags)) > 0) {
      while (cnt > 0) {
         if (!skip_fstype(mntinfo->f_fstypename) &&
             stat(mntinfo->f_mntonname, &st) == 0) {
            add_mntent_mapping(st.st_dev,
                               mntinfo->f_mntfromname,
                               mntinfo->f_mntonname,
                               mntinfo->f_fstypename,
                               NULL);
         }
         mntinfo++;
         cnt--;
      }
   }
#elif defined(HAVE_AIX_OS)
   int bufsize;
   char *entries, *current;
   struct vmount *vmp;
   struct stat st;
   struct vfs_ent *ve;
   int n_entries, cnt;

   if (mntctl(MCTL_QUERY, sizeof(bufsize), (struct vmount *)&bufsize) != 0) {
      return;
   }

   entries = malloc(bufsize);
   if ((n_entries = mntctl(MCTL_QUERY, bufsize, (struct vmount *) entries)) < 0) {
      free(entries);
      return;
   }

   cnt = 0;
   current = entries;
   while (cnt < n_entries) {
      vmp = (struct vmount *)current;

      if (skip_fstype(ve->vfsent_name)) {
         continue;
      }

      if (stat(current + vmp->vmt_data[VMT_STUB].vmt_off, &st) < 0) {
         continue;
      }

      ve = getvfsbytype(vmp->vmt_gfstype);
      if (ve && ve->vfsent_name) {
         add_mntent_mapping(st.st_dev,
                            current + vmp->vmt_data[VMT_OBJECT].vmt_off,
                            current + vmp->vmt_data[VMT_STUB].vmt_off,
                            ve->vfsent_name,
                            current + vmp->vmt_data[VMT_ARGS].vmt_off);
      }
      current = current + vmp->vmt_length;
      cnt++;
   }
   free(entries);
#elif defined(HAVE_OSF1_OS)
   struct statfs *entries, *current;
   struct stat st;
   int n_entries, cnt;
   int size;

   if ((n_entries = getfsstat((struct statfs *)0, 0L, MNT_NOWAIT)) < 0) {
      return;
   }

   size = (n_entries + 1) * sizeof(struct statfs);
   entries = malloc(size);

   if ((n_entries = getfsstat(entries, size, MNT_NOWAIT)) < 0) {
      free(entries);
      return;
   }

   cnt = 0;
   current = entries;
   while (cnt < n_entries) {
      if (skip_fstype(current->f_fstypename)) {
         continue;
      }

      if (stat(current->f_mntonname, &st) < 0) {
         continue;
      }
      add_mntent_mapping(st.st_dev,
                         current->f_mntfromname,
                         current->f_mntonname,
                         current->f_fstypename,
                         NULL);
      current++;
      cnt++;
   }
   free(stats);
#endif
}

/**
 * Clear the cache (either by flushing it or by initializing it.)
 * This function should be called with a write lock on the mntent_cache.
 */
static void clear_mount_cache()
{
   mntent_cache_entry_t *mce = NULL;

   if (!mntent_cache_entry_hashtable) {
      /**
       * Initialize the hash table.
       */
      mntent_cache_entry_hashtable = (htable *)malloc(sizeof(htable));
      mntent_cache_entry_hashtable->init(mce, &mce->link,
                                         NR_MNTENT_CACHE_ENTRIES,
                                         NR_MNTENT_HTABLE_PAGES);
   } else {
      /**
       * Clear the previous_cache_hit.
       */
      previous_cache_hit = NULL;

      /**
       * Destroy the current content and (re)initialize the hashtable.
       */
      mntent_cache_entry_hashtable->destroy();
      mntent_cache_entry_hashtable->init(mce, &mce->link,
                                         NR_MNTENT_CACHE_ENTRIES,
                                         NR_MNTENT_HTABLE_PAGES);
   }
}

/**
 * Initialize the cache for use.
 * This function should be called with a write lock on the mntent_cache.
 */
static void initialize_mntent_cache(void)
{
   /**
    * Make sure the cache is empty (either by flushing it or by initializing it.)
    */
   clear_mount_cache();

   /**
    * Refresh the cache.
    */
   refresh_mount_cache();
}

/**
 * Flush the current content from the cache.
 */
void flush_mntent_cache(void)
{
   /**
    * Lock the cache.
    */
   P(mntent_cache_lock);

   if (mntent_cache_entry_hashtable) {
      previous_cache_hit = NULL;
      mntent_cache_entry_hashtable->destroy();
      mntent_cache_entry_hashtable = NULL;
   }

   V(mntent_cache_lock);
}

/**
 * Find a mapping in the cache.
 */
mntent_cache_entry_t *find_mntent_mapping(uint32_t dev)
{
   mntent_cache_entry_t *mce = NULL;
   time_t now;

   /**
    * Lock the cache.
    */
   P(mntent_cache_lock);

   /**
    * Shortcut when we get a request for the same device again.
    */
   if (previous_cache_hit && previous_cache_hit->dev == dev) {
      mce = previous_cache_hit;
      goto ok_out;
   }

   /**
    * Initialize the cache if that was not done before.
    */
   if (!mntent_cache_entry_hashtable) {
      initialize_mntent_cache();
      last_rescan = time(NULL);
   } else {
      /**
       * We rescan the mountlist when called when more then
       * MNTENT_RESCAN_INTERVAL seconds have past since the
       * last rescan. This way we never work with data older
       * then MNTENT_RESCAN_INTERVAL seconds.
       */
      now = time(NULL);
      if ((now - last_rescan) > MNTENT_RESCAN_INTERVAL) {
         initialize_mntent_cache();
      }
   }

   mce = (mntent_cache_entry_t *)mntent_cache_entry_hashtable->lookup(dev);

   /**
    * If we fail to lookup the mountpoint its probably a mountpoint added
    * after we did our initial scan. Lets rescan the mountlist and try
    * the lookup again.
    */
   if (!mce) {
      initialize_mntent_cache();
      mce = (mntent_cache_entry_t *)mntent_cache_entry_hashtable->lookup(dev);
   }

   /*
    * Store the last successfull lookup as the previous_cache_hit.
    */
   if (mce) {
      previous_cache_hit = mce;
   }

ok_out:
   V(mntent_cache_lock);
   return mce;
}
