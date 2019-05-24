/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, August MM
 * Extracted from other source files by Marco van Wieringen, December 2011
 */
/**
 * @file
 * send commands to Storage daemon
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/authenticate.h"
#include "dird/getmsg.h"
#include "dird/job.h"
#include "dird/msgchan.h"
#include "dird/storage.h"
#include "dird/ua_label.h"
#include "dird/ua_server.h"
#include "lib/bsock_tcp.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/watchdog.h"

namespace directordaemon {

/* Commands sent to Storage daemon */
static char readlabelcmd[] = "readlabel %s Slot=%hd drive=%hd\n";
static char changerlistallcmd[] = "autochanger listall %s \n";
static char changerlistcmd[] = "autochanger list %s \n";
static char changerslotscmd[] = "autochanger slots %s\n";
static char changerdrivescmd[] = "autochanger drives %s\n";
static char changertransfercmd[] = "autochanger transfer %s %hd %hd \n";
static char changervolopslotcmd[] = "%s %s drive=%hd slot=%hd\n";
static char changervolopcmd[] = "%s %s drive=%hd\n";
static char canceljobcmd[] = "cancel Job=%s\n";
static char dotstatuscmd[] = ".status %s\n";
static char statuscmd[] = "status %s\n";
static char bandwidthcmd[] = "setbandwidth=%lld Job=%s\n";
static char pluginoptionscmd[] = "pluginoptions %s\n";
static char getSecureEraseCmd[] = "getSecureEraseCmd\n";

/* Responses received from Storage daemon */
static char OKBandwidth[] = "2000 OK Bandwidth\n";
static char OKpluginoptions[] = "2000 OK plugin options\n";
static char OKSecureEraseCmd[] = "2000 OK SDSecureEraseCmd %s\n";

/* Commands received from storage daemon that need scanning */
static char readlabelresponse[] = "3001 Volume=%s Slot=%hd";
static char changerslotsresponse[] = "slots=%hd\n";
static char changerdrivesresponse[] = "drives=%hd\n";

static void TerminateAndCloseJcrStoreSocket(JobControlRecord* jcr)
{
  if (jcr && jcr->store_bsock) {
    jcr->store_bsock->signal(BNET_TERMINATE);
    jcr->store_bsock->close();
    delete jcr->store_bsock;
    jcr->store_bsock = nullptr;
  }
}

bool ConnectToStorageDaemon(JobControlRecord* jcr,
                            int retry_interval,
                            int max_retry_time,
                            bool verbose)
{
  if (jcr->store_bsock) { return true; /* already connected */ }

  StorageResource* store;
  if (jcr->res.write_storage) {
    store = jcr->res.write_storage;
  } else {
    store = jcr->res.read_storage;
  }

  if (!store) {
    Dmsg0(200, "ConnectToStorageDaemon: Storage resource pointer is invalid\n");
    return false;
  }

  utime_t heart_beat;
  if (store->heartbeat_interval) {
    heart_beat = store->heartbeat_interval;
  } else {
    heart_beat = me->heartbeat_interval;
  }

  Dmsg2(100, "bNetConnect to Storage daemon %s:%d\n", store->address,
        store->SDport);
  std::unique_ptr<BareosSocket> sd(new BareosSocketTCP);
  if (!sd) { return false; }
  sd->SetSourceAddress(me->DIRsrc_addr);
  if (!sd->connect(jcr, retry_interval, max_retry_time, heart_beat,
                   _("Storage daemon"), store->address, nullptr, store->SDport,
                   verbose)) {
    return false;
  }

  if (store->IsTlsConfigured()) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            me->resource_name_, my_config->r_own_, qualified_resource_name)) {
      Dmsg0(100,
            "Could not generate qualified resource name for a storage "
            "resource\n");
      sd->close();
      return false;
    }

    if (!sd->DoTlsHandshake(TlsPolicy::kBnetTlsAuto, store, false,
                            qualified_resource_name.c_str(),
                            store->password_.value, jcr)) {
      Dmsg0(100, "Could not DoTlsHandshake() with storagedaemon\n");
      sd->close();
      return false;
    }
  }

  if (!AuthenticateWithStorageDaemon(sd.get(), jcr, store)) {
    sd->close();
    return false;
  }

  jcr->store_bsock = sd.release();
  return true;
}

