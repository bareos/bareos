/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, May 2015
 */
/**
 * @file
 * Storage specific NDMP Data Management Application (DMA) routines
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"

#if HAVE_NDMP

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

/* Imported variables */

/* Forward referenced functions */

/**
 * Output the status of a storage daemon when its a normal storage
 * daemon accessed via the NDMP protocol or query the TAPE and ROBOT
 * agent of a native NDMP server.
 */
void do_ndmp_storage_status(UaContext *ua, StoreResource *store, char *cmd)
{
   /*
    * See if the storage is just a NDMP instance of a normal storage daemon.
    */
   if (store->paired_storage) {
      do_native_storage_status(ua, store->paired_storage, cmd);
   } else {
      struct ndm_job_param ndmp_job;

      if (!ndmp_build_storage_job(ua->jcr,
                                  store,
                                  true, /* Query Tape Agent */
                                  true, /* Query Robot Agent */
                                  NDM_JOB_OP_QUERY_AGENTS,
                                  &ndmp_job)) {
         return;
      }

      ndmp_do_query(ua, &ndmp_job, me->ndmp_loglevel);
   }
}

/**
 * Interface function which glues the logging infra of the NDMP lib for debugging.
 */
extern "C" void ndmp_robot_status_handler(struct ndmlog *log, char *tag, int lev, char *msg)
{
   NIS *nis;

   /*
    * Make sure if the logging system was setup properly.
    */
   nis = (NIS *)log->ctx;
   if (!nis) {
      return;
   }

   Dmsg1(100, "%s\n", msg);
}

/**
 * Generic cleanup function that can be used after a successfull or failed NDMP Job ran.
 */
static void cleanup_ndmp_session(struct ndm_session *ndmp_sess)
{
   /*
    * Destroy the session.
    */
   ndma_session_destroy(ndmp_sess);

   /*
    * Free the param block.
    */
   free(ndmp_sess->param->log_tag);
   free(ndmp_sess->param->log.ctx);
   free(ndmp_sess->param);
   free(ndmp_sess);
}

/**
 * Generic function to run a storage Job on a remote NDMP server.
 */
static bool ndmp_run_storage_job(JobControlRecord *jcr, StoreResource *store, struct ndm_session *ndmp_sess, struct ndm_job_param *ndmp_job)
{
   NIS *nis;

   ndmp_sess->conn_snooping = (me->ndmp_snooping) ? 1 : 0;
   ndmp_sess->control_agent_enabled = 1;

   ndmp_sess->param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
   memset(ndmp_sess->param, 0, sizeof(struct ndm_session_param));
   ndmp_sess->param->log.deliver = ndmp_robot_status_handler;
   nis = (NIS *)malloc(sizeof(NIS));
   memset(nis, 0, sizeof(NIS));
   ndmp_sess->param->log_level = native_to_ndmp_loglevel(me->ndmp_loglevel, debug_level, nis);
   ndmp_sess->param->log.ctx = nis;
   ndmp_sess->param->log_tag = bstrdup("DIR-NDMP");

   /*
    * Initialize the session structure.
    */
   if (ndma_session_initialize(ndmp_sess)) {
      goto bail_out;
   }

   /*
    * Copy the actual job to perform.
    */
   memcpy(&ndmp_sess->control_acb->job, ndmp_job, sizeof(struct ndm_job_param));
   if (!ndmp_validate_job(jcr, &ndmp_sess->control_acb->job)) {
      goto bail_out;
   }

   /*
    * Commission the session for a run.
    */
   if (ndma_session_commission(ndmp_sess)) {
      goto bail_out;
   }

   /*
    * Setup the DMA.
    */
   if (ndmca_connect_control_agent(ndmp_sess)) {
      goto bail_out;
   }

   ndmp_sess->conn_open = 1;
   ndmp_sess->conn_authorized = 1;

   /*
    * Let the DMA perform its magic.
    */
   if (ndmca_control_agent(ndmp_sess) != 0) {
      goto bail_out;
   }

   return true;

bail_out:
   return false;
}

