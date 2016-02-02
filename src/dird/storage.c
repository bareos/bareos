/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
 * BAREOS Director -- Storage specific routines.
 *
 * Kern Sibbald, October MM
 *
 * Extracted from other source files by Marco van Wieringen, February 2016
 */

#include "bareos.h"
#include "dird.h"

/* Forward referenced functions */

/*
 * Copy the storage definitions from an alist to the JCR
 */
void copy_rwstorage(JCR *jcr, alist *storage, const char *where)
{
   if (jcr->JobReads()) {
      copy_rstorage(jcr, storage, where);
   }
   copy_wstorage(jcr, storage, where);
}

/*
 * Set storage override.
 * Releases any previous storage definition.
 */
void set_rwstorage(JCR *jcr, USTORERES *store)
{
   if (!store) {
      Jmsg(jcr, M_FATAL, 0, _("No storage specified.\n"));
      return;
   }
   if (jcr->JobReads()) {
      set_rstorage(jcr, store);
   }
   set_wstorage(jcr, store);
}

void free_rwstorage(JCR *jcr)
{
   free_rstorage(jcr);
   free_wstorage(jcr);
}

/*
 * Copy the storage definitions from an alist to the JCR
 */
void copy_rstorage(JCR *jcr, alist *storage, const char *where)
{
   if (storage) {
      STORERES *store;
      if (jcr->res.rstorage) {
         delete jcr->res.rstorage;
      }
      jcr->res.rstorage = New(alist(10, not_owned_by_alist));
      foreach_alist(store, storage) {
         jcr->res.rstorage->append(store);
      }
      if (!jcr->res.rstore_source) {
         jcr->res.rstore_source = get_pool_memory(PM_MESSAGE);
      }
      pm_strcpy(jcr->res.rstore_source, where);
      if (jcr->res.rstorage) {
         jcr->res.rstore = (STORERES *)jcr->res.rstorage->first();
      }
   }
}

/*
 * Set storage override.
 * Remove all previous storage.
 */