BareosSocket* open_sd_bsock(UaContext* ua)
{
  StorageResource* store = ua->jcr->res.write_storage;

  if (!store) {
    Dmsg0(200, "open_sd_bsock: No storage resource pointer set\n");
    return nullptr;
  }

  if (!ua->jcr->store_bsock) {
    ua->SendMsg(_("Connecting to Storage daemon %s at %s:%d ...\n"),
                store->resource_name_, store->address, store->SDport);
    /* the next call will set ua->jcr->store_bsock */
    if (!ConnectToStorageDaemon(ua->jcr, 10, me->SDConnectTimeout, true)) {
      ua->ErrorMsg(_("Failed to connect to Storage daemon.\n"));
      return nullptr;
    }
  }
  return ua->jcr->store_bsock;
}

void CloseSdBsock(UaContext* ua)
{
  if (ua) { TerminateAndCloseJcrStoreSocket(ua->jcr); }
}

char* get_volume_name_from_SD(UaContext* ua,
                              slot_number_t Slot,
                              drive_number_t drive)
{
  BareosSocket* sd;
  StorageResource* store = ua->jcr->res.write_storage;
  char dev_name[MAX_NAME_LENGTH];
  char* VolName = nullptr;
  int rtn_slot;

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) {
    ua->ErrorMsg(_("Could not open SD socket.\n"));
    return nullptr;
  }
  bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
  BashSpaces(dev_name);

  /*
   * Ask storage daemon to read the label of the volume in a
   * specific slot of the autochanger using the drive number given.
   * This could change the loaded volume in the drive.
   */
  sd->fsend(readlabelcmd, dev_name, Slot, drive);
  Dmsg1(100, "Sent: %s", sd->msg);

  /*
   * Get Volume name in this Slot
   */
  while (sd->recv() >= 0) {
    ua->SendMsg("%s", sd->msg);
    Dmsg1(100, "Got: %s", sd->msg);
    if (strncmp(sd->msg, NT_("3001 Volume="), 12) == 0) {
      VolName = (char*)malloc(sd->message_length);
      if (sscanf(sd->msg, readlabelresponse, VolName, &rtn_slot) == 2) {
        break;
      }
      free(VolName);
      VolName = nullptr;
    }
  }
  CloseSdBsock(ua);
  Dmsg1(100, "get_vol_name=%s\n", NPRT(VolName));
  return VolName;
}

/**
 * We get the slot list from the Storage daemon.
 * If listall is set we run an 'autochanger listall' cmd
 * otherwise an 'autochanger list' cmd
 * If scan is set and listall is not, we return all slots found,
 * otherwise, we return only slots with valid barcodes (Volume names)
 *
 * Input (output of mxt-changer list):
 *
 * 0:vol2                Slot num:Volume Name
 *
 * Input (output of mxt-changer listall):
 *
 * Drive content:         D:Drive num:F:Slot loaded:Volume Name
 * D:0:F:2:vol2        or D:Drive num:E
 * D:1:F:42:vol42
 * D:3:E
 *
 * Slot content:
 * S:1:F:vol1             S:Slot num:F:Volume Name
 * S:2:E               or S:Slot num:E
 * S:3:F:vol4
 *
 * Import/Export tray slots:
 * I:10:F:vol10           I:Slot num:F:Volume Name
 * I:11:E              or I:Slot num:E
 * I:12:F:vol40
 *
 * If a drive is loaded, the slot *should* be empty
 */