/**
 * Generic function to get the current element status of a NDMP robot.
 */
static bool get_robot_element_status(JobControlRecord *jcr, StoreResource *store, struct ndm_session **ndmp_sess)
{
   struct ndm_job_param ndmp_job;

   /*
    * See if this is an autochanger.
    */
   if (!store->autochanger || !store->ndmp_changer_device) {
      return false;
   }

   if (!ndmp_build_storage_job(jcr,
                               store,
                               false, /* Setup Tape Agent */
                               true, /* Setup Robot Agent */
                               NDM_JOB_OP_INIT_ELEM_STATUS,
                               &ndmp_job)) {
      return false;
   }

   /*
    * Set the remote robotics name to use.
    * We use the ndmscsi_target_from_str() function which parses the NDMJOB format of a
    * device in the form NAME[,[CNUM,]SID[,LUN]
    */
   ndmp_job.robot_target = (struct ndmscsi_target *)actuallymalloc(sizeof(struct ndmscsi_target));
   if (ndmscsi_target_from_str(ndmp_job.robot_target, store->ndmp_changer_device) != 0) {
      actuallyfree(ndmp_job.robot_target);
      return false;
   }
   ndmp_job.have_robot = 1;
   ndmp_job.auto_remedy = 1;

   /*
    * Initialize a new NDMP session
    */
   *ndmp_sess = (struct ndm_session *)malloc(sizeof(struct ndm_session));
   memset(*ndmp_sess, 0, sizeof(struct ndm_session));

   if (!ndmp_run_storage_job(jcr, store, *ndmp_sess, &ndmp_job)) {
      cleanup_ndmp_session(*ndmp_sess);
      return false;
   }

   return true;
}

/**
 * Get the volume names from a smc_element_descriptor.
 */
static void fill_volume_name(vol_list_t *vl, struct smc_element_descriptor *edp)
{
   if (edp->PVolTag) {
      vl->VolName = bstrdup((char *)edp->primary_vol_tag->volume_id);
      strip_trailing_junk(vl->VolName);
   } else if (edp->AVolTag) {
      vl->VolName = bstrdup((char *)edp->alternate_vol_tag->volume_id);
      strip_trailing_junk(vl->VolName);
   }
}

/**
 * Fill the mapping table from logical to physical storage addresses.
 *
 * The robot mapping table is used when we need to map from a logical
 * number to a physical storage address and for things like counting
 * the number of slots or drives. Caching this mapping data is no problem
 * as its only a physical mapping which won't change much (if at all) over
 * the time the daemon runs. We don't capture anything like volumes and
 * the fact if things are full or empty as that data is kind of volatile
 * and you should use a vol_list for that.
 */
static void ndmp_fill_storage_mappings(StoreResource *store, struct ndm_session *ndmp_sess)
{
   drive_number_t drive;
   slot_number_t slot,
                 picker;
   storage_mapping_t *mapping = NULL;
   struct smc_ctrl_block *smc;
   struct smc_element_descriptor *edp;

   store->rss->storage_mappings = New(dlist(mapping, &mapping->link));

   /*
    * Loop over the robot element status and add each element to
    * the mapping table. We first add each element without a logical
    * slot number so things are inserted based on their Physical address
    * into the linked list using binary insert on the Index field.
    */
   smc = ndmp_sess->control_acb->smc_cb;
   for (edp = smc->elem_desc; edp; edp = edp->next) {
      mapping = (storage_mapping_t *)malloc(sizeof(storage_mapping_t));
      memset(mapping, 0, sizeof(storage_mapping_t));

      switch (edp->element_type_code) {
      case SMC_ELEM_TYPE_MTE:
         mapping->Type = slot_type_picker;
         break;
      case SMC_ELEM_TYPE_SE:
         mapping->Type = slot_type_normal;
         break;
      case SMC_ELEM_TYPE_IEE:
         mapping->Type = slot_type_import;
         break;
      case SMC_ELEM_TYPE_DTE:
         mapping->Type = slot_type_drive;
         break;
      default:
         mapping->Type = slot_type_unknown;
         break;
      }
      mapping->Index = edp->element_address;

      store->rss->storage_mappings->binary_insert(mapping, compare_storage_mapping);
   }

   /*
    * Pickers and Drives start counting at 0 slots at 1.
    */
   picker = 0;
   drive = 0;
   slot = 1;

   /*
    * Loop over each mapping entry ordered by the Index field and assign a
    * logical number to it.
    * For the slots,
    * - first do the normal slots
    * - second the I/E slots so that they are always at the end
    */
   foreach_dlist(mapping, store->rss->storage_mappings) {
      switch (mapping->Type) {
      case slot_type_picker:
         mapping->Slot = picker++;
         break;
      case slot_type_drive:
         mapping->Slot = drive++;
         break;
      case slot_type_normal:
      case slot_type_import:
         mapping->Slot = slot++;
         break;
      default:
         break;
      }
   }

}

