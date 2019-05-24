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
#include "dird/dird_globals.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "dird/ndmp_slot2elemaddr.h"

#if HAVE_NDMP
#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"
#endif

namespace directordaemon {

#if HAVE_NDMP
/* Imported variables */

/* Forward referenced functions */

/**
 * ndmp query callback
 */
int get_tape_info_cb(struct ndm_session* sess,
                     ndmp9_device_info* info,
                     unsigned n_info)
{
  Dmsg0(100, "Get tape info called\n");
  unsigned int i, j, k;
  const char* what = "tape";
  JobControlRecord* jcr = NULL;
  StorageResource* store = NULL;
  NIS* nis = (NIS*)sess->param->log.ctx;

  if (nis->jcr) {
    jcr = nis->jcr;
  } else if (nis->ua && nis->ua->jcr) {
    jcr = nis->ua->jcr;
  } else {
    return -1;
  }

  if (jcr->is_JobType(JT_BACKUP)) {
    store = jcr->res.write_storage;

  } else if (jcr->is_JobType(JT_RESTORE)) {
    store = jcr->res.read_storage;

  } else {
    return -1;
  }

  /* if (store->runtime_storage_status->ndmp_deviceinfo) { */
  /*    delete(store->runtime_storage_status->ndmp_deviceinfo); */
  /*    store->runtime_storage_status->ndmp_deviceinfo = NULL; */
  /* } */
  if (store->runtime_storage_status->ndmp_deviceinfo.empty()) {
    for (i = 0; i < n_info; i++) {
      Dmsg2(100, "  %s %s\n", what, info[i].model);

      ndmp_deviceinfo_t* devinfo = new (ndmp_deviceinfo_t);
      devinfo->JobIdUsingDevice = 0;

      ndmp9_device_capability* info_dc;
      info_dc = info[i].caplist.caplist_val;
      devinfo->model = info[i].model;
      devinfo->device = info_dc->device;
      store->runtime_storage_status->ndmp_deviceinfo.push_back(*devinfo);

      for (j = 0; j < info[i].caplist.caplist_len; j++) {
        ndmp9_device_capability* dc;
        uint32_t attr;
        dc = &info[i].caplist.caplist_val[j];
        Dmsg1(100, "    device     %s\n", dc->device);


        if (!strcmp(what, "tape\n")) {
#ifndef NDMOS_OPTION_NO_NDMP3
          if (sess->plumb.tape->protocol_version == 3) {
            attr = dc->v3attr.value;
            Dmsg1(100, "      attr       0x%lx\n", attr);
            if (attr & NDMP3_TAPE_ATTR_REWIND) Dmsg0(100, "        REWIND\n");
            if (attr & NDMP3_TAPE_ATTR_UNLOAD) Dmsg0(100, "        UNLOAD\n");
          }
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
          if (sess->plumb.tape->protocol_version == 4) {
            attr = dc->v4attr.value;
            Dmsg1(100, "      attr       0x%lx\n", attr);
            if (attr & NDMP4_TAPE_ATTR_REWIND) Dmsg0(100, "        REWIND\n");
            if (attr & NDMP4_TAPE_ATTR_UNLOAD) Dmsg0(100, "        UNLOAD\n");
          }
#endif /* !NDMOS_OPTION_NO_NDMP4 */
        }
        for (k = 0; k < dc->capability.capability_len; k++) {
          Dmsg2(100, "      set        %s=%s\n",
                dc->capability.capability_val[k].name,
                dc->capability.capability_val[k].value);
        }
        if (k == 0) Dmsg0(100, "      empty capabilities\n");
      }
      if (j == 0) Dmsg0(100, "    empty caplist\n");
      Dmsg0(100, "\n");
    }
  }
  if (i == 0) Dmsg1(100, "  Empty %s info\n", what);
  return 0;
}

/**
 *  execute NDMP_QUERY_AGENTS on Tape and Robot
 */
bool do_ndmp_native_query_tape_and_robot_agents(JobControlRecord* jcr,
                                                StorageResource* store)
{
  struct ndm_job_param ndmp_job;

  if (!NdmpBuildStorageJob(jcr, store, true, /* Query Tape Agent */
                           true,             /* Query Robot Agent */
                           NDM_JOB_OP_QUERY_AGENTS, &ndmp_job)) {
    Dmsg0(100, "error in NdmpBuildStorageJob");
    return false;
  }

  struct ndmca_query_callbacks query_callbacks;
  query_callbacks.get_tape_info = get_tape_info_cb;
  ndmca_query_callbacks* query_cbs = &query_callbacks;

  NdmpDoQuery(NULL, jcr, &ndmp_job, me->ndmp_loglevel, query_cbs);

  /*
   * Debug output
   */

  if (!store->runtime_storage_status->ndmp_deviceinfo.empty()) {
    Jmsg(jcr, M_INFO, 0, "NDMP Devices for storage %s:(%s)\n",
         store->resource_name_, store->runtime_storage_status->smc_ident);
  } else {
    Jmsg(jcr, M_INFO, 0, "No NDMP Devices for storage %s:(%s)\n",
         store->resource_name_, store->runtime_storage_status->smc_ident);
    return false;
  }
  for (auto devinfo = store->runtime_storage_status->ndmp_deviceinfo.begin();
       devinfo != store->runtime_storage_status->ndmp_deviceinfo.end();
       devinfo++) {
    Jmsg(jcr, M_INFO, 0, " %s\n", devinfo->device.c_str(),
         devinfo->model.c_str());
  }
  return true;
}

/**
 * get status of a NDMP Native storage and store the information
 * coming in via the NDMP protocol
 */
void DoNdmpNativeStorageStatus(UaContext* ua, StorageResource* store, char* cmd)
{
  struct ndm_job_param ndmp_job;

  ua->jcr->res.write_storage = store;

  if (!NdmpBuildStorageJob(ua->jcr, store, true, /* Query Tape Agent */
                           true,                 /* Query Robot Agent */
                           NDM_JOB_OP_QUERY_AGENTS, &ndmp_job)) {
    ua->InfoMsg("build_storage_job failed\n");
  }

  struct ndmca_query_callbacks query_callbacks;
  query_callbacks.get_tape_info = get_tape_info_cb;
  ndmca_query_callbacks* query_cbs = &query_callbacks;

  NdmpDoQuery(ua, NULL, &ndmp_job, me->ndmp_loglevel, query_cbs);

  ndmp_deviceinfo_t* deviceinfo = NULL;
  int i = 0;
  if (!store->runtime_storage_status->ndmp_deviceinfo.empty()) {
    ua->InfoMsg("NDMP Devices for storage %s:(%s)\n", store->resource_name_,
                store->runtime_storage_status->smc_ident);
    ua->InfoMsg(" element_address   Device   Model   (JobId)   \n");
    for (auto devinfo = store->runtime_storage_status->ndmp_deviceinfo.begin();
         devinfo != store->runtime_storage_status->ndmp_deviceinfo.end();
         devinfo++) {
      ua->InfoMsg("   %d   %s   %s   (%d)\n", i++, devinfo->device.c_str(),
                  devinfo->model.c_str(), devinfo->JobIdUsingDevice);
    }
  }
}

/**
 * Output the status of a storage daemon when its a normal storage
 * daemon accessed via the NDMP protocol or query the TAPE and ROBOT
 * agent of a native NDMP server.
 */
void DoNdmpStorageStatus(UaContext* ua, StorageResource* store, char* cmd)
{
  /*
   * See if the storage is just a NDMP instance of a normal storage daemon.
   */
  if (store->paired_storage) {
    DoNativeStorageStatus(ua, store->paired_storage, cmd);
  } else {
    DoNdmpNativeStorageStatus(ua, store, cmd);
  }
}

/**
 * Interface function which glues the logging infra of the NDMP lib for
 * debugging.
 */
extern "C" void NdmpRobotStatusHandler(struct ndmlog* log,
                                       char* tag,
                                       int lev,
                                       char* msg)
{
  NIS* nis;

  /*
   * Make sure if the logging system was setup properly.
   */
  nis = (NIS*)log->ctx;
  if (!nis) { return; }

  Dmsg1(100, "%s\n", msg);
}

/**
 * Generic cleanup function that can be used after a successful or failed NDMP
 * Job ran.
 */
static void CleanupNdmpSession(struct ndm_session* ndmp_sess)
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
static bool NdmpRunStorageJob(JobControlRecord* jcr,
                              StorageResource* store,
                              struct ndm_session* ndmp_sess,
                              struct ndm_job_param* ndmp_job)
{
  NIS* nis;