dlist* native_get_vol_list(UaContext* ua,
                           StorageResource* store,
                           bool listall,
                           bool scan)
{
  int nr_fields;
  char* bp;
  char dev_name[MAX_NAME_LENGTH];
  char *field1, *field2, *field3, *field4, *field5;
  vol_list_t* vl = nullptr;
  dlist* vol_list;
  BareosSocket* sd = nullptr;

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) { return nullptr; }

  bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
  BashSpaces(dev_name);

  /*
   * Ask for autochanger list of volumes
   */
  if (listall) {
    sd->fsend(changerlistallcmd, dev_name);
  } else {
    sd->fsend(changerlistcmd, dev_name);
  }

  vol_list = new dlist(vl, &vl->link);

  /*
   * Read and organize list of Volumes
   */
  while (BnetRecv(sd) >= 0) {
    StripTrailingJunk(sd->msg);

    /*
     * Check for returned SD messages
     */
    if (sd->msg[0] == '3' && B_ISDIGIT(sd->msg[1]) && B_ISDIGIT(sd->msg[2]) &&
        B_ISDIGIT(sd->msg[3]) && sd->msg[4] == ' ') {
      ua->SendMsg("%s\n", sd->msg); /* pass them on to user */
      continue;
    }

    /*
     * Parse the message. list gives max 2 fields listall max 5.
     * We always make sure all fields are initialized to either
     * a value or nullptr.
     *
     * For autochanger list the following mapping is used:
     * - field1 == slotnr
     * - field2 == volumename
     *
     * For autochanger listall the following mapping is used:
     * - field1 == type
     * - field2 == slotnr
     * - field3 == content (E for Empty, F for Full)
     * - field4 == loaded (loaded slot if type == D)
     * - field4 == volumename (if type == S or I)
     * - field5 == volumename (if type == D)
     */
    field1 = sd->msg;
    field2 = strchr(sd->msg, ':');
    if (field2) {
      *field2++ = '\0';
      if (listall) {
        field3 = strchr(field2, ':');
        if (field3) {
          *field3++ = '\0';
          field4 = strchr(field3, ':');
          if (field4) {
            *field4++ = '\0';
            field5 = strchr(field4, ':');
            if (field5) {
              *field5++ = '\0';
              nr_fields = 5;
            } else {
              nr_fields = 4;
            }
          } else {
            nr_fields = 3;
            field5 = nullptr;
          }
        } else {
          nr_fields = 2;
          field4 = nullptr;
          field5 = nullptr;
        }
      } else {
        nr_fields = 2;
        field3 = nullptr;
        field4 = nullptr;
        field5 = nullptr;
      }
    } else {
      nr_fields = 1;
      field3 = nullptr;
      field4 = nullptr;
      field5 = nullptr;
    }

    /*
     * See if this is a parsable string from either list or listall
     * e.g. at least f1:f2
     */
    if (!field1 && !field2) { goto parse_error; }

    vl = (vol_list_t*)malloc(sizeof(vol_list_t));
    memset(vl, 0, sizeof(vol_list_t));

    if (scan && !listall) {
      /*
       * Scanning -- require only valid slot
       */
      vl->bareos_slot_number = atoi(field1);
      if (vl->bareos_slot_number <= 0) {
        ua->ErrorMsg(_("Invalid Slot number: %s\n"), sd->msg);
        free(vl);
        continue;
      }

      vl->slot_type = slot_type_t::kSlotTypeStorage;
      if (strlen(field2) > 0) {
        vl->slot_status = slot_status_t::kSlotStatusFull;
        vl->VolName = strdup(field2);
      } else {
        vl->slot_status = slot_status_t::kSlotStatusEmpty;
      }
      vl->element_address = INDEX_SLOT_OFFSET + vl->bareos_slot_number;
    } else if (!listall) {
      /*
       * Not scanning and not listall.
       */
      if (strlen(field2) == 0) {
        free(vl);
        continue;
      }

      if (!IsAnInteger(field1) ||
          (vl->bareos_slot_number = atoi(field1)) <= 0) {
        ua->ErrorMsg(_("Invalid Slot number: %s\n"), field1);
        free(vl);
        continue;
      }

      if (!IsVolumeNameLegal(ua, field2)) {
        ua->ErrorMsg(_("Invalid Volume name: %s\n"), field2);
        free(vl);
        continue;
      }

      vl->slot_type = slot_type_t::kSlotTypeStorage;
      vl->slot_status = slot_status_t::kSlotStatusFull;
      vl->VolName = strdup(field2);
      vl->element_address = INDEX_SLOT_OFFSET + vl->bareos_slot_number;
    } else {
      /*
       * Listall.
       */
      if (!field3) { goto parse_error; }

      switch (*field1) {
        case 'D':
          vl->slot_type = slot_type_t::kSlotTypeDrive;
          break;
        case 'S':
          vl->slot_type = slot_type_t::kSlotTypeStorage;
          break;
        case 'I':
          vl->slot_type = slot_type_t::kSlotTypeImport;
          vl->flags |= (can_import | can_export | by_oper);
          break;
        default:
          vl->slot_type = slot_type_t::kSlotTypeUnknown;
          break;
      }

      /*
       * For drives the Slot is the actual drive number.
       * For any other type its the actual slot number.
       */
      switch (vl->slot_type) {
        case slot_type_t::kSlotTypeDrive:
          if (!IsAnInteger(field2) ||
              (vl->bareos_slot_number = static_cast<slot_number_t>(
                   atoi(field2))) == kInvalidSlotNumber) {
            ua->ErrorMsg(_("Invalid Drive number: %s\n"), field2);
            free(vl);
            continue;
          }
          vl->element_address = INDEX_DRIVE_OFFSET + vl->bareos_slot_number;
          if (vl->element_address >= INDEX_MAX_DRIVES) {
            ua->ErrorMsg(_("Drive number %hd greater then "
                           "INDEX_MAX_DRIVES(%hd) please increase define\n"),
                         vl->bareos_slot_number, INDEX_MAX_DRIVES);
            free(vl);
            continue;
          }
          break;
        default:
          if (!IsAnInteger(field2) ||
              (vl->bareos_slot_number = atoi(field2)) <= 0) {
            ua->ErrorMsg(_("Invalid Slot number: %s\n"), field2);
            free(vl);
            continue;
          }
          vl->element_address = INDEX_SLOT_OFFSET + vl->bareos_slot_number;
          break;
      }

      switch (*field3) {
        case 'E':
          vl->slot_status = slot_status_t::kSlotStatusEmpty;
          break;
        case 'F':
          vl->slot_status = slot_status_t::kSlotStatusFull;
          switch (vl->slot_type) {
            case slot_type_t::kSlotTypeStorage:
            case slot_type_t::kSlotTypeImport:
              if (field4) { vl->VolName = strdup(field4); }
              break;
            case slot_type_t::kSlotTypeDrive:
              if (field4) {
                vl->currently_loaded_slot_number =
                    static_cast<slot_number_t>(atoi(field4));
              }
              if (field5) { vl->VolName = strdup(field5); }
              break;
            default:
              break;
          }
          break;
        default:
          vl->slot_status = slot_status_t::kSlotStatusUnknown;
          break;
      }
    }

    if (vl->VolName) {
      Dmsg6(100,
            "Add index = %hd slot=%hd loaded=%hd type=%hd content=%hd Vol=%s "
            "to SD list.\n",
            vl->element_address, vl->bareos_slot_number,
            vl->currently_loaded_slot_number, vl->slot_type, vl->slot_type,
            NPRT(vl->VolName));
    } else {
      Dmsg5(100,
            "Add index = %hd slot=%hd loaded=%hd type=%hd content=%hd Vol=NULL "
            "to SD list.\n",
            vl->element_address, vl->bareos_slot_number,
            vl->currently_loaded_slot_number, vl->slot_type, vl->slot_type);
    }

    vol_list->binary_insert(vl, StorageCompareVolListEntry);
    continue;

  parse_error:
    /*
     * We encountered a parse error, see how many replacements
     * we done of ':' with '\0' by looking at the nr_fields
     * variable and undo those. Number of undo's are nr_fields - 1
     */
    while (nr_fields > 1 && (bp = strchr(sd->msg, '\0')) != nullptr) {
      *bp = ':';
      nr_fields--;
    }
    ua->ErrorMsg(_("Illegal output from autochanger %s: %s\n"),
                 (listall) ? _("listall") : _("list"), sd->msg);
    free(vl);
    continue;
  }

  CloseSdBsock(ua);

  if (vol_list->size() == 0) {
    delete vol_list;
    vol_list = nullptr;
  }

  return vol_list;
}