/**
 * Get the current content of the autochanger as a generic vol_list dlist.
 */
dlist *ndmp_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan)
{
   struct ndm_session *ndmp_sess;
   struct smc_ctrl_block *smc;
   struct smc_element_descriptor *edp;
   vol_list_t *vl = NULL;
   dlist *vol_list = NULL;

   ua->warning_msg(_("get ndmp_vol_list...\n"));
   if (!get_robot_element_status(ua->jcr, store, &ndmp_sess)) {
      return (dlist *)NULL;
   }

   /*
    * If we have no storage mappings create them now from the data we just retrieved.
    */
   if (!store->rss->storage_mappings) {
      ndmp_fill_storage_mappings(store, ndmp_sess);
   }

   /*
    * Start with an empty dlist().
    */
   vol_list = New(dlist(vl, &vl->link));

   /*
    * Process the robot element status retrieved.
    */
   smc = ndmp_sess->control_acb->smc_cb;
   for (edp = smc->elem_desc; edp; edp = edp->next) {
      vl = (vol_list_t *)malloc(sizeof(vol_list_t));
      memset(vl, 0, sizeof(vol_list_t));

      if (scan && !listall) {
         /*
          * Scanning -- require only valid slot
          */
         switch (edp->element_type_code) {
         case SMC_ELEM_TYPE_SE:
            /*
             * Normal slot
             */
            vl->Type = slot_type_normal;
            if (edp->Full) {
               vl->Content = slot_content_full;
               fill_volume_name(vl, edp);
            } else {
               vl->Content = slot_content_empty;
            }
            vl->Index = edp->element_address;
            break;
         default:
            free(vl);
            continue;
         }
      } else if (!listall) {
         /*
          * Not scanning and not listall.
          */
         switch (edp->element_type_code) {
         case SMC_ELEM_TYPE_SE:
            /*
             * Normal slot
             */
            vl->Type = slot_type_normal;
            vl->Index = edp->element_address;
            if (!edp->Full) {
               free(vl);
               continue;
            } else {
               vl->Content = slot_content_full;
               fill_volume_name(vl, edp);
            }
            break;
         default:
            free(vl);
            continue;
         }
      } else {
         /*
          * Listall.
          */
         switch (edp->element_type_code) {
         case SMC_ELEM_TYPE_MTE:
            /*
             * Transport
             */
            free(vl);
            continue;
         case SMC_ELEM_TYPE_SE:
            /*
             * Normal slot
             */
            vl->Type = slot_type_normal;
            vl->Index = edp->element_address;
            if (edp->Full) {
               vl->Content = slot_content_full;
               fill_volume_name(vl, edp);
            } else {
               vl->Content = slot_content_empty;
            }
            break;
         case SMC_ELEM_TYPE_IEE:
            /*
             * Import/Export Slot
             */
            vl->Type = slot_type_import;
            vl->Index = edp->element_address;
            if (edp->Full) {
               vl->Content = slot_content_full;
               fill_volume_name(vl, edp);
            } else {
               vl->Content = slot_content_empty;
            }
            if (edp->InEnab) {
               vl->Flags |= can_import;
            }
            if (edp->ExEnab) {
               vl->Flags |= can_export;
            }
            if (edp->ImpExp) {
               vl->Flags |= by_oper;
            } else {
               vl->Flags |= by_mte;
            }
            break;
         case SMC_ELEM_TYPE_DTE:
            /*
             * Drive
             */
            vl->Type = slot_type_drive;
            vl->Index = edp->element_address;
            if (edp->Full) {
               slot_number_t slot_mapping;

               vl->Content = slot_content_full;
               slot_mapping = lookup_storage_mapping(store, slot_type_normal, PHYSICAL_TO_LOGICAL, edp->src_se_addr);
               vl->Loaded = slot_mapping;
               fill_volume_name(vl, edp);
            } else {
               vl->Content = slot_content_empty;
            }
            break;
         default:
            vl->Type = slot_type_unknown;
            vl->Index = edp->element_address;
            break;
         }
      }

      /*
       * Map physical storage address to logical one using the storage mappings.
       */
      vl->Slot = lookup_storage_mapping(store, vl->Type, PHYSICAL_TO_LOGICAL, vl->Index);

      if (vl->VolName) {
         Dmsg6(100, "Add index = %hd slot=%hd loaded=%hd type=%hd content=%hd Vol=%s to SD list.\n",
               vl->Index, vl->Slot, vl->Loaded, vl->Type, vl->Content, NPRT(vl->VolName));
      } else {
         Dmsg5(100, "Add index = %hd slot=%hd loaded=%hd type=%hd content=%hd Vol=NULL to SD list.\n",
               vl->Index, vl->Slot, vl->Loaded, vl->Type, vl->Content);
      }

      vol_list->binary_insert(vl, storage_compare_vol_list_entry);
   }

   if (vol_list->size() == 0) {
      delete vol_list;
      vol_list = NULL;
   }

   cleanup_ndmp_session(ndmp_sess);

   return vol_list;
}