  ndmp_sess->conn_snooping = (me->ndmp_snooping) ? 1 : 0;
  ndmp_sess->control_agent_enabled = 1;

  ndmp_sess->param =
      (struct ndm_session_param*)malloc(sizeof(struct ndm_session_param));
  memset(ndmp_sess->param, 0, sizeof(struct ndm_session_param));
  ndmp_sess->param->log.deliver = NdmpRobotStatusHandler;
  nis = (NIS*)malloc(sizeof(NIS));
  memset(nis, 0, sizeof(NIS));
  ndmp_sess->param->log_level =
      NativeToNdmpLoglevel(me->ndmp_loglevel, debug_level, nis);
  ndmp_sess->param->log.ctx = nis;
  ndmp_sess->param->log_tag = strdup("DIR-NDMP");
  nis->jcr = jcr;

  /*
   * Initialize the session structure.
   */
  if (ndma_session_initialize(ndmp_sess)) { goto bail_out; }

  /*
   * Copy the actual job to perform.
   */
  memcpy(&ndmp_sess->control_acb->job, ndmp_job, sizeof(struct ndm_job_param));
  if (!NdmpValidateJob(jcr, &ndmp_sess->control_acb->job)) { goto bail_out; }

  /*
   * Commission the session for a run.
   */
  if (ndma_session_commission(ndmp_sess)) { goto bail_out; }