/**
 * We get the number of slots in the changer from the SD
 */
slot_number_t NativeGetNumSlots(UaContext* ua, StorageResource* store)
{
  char dev_name[MAX_NAME_LENGTH];
  BareosSocket* sd;
  slot_number_t slots = 0;

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) { return 0; }

  bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
  BashSpaces(dev_name);

  /*
   * Ask for autochanger number of slots
   */
  sd->fsend(changerslotscmd, dev_name);
  while (sd->recv() >= 0) {
    if (sscanf(sd->msg, changerslotsresponse, &slots) == 1) {
      break;
    } else {
      ua->SendMsg("%s", sd->msg);
    }
  }
  CloseSdBsock(ua);
  ua->SendMsg(_("Device \"%s\" has %hd slots.\n"), store->dev_name(), slots);

  return slots;
}

/**
 * We get the number of drives in the changer from the SD
 */
drive_number_t NativeGetNumDrives(UaContext* ua, StorageResource* store)
{
  char dev_name[MAX_NAME_LENGTH];
  BareosSocket* sd;
  drive_number_t drives = 0;

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) { return 0; }

  bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
  BashSpaces(dev_name);

  /*
   * Ask for autochanger number of drives
   */
  sd->fsend(changerdrivescmd, dev_name);
  while (sd->recv() >= 0) {
    if (sscanf(sd->msg, changerdrivesresponse, &drives) == 1) {
      break;
    } else {
      ua->SendMsg("%s", sd->msg);
    }
  }
  CloseSdBsock(ua);

  return drives;
}