/**
 * Update the mapping table from logical to physical storage addresses.
 */
bool ndmp_update_storage_mappings(JobControlRecord* jcr, StoreResource *store)
{
   struct ndm_session *ndmp_sess;

   if (!get_robot_element_status(jcr, store, &ndmp_sess)) {
      return false;
   }

   ndmp_fill_storage_mappings(store, ndmp_sess);

   cleanup_ndmp_session(ndmp_sess);

   return true;

}

/**
 * Update the mapping table from logical to physical storage addresses.
 */
bool ndmp_update_storage_mappings(UaContext *ua, StoreResource *store)
{
   struct ndm_session *ndmp_sess;

   if (!get_robot_element_status(ua->jcr, store, &ndmp_sess)) {
      return false;
   }

   ndmp_fill_storage_mappings(store, ndmp_sess);

   cleanup_ndmp_session(ndmp_sess);

   return true;
}

/**
 * Number of slots in a NDMP autochanger.
 */
slot_number_t ndmp_get_num_slots(UaContext *ua, StoreResource *store)
{
   slot_number_t slots = 0;
   storage_mapping_t *mapping;

   /*
    * See if the mappings are already determined.
    */
   if (!store->rss->storage_mappings) {
      if (!ndmp_update_storage_mappings(ua, store)) {
         return slots;
      }
   }

   /*
    * Walk over all mappings and count the number of slots.
    */
   foreach_dlist(mapping, store->rss->storage_mappings) {
      switch (mapping->Type) {
      case slot_type_normal:
      case slot_type_import:
         slots++;
         break;
      default:
         break;
      }
   }

   return slots;
}

/**
 * Number of drives in a NDMP autochanger.
 */