  /*
   * Setup the DMA.
   */
  if (ndmca_connect_control_agent(ndmp_sess)) { goto bail_out; }

  ndmp_sess->conn_open = 1;
  ndmp_sess->conn_authorized = 1;

  /*
   * Let the DMA perform its magic.
   */
  if (ndmca_control_agent(ndmp_sess) != 0) { goto bail_out; }

  return true;

bail_out:
  return false;
}

/**
 * Generic function to get the current element status of a NDMP robot.
 */
static bool GetRobotElementStatus(JobControlRecord* jcr,
                                  StorageResource* store,
                                  struct ndm_session** ndmp_sess)
{
  struct ndm_job_param ndmp_job;

  /*
   * See if this is an autochanger.
   */
  if (!store->autochanger || !store->ndmp_changer_device) { return false; }

  if (!NdmpBuildStorageJob(jcr, store, false, /* Setup Tape Agent */
                           true,              /* Setup Robot Agent */
                           NDM_JOB_OP_INIT_ELEM_STATUS, &ndmp_job)) {
    return false;
  }

  /*
   * Set the remote robotics name to use.
   * We use the ndmscsi_target_from_str() function which parses the NDMJOB
   * format of a device in the form NAME[,[CNUM,]SID[,LUN]
   */
  ndmp_job.robot_target =
      (struct ndmscsi_target*)malloc(sizeof(struct ndmscsi_target));
  if (ndmscsi_target_from_str(ndmp_job.robot_target,
                              store->ndmp_changer_device) != 0) {
    free(ndmp_job.robot_target);
    return false;
  }
  ndmp_job.have_robot = 1;
  ndmp_job.auto_remedy = 1;

  /*
   * Initialize a new NDMP session
   */
  *ndmp_sess = (struct ndm_session*)malloc(sizeof(struct ndm_session));
  memset(*ndmp_sess, 0, sizeof(struct ndm_session));

  if (!NdmpRunStorageJob(jcr, store, *ndmp_sess, &ndmp_job)) {
    CleanupNdmpSession(*ndmp_sess);
    return false;
  }

  return true;
}

/**
 * Get the volume names from a smc_element_descriptor.
 */
static void FillVolumeName(vol_list_t* vl, struct smc_element_descriptor* edp)
{
  if (edp->PVolTag) {
    vl->VolName = strdup((char*)edp->primary_vol_tag->volume_id);
    StripTrailingJunk(vl->VolName);
  } else if (edp->AVolTag) {
    vl->VolName = strdup((char*)edp->alternate_vol_tag->volume_id);
    StripTrailingJunk(vl->VolName);
  }
}

/**
 * Get the information to map logical addresses (index) to
 * physical address (scsi element address)
 *
 * Everything that is needed for that is stored in the
 * smc smc_element_address_assignment.
 *
 * For each type of  element a start address and the
 * number of entries (count) is stored there.
 */
static void NdmpFillStorageMappings(StorageResource* store,
                                    struct ndm_session* ndmp_sess)
{
  drive_number_t drive;
  slot_number_t slot, picker;
  struct smc_ctrl_block* smc;

  smc = ndmp_sess->control_acb->smc_cb;
  memcpy(store->runtime_storage_status->smc_ident, smc->ident,
         sizeof(store->runtime_storage_status->smc_ident));