/**
 * Cancel a running job on a storage daemon. Used by the interactive cancel
 * command to cancel a JobId on a Storage Daemon this can be used when the
 * Director already removed the Job and thinks it finished but the Storage
 * Daemon still thinks its active.
 */
bool CancelStorageDaemonJob(UaContext* ua, StorageResource* store, char* JobId)
{
  JobControlRecord* control_jcr;

  control_jcr = new_control_jcr("*JobCancel*", JT_SYSTEM);

  control_jcr->res.write_storage = store;

  /* the next call will set control_jcr->store_bsock */
  if (!ConnectToStorageDaemon(control_jcr, 10, me->SDConnectTimeout, true)) {
    ua->ErrorMsg(_("Failed to connect to Storage daemon.\n"));
    return false;
  }

  Dmsg0(200, "Connected to storage daemon\n");
  control_jcr->store_bsock->fsend("cancel Job=%s\n", JobId);
  while (control_jcr->store_bsock->recv() >= 0) {
    ua->SendMsg("%s", control_jcr->store_bsock->msg);
  }
  TerminateAndCloseJcrStoreSocket(control_jcr);
  FreeJcr(control_jcr);

  return true;
}

/**
 * Cancel a running job on a storage daemon. The interactive flag sets
 * if we are interactive or not e.g. when doing an interactive cancel
 * or a system invoked one.
 */
bool CancelStorageDaemonJob(UaContext* ua,
                            JobControlRecord* jcr,
                            bool interactive)
{
  if (!ua->jcr->res.write_storage_list) {
    if (jcr->res.read_storage_list) {
      CopyWstorage(ua->jcr, jcr->res.read_storage_list, _("Job resource"));
    } else {
      CopyWstorage(ua->jcr, jcr->res.write_storage_list, _("Job resource"));
    }
  } else {
    UnifiedStorageResource store;
    if (jcr->res.read_storage_list) {
      store.store = jcr->res.read_storage;
    } else {
      store.store = jcr->res.write_storage;
    }
    if (!store.store) {
      Dmsg0(200, "CancelStorageDaemonJob: No storage resource pointer set\n");
      return false;
    }
    SetWstorage(ua->jcr, &store);
  }

  /* the next call will set ua->jcr->store_bsock */
  if (!ConnectToStorageDaemon(ua->jcr, 10, me->SDConnectTimeout, true)) {
    if (interactive) {
      ua->ErrorMsg(_("Failed to connect to Storage daemon.\n"));
    }
    return false;
  }
  Dmsg0(200, "Connected to storage daemon\n");
  ua->jcr->store_bsock->fsend(canceljobcmd, jcr->Job);
  while (ua->jcr->store_bsock->recv() >= 0) {
    if (interactive) { ua->SendMsg("%s", ua->jcr->store_bsock->msg); }
  }

  if (jcr->store_bsock) {
    jcr->store_bsock->SetTimedOut();
    jcr->store_bsock->SetTerminated();
  }

  TerminateAndCloseJcrStoreSocket(ua->jcr);

  if (!interactive) { jcr->sd_canceled = true; }

  SdMsgThreadSendSignal(jcr, TIMEOUT_SIGNAL);

  if (interactive) { jcr->MyThreadSendSignal(TIMEOUT_SIGNAL); }

  return true;
}

