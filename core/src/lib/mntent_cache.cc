/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2011 Free Software Foundation Europe e.V.
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

/**
 * This code implements a cache with the current mounted filesystems for which
 * its uses the mostly in kernel mount information and export the different OS
 * specific interfaces using a generic interface. We use a linked list cache
 * which is accessed using a binary search on the device id and we keep the
 * previous cache hit as most of the time we get called quite a lot with most
 * of the time the same device so keeping the previous cache hit we have a
 * very optimized code path.
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

#include "include/bareos.h"
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
static dlist *mntent_cache_entries = NULL;

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
 * Simple comparison function for binary search and insert.
 */
static int compare_mntent_mapping(void *e1, void *e2)
{
   mntent_cache_entry_t *mce1, *mce2;

   mce1 = (mntent_cache_entry_t *)e1;
   mce2 = (mntent_cache_entry_t *)e2;

   if (mce1->dev == mce2->dev) {
      return 0;
   } else {
      return (mce1->dev < mce2->dev) ? -1 : 1;
   }
}

/**
 * Free the members of the mntent_cache structure not the structure itself.
 */
static inline void destroy_mntent_cache_entry(mntent_cache_entry_t *mce)
{
   if (mce->mntopts) {
      free(mce->mntopts);
   }
   free(mce->fstype);
   free(mce->mountpoint);
   free(mce->special);
}

/**
 * Add a new entry to the cache.
 * This function should be called with a write lock on the mntent_cache.
 */
static mntent_cache_entry_t *add_mntent_mapping(uint32_t dev,
                                                const char *special,
                                                const char *mountpoint,
                                                const char *fstype,
                                                const char *mntopts)
{
   mntent_cache_entry_t *mce;

   mce = (mntent_cache_entry_t *)malloc(sizeof(mntent_cache_entry_t));
   memset(mce, 0, sizeof(mntent_cache_entry_t));
   mce->dev = dev;
   mce->special = bstrdup(special);
   mce->mountpoint = bstrdup(mountpoint);
   mce->fstype = bstrdup(fstype);
   if (mntopts) {
      mce->mntopts = bstrdup(mntopts);
   }

   mntent_cache_entries->binary_insert(mce, compare_mntent_mapping);

   return mce;
}

/**
 * Update an entry in the cache.
 * This function should be called with a write lock on the mntent_cache.
 */