  if (smc->valid_elem_aa) {
    memcpy(&store->runtime_storage_status->storage_mapping, &smc->elem_aa,
           sizeof(store->runtime_storage_status->storage_mapping));

  } else {
    Dmsg0(0, "Warning, smc does not have valid elem_aa info\n");
  }
}

/**
 * Get the current content of the autochanger as a generic vol_list dlist.
 */
dlist* ndmp_get_vol_list(UaContext* ua,
                         StorageResource* store,
                         bool listall,
                         bool scan)
{
  struct ndm_session* ndmp_sess;
  struct smc_ctrl_block* smc;
  struct smc_element_descriptor* edp;
  vol_list_t* vl = NULL;
  dlist* vol_list = NULL;

  ua->WarningMsg(_("get ndmp_vol_list...\n"));
  if (!GetRobotElementStatus(ua->jcr, store, &ndmp_sess)) {
    return (dlist*)NULL;
  }

  /*
   * If we have no storage mappings create them now from the data we just
   * retrieved.
   */
  NdmpFillStorageMappings(store, ndmp_sess);

  /*
   * Start with an empty dlist().
   */
  vol_list = new dlist(vl, &vl->link);

  /*
   * Process the robot element status retrieved.
   */
  smc = ndmp_sess->control_acb->smc_cb;
  for (edp = smc->elem_desc; edp; edp = edp->next) {
    vl = (vol_list_t*)malloc(sizeof(vol_list_t));
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
          vl->slot_type = slot_type_t::kSlotTypeStorage;
          if (edp->Full) {
            vl->slot_status = slot_status_t::kSlotStatusFull;
            FillVolumeName(vl, edp);
          } else {
            vl->slot_status = slot_status_t::kSlotStatusEmpty;
          }
          vl->element_address = edp->element_address;
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
          vl->slot_type = slot_type_t::kSlotTypeStorage;
          vl->element_address = edp->element_address;
          if (!edp->Full) {
            free(vl);
            continue;
          } else {
            vl->slot_status = slot_status_t::kSlotStatusFull;
            FillVolumeName(vl, edp);
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
          vl->slot_type = slot_type_t::kSlotTypeStorage;
          vl->element_address = edp->element_address;
          if (edp->Full) {
            vl->slot_status = slot_status_t::kSlotStatusFull;
            FillVolumeName(vl, edp);
          } else {
            vl->slot_status = slot_status_t::kSlotStatusEmpty;
          }
          break;
        case SMC_ELEM_TYPE_IEE:
          /*
           * Import/Export bareos_slot_number
           */
          vl->slot_type = slot_type_t::kSlotTypeImport;
          vl->element_address = edp->element_address;
          if (edp->Full) {
            vl->slot_status = slot_status_t::kSlotStatusFull;
            FillVolumeName(vl, edp);
          } else {
            vl->slot_status = slot_status_t::kSlotStatusEmpty;
          }
          if (edp->InEnab) { vl->flags |= can_import; }
          if (edp->ExEnab) { vl->flags |= can_export; }
          if (edp->ImpExp) {
            vl->flags |= by_oper;
          } else {
            vl->flags |= by_mte;
          }
          break;
        case SMC_ELEM_TYPE_DTE:
          /*
           * Drive
           */
          vl->slot_type = slot_type_t::kSlotTypeDrive;
          vl->element_address = edp->element_address;
          if (edp->Full) {
            slot_number_t slot_mapping;
            vl->slot_status = slot_status_t::kSlotStatusFull;
            slot_mapping = GetBareosSlotNumberByElementAddress(
                &store->runtime_storage_status->storage_mapping,
                slot_type_t::kSlotTypeStorage, edp->src_se_addr);
            vl->currently_loaded_slot_number = slot_mapping;
            FillVolumeName(vl, edp);
          } else {
            vl->slot_status = slot_status_t::kSlotStatusEmpty;
          }
          break;
        default:
          vl->slot_type = slot_type_t::kSlotTypeUnknown;
          vl->element_address = edp->element_address;
          break;
      }
    }


    /*
     * Map physical storage address to logical one using the storage mappings.
     */
    vl->bareos_slot_number = GetBareosSlotNumberByElementAddress(
        &store->runtime_storage_status->storage_mapping, vl->slot_type,
        edp->element_address);
    if (vl->VolName) {
      Dmsg6(100,
            "Add phys_slot = %hd logi_slot=%hd loaded=%hd type=%hd status=%hd "
            "Vol=%s to SD list.\n",
            vl->element_address, vl->bareos_slot_number,
            vl->currently_loaded_slot_number, vl->slot_type, vl->slot_status,
            NPRT(vl->VolName));
    } else {
      Dmsg5(100,
            "Add phys_slot = %hd logi_slot=%hd loaded=%hd type=%hd status=%hd "
            "Vol=NULL to SD list.\n",
            vl->element_address, vl->bareos_slot_number,
            vl->currently_loaded_slot_number, vl->slot_type, vl->slot_status);
    }

    vol_list->binary_insert(vl, StorageCompareVolListEntry);
  } /* for */

  if (vol_list->size() == 0) {
    delete vol_list;
    vol_list = NULL;
  }

  CleanupNdmpSession(ndmp_sess);

  return vol_list;
}

/**
 * Update the mapping table from logical to physical storage addresses.
 */
bool NdmpUpdateStorageMappings(JobControlRecord* jcr, StorageResource* store)
{
  struct ndm_session* ndmp_sess;

  if (!GetRobotElementStatus(jcr, store, &ndmp_sess)) { return false; }

  NdmpFillStorageMappings(store, ndmp_sess);

  CleanupNdmpSession(ndmp_sess);

  return true;
}

/**
 * Update the mapping table from logical to physical storage addresses.
 */
bool NdmpUpdateStorageMappings(UaContext* ua, StorageResource* store)
{
  struct ndm_session* ndmp_sess;

  if (!GetRobotElementStatus(ua->jcr, store, &ndmp_sess)) { return false; }

  NdmpFillStorageMappings(store, ndmp_sess);

  CleanupNdmpSession(ndmp_sess);

  return true;
}