drive_number_t ndmp_get_num_drives(UaContext *ua, StoreResource *store)
{
   drive_number_t drives = 0;
   storage_mapping_t *mapping;

   /*
    * See if the mappings are already determined.
    */
   if (!store->rss->storage_mappings) {
      if (!ndmp_update_storage_mappings(ua, store)) {
         return drives;
      }
   }

   /*
    * Walk over all mappings and count the number of drives.
    */
   foreach_dlist(mapping, store->rss->storage_mappings) {
      switch (mapping->Type) {
      case slot_type_drive:
         drives++;
         break;
      default:
         break;
      }
   }

   return drives;
}

/**
 * Move a volume from one slot to an other in a NDMP autochanger.
 */
bool ndmp_transfer_volume(UaContext *ua, StoreResource *store,
                          slot_number_t src_slot, slot_number_t dst_slot)
{
   bool retval = false;
   slot_number_t slot_mapping;
   struct ndm_job_param ndmp_job;
   struct ndm_session *ndmp_sess;

   /*
    * See if this is an autochanger.
    */
   if (!store->autochanger || !store->ndmp_changer_device) {
      return retval;
   }

   if (!ndmp_build_storage_job(ua->jcr,
                               store,
                               false, /* Setup Tape Agent */
                               true, /* Setup Robot Agent */
                               NDM_JOB_OP_MOVE_TAPE,
                               &ndmp_job)) {
      return retval;
   }

   /*
    * Fill in the from and to address.
    *
    * As the upper level functions work with logical slot numbers convert them
    * to physical slot numbers for the actual NDMP operation.
    */
   slot_mapping = lookup_storage_mapping(store, slot_type_normal, LOGICAL_TO_PHYSICAL, src_slot);
   if (slot_mapping == -1) {
      ua->error_msg("No slot mapping for slot %hd\n", src_slot);
      return retval;
   }
   ndmp_job.from_addr = slot_mapping;
   ndmp_job.from_addr_given = 1;

   slot_mapping = lookup_storage_mapping(store, slot_type_normal, LOGICAL_TO_PHYSICAL, dst_slot);
   if (slot_mapping == -1) {
      ua->error_msg("No slot mapping for slot %hd\n", dst_slot);
      return retval;
   }
   ndmp_job.to_addr = slot_mapping;
   ndmp_job.to_addr_given = 1;

   ua->warning_msg(_ ("transferring form slot %hd to slot %hd...\n"), src_slot, dst_slot );

   /*
    * Set the remote robotics name to use.
    * We use the ndmscsi_target_from_str() function which parses the NDMJOB format of a
    * device in the form NAME[,[CNUM,]SID[,LUN]
    */
   ndmp_job.robot_target = (struct ndmscsi_target *)actuallymalloc(sizeof(struct ndmscsi_target));
   if (ndmscsi_target_from_str(ndmp_job.robot_target, store->ndmp_changer_device) != 0) {
      actuallyfree(ndmp_job.robot_target);
      return retval;
   }
   ndmp_job.have_robot = 1;
   ndmp_job.auto_remedy = 1;

   /*
    * Initialize a new NDMP session
    */
   ndmp_sess = (struct ndm_session *)malloc(sizeof(struct ndm_session));
   memset(ndmp_sess, 0, sizeof(struct ndm_session));

   if (!ndmp_run_storage_job(ua->jcr, store, ndmp_sess, &ndmp_job)) {
      cleanup_ndmp_session(ndmp_sess);
      return retval;
   }

   retval = true;

   cleanup_ndmp_session(ndmp_sess);

   return retval;
}

/**
 * Lookup the name of a drive in a NDMP autochanger.
 */
char *lookup_ndmp_drive(StoreResource *store, drive_number_t drivenumber)
{
   int cnt = 0;
   char *tapedevice;
   CommonResourceHeader *tapedeviceres;

   if (store->device) {
      foreach_alist(tapedeviceres, store->device) {
         if (cnt == drivenumber) {
            tapedevice = tapedeviceres->name;
            return tapedevice;
         }
         cnt++;
      }
   }

   return NULL;
}

/**
 * Perform an autochanger operation in a NDMP autochanger.
 */