void CancelStorageDaemonJob(JobControlRecord* jcr)
{
  if (jcr->sd_canceled) { return; /* cancel only once */ }

  UaContext* ua = new_ua_context(jcr);
  JobControlRecord* control_jcr = new_control_jcr("*JobCancel*", JT_SYSTEM);

  ua->jcr = control_jcr;
  if (jcr->store_bsock) {
    if (!CancelStorageDaemonJob(ua, jcr, false)) { goto bail_out; }
  }

bail_out:
  FreeJcr(control_jcr);
  FreeUaContext(ua);
}

void DoNativeStorageStatus(UaContext* ua, StorageResource* store, char* cmd)
{
  UnifiedStorageResource lstore;

  lstore.store = store;
  PmStrcpy(lstore.store_source, _("unknown source"));
  SetWstorage(ua->jcr, &lstore);

  if (!ua->api) {
    ua->SendMsg(_("Connecting to Storage daemon %s at %s:%d\n"),
                store->resource_name_, store->address, store->SDport);
  }

  /* the next call will set ua->jcr->store_bsock */
  if (!ConnectToStorageDaemon(ua->jcr, 10, me->SDConnectTimeout, false)) {
    ua->SendMsg(_("\nFailed to connect to Storage daemon %s.\n====\n"),
                store->resource_name_);
    return;
  }

  Dmsg0(20, _("Connected to storage daemon\n"));

  if (cmd) {
    ua->jcr->store_bsock->fsend(dotstatuscmd, cmd);

  } else {
    int cnt = 0;
    DeviceResource* device = nullptr;
    PoolMem devicenames;

    /*
     * Build a list of devicenames that belong to this storage defintion.
     */
    foreach_alist (device, store->device) {
      if (cnt == 0) {
        PmStrcpy(devicenames, device->resource_name_);
      } else {
        PmStrcat(devicenames, ",");
        PmStrcat(devicenames, device->resource_name_);
      }
      cnt++;
    }

    BashSpaces(devicenames);
    ua->jcr->store_bsock->fsend(statuscmd, devicenames.c_str());
  }

  while (ua->jcr->store_bsock->recv() >= 0) {
    ua->SendMsg("%s", ua->jcr->store_bsock->msg);
  }

  TerminateAndCloseJcrStoreSocket(ua->jcr);

  return;
}

/**
 * Ask the autochanger to move a volume from one slot to another.
 * You have to update the database slots yourself afterwards.
 */
bool NativeTransferVolume(UaContext* ua,
                          StorageResource* store,
                          slot_number_t src_slot,
                          slot_number_t dst_slot)
{
  BareosSocket* sd = nullptr;
  bool retval = true;
  char dev_name[MAX_NAME_LENGTH];

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) { return false; }

  bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
  BashSpaces(dev_name);

  /*
   * Ask for autochanger transfer of volumes
   */
  sd->fsend(changertransfercmd, dev_name, src_slot, dst_slot);
  while (BnetRecv(sd) >= 0) {
    StripTrailingJunk(sd->msg);

    /*
     * Check for returned SD messages
     */
    if (sd->msg[0] == '3' && B_ISDIGIT(sd->msg[1]) && B_ISDIGIT(sd->msg[2]) &&
        B_ISDIGIT(sd->msg[3]) && sd->msg[4] == ' ') {
      /*
       * See if this is a failure msg.
       */
      if (sd->msg[1] == '9') retval = false;

      ua->SendMsg("%s\n", sd->msg); /* pass them on to user */
      continue;
    }

    ua->SendMsg("%s\n", sd->msg); /* pass them on to user */
  }
  CloseSdBsock(ua);

  return retval;
}

/**
 * Ask the autochanger to perform a mount, umount or release operation.
 */