/**
 * Number of slots in a NDMP autochanger.
 */
slot_number_t NdmpGetNumSlots(UaContext* ua, StorageResource* store)
{
  slot_number_t slots = 0;

  /*
   * See if the mappings are already determined.
   */
  if (!NdmpUpdateStorageMappings(ua, store)) { return slots; }

  return store->runtime_storage_status->storage_mapping.se_count +
         store->runtime_storage_status->storage_mapping.iee_count;
}

/**
 * Number of drives in a NDMP autochanger.
 */
drive_number_t NdmpGetNumDrives(UaContext* ua, StorageResource* store)
{
  drive_number_t drives = 0;

  /*
   * See if the mappings are already determined.
   */
  if (!NdmpUpdateStorageMappings(ua, store)) { return drives; }

  return store->runtime_storage_status->storage_mapping.dte_count;
}

/**
 * Move a volume from one slot to an other in a NDMP autochanger.
 */
bool NdmpTransferVolume(UaContext* ua,
                        StorageResource* store,
                        slot_number_t src_slot,
                        slot_number_t dst_slot)
{
  bool retval = false;
  slot_number_t from_addr;
  slot_number_t to_addr;
  struct ndm_job_param ndmp_job;
  struct ndm_session* ndmp_sess;

  /*
   * See if this is an autochanger.
   */
  if (!store->autochanger || !store->ndmp_changer_device) { return retval; }

  if (!NdmpBuildStorageJob(ua->jcr, store, false, /* Setup Tape Agent */
                           true,                  /* Setup Robot Agent */
                           NDM_JOB_OP_MOVE_TAPE, &ndmp_job)) {
    return retval;
  }

  /*
   * Fill in the from and to address.
   *
   * As the upper level functions work with logical slot numbers convert them
   * to physical slot numbers for the actual NDMP operation.
   */
  from_addr = GetBareosSlotNumberByElementAddress(
      &store->runtime_storage_status->storage_mapping,
      slot_type_t::kSlotTypeStorage, src_slot);
  if (from_addr == kInvalidSlotNumber) {
    ua->ErrorMsg("No slot mapping for slot %hd\n", src_slot);
    return retval;
  }
  ndmp_job.from_addr = from_addr;
  ndmp_job.from_addr_given = 1;

  to_addr = GetElementAddressByBareosSlotNumber(
      &store->runtime_storage_status->storage_mapping,
      slot_type_t::kSlotTypeStorage, dst_slot);
  if (to_addr == kInvalidSlotNumber) {
    ua->ErrorMsg("No slot mapping for slot %hd\n", dst_slot);
    return retval;
  }
  ndmp_job.to_addr = to_addr;
  ndmp_job.to_addr_given = 1;

  ua->WarningMsg(_("transferring form slot %hd to slot %hd...\n"), src_slot,
                 dst_slot);

  /*
   * Set the remote robotics name to use.
   * We use the ndmscsi_target_from_str() function which parses the NDMJOB
   * format of a device in the form NAME[,[CNUM,]SID[,LUN]
   */
  ndmp_job.robot_target =
      (struct ndmscsi_target*)malloc(sizeof(struct ndmscsi_target));
  if (ndmscsi_target_from_str(ndmp_job.robot_target,
                              store->ndmp_changer_device) != 0) {
    free(ndmp_job.robot_target);
    return retval;
  }
  ndmp_job.have_robot = 1;
  ndmp_job.auto_remedy = 1;

  /*
   * Initialize a new NDMP session
   */
  ndmp_sess = (struct ndm_session*)malloc(sizeof(struct ndm_session));
  memset(ndmp_sess, 0, sizeof(struct ndm_session));

  if (!NdmpRunStorageJob(ua->jcr, store, ndmp_sess, &ndmp_job)) {
    CleanupNdmpSession(ndmp_sess);
    return retval;
  }

  retval = true;

  CleanupNdmpSession(ndmp_sess);

  return retval;
}

/**
 * reserve a NDMP Tape drive for a certain job
 * lock the devinfo list
 * check if any of the devices is available (deviceinfo.JobUsingDevice == 0)
 * set the JobId into deviceinfo.JobUsingDevice
 * unlock devinfo
 * return name of device that was reserved
 */
std::string reserve_ndmp_tapedevice_for_job(StorageResource* store,
                                            JobControlRecord* jcr)
{
  JobId_t jobid = jcr->JobId;
  std::string returnvalue;
  P(store->runtime_storage_status->ndmp_deviceinfo_lock);

  if (!store->runtime_storage_status->ndmp_deviceinfo.empty()) {
    for (auto devinfo = store->runtime_storage_status->ndmp_deviceinfo.begin();
         devinfo != store->runtime_storage_status->ndmp_deviceinfo.end();
         devinfo++) {
      if (devinfo->JobIdUsingDevice == 0) {
        devinfo->JobIdUsingDevice = jobid;
        returnvalue = devinfo->device;
        Jmsg(jcr, M_INFO, 0,
             _("successfully reserved NDMP Tape Device %s for job %d\n"),
             returnvalue.c_str(), jobid);
        break;
      } else {
        Jmsg(jcr, M_INFO, 0,
             _("NDMP Tape Device %s is already reserved for for job %d\n"),
             devinfo->device.c_str(), devinfo->JobIdUsingDevice);
      }
    }
  }
  V(store->runtime_storage_status->ndmp_deviceinfo_lock);
  return returnvalue;
}