bool ndmp_autochanger_volume_operation(UaContext *ua, StoreResource *store, const char *operation,
                                       drive_number_t drive, slot_number_t slot)
{
   drive_number_t drive_mapping;
   int ndmp_operation;
   bool retval = false;
   struct ndm_job_param ndmp_job;
   struct ndm_session *ndmp_sess;

   Dmsg3(100, "ndmp_autochanger_volume_operation: operation %s, drive %hd, slot %hd\n", operation, drive, slot);
   ua->warning_msg(_("ndmp_autochanger_volume_operation: operation %s, drive %hd, slot %hd\n"), operation, drive, slot);
   /*
    * See if this is an autochanger.
    */
   if (!store->autochanger || !store->ndmp_changer_device) {
      return retval;
   }

   if (bstrcmp(operation, "unmount") || bstrcmp(operation, "release")) {
      ndmp_operation = NDM_JOB_OP_UNLOAD_TAPE;
   } else if (bstrcmp(operation, "mount")) {
      ndmp_operation = NDM_JOB_OP_LOAD_TAPE;
   } else {
      ua->error_msg("Illegal autochanger operation %s\n", operation);
      return retval;
   }

   if (!ndmp_build_storage_job(ua->jcr,
                               store,
                               false, /* Setup Tape Agent */
                               true, /* Setup Robot Agent */
                               ndmp_operation,
                               &ndmp_job)) {
      return retval;
   }

   /*
    * See if the mappings are already determined.
    */
   if (!store->rss->storage_mappings) {
      if (!ndmp_update_storage_mappings(ua, store)) {
         return false;
      }
   }

   if (slot >= 0) {
      slot_number_t slot_mapping;

      /*
       * Map the logical address to a physical one.
       */
      slot_mapping = lookup_storage_mapping(store, slot_type_normal, LOGICAL_TO_PHYSICAL, slot);
      if (slot_mapping == -1) {
         ua->error_msg("No slot mapping for slot %hd\n", slot);
         return retval;
      }
      ndmp_job.from_addr = slot_mapping;
      ndmp_job.from_addr_given = 1;
   }

   /*
    * Map the logical address to a physical one.
    */
   drive_mapping = lookup_storage_mapping(store, slot_type_drive, PHYSICAL_TO_LOGICAL, slot);
   if (drive_mapping == -1) {
      ua->error_msg("No slot mapping for drive %hd\n", drive);
      return retval;
   }
   ndmp_job.drive_addr = drive_mapping;
   ndmp_job.drive_addr_given = 1;

   /*
    * Set the remote robotics name to use.
    * We use the ndmscsi_target_from_str() function which parses the NDMJOB format of a
    * device in the form NAME[,[CNUM,]SID[,LUN]
    */
   ndmp_job.robot_target = (struct ndmscsi_target *)actuallymalloc(sizeof(struct ndmscsi_target));
   if (ndmscsi_target_from_str(ndmp_job.robot_target, store->ndmp_changer_device) != 0) {
      actuallyfree(ndmp_job.robot_target);
      return retval;
   }
   ndmp_job.have_robot = 1;
   ndmp_job.auto_remedy = 1;

   /*
    * Initialize a new NDMP session
    */
   ndmp_sess = (struct ndm_session *)malloc(sizeof(struct ndm_session));
   memset(ndmp_sess, 0, sizeof(struct ndm_session));

   if (!ndmp_run_storage_job(ua->jcr, store, ndmp_sess, &ndmp_job)) {
      cleanup_ndmp_session(ndmp_sess);
      return retval;
   }

   retval = true;

   cleanup_ndmp_session(ndmp_sess);

   return retval;
}

/**
 * Label a volume in a NDMP autochanger.
 */