static mntent_cache_entry_t *update_mntent_mapping(uint32_t dev,
                                                   const char *special,
                                                   const char *mountpoint,
                                                   const char *fstype,
                                                   const char *mntopts)
{
   mntent_cache_entry_t lookup, *mce;

   lookup.dev = dev;
   mce = (mntent_cache_entry_t *)mntent_cache_entries->binary_search(&lookup, compare_mntent_mapping);
   if (mce) {
      /*
       * See if the info changed.
       */
      if (!bstrcmp(mce->special, special)) {
         free(mce->special);
         mce->special = bstrdup(special);
      }

      if (!bstrcmp(mce->mountpoint, mountpoint)) {
         free(mce->mountpoint);
         mce->mountpoint = bstrdup(mountpoint);
      }

      if (!bstrcmp(mce->fstype, fstype)) {
         free(mce->fstype);
         mce->fstype = bstrdup(fstype);
      }

      if (!bstrcmp(mce->mntopts, mntopts)) {
         free(mce->mntopts);
         mce->mntopts = bstrdup(mntopts);
      }
   } else {
      mce = add_mntent_mapping(dev, special, mountpoint, fstype, mntopts);
   }

   mce->validated = true;
   return mce;
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
static void refresh_mount_cache(mntent_cache_entry_t *handle_entry(uint32_t dev,
                                                                   const char *special,
                                                                   const char *mountpoint,
                                                                   const char *fstype,
                                                                   const char *mntopts))
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

      handle_entry(st.st_dev,
                   mnt->mnt_fsname,
                   mnt->mnt_dir,
                   mnt->mnt_type,
                   mnt->mnt_opts);
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

      handle_entry(st.st_dev,
                   mnt.mnt_special,
                   mnt.mnt_mountp,
                   mnt.mnt_fstype,
                   mnt.mnt_mntopts);
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
            handle_entry(st.st_dev,
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
         handle_entry(st.st_dev,
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
      handle_entry(st.st_dev,
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
 * Initialize the cache for use.
 * This function should be called with a write lock on the mntent_cache.
 */
static inline void initialize_mntent_cache(void)
{
   mntent_cache_entry_t *mce = NULL;

   mntent_cache_entries = New(dlist(mce, &mce->link));

   /**
    * Refresh the cache.
    */
   refresh_mount_cache(add_mntent_mapping);
}

/**
 * Repopulate the cache with new data.
 * This function should be called with a write lock on the mntent_cache.
 */
static void repopulate_mntent_cache(void)
{
   mntent_cache_entry_t *mce, *next_mce;

   /**
    * Reset validated flag on all entries in the cache.
    */
   foreach_dlist(mce, mntent_cache_entries) {
      mce->validated = false;
   }

   /**
    * Refresh the cache.
    */
   refresh_mount_cache(update_mntent_mapping);

   /**
    * Remove any entry that is not validated in
    * the previous refresh run.
    */
   mce = (mntent_cache_entry_t *)mntent_cache_entries->first();
   while (mce) {
      next_mce = (mntent_cache_entry_t *)mntent_cache_entries->next(mce);
      if (!mce->validated) {
         /**
          * Invalidate the previous cache hit if we are removing it.
          */
         if (previous_cache_hit == mce) {
            previous_cache_hit = NULL;
         }

         /**
          * See if this is an outstanding entry.
          * e.g. when reference_count > 0 set
          * the entry to destroyed and remove it
          * from the list. But don't free the data
          * yet. The put_mntent_mapping function will
          * handle these dangling entries.
          */
         if (mce->reference_count == 0) {
            mntent_cache_entries->remove(mce);
            destroy_mntent_cache_entry(mce);
            free(mce);
         } else {
            mce->destroyed = true;
            mntent_cache_entries->remove(mce);
         }
      }
      mce = next_mce;
   }
}

/**
 * Flush the current content from the cache.
 */
void flush_mntent_cache(void)
{
   mntent_cache_entry_t *mce;

   /**
    * Lock the cache.
    */
   P(mntent_cache_lock);

   if (mntent_cache_entries) {
      previous_cache_hit = NULL;
      foreach_dlist(mce, mntent_cache_entries) {
         destroy_mntent_cache_entry(mce);
      }
      mntent_cache_entries->destroy();
      delete mntent_cache_entries;
      mntent_cache_entries = NULL;
   }

   V(mntent_cache_lock);
}

/**
 * Release a mntent mapping reference returned
 * by a successfull call to find_mntent_mapping.
 */
void release_mntent_mapping(mntent_cache_entry_t *mce)
{
   /**
    * Lock the cache.
    */
   P(mntent_cache_lock);

   mce->reference_count--;

   /**
    * See if this entry is a dangling entry.
    */
   if (mce->reference_count == 0 && mce->destroyed) {
      destroy_mntent_cache_entry(mce);
      free(mce);
   }

   V(mntent_cache_lock);
}

/**
 * Find a mapping in the cache.
 */
mntent_cache_entry_t *find_mntent_mapping(uint32_t dev)
{
   mntent_cache_entry_t lookup, *mce = NULL;
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
      mce->reference_count++;
      goto ok_out;
   }

   /**
    * Initialize the cache if that was not done before.
    */
   if (!mntent_cache_entries) {
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
         repopulate_mntent_cache();
         last_rescan = time(NULL);
      }
   }

   lookup.dev = dev;
   mce = (mntent_cache_entry_t *)mntent_cache_entries->binary_search(&lookup, compare_mntent_mapping);

   /**
    * If we fail to lookup the mountpoint its probably a mountpoint added
    * after we did our initial scan. Lets rescan the mountlist and try
    * the lookup again.
    */
   if (!mce) {
      repopulate_mntent_cache();
      mce = (mntent_cache_entry_t *)mntent_cache_entries->binary_search(&lookup, compare_mntent_mapping);
   }

   /**
    * Store the last successfull lookup as the previous_cache_hit.
    * And increment the reference count.
    */
   if (mce) {
      previous_cache_hit = mce;
      mce->reference_count++;
   }

ok_out:
   V(mntent_cache_lock);
   return mce;
}