/*
 * remove job from tapedevice
 */
bool unreserve_ndmp_tapedevice_for_job(StorageResource* store,
                                       JobControlRecord* jcr)
{
  JobId_t jobid = jcr->JobId;
  bool retval = false;
  P(store->runtime_storage_status->ndmp_deviceinfo_lock);

  if (!store->runtime_storage_status->ndmp_deviceinfo.empty()) {
    for (auto devinfo = store->runtime_storage_status->ndmp_deviceinfo.begin();
         devinfo != store->runtime_storage_status->ndmp_deviceinfo.end();
         devinfo++) {
      if (devinfo->JobIdUsingDevice == jobid) {
        devinfo->JobIdUsingDevice = 0;
        retval = true;
        Jmsg(jcr, M_INFO, 0,
             _("removed reservation of NDMP Tape Device %s for job %d\n"),
             devinfo->device.c_str(), jobid);
        break;
      }
    }
  }
  V(store->runtime_storage_status->ndmp_deviceinfo_lock);
  return retval;
}

/**
 * Lookup the name of a drive in a NDMP autochanger.
 */
char* lookup_ndmp_drive(StorageResource* store, drive_number_t drivenumber)
{
  int cnt = 0;
  char* tapedevice;
  BareosResource* tapedeviceres = nullptr;

  if (store->device) {
    foreach_alist (tapedeviceres, store->device) {
      if (cnt == drivenumber) {
        tapedevice = tapedeviceres->resource_name_;
        return tapedevice;
      }
      cnt++;
    }
  }

  return NULL;
}

/**
 * Lookup the drive index by device name in a NDMP autochanger.
 */
int lookup_ndmp_driveindex_by_name(StorageResource* store, char* drivename)
{
  int cnt = 0;

  if (!drivename) { return -1; }

  if (!store->runtime_storage_status->ndmp_deviceinfo.empty()) {
    for (auto devinfo = store->runtime_storage_status->ndmp_deviceinfo.begin();
         devinfo != store->runtime_storage_status->ndmp_deviceinfo.end();
         devinfo++) {
      if ((drivename == devinfo->device)) { return cnt; }
      cnt++;
    }
  }
  return -1;
}

/**
 * Perform an autochanger operation in a NDMP autochanger.
 */
bool NdmpAutochangerVolumeOperation(UaContext* ua,
                                    StorageResource* store,
                                    const char* operation,
                                    drive_number_t drive,
                                    slot_number_t slot)
{
  drive_number_t drive_mapping;
  int ndmp_operation;
  bool retval = false;
  struct ndm_job_param ndmp_job;
  struct ndm_session* ndmp_sess;

  Dmsg3(100,
        "NdmpAutochangerVolumeOperation: operation %s, drive %hd, slot %hd\n",
        operation, drive, slot);
  ua->WarningMsg(
      _("NdmpAutochangerVolumeOperation: operation %s, drive %hd, slot %hd\n"),
      operation, drive, slot);
  /*
   * See if this is an autochanger.
   */
  if (!store->autochanger || !store->ndmp_changer_device) { return retval; }

  if (bstrcmp(operation, "unmount") || bstrcmp(operation, "release")) {
    ndmp_operation = NDM_JOB_OP_UNLOAD_TAPE;
  } else if (bstrcmp(operation, "mount")) {
    ndmp_operation = NDM_JOB_OP_LOAD_TAPE;
  } else {
    ua->ErrorMsg("Illegal autochanger operation %s\n", operation);
    return retval;
  }

  if (!NdmpBuildStorageJob(ua->jcr, store, false, /* Setup Tape Agent */
                           true,                  /* Setup Robot Agent */
                           ndmp_operation, &ndmp_job)) {
    return retval;
  }

  /*
   * See if the mappings are already determined.
   */
  if (!NdmpUpdateStorageMappings(ua, store)) { return false; }

  if (slot != kInvalidSlotNumber) {
    slot_number_t slot_mapping;

    /*
     * Map the logical address to a physical one.
     */
    slot_mapping = GetElementAddressByBareosSlotNumber(
        &store->runtime_storage_status->storage_mapping,
        slot_type_t::kSlotTypeStorage, slot);
    if (slot_mapping == kInvalidSlotNumber) {
      ua->ErrorMsg("No slot mapping for slot %hd\n", slot);
      return retval;
    }
    ndmp_job.from_addr = slot_mapping;
    ndmp_job.from_addr_given = 1;
  }

  /*
   * Map the logical address to a physical one.
   */
  drive_mapping = GetElementAddressByBareosSlotNumber(
      &store->runtime_storage_status->storage_mapping,
      slot_type_t::kSlotTypeDrive, slot);
  if (drive_mapping == kInvalidSlotNumber) {
    ua->ErrorMsg("No slot mapping for drive %hd\n", drive);
    return retval;
  }
  ndmp_job.drive_addr = drive_mapping;
  ndmp_job.drive_addr_given = 1;

  /*
   * Set the remote robotics name to use.
   * We use the ndmscsi_target_from_str() function which parses the NDMJOB
   * format of a device in the form NAME[,[CNUM,]SID[,LUN]
   */
  ndmp_job.robot_target =
      (struct ndmscsi_target*)malloc(sizeof(struct ndmscsi_target));
  if (ndmscsi_target_from_str(ndmp_job.robot_target,
                              store->ndmp_changer_device) != 0) {
    free(ndmp_job.robot_target);
    return retval;
  }
  ndmp_job.have_robot = 1;
  ndmp_job.auto_remedy = 1;

  /*
   * Initialize a new NDMP session
   */
  ndmp_sess = (struct ndm_session*)malloc(sizeof(struct ndm_session));
  memset(ndmp_sess, 0, sizeof(struct ndm_session));

  if (!NdmpRunStorageJob(ua->jcr, store, ndmp_sess, &ndmp_job)) {
    CleanupNdmpSession(ndmp_sess);
    return retval;
  }

  retval = true;

  CleanupNdmpSession(ndmp_sess);

  return retval;
}