bool NativeAutochangerVolumeOperation(UaContext* ua,
                                      StorageResource* store,
                                      const char* operation,
                                      drive_number_t drive,
                                      slot_number_t slot)
{
  BareosSocket* sd = nullptr;
  bool retval = true;
  char dev_name[MAX_NAME_LENGTH];

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) { return false; }

  bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
  BashSpaces(dev_name);

  if (IsSlotNumberValid(slot)) {
    sd->fsend(changervolopslotcmd, operation, dev_name, drive, slot);
  } else {
    sd->fsend(changervolopcmd, operation, dev_name, drive);
  }

  /*
   * We use BgetDirmsg here and not BnetRecv because as part of
   * the mount request the stored can request catalog information for
   * any plugin who listens to the bsdEventLabelVerified event.
   * As we don't want to loose any non protocol data e.g. errors
   * without a 3xxx prefix we set the allow_any_message of
   * BgetDirmsg to true and as such is behaves like a normal
   * BnetRecv for any non protocol messages.
   */
  while (BgetDirmsg(sd, true) >= 0) { ua->SendMsg("%s", sd->msg); }

  CloseSdBsock(ua);

  return retval;
}

/**
 * sends request to report secure erase command
 * and receives the command or "*None*" of not set
 */
bool SendSecureEraseReqToSd(JobControlRecord* jcr)
{
  int32_t n;
  BareosSocket* sd = jcr->store_bsock;

  if (!jcr->SDSecureEraseCmd) {
    jcr->SDSecureEraseCmd = GetPoolMemory(PM_NAME);
  }

  sd->fsend(getSecureEraseCmd);
  while ((n = BgetDirmsg(sd)) >= 0) {
    jcr->SDSecureEraseCmd =
        CheckPoolMemorySize(jcr->SDSecureEraseCmd, sd->message_length);
    if (sscanf(sd->msg, OKSecureEraseCmd, jcr->SDSecureEraseCmd) == 1) {
      Dmsg1(421, "Got SD Secure Erase Cmd: %s\n", jcr->SDSecureEraseCmd);
      break;
    } else {
      Jmsg(jcr, M_WARNING, 0, _("Unexpected SD Secure Erase Cmd: %s\n"),
           sd->msg);
      PmStrcpy(jcr->SDSecureEraseCmd, "*None*");
      return false;
    }
  }

  return true;
}

bool SendBwlimitToSd(JobControlRecord* jcr, const char* Job)
{
  BareosSocket* sd = jcr->store_bsock;

  sd->fsend(bandwidthcmd, jcr->max_bandwidth, Job);
  if (!response(jcr, sd, OKBandwidth, "Bandwidth", DISPLAY_ERROR)) {
    jcr->max_bandwidth = 0; /* can't set bandwidth limit */
    return false;
  }

  return true;
}

bool DoStorageResolve(UaContext* ua, StorageResource* store)
{
  BareosSocket* sd;
  UnifiedStorageResource lstore;

  lstore.store = store;
  PmStrcpy(lstore.store_source, _("unknown source"));
  SetWstorage(ua->jcr, &lstore);

  ua->jcr->res.write_storage = store;
  if (!(sd = open_sd_bsock(ua))) { return false; }

  for (int i = 1; i < ua->argc; i++) {
    if (!*ua->argk[i]) { continue; }

    sd->fsend("resolve %s", ua->argk[i]);
    while (sd->recv() >= 0) { ua->SendMsg("%s", sd->msg); }
  }

  CloseSdBsock(ua);

  return true;
}

bool SendStoragePluginOptions(JobControlRecord* jcr)
{
  int i;
  PoolMem cur_plugin_options(PM_MESSAGE);
  const char* plugin_options;
  BareosSocket* sd = jcr->store_bsock;

  if (jcr->res.job && jcr->res.job->SdPluginOptions &&
      jcr->res.job->SdPluginOptions->size()) {
    foreach_alist_index (i, plugin_options, jcr->res.job->SdPluginOptions) {
      PmStrcpy(cur_plugin_options, plugin_options);
      BashSpaces(cur_plugin_options.c_str());

      sd->fsend(pluginoptionscmd, cur_plugin_options.c_str());
      if (!response(jcr, sd, OKpluginoptions, "PluginOptions", DISPLAY_ERROR)) {
        return false;
      }
    }
  }

  return true;
}
} /* namespace directordaemon */