bool ndmp_send_label_request(UaContext *ua, StoreResource *store, MediaDbRecord *mr,
                             MediaDbRecord *omr, PoolDbRecord *pr, bool relabel,
                             drive_number_t drive, slot_number_t slot)
{
   bool retval = false;
   struct ndm_job_param ndmp_job;
   struct ndm_session *ndmp_sess;
   struct ndmmedia *media;

   Dmsg4(100,"ndmp_send_label_request: VolumeName=%s MediaType=%s PoolName=%s drive=%hd\n", mr->VolumeName, mr->MediaType, pr->Name, drive);
   ua->warning_msg(_("ndmp_send_label_request: VolumeName=%s MediaType=%s PoolName=%s drive=%hd\n"), mr->VolumeName, mr->MediaType, pr->Name, drive);

   /*
    * See if this is an autochanger.
    */
   if (!store->autochanger || !store->ndmp_changer_device) {
      return retval;
   }

   if (!ndmp_build_storage_job(ua->jcr,
                               store,
                               true, /* Setup Tape Agent */
                               true, /* Setup Robot Agent */
                               NDM_JOB_OP_INIT_LABELS,
                               &ndmp_job)) {
      return retval;
   }

   /*
    * Set the remote robotics name to use.
    * We use the ndmscsi_target_from_str() function which parses the NDMJOB format of a
    * device in the form NAME[,[CNUM,]SID[,LUN]
    */
   ndmp_job.robot_target = (struct ndmscsi_target *)actuallymalloc(sizeof(struct ndmscsi_target));
   if (ndmscsi_target_from_str(ndmp_job.robot_target, store->ndmp_changer_device) != 0) {
      actuallyfree(ndmp_job.robot_target);
      Dmsg0(100,"ndmp_send_label_request: no robot to use\n");
      return retval;
   }
   ndmp_job.have_robot = 1;
   ndmp_job.auto_remedy = 1;

   /*
    * Set the remote tape drive to use.
    */
   ndmp_job.tape_device = lookup_ndmp_drive(store, drive);
   if (!ndmp_job.tape_device) {
      actuallyfree(ndmp_job.robot_target);
   }

   /*
    * Insert a media entry of the slot to label.
    */
   if (slot > 0) {
      slot_number_t slot_mapping;

      slot_mapping = lookup_storage_mapping(store, slot_type_normal, LOGICAL_TO_PHYSICAL, slot);
      if (slot_mapping == -1) {
         ua->error_msg("No slot mapping for slot %hd\n", slot);
         return retval;
      }
      media = ndma_store_media(&ndmp_job.media_tab, slot_mapping);
   } else {
      media = ndma_store_media(&ndmp_job.media_tab, 0);
   }
   bstrncpy(media->label, mr->VolumeName, NDMMEDIA_LABEL_MAX - 1);
   media->valid_label =  NDMP9_VALIDITY_VALID;


   /*
    * Initialize a new NDMP session
    */
   ndmp_sess = (struct ndm_session *)malloc(sizeof(struct ndm_session));
   memset(ndmp_sess, 0, sizeof(struct ndm_session));

   if (!ndmp_run_storage_job(ua->jcr, store, ndmp_sess, &ndmp_job)) {
      cleanup_ndmp_session(ndmp_sess);
      return retval;
   }

   retval = true;

   cleanup_ndmp_session(ndmp_sess);

   return retval;
}
#else
/**
 * Dummy entry points when NDMP not enabled.
 */
void do_ndmp_storage_status(UaContext *ua, StoreResource *store, char *cmd)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}

dlist *ndmp_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return (dlist *)NULL;
}

slot_number_t ndmp_get_num_slots(UaContext *ua, StoreResource *store)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return 0;
}

drive_number_t ndmp_get_num_drives(UaContext *ua, StoreResource *store)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return 0;
}

bool ndmp_transfer_volume(UaContext *ua, StoreResource *store,
                          slot_number_t src_slot, slot_number_t dst_slot)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

bool ndmp_autochanger_volume_operation(UaContext *ua, StoreResource *store,
                                       const char *operation, drive_number_t drive, slot_number_t slot)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

bool ndmp_send_label_request(UaContext *ua, StoreResource *store, MediaDbRecord *mr,
                             MediaDbRecord *omr, PoolDbRecord *pr, bool relabel,
                             drive_number_t drive, slot_number_t slot)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}
#endif /* HAVE_NDMP */