/**
 * Label a volume in a NDMP autochanger.
 */
bool NdmpSendLabelRequest(UaContext* ua,
                          StorageResource* store,
                          MediaDbRecord* mr,
                          MediaDbRecord* omr,
                          PoolDbRecord* pr,
                          bool relabel,
                          drive_number_t drive,
                          slot_number_t slot)
{
  bool retval = false;
  struct ndm_job_param ndmp_job;
  struct ndm_session* ndmp_sess;
  struct ndmmedia* media;

  Dmsg4(100,
        "ndmp_send_label_request: VolumeName=%s MediaType=%s PoolName=%s "
        "drive=%hd\n",
        mr->VolumeName, mr->MediaType, pr->Name, drive);

  /*
   * See if this is an autochanger.
   */
  if (!store->autochanger || !store->ndmp_changer_device) { return retval; }

  if (!NdmpBuildStorageJob(ua->jcr, store, true, /* Setup Tape Agent */
                           true,                 /* Setup Robot Agent */
                           NDM_JOB_OP_INIT_LABELS, &ndmp_job)) {
    return retval;
  }

  /*
   * Set the remote robotics name to use.
   * We use the ndmscsi_target_from_str() function which parses the NDMJOB
   * format of a device in the form NAME[,[CNUM,]SID[,LUN]
   */
  ndmp_job.robot_target =
      (struct ndmscsi_target*)malloc(sizeof(struct ndmscsi_target));
  if (ndmscsi_target_from_str(ndmp_job.robot_target,
                              store->ndmp_changer_device) != 0) {
    free(ndmp_job.robot_target);
    Dmsg0(100, "NdmpSendLabelRequest: no robot to use\n");
    return retval;
  }
  ndmp_job.have_robot = 1;
  ndmp_job.auto_remedy = 1;

  /*
   * Set the remote tape drive to use.
   */
  ndmp_job.tape_device =
      strdup(((DeviceResource*)(store->device->first()))->resource_name_);
  if (!ndmp_job.tape_device) { free(ndmp_job.robot_target); }

  /*
   * Insert a media entry of the slot to label.
   */
  if (IsSlotNumberValid(slot)) {
    slot_number_t slot_mapping;

    slot_mapping = GetElementAddressByBareosSlotNumber(
        &store->runtime_storage_status->storage_mapping,
        slot_type_t::kSlotTypeStorage, slot);
    if (slot_mapping == kInvalidSlotNumber) {
      ua->ErrorMsg("No slot mapping for slot %hd\n", slot);
      return retval;
    }
    media = ndma_store_media(&ndmp_job.media_tab, slot_mapping);
  } else {
    media = ndma_store_media(&ndmp_job.media_tab, 0);
  }
  bstrncpy(media->label, mr->VolumeName, NDMMEDIA_LABEL_MAX - 1);
  media->valid_label = NDMP9_VALIDITY_VALID;


  /*
   * Initialize a new NDMP session
   */
  ndmp_sess = (struct ndm_session*)malloc(sizeof(struct ndm_session));
  memset(ndmp_sess, 0, sizeof(struct ndm_session));

  if (!NdmpRunStorageJob(ua->jcr, store, ndmp_sess, &ndmp_job)) {
    CleanupNdmpSession(ndmp_sess);
    return retval;
  }

  retval = true;

  CleanupNdmpSession(ndmp_sess);

  return retval;
}


