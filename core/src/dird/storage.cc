/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October MM
 * Extracted from other source files by Marco van Wieringen, February 2016
 */
/**
 * @file
 * Storage specific routines.
 */

#include "include/bareos.h"
#include "dird/dird.h"
#include "dird/dird_globals.h"
#include "dird/sd_cmds.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#if HAVE_NDMP
#include "ndmp/ndmagents.h"
#endif

#include "dird/ndmp_dma_storage.h"
#include "dird/storage.h"

namespace directordaemon {

/* Forward referenced functions */

/**
 * Copy the storage definitions from an alist to the JobControlRecord
 */
void CopyRwstorage(JobControlRecord* jcr, alist* storage, const char* where)
{
  if (jcr->JobReads()) { CopyRstorage(jcr, storage, where); }
  CopyWstorage(jcr, storage, where);
}

/**
 * Set storage override.
 * Releases any previous storage definition.
 */
void SetRwstorage(JobControlRecord* jcr, UnifiedStorageResource* store)
{
  if (!store) {
    Jmsg(jcr, M_FATAL, 0, _("No storage specified.\n"));
    return;
  }
  if (jcr->JobReads()) { SetRstorage(jcr, store); }
  SetWstorage(jcr, store);
}

void FreeRwstorage(JobControlRecord* jcr)
{
  FreeRstorage(jcr);
  FreeWstorage(jcr);
}

/**
 * Copy the storage definitions from an alist to the JobControlRecord
 */
void CopyRstorage(JobControlRecord* jcr, alist* storage, const char* where)
{
  if (storage) {
    StorageResource* store = nullptr;
    if (jcr->res.read_storage_list) { delete jcr->res.read_storage_list; }
    jcr->res.read_storage_list = new alist(10, not_owned_by_alist);
    foreach_alist (store, storage) {
      jcr->res.read_storage_list->append(store);
    }
    if (!jcr->res.rstore_source) {
      jcr->res.rstore_source = GetPoolMemory(PM_MESSAGE);
    }
    PmStrcpy(jcr->res.rstore_source, where);
    if (jcr->res.read_storage_list) {
      jcr->res.read_storage =
          (StorageResource*)jcr->res.read_storage_list->first();
    }
  }
}

/**
 * Set storage override.
 * Remove all previous storage.
 */
void SetRstorage(JobControlRecord* jcr, UnifiedStorageResource* store)
{
  StorageResource* storage = nullptr;

  if (!store->store) { return; }
  if (jcr->res.read_storage_list) { FreeRstorage(jcr); }
  if (!jcr->res.read_storage_list) {
    jcr->res.read_storage_list = new alist(10, not_owned_by_alist);
  }
  jcr->res.read_storage = store->store;
  if (!jcr->res.rstore_source) {
    jcr->res.rstore_source = GetPoolMemory(PM_MESSAGE);
  }
  PmStrcpy(jcr->res.rstore_source, store->store_source);
  foreach_alist (storage, jcr->res.read_storage_list) {
    if (store->store == storage) { return; }
  }
  /* Store not in list, so add it */
  jcr->res.read_storage_list->prepend(store->store);
}

void FreeRstorage(JobControlRecord* jcr)
{
  if (jcr->res.read_storage_list) {
    delete jcr->res.read_storage_list;
    jcr->res.read_storage_list = NULL;
  }
  jcr->res.read_storage = NULL;
}

/**
 * Copy the storage definitions from an alist to the JobControlRecord
 */
void CopyWstorage(JobControlRecord* jcr, alist* storage, const char* where)
{
  if (storage) {
    StorageResource* st = nullptr;
    if (jcr->res.write_storage_list) { delete jcr->res.write_storage_list; }
    jcr->res.write_storage_list = new alist(10, not_owned_by_alist);
    foreach_alist (st, storage) {
      Dmsg1(100, "write_storage_list=%s\n", st->resource_name_);
      jcr->res.write_storage_list->append(st);
    }
    if (!jcr->res.wstore_source) {
      jcr->res.wstore_source = GetPoolMemory(PM_MESSAGE);
    }
    PmStrcpy(jcr->res.wstore_source, where);
    if (jcr->res.write_storage_list) {
      jcr->res.write_storage =
          (StorageResource*)jcr->res.write_storage_list->first();
      Dmsg2(100, "write_storage=%s where=%s\n", jcr->res.write_storage->resource_name_,
            jcr->res.wstore_source);
    }
  }
}

/**
 * Set storage override.
 * Remove all previous storage.
 */
void SetWstorage(JobControlRecord* jcr, UnifiedStorageResource* store)
{
  StorageResource* storage = nullptr;

  if (!store->store) { return; }
  if (jcr->res.write_storage_list) { FreeWstorage(jcr); }
  if (!jcr->res.write_storage_list) {
    jcr->res.write_storage_list = new alist(10, not_owned_by_alist);
  }
  jcr->res.write_storage = store->store;
  if (!jcr->res.wstore_source) {
    jcr->res.wstore_source = GetPoolMemory(PM_MESSAGE);
  }
  PmStrcpy(jcr->res.wstore_source, store->store_source);
  Dmsg2(50, "write_storage=%s where=%s\n", jcr->res.write_storage->resource_name_,
        jcr->res.wstore_source);
  foreach_alist (storage, jcr->res.write_storage_list) {
    if (store->store == storage) { return; }
  }

  /*
   * Store not in list, so add it
   */
  jcr->res.write_storage_list->prepend(store->store);
}

void FreeWstorage(JobControlRecord* jcr)
{
  if (jcr->res.write_storage_list) {
    delete jcr->res.write_storage_list;
    jcr->res.write_storage_list = NULL;
  }
  jcr->res.write_storage = NULL;
}

/**
 * For NDMP backup we can setup the backup to run to a NDMP instance
 * of the same Storage Daemon that also does native native backups.
 * This way a Normal Storage Daemon can perform NDMP protocol based
 * saves and restores.
 */
void SetPairedStorage(JobControlRecord* jcr)
{
  StorageResource *store = nullptr, *paired_read_write_storage = nullptr;

  switch (jcr->getJobType()) {
    case JT_BACKUP:
      /*
       * For a backup we look at the write storage.
       */
      if (jcr->res.write_storage_list) {
        /*
         * Setup the jcr->res.write_storage_list to point to all paired_storage
         * entries of all the storage currently in the
         * jcrres.->write_storage_list. Save the original list under
         * jcr->res.paired_read_write_storage_list.
         */
        jcr->res.paired_read_write_storage_list = jcr->res.write_storage_list;
        jcr->res.write_storage_list = new alist(10, not_owned_by_alist);
        foreach_alist (store, jcr->res.paired_read_write_storage_list) {
          if (store->paired_storage) {
            Dmsg1(100, "write_storage_list=%s\n",
                  store->paired_storage->resource_name_);
            jcr->res.write_storage_list->append(store->paired_storage);
          }
        }

        /*
         * Swap the actual jcr->res.write_storage to point to the paired storage
         * entry. We save the actual storage entry in paired_read_write_storage
         * which is for restore in the FreePairedStorage() function.
         */
        store = jcr->res.write_storage;
        if (store->paired_storage) {
          jcr->res.write_storage = store->paired_storage;
          jcr->res.paired_read_write_storage = store;
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
      if (jcr->res.read_storage_list) {
        /*
         * Setup the jcr->res.paired_read_write_storage_list to point to all
         * paired_storage entries of all the storage currently in the
         * jcr->res.read_storage_list.
         */
        jcr->res.paired_read_write_storage_list =
            new alist(10, not_owned_by_alist);
        foreach_alist (paired_read_write_storage, jcr->res.read_storage_list) {
          store = (StorageResource*)my_config->GetNextRes(R_STORAGE, NULL);
          while (store) {
            if (store->paired_storage == paired_read_write_storage) { break; }

            store = (StorageResource*)my_config->GetNextRes(
                R_STORAGE, (BareosResource*)store);
          }

          /*
           * See if we found a store that has the current
           * paired_read_write_storage as its paired storage.
           */
          if (store) {
            jcr->res.paired_read_write_storage_list->append(store);

            /*
             * If the current processed paired_read_write_storage is also the
             * current entry in jcr->res.read_storage update the
             * jcr->paired_read_write_storage to point to this storage entry.
             */
            if (paired_read_write_storage == jcr->res.read_storage) {
              jcr->res.paired_read_write_storage = store;
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
      if (jcr->res.read_storage_list) {
        /*
         * Setup the jcr->res.read_storage_list to point to all paired_storage
         * entries of all the storage currently in the
         * jcr->res.read_storage_list. Save the original list under
         * jcr->res.paired_read_write_storage_list.
         */
        jcr->res.paired_read_write_storage_list = jcr->res.read_storage_list;
        jcr->res.read_storage_list = new alist(10, not_owned_by_alist);
        foreach_alist (store, jcr->res.paired_read_write_storage_list) {
          if (store->paired_storage) {
            Dmsg1(100, "read_storage_list=%s\n", store->paired_storage->resource_name_);
            jcr->res.read_storage_list->append(store->paired_storage);
          }
        }

        /*
         * Swap the actual jcr->res.read_storage to point to the paired storage
         * entry. We save the actual storage entry in paired_read_write_storage
         * which is for restore in the FreePairedStorage() function.
         */
        store = jcr->res.read_storage;
        if (store->paired_storage) {
          jcr->res.read_storage = store->paired_storage;
          jcr->res.paired_read_write_storage = store;
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

/**
 * This performs an undo of the actions the SetPairedStorage() function
 * performed. We reset the storage write storage back to its original
 * and remove the paired storage override if any.
 */
void FreePairedStorage(JobControlRecord* jcr)
{
  if (jcr->res.paired_read_write_storage_list) {
    switch (jcr->getJobType()) {
      case JT_BACKUP:
        /*
         * For a backup we look at the write storage.
         */
        if (jcr->res.write_storage_list) {
          /*
           * The jcr->res.write_storage_list contain a set of paired storages.
           * We just delete it content and swap back to the real master storage.
           */
          delete jcr->res.write_storage_list;
          jcr->res.write_storage_list = jcr->res.paired_read_write_storage_list;
          jcr->res.paired_read_write_storage_list = NULL;
          jcr->res.write_storage = jcr->res.paired_read_write_storage;
          jcr->res.paired_read_write_storage = NULL;
        }
        break;
      case JT_RESTORE:
        /*
         * The jcr->res.read_storage_list contain a set of paired storages.
         * For the read we created a list of alternative storage which we
         * can just drop now.
         */
        delete jcr->res.paired_read_write_storage_list;
        jcr->res.paired_read_write_storage_list = NULL;
        jcr->res.paired_read_write_storage = NULL;
        break;
      case JT_MIGRATE:
      case JT_COPY:
        /*
         * For a migrate or copy we look at the read storage.
         */
        if (jcr->res.read_storage_list) {
          /*
           * The jcr->res.read_storage_list contains a set of paired storages.
           * We just delete it content and swap back to the real master storage.
           */
          delete jcr->res.read_storage_list;
          jcr->res.read_storage_list = jcr->res.paired_read_write_storage_list;
          jcr->res.paired_read_write_storage_list = NULL;
          jcr->res.read_storage = jcr->res.paired_read_write_storage;
          jcr->res.paired_read_write_storage = NULL;
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

/**
 * Check if every possible storage has paired storage associated.
 */
bool HasPairedStorage(JobControlRecord* jcr)
{
  StorageResource* store = nullptr;

  switch (jcr->getJobType()) {
    case JT_BACKUP:
      /*
       * For a backup we look at the write storage.
       */
      if (jcr->res.write_storage_list) {
        foreach_alist (store, jcr->res.write_storage_list) {
          if (!store->paired_storage) { return false; }
        }
      } else {
        Jmsg(jcr, M_FATAL, 0,
             _("No write storage, don't know how to check for paired "
               "storage\n"));
        return false;
      }
      break;
    case JT_RESTORE:
    case JT_MIGRATE:
    case JT_COPY:
      if (jcr->res.read_storage_list) {
        foreach_alist (store, jcr->res.read_storage_list) {
          if (!store->paired_storage) { return false; }
        }
      } else {
        Jmsg(
            jcr, M_FATAL, 0,
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

#define MAX_TRIES 6 * 360 /* 6 hours (10 sec intervals) */

/**
 * Change the read storage resource for the current job.
 */
bool SelectNextRstore(JobControlRecord* jcr, bootstrap_info& info)
{
  UnifiedStorageResource ustore;

  if (bstrcmp(jcr->res.read_storage->resource_name_, info.storage)) {
    return true; /* Same SD nothing to change */
  }

  if (!(ustore.store = (StorageResource*)my_config->GetResWithName(
            R_STORAGE, info.storage))) {
    Jmsg(jcr, M_FATAL, 0, _("Could not get storage resource '%s'.\n"),
         info.storage);
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
  DecReadStore(jcr);
  FreeRstorage(jcr);
  SetRstorage(jcr, &ustore);
  jcr->setJobStatus(JS_WaitSD);

  /*
   * Wait for up to 6 hours to increment read stoage counter
   */
  for (int i = 0; i < MAX_TRIES; i++) {
    /*
     * Try to get read storage counter incremented
     */
    if (IncReadStore(jcr)) {
      jcr->setJobStatus(JS_Running);
      return true;
    }
    Bmicrosleep(10, 0); /* Sleep 10 secs */
    if (JobCanceled(jcr)) {
      FreeRstorage(jcr);
      return false;
    }
  }

  /*
   * Failed to IncReadStore()
   */
  FreeRstorage(jcr);
  Jmsg(jcr, M_FATAL, 0, _("Could not acquire read storage lock for \"%s\""),
       info.storage);
  return false;
}

void StorageStatus(UaContext* ua, StorageResource* store, char* cmd)
{
  switch (store->Protocol) {
    case APT_NATIVE:
      return DoNativeStorageStatus(ua, store, cmd);
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      return DoNdmpStorageStatus(ua, store, cmd);
    default:
      break;
  }
}

/**
 * Simple comparison function for binary insert of vol_list_t
 */
int StorageCompareVolListEntry(void* e1, void* e2)
{
  vol_list_t *v1, *v2;

  v1 = (vol_list_t*)e1;
  v2 = (vol_list_t*)e2;

  ASSERT(v1);
  ASSERT(v2);

  if (v1->element_address == v2->element_address) {
    return 0;
  } else {
    return (v1->element_address < v2->element_address) ? -1 : 1;
  }
}

static inline void FreeVolList(changer_vol_list_t* vol_list)
{
  vol_list_t* vl;

  if (vol_list->contents) {
    foreach_dlist (vl, vol_list->contents) {
      if (vl->VolName) { free(vl->VolName); }
    }
    vol_list->contents->destroy();
    delete vol_list->contents;
    vol_list->contents = NULL;
  } else {
    Dmsg0(100, "FreeVolList: vol_list->contents already empty\n");
  }
}

/**
 * Generic routine to get the content of a storage autochanger.
 */
changer_vol_list_t* get_vol_list_from_storage(UaContext* ua,
                                              StorageResource* store,
                                              bool listall,
                                              bool scan,
                                              bool cached)
{
  vol_list_type type;
  dlist* contents = NULL;
  changer_vol_list_t* vol_list = NULL;

  P(store->runtime_storage_status->changer_lock);

  if (listall) {
    type = VOL_LIST_ALL;
  } else {
    type = VOL_LIST_PARTIAL;
  }

  /*
   * Do we have a cached version of the content that is still valid ?
   */
  if (store->runtime_storage_status->vol_list) {
    utime_t now;

    now = (utime_t)time(NULL);

    /*
     * Are we allowed to return a cached list ?
     */
    if (cached && store->runtime_storage_status->vol_list->type == type) {
      if ((now - store->runtime_storage_status->vol_list->timestamp) <=
          store->cache_status_interval) {
        Dmsg0(100, "Using cached storage status\n");
        vol_list = store->runtime_storage_status->vol_list;
        vol_list->reference_count++;
        goto bail_out;
      }
    }

    /*
     * Cached version expired or want non-cached version or wrong type.
     * Remove the cached contents and retrieve the new contents from the
     * autochanger.
     */
    Dmsg0(100, "Freeing volume list\n");
    if (store->runtime_storage_status->vol_list->reference_count == 0) {
      FreeVolList(store->runtime_storage_status->vol_list);
    } else {
      /*
       * Need to cleanup but things are still referenced.
       * We just remove the old changer_vol_list_t and on the next call to
       * StorageReleaseVolList() this orphaned changer_vol_list_t will
       * then be destroyed.
       */
      Dmsg0(100, "Need to free still referenced vol_list\n");
      store->runtime_storage_status->vol_list =
          (changer_vol_list_t*)malloc(sizeof(changer_vol_list_t));
      memset(store->runtime_storage_status->vol_list, 0, sizeof(changer_vol_list_t));
    }
  }

  /*
   * Nothing cached or uncached data wanted so perform retrieval.
   */
  switch (store->Protocol) {
    case APT_NATIVE:
      contents = native_get_vol_list(ua, store, listall, scan);
      break;
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      contents = ndmp_get_vol_list(ua, store, listall, scan);
      break;
    default:
      break;
  }

  if (contents) {
    /*
     * Cache the returned content of the autochanger.
     */
    if (!store->runtime_storage_status->vol_list) {
      store->runtime_storage_status->vol_list =
          (changer_vol_list_t*)malloc(sizeof(changer_vol_list_t));
      memset(store->runtime_storage_status->vol_list, 0, sizeof(changer_vol_list_t));
    }
    vol_list = store->runtime_storage_status->vol_list;
    vol_list->reference_count++;
    vol_list->contents = contents;
    vol_list->timestamp = (utime_t)time(NULL);
    vol_list->type = type;
  }

bail_out:
  V(store->runtime_storage_status->changer_lock);

  return vol_list;
}

slot_number_t GetNumSlots(UaContext* ua, StorageResource* store)
{
  slot_number_t slots = 0;

  /*
   * See if we can use the cached number of slots.
   */
  if (store->runtime_storage_status->slots > 0) { return store->runtime_storage_status->slots; }

  P(store->runtime_storage_status->changer_lock);

  switch (store->Protocol) {
    case APT_NATIVE:
      slots = NativeGetNumSlots(ua, store);
      break;
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      slots = NdmpGetNumSlots(ua, store);
      break;
    default:
      break;
  }

  store->runtime_storage_status->slots = slots;

  V(store->runtime_storage_status->changer_lock);

  return slots;
}

slot_number_t GetNumDrives(UaContext* ua, StorageResource* store)
{
  drive_number_t drives = 0;

  /*
   * See if we can use the cached number of drives.
   */
  if (store->runtime_storage_status->drives > 0) { return store->runtime_storage_status->drives; }

  P(store->runtime_storage_status->changer_lock);

  switch (store->Protocol) {
    case APT_NATIVE:
      drives = NativeGetNumDrives(ua, store);
      break;
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      drives = NdmpGetNumDrives(ua, store);
      break;
    default:
      break;
  }

  store->runtime_storage_status->drives = drives;

  V(store->runtime_storage_status->changer_lock);

  return drives;
}

bool transfer_volume(UaContext* ua,
                     StorageResource* store,
                     slot_number_t src_slot,
                     slot_number_t dst_slot)
{
  bool retval = false;

  P(store->runtime_storage_status->changer_lock);

  switch (store->Protocol) {
    case APT_NATIVE:
      retval = NativeTransferVolume(ua, store, src_slot, dst_slot);
      break;
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      retval = NdmpTransferVolume(ua, store, src_slot, dst_slot);
      break;
    default:
      break;
  }

  V(store->runtime_storage_status->changer_lock);

  return retval;
}

bool DoAutochangerVolumeOperation(UaContext* ua,
                                  StorageResource* store,
                                  const char* operation,
                                  drive_number_t drive,
                                  slot_number_t slot)
{
  bool retval = false;

  P(store->runtime_storage_status->changer_lock);

  switch (store->Protocol) {
    case APT_NATIVE:
      retval =
          NativeAutochangerVolumeOperation(ua, store, operation, drive, slot);
      break;
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      retval =
          NdmpAutochangerVolumeOperation(ua, store, operation, drive, slot);
      break;
    default:
      break;
  }

  V(store->runtime_storage_status->changer_lock);

  return retval;
}

/**
 * See if a specific slot is loaded in one of the drives.
 */
vol_list_t* vol_is_loaded_in_drive(StorageResource* store,
                                   changer_vol_list_t* vol_list,
                                   slot_number_t slot)
{
  vol_list_t* vl;

  vl = (vol_list_t*)vol_list->contents->first();
  while (vl) {
    switch (vl->slot_type) {
      case kSlotTypeDrive:
        Dmsg2(100, "Checking drive %hd for loaded volume == %hd\n",
              vl->bareos_slot_number, vl->currently_loaded_slot_number);
        if (vl->currently_loaded_slot_number == slot) { return vl; }
        break;
      default:
        break;
    }
    vl = (vol_list_t*)vol_list->contents->next((void*)vl);
  }

  return NULL;
}

/**
 * Release the reference to the volume list returned from
 * get_vol_list_from_storage()
 */
void StorageReleaseVolList(StorageResource* store, changer_vol_list_t* vol_list)
{
  P(store->runtime_storage_status->changer_lock);

  Dmsg0(100, "Releasing volume list\n");

  /*
   * See if we are releasing a reference to the currently cached value.
   */
  if (store->runtime_storage_status->vol_list == vol_list) {
    vol_list->reference_count--;
  } else {
    vol_list->reference_count--;
    if (vol_list->reference_count == 0) {
      /*
       * It seems this is a release of an uncached version of the vol_list.
       * We just destroy this vol_list as there are no more references to it.
       */
      FreeVolList(vol_list);
      free(vol_list);
    }
  }

  V(store->runtime_storage_status->changer_lock);
}

/**
 * Destroy the volume list returned from get_vol_list_from_storage()
 */
void StorageFreeVolList(StorageResource* store, changer_vol_list_t* vol_list)
{
  P(store->runtime_storage_status->changer_lock);

  Dmsg1(100, "Freeing volume list at %p\n", vol_list);

  FreeVolList(vol_list);

  /*
   * Clear the cached vol_list if needed.
   */
  if (store->runtime_storage_status->vol_list == vol_list) { store->runtime_storage_status->vol_list = NULL; }

  V(store->runtime_storage_status->changer_lock);
}

/**
 * Invalidate a cached volume list returned from get_vol_list_from_storage()
 * Called by functions that change the content of the storage like mount,
 * umount, release.
 */
void InvalidateVolList(StorageResource* store)
{
  P(store->runtime_storage_status->changer_lock);

  if (store->runtime_storage_status->vol_list) {
    Dmsg1(100, "Invalidating volume list at %p\n", store->runtime_storage_status->vol_list);

    /*
     * If the volume list is unreferenced we can destroy it otherwise we just
     * reset the pointer and the StorageReleaseVolList() will destroy it for
     * us the moment there are no more references.
     */
    if (store->runtime_storage_status->vol_list->reference_count == 0) {
      FreeVolList(store->runtime_storage_status->vol_list);
    } else {
      store->runtime_storage_status->vol_list = NULL;
    }
  }

  V(store->runtime_storage_status->changer_lock);
}


} /* namespace directordaemon */