void set_rstorage(JCR *jcr, USTORERES *store)
{
   STORERES *storage;

   if (!store->store) {
      return;
   }
   if (jcr->res.rstorage) {
      free_rstorage(jcr);
   }
   if (!jcr->res.rstorage) {
      jcr->res.rstorage = New(alist(10, not_owned_by_alist));
   }
   jcr->res.rstore = store->store;
   if (!jcr->res.rstore_source) {
      jcr->res.rstore_source = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(jcr->res.rstore_source, store->store_source);
   foreach_alist(storage, jcr->res.rstorage) {
      if (store->store == storage) {
         return;
      }
   }
   /* Store not in list, so add it */
   jcr->res.rstorage->prepend(store->store);
}

void free_rstorage(JCR *jcr)
{
   if (jcr->res.rstorage) {
      delete jcr->res.rstorage;
      jcr->res.rstorage = NULL;
   }
   jcr->res.rstore = NULL;
}

/*
 * Copy the storage definitions from an alist to the JCR
 */
void copy_wstorage(JCR *jcr, alist *storage, const char *where)
{
   if (storage) {
      STORERES *st;
      if (jcr->res.wstorage) {
         delete jcr->res.wstorage;
      }
      jcr->res.wstorage = New(alist(10, not_owned_by_alist));
      foreach_alist(st, storage) {
         Dmsg1(100, "wstorage=%s\n", st->name());
         jcr->res.wstorage->append(st);
      }
      if (!jcr->res.wstore_source) {
         jcr->res.wstore_source = get_pool_memory(PM_MESSAGE);
      }
      pm_strcpy(jcr->res.wstore_source, where);
      if (jcr->res.wstorage) {
         jcr->res.wstore = (STORERES *)jcr->res.wstorage->first();
         Dmsg2(100, "wstore=%s where=%s\n", jcr->res.wstore->name(), jcr->res.wstore_source);
      }
   }
}

/*
 * Set storage override.
 * Remove all previous storage.
 */
void set_wstorage(JCR *jcr, USTORERES *store)
{
   STORERES *storage;

   if (!store->store) {
      return;
   }
   if (jcr->res.wstorage) {
      free_wstorage(jcr);
   }
   if (!jcr->res.wstorage) {
      jcr->res.wstorage = New(alist(10, not_owned_by_alist));
   }
   jcr->res.wstore = store->store;
   if (!jcr->res.wstore_source) {
      jcr->res.wstore_source = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(jcr->res.wstore_source, store->store_source);
   Dmsg2(50, "wstore=%s where=%s\n", jcr->res.wstore->name(), jcr->res.wstore_source);
   foreach_alist(storage, jcr->res.wstorage) {
      if (store->store == storage) {
         return;
      }
   }

   /*
    * Store not in list, so add it
    */
   jcr->res.wstorage->prepend(store->store);
}

void free_wstorage(JCR *jcr)
{
   if (jcr->res.wstorage) {
      delete jcr->res.wstorage;
      jcr->res.wstorage = NULL;
   }
   jcr->res.wstore = NULL;
}

/*
 * For NDMP backup we can setup the backup to run to a NDMP instance
 * of the same Storage Daemon that also does native native backups.
 * This way a Normal Storage Daemon can perform NDMP protocol based
 * saves and restores.
 */
void set_paired_storage(JCR *jcr)
{
   STORERES *store, *pstore;

   switch (jcr->getJobType()) {
   case JT_BACKUP:
      /*
       * For a backup we look at the write storage.
       */
      if (jcr->res.wstorage) {
         /*
          * Setup the jcr->res.wstorage to point to all paired_storage
          * entries of all the storage currently in the jcrres.->wstorage.
          * Save the original list under jcr->res.pstorage.
          */
         jcr->res.pstorage = jcr->res.wstorage;
         jcr->res.wstorage = New(alist(10, not_owned_by_alist));
         foreach_alist(store, jcr->res.pstorage) {
            if (store->paired_storage) {
               Dmsg1(100, "wstorage=%s\n", store->paired_storage->name());
               jcr->res.wstorage->append(store->paired_storage);
            }
         }

         /*
          * Swap the actual jcr->res.wstore to point to the paired storage entry.
          * We save the actual storage entry in pstore which is for restore
          * in the free_paired_storage() function.
          */
         store = jcr->res.wstore;
         if (store->paired_storage) {
            jcr->res.wstore = store->paired_storage;
            jcr->res.pstore = store;
         }
      } else {
         Jmsg(jcr, M_FATAL, 0,
              _("No write storage, don't know how to setup paired storage\n"));
      }
      break;
   case JT_RESTORE:
      /*
       * For a restores we look at the read storage.
       */
      if (jcr->res.rstorage) {
         /*
          * Setup the jcr->res.pstorage to point to all paired_storage
          * entries of all the storage currently in the jcr->res.rstorage.
          */
         jcr->res.pstorage = New(alist(10, not_owned_by_alist));
         foreach_alist(pstore, jcr->res.rstorage) {
            store = (STORERES *)GetNextRes(R_STORAGE, NULL);
            while (store) {
               if (store->paired_storage == pstore) {
                  break;
               }

               store = (STORERES *)GetNextRes(R_STORAGE, (RES *)store);
            }

            /*
             * See if we found a store that has the current pstore as
             * its paired storage.
             */
            if (store) {
               jcr->res.pstorage->append(store);

               /*
                * If the current processed pstore is also the current
                * entry in jcr->res.rstore update the jcr->pstore to point
                * to this storage entry.
                */
               if (pstore == jcr->res.rstore) {
                  jcr->res.pstore = store;
               }
            }
         }
      } else {
         Jmsg(jcr, M_FATAL, 0,
              _("No read storage, don't know how to setup paired storage\n"));
      }
      break;
   case JT_MIGRATE:
   case JT_COPY:
      /*
       * For a migrate or copy we look at the read storage.
       */
      if (jcr->res.rstorage) {
         /*
          * Setup the jcr->res.rstorage to point to all paired_storage
          * entries of all the storage currently in the jcr->res.rstorage.
          * Save the original list under jcr->res.pstorage.
          */
         jcr->res.pstorage = jcr->res.rstorage;
         jcr->res.rstorage = New(alist(10, not_owned_by_alist));
         foreach_alist(store, jcr->res.pstorage) {
            if (store->paired_storage) {
               Dmsg1(100, "rstorage=%s\n", store->paired_storage->name());
               jcr->res.rstorage->append(store->paired_storage);
            }
         }

         /*
          * Swap the actual jcr->res.rstore to point to the paired storage entry.
          * We save the actual storage entry in pstore which is for restore
          * in the free_paired_storage() function.
          */
         store = jcr->res.rstore;
         if (store->paired_storage) {
            jcr->res.rstore = store->paired_storage;
            jcr->res.pstore = store;
         }
      } else {
         Jmsg(jcr, M_FATAL, 0,
              _("No read storage, don't know how to setup paired storage\n"));
      }
      break;
   default:
      Jmsg(jcr, M_FATAL, 0,
           _("Unknown Job Type %s, don't know how to setup paired storage\n"),
           job_type_to_str(jcr->getJobType()));
      break;
   }
}

/*
 * This performs an undo of the actions the set_paired_storage() function
 * performed. We reset the storage write storage back to its original
 * and remove the paired storage override if any.
 */
void free_paired_storage(JCR *jcr)
{
   if (jcr->res.pstorage) {
      switch (jcr->getJobType()) {
      case JT_BACKUP:
         /*
          * For a backup we look at the write storage.
          */
         if (jcr->res.wstorage) {
            /*
             * The jcr->res.wstorage contain a set of paired storages.
             * We just delete it content and swap back to the real master storage.
             */
            delete jcr->res.wstorage;
            jcr->res.wstorage = jcr->res.pstorage;
            jcr->res.pstorage = NULL;
            jcr->res.wstore = jcr->res.pstore;
            jcr->res.pstore = NULL;
         }
         break;
      case JT_RESTORE:
         /*
          * The jcr->res.rstorage contain a set of paired storages.
          * For the read we created a list of alternative storage which we
          * can just drop now.
          */
         delete jcr->res.pstorage;
         jcr->res.pstorage = NULL;
         jcr->res.pstore = NULL;
         break;
      case JT_MIGRATE:
      case JT_COPY:
         /*
          * For a migrate or copy we look at the read storage.
          */
         if (jcr->res.rstorage) {
            /*
             * The jcr->res.rstorage contains a set of paired storages.
             * We just delete it content and swap back to the real master storage.
             */
            delete jcr->res.rstorage;
            jcr->res.rstorage = jcr->res.pstorage;
            jcr->res.pstorage = NULL;
            jcr->res.rstore = jcr->res.pstore;
            jcr->res.pstore = NULL;
         }
         break;
      default:
         Jmsg(jcr, M_FATAL, 0,
              _("Unknown Job Type %s, don't know how to free paired storage\n"),
              job_type_to_str(jcr->getJobType()));
         break;
      }
   }
}

/*
 * Check if every possible storage has paired storage associated.
 */
bool has_paired_storage(JCR *jcr)
{
   STORERES *store;

   switch (jcr->getJobType()) {
   case JT_BACKUP:
      /*
       * For a backup we look at the write storage.
       */
      if (jcr->res.wstorage) {
         foreach_alist(store, jcr->res.wstorage) {
            if (!store->paired_storage) {
               return false;
            }
         }
      } else {
         Jmsg(jcr, M_FATAL, 0,
               _("No write storage, don't know how to check for paired storage\n"));
         return false;
      }
      break;
   case JT_RESTORE:
   case JT_MIGRATE:
   case JT_COPY:
      if (jcr->res.rstorage) {
         foreach_alist(store, jcr->res.rstorage) {
            if (!store->paired_storage) {
               return false;
            }
         }
      } else {
         Jmsg(jcr, M_FATAL, 0,
               _("No read storage, don't know how to check for paired storage\n"));
         return false;
      }
      break;
   default:
      Jmsg(jcr, M_FATAL, 0,
           _("Unknown Job Type %s, don't know how to free paired storage\n"),
           job_type_to_str(jcr->getJobType()));
      return false;
   }

   return true;
}

#define MAX_TRIES 6 * 360   /* 6 hours (10 sec intervals) */

/*
 * Change the read storage resource for the current job.
 */
bool select_next_rstore(JCR *jcr, bootstrap_info &info)
{
   USTORERES ustore;

   if (bstrcmp(jcr->res.rstore->name(), info.storage)) {
      return true;                 /* Same SD nothing to change */
   }

   if (!(ustore.store = (STORERES *)GetResWithName(R_STORAGE,info.storage))) {
      Jmsg(jcr, M_FATAL, 0,
           _("Could not get storage resource '%s'.\n"), info.storage);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   /*
    * We start communicating with a new storage daemon so close the
    * old connection when it is still open.
    */
   if (jcr->store_bsock) {
      jcr->store_bsock->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
   }

   /*
    * Release current read storage and get a new one
    */
   dec_read_store(jcr);
   free_rstorage(jcr);
   set_rstorage(jcr, &ustore);
   jcr->setJobStatus(JS_WaitSD);

   /*
    * Wait for up to 6 hours to increment read stoage counter
    */
   for (int i = 0; i < MAX_TRIES; i++) {
      /*
       * Try to get read storage counter incremented
       */
      if (inc_read_store(jcr)) {
         jcr->setJobStatus(JS_Running);
         return true;
      }
      bmicrosleep(10, 0);          /* Sleep 10 secs */
      if (job_canceled(jcr)) {
         free_rstorage(jcr);
         return false;
      }
   }

   /*
    * Failed to inc_read_store()
    */
   free_rstorage(jcr);
   Jmsg(jcr, M_FATAL, 0,
      _("Could not acquire read storage lock for \"%s\""), info.storage);
   return false;
}

void storage_status(UAContext *ua, STORERES *store, char *cmd)
{
   switch (store->Protocol) {
   case APT_NATIVE:
      return do_native_storage_status(ua, store, cmd);
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      return do_ndmp_storage_status(ua, store, cmd);
   default:
      break;
   }
}

/*
 * Simple comparison function for binary insert of vol_list_t
 */
int storage_compare_vol_list_entry(void *e1, void *e2)
{
   vol_list_t *v1, *v2;

   v1 = (vol_list_t *)e1;
   v2 = (vol_list_t *)e2;

   ASSERT(v1);
   ASSERT(v2);

   if (v1->Index == v2->Index) {
      return 0;
   } else {
      return (v1->Index < v2->Index) ? -1 : 1;
   }
}

/*
 * Generic routine to get the content of a storage device.
 */
dlist *get_vol_list_from_storage(UAContext *ua, STORERES *store, bool listall, bool scan)
{
   switch (store->Protocol) {
   case APT_NATIVE:
      return native_get_vol_list(ua, store, listall, scan);
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
   default:
      return (dlist *)NULL;
   }
}

int get_num_slots(UAContext *ua, STORERES *store)
{
   int slots;

   /*
    * See if we can use the cached number of slots.
    */
   if (store->slots) {
      return store->slots;
   }

   switch (store->Protocol) {
   case APT_NATIVE:
      slots = native_get_num_slots(ua, store);
      break;
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
   default:
      return 0;
   }

   store->slots = slots;
   return slots;
}

int get_num_drives(UAContext *ua, STORERES *store)
{
   int drives;

   /*
    * See if we can use the cached number of drives.
    */
   if (store->drives) {
      return store->drives;
   }

   switch (store->Protocol) {
   case APT_NATIVE:
      drives = native_get_num_drives(ua, store);
      break;
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
   default:
      return 0;
   }

   store->drives = drives;
   return drives;
}

bool transfer_volume(UAContext *ua, STORERES *store, int src_slot, int dst_slot)
{
   switch (store->Protocol) {
   case APT_NATIVE:
      return native_transfer_volume(ua, store, src_slot, dst_slot);
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
   default:
      return false;
   }
}

bool do_autochanger_volume_operation(UAContext *ua, STORERES *store, const char *operation,
                                     int drive, int slot)
{
   switch (store->Protocol) {
   case APT_NATIVE:
      return native_autochanger_volume_operation(ua, store, operation, drive, slot);
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
   default:
      return false;
   }
}

/*
 * See if a specific slot is loaded in one of the drives.
 */
vol_list_t *vol_is_loaded_in_drive(STORERES *store, dlist *vol_list, int slot)
{
   vol_list_t *vl;

   vl = (vol_list_t *)vol_list->first();
   while (vl) {
      switch (vl->Type) {
      case slot_type_drive:
         Dmsg2(100, "Checking drive %d for loaded volume == %d\n", vl->Slot, vl->Loaded);
         if (vl->Loaded == slot) {
            return vl;
         }
         break;
      default:
         break;
      }
      vl = (vol_list_t *)vol_list->next((void *)vl);
   }

   return NULL;
}

/*
 * Destroy the volume list returned from get_vol_list_from_storage()
 */
void storage_free_vol_list(dlist *vol_list)
{
   vol_list_t *vl;

   foreach_dlist(vl, vol_list) {
      if (vl->VolName) {
         free(vl->VolName);
      }
   }
   vol_list->destroy();
   delete vol_list;
}