static bool ndmp_native_update_runtime_storage_status(JobControlRecord* jcr,
                                                      StorageResource* store)
{
  bool retval = true;

  P(store->runtime_storage_status->changer_lock);
  if (!do_ndmp_native_query_tape_and_robot_agents(jcr, store)) {
    retval = false;
  }
  V(store->runtime_storage_status->changer_lock);

  P(store->runtime_storage_status->changer_lock);
  if (!NdmpUpdateStorageMappings(jcr, store)) { retval = false; }
  V(store->runtime_storage_status->changer_lock);

  return retval;
}

bool ndmp_native_setup_robot_and_tape_for_native_backup_job(
    JobControlRecord* jcr,
    StorageResource* store,
    ndm_job_param& ndmp_job)
{
  slot_number_t driveaddress;
  std::string tapedevice;
  bool retval = false;
  if (!ndmp_native_update_runtime_storage_status(jcr, store)) {
    Jmsg(jcr, M_ERROR, 0,
         _("Failed getting updating runtime storage status\n"));
    return retval;
  }

  ndmp_job.robot_target =
      (struct ndmscsi_target*)malloc(sizeof(struct ndmscsi_target));
  if (ndmscsi_target_from_str(ndmp_job.robot_target,
                              store->ndmp_changer_device) != 0) {
    free(ndmp_job.robot_target);
    Dmsg0(100, "no robot to use\n");
    return retval;
  }

  tapedevice = reserve_ndmp_tapedevice_for_job(store, jcr);
  ndmp_job.tape_device = (char*)tapedevice.c_str();

  int driveindex = lookup_ndmp_driveindex_by_name(store, ndmp_job.tape_device);

  if (driveindex == -1) {
    Jmsg(jcr, M_ERROR, 0, _("Could not find driveindex of drive %s\n"),
         ndmp_job.tape_device);
    return retval;
  }

  driveaddress = GetElementAddressByBareosSlotNumber(
      &store->runtime_storage_status->storage_mapping,
      slot_type_t::kSlotTypeDrive, driveindex);
  if (driveaddress == kInvalidSlotNumber) {
    Jmsg(jcr, M_ERROR, 0,
         _("Could not lookup driveaddress for driveindex %d\n"), driveaddress);
    return retval;
  }
  ndmp_job.drive_addr = driveaddress;
  ndmp_job.drive_addr_given = 1;

  ndmp_job.have_robot = 1;
  /*
   * unload tape if tape is in drive
   */
  ndmp_job.auto_remedy = 1;
  ndmp_job.record_size = jcr->res.client->ndmp_blocksize;

  Jmsg(jcr, M_INFO, 0, _("Using Data  host %s\n"), ndmp_job.data_agent.host);
  Jmsg(jcr, M_INFO, 0, _("Using Tape  host:device:address  %s:%s:@%d\n"),
       ndmp_job.tape_agent.host, ndmp_job.tape_device, ndmp_job.drive_addr);
  Jmsg(jcr, M_INFO, 0, _("Using Robot host:device(ident)  %s:%s(%s)\n"),
       ndmp_job.robot_agent.host, ndmp_job.robot_target,
       store->runtime_storage_status->smc_ident);
  Jmsg(jcr, M_INFO, 0, _("Using Tape record size %d\n"), ndmp_job.record_size);

  return true;
}


#else
/**
 * Dummy entry points when NDMP not enabled.
 */
void DoNdmpStorageStatus(UaContext* ua, StorageResource* store, char* cmd)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}

dlist* ndmp_get_vol_list(UaContext* ua,
                         StorageResource* store,
                         bool listall,
                         bool scan)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return (dlist*)NULL;
}

slot_number_t NdmpGetNumSlots(UaContext* ua, StorageResource* store)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return 0;
}

drive_number_t NdmpGetNumDrives(UaContext* ua, StorageResource* store)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return 0;
}

bool NdmpTransferVolume(UaContext* ua,
                        StorageResource* store,
                        slot_number_t src_slot,
                        slot_number_t dst_slot)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

bool NdmpAutochangerVolumeOperation(UaContext* ua,
                                    StorageResource* store,
                                    const char* operation,
                                    drive_number_t drive,
                                    slot_number_t slot)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

bool NdmpSendLabelRequest(UaContext* ua,
                          StorageResource* store,
                          MediaDbRecord* mr,
                          MediaDbRecord* omr,
                          PoolDbRecord* pr,
                          bool relabel,
                          drive_number_t drive,
                          slot_number_t slot)
{
  Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

#endif /* HAVE_NDMP */
} /* namespace directordaemon */
