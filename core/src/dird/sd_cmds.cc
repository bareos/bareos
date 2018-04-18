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

#include "bareos.h"
#include "dird.h"
#include "dird/authenticate.h"
#include "dird/getmsg.h"
#include "dird/job.h"
#include "dird/msgchan.h"
#include "dird/storage.h"
#include "dird/ua_label.h"
#include "dird/ua_server.h"
#include "lib/bnet.h"

/* Commands sent to Storage daemon */
static char readlabelcmd[] =
   "readlabel %s Slot=%hd drive=%hd\n";
static char changerlistallcmd[] =
   "autochanger listall %s \n";
static char changerlistcmd[] =
   "autochanger list %s \n";
static char changerslotscmd[] =
   "autochanger slots %s\n";
static char changerdrivescmd[] =
   "autochanger drives %s\n";
static char changertransfercmd[] =
   "autochanger transfer %s %hd %hd \n";
static char changervolopslotcmd[] =
   "%s %s drive=%hd slot=%hd\n";
static char changervolopcmd[] =
   "%s %s drive=%hd\n";
static char canceljobcmd[] =
   "cancel Job=%s\n";
static char dotstatuscmd[] =
   ".status %s\n";
static char statuscmd[] =
   "status %s\n";
static char bandwidthcmd[] =
   "setbandwidth=%lld Job=%s\n";
static char pluginoptionscmd[] =
   "pluginoptions %s\n";
static char getSecureEraseCmd[] =
   "getSecureEraseCmd\n";

/* Responses received from Storage daemon */
static char OKBandwidth[] =
   "2000 OK Bandwidth\n";
static char OKpluginoptions[] =
   "2000 OK plugin options\n";
static char OKSecureEraseCmd[] =
   "2000 OK SDSecureEraseCmd %s\n";

/* Commands received from storage daemon that need scanning */
static char readlabelresponse[] =
   "3001 Volume=%s Slot=%hd";
static char changerslotsresponse[] =
   "slots=%hd\n";
static char changerdrivesresponse[] =
   "drives=%hd\n";

/**
 * Establish a connection with the Storage daemon and perform authentication.
 */
bool connect_to_storage_daemon(JobControlRecord *jcr, int retry_interval,
                               int max_retry_time, bool verbose)
{
   BareosSocket *sd;
   StoreResource *store;
   utime_t heart_beat;

   if (jcr->store_bsock) {
      return true;                    /* already connected */
   }

   sd = New(BareosSocketTCP);

   /*
    * If there is a write storage use it
    */
   if (jcr->res.wstore) {
      store = jcr->res.wstore;
   } else {
      store = jcr->res.rstore;
   }

   if (store->heartbeat_interval) {
      heart_beat = store->heartbeat_interval;
   } else {
      heart_beat = me->heartbeat_interval;
   }

   /*
    * Open message channel with the Storage daemon
    */
   Dmsg2(100, "bnet_connect to Storage daemon %s:%d\n", store->address, store->SDport);
   sd->set_source_address(me->DIRsrc_addr);
   if (!sd->connect(jcr, retry_interval, max_retry_time, heart_beat, _("Storage daemon"),
                    store->address, NULL, store->SDport, verbose)) {
      delete sd;
      sd = NULL;
   }

   if (sd == NULL) {
      return false;
   }
   jcr->store_bsock = sd;

   if (!authenticate_with_storage_daemon(jcr, store)) {
      sd->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
      return false;
   }

   return true;
}

/**
 * Open a connection to a SD.
 */
BareosSocket *open_sd_bsock(UaContext *ua)
{
   StoreResource *store = ua->jcr->res.wstore;

   if (!ua->jcr->store_bsock) {
      ua->send_msg(_("Connecting to Storage daemon %s at %s:%d ...\n"),
                   store->name(), store->address, store->SDport);
      if (!connect_to_storage_daemon(ua->jcr, 10, me->SDConnectTimeout, true)) {
         ua->error_msg(_("Failed to connect to Storage daemon.\n"));
         return NULL;
      }
   }
   return ua->jcr->store_bsock;
}

/**
 * Close a connection to a SD.
 */
void close_sd_bsock(UaContext *ua)
{
   if (ua->jcr->store_bsock) {
      ua->jcr->store_bsock->signal(BNET_TERMINATE);
      ua->jcr->store_bsock->close();
      delete ua->jcr->store_bsock;
      ua->jcr->store_bsock = NULL;
   }
}

/**
 * We get the volume name from the SD
 */
char *get_volume_name_from_SD(UaContext *ua, slot_number_t Slot, drive_number_t drive)
{
   BareosSocket *sd;
   StoreResource *store = ua->jcr->res.wstore;
   char dev_name[MAX_NAME_LENGTH];
   char *VolName = NULL;
   int rtn_slot;

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      ua->error_msg(_("Could not open SD socket.\n"));
      return NULL;
   }
   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);

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
      ua->send_msg("%s", sd->msg);
      Dmsg1(100, "Got: %s", sd->msg);
      if (strncmp(sd->msg, NT_("3001 Volume="), 12) == 0) {
         VolName = (char *)malloc(sd->msglen);
         if (sscanf(sd->msg, readlabelresponse, VolName, &rtn_slot) == 2) {
            break;
         }
         free(VolName);
         VolName = NULL;
      }
   }
   close_sd_bsock(ua);
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
dlist *native_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan)
{
   int nr_fields;
   char *bp;
   char dev_name[MAX_NAME_LENGTH];
   char *field1, *field2, *field3, *field4, *field5;
   vol_list_t *vl = NULL;
   dlist *vol_list;
   BareosSocket *sd = NULL;

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      return NULL;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);

   /*
    * Ask for autochanger list of volumes
    */
   if (listall) {
      sd->fsend(changerlistallcmd , dev_name);
   } else {
      sd->fsend(changerlistcmd, dev_name);
   }

   vol_list = New(dlist(vl, &vl->link));

   /*
    * Read and organize list of Volumes
    */
   while (bnet_recv(sd) >= 0) {
      strip_trailing_junk(sd->msg);

      /*
       * Check for returned SD messages
       */
      if (sd->msg[0] == '3' && B_ISDIGIT(sd->msg[1]) &&
          B_ISDIGIT(sd->msg[2]) && B_ISDIGIT(sd->msg[3]) &&
          sd->msg[4] == ' ') {
         ua->send_msg("%s\n", sd->msg);   /* pass them on to user */
         continue;
      }

      /*
       * Parse the message. list gives max 2 fields listall max 5.
       * We always make sure all fields are initialized to either
       * a value or NULL.
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
                  field5 = NULL;
               }
            } else {
               nr_fields = 2;
               field4 = NULL;
               field5 = NULL;
            }
         } else {
            nr_fields = 2;
            field3 = NULL;
            field4 = NULL;
            field5 = NULL;
         }
      } else {
         nr_fields = 1;
         field3 = NULL;
         field4 = NULL;
         field5 = NULL;
      }

      /*
       * See if this is a parsable string from either list or listall
       * e.g. at least f1:f2
       */
      if (!field1 && !field2) {
         goto parse_error;
      }

      vl = (vol_list_t *)malloc(sizeof(vol_list_t));
      memset(vl, 0, sizeof(vol_list_t));

      if (scan && !listall) {
         /*
          * Scanning -- require only valid slot
          */
         vl->Slot = atoi(field1);
         if (vl->Slot <= 0) {
            ua->error_msg(_("Invalid Slot number: %s\n"), sd->msg);
            free(vl);
            continue;
         }

         vl->Type = slot_type_normal;
         if (strlen(field2) > 0) {
            vl->Content = slot_content_full;
            vl->VolName = bstrdup(field2);
         } else {
            vl->Content = slot_content_empty;
         }
         vl->Index = INDEX_SLOT_OFFSET + vl->Slot;
      } else if (!listall) {
         /*
          * Not scanning and not listall.
          */
         if (strlen(field2) == 0) {
            free(vl);
            continue;
         }

         if (!is_an_integer(field1) || (vl->Slot = atoi(field1)) <= 0) {
            ua->error_msg(_("Invalid Slot number: %s\n"), field1);
            free(vl);
            continue;
         }

         if (!is_volume_name_legal(ua, field2)) {
            ua->error_msg(_("Invalid Volume name: %s\n"), field2);
            free(vl);
            continue;
         }

         vl->Type = slot_type_normal;
         vl->Content = slot_content_full;
         vl->VolName = bstrdup(field2);
         vl->Index = INDEX_SLOT_OFFSET + vl->Slot;
      } else {
         /*
          * Listall.
          */
         if (!field3) {
            goto parse_error;
         }

         switch (*field1) {
         case 'D':
            vl->Type = slot_type_drive;
            break;
         case 'S':
            vl->Type = slot_type_normal;
            break;
         case 'I':
            vl->Type = slot_type_import;
            vl->Flags |= (can_import | can_export | by_oper);
            break;
         default:
            vl->Type = slot_type_unknown;
            break;
         }

         /*
          * For drives the Slot is the actual drive number.
          * For any other type its the actual slot number.
          */
         switch (vl->Type) {
         case slot_type_drive:
            if (!is_an_integer(field2) || (vl->Slot = atoi(field2)) < 0) {
               ua->error_msg(_("Invalid Drive number: %s\n"), field2);
               free(vl);
               continue;
            }
            vl->Index = INDEX_DRIVE_OFFSET + vl->Slot;
            if (vl->Index >= INDEX_MAX_DRIVES) {
               ua->error_msg(_("Drive number %hd greater then INDEX_MAX_DRIVES(%hd) please increase define\n"),
                             vl->Slot, INDEX_MAX_DRIVES);
               free(vl);
               continue;
            }
            break;
         default:
            if (!is_an_integer(field2) || (vl->Slot = atoi(field2)) <= 0) {
               ua->error_msg(_("Invalid Slot number: %s\n"), field2);
               free(vl);
               continue;
            }
            vl->Index = INDEX_SLOT_OFFSET + vl->Slot;
            break;
         }

         switch (*field3) {
         case 'E':
            vl->Content = slot_content_empty;
            break;
         case 'F':
            vl->Content = slot_content_full;
            switch (vl->Type) {
            case slot_type_normal:
            case slot_type_import:
               if (field4) {
                  vl->VolName = bstrdup(field4);
               }
               break;
            case slot_type_drive:
               if (field4) {
                  vl->Loaded = atoi(field4);
               }
               if (field5) {
                  vl->VolName = bstrdup(field5);
               }
               break;
            default:
               break;
            }
            break;
         default:
            vl->Content = slot_content_unknown;
            break;
         }
      }

      if (vl->VolName) {
         Dmsg6(100, "Add index = %hd slot=%hd loaded=%hd type=%hd content=%hd Vol=%s to SD list.\n",
               vl->Index, vl->Slot, vl->Loaded, vl->Type, vl->Content, NPRT(vl->VolName));
      } else {
         Dmsg5(100, "Add index = %hd slot=%hd loaded=%hd type=%hd content=%hd Vol=NULL to SD list.\n",
               vl->Index, vl->Slot, vl->Loaded, vl->Type, vl->Content);
      }

      vol_list->binary_insert(vl, storage_compare_vol_list_entry);
      continue;

parse_error:
      /*
       * We encountered a parse error, see how many replacements
       * we done of ':' with '\0' by looking at the nr_fields
       * variable and undo those. Number of undo's are nr_fields - 1
       */
      while (nr_fields > 1 && (bp = strchr(sd->msg, '\0')) != NULL) {
         *bp = ':';
         nr_fields--;
      }
      ua->error_msg(_("Illegal output from autochanger %s: %s\n"),
                   (listall) ? _("listall") : _("list"), sd->msg);
      free(vl);
      continue;
   }

   close_sd_bsock(ua);

   if (vol_list->size() == 0) {
      delete vol_list;
      vol_list = NULL;
   }

   return vol_list;
}

/**
 * We get the number of slots in the changer from the SD
 */
slot_number_t native_get_num_slots(UaContext *ua, StoreResource *store)
{
   char dev_name[MAX_NAME_LENGTH];
   BareosSocket *sd;
   slot_number_t slots = 0;

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      return 0;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);

   /*
    * Ask for autochanger number of slots
    */
   sd->fsend(changerslotscmd, dev_name);
   while (sd->recv() >= 0) {
      if (sscanf(sd->msg, changerslotsresponse, &slots) == 1) {
         break;
      } else {
         ua->send_msg("%s", sd->msg);
      }
   }
   close_sd_bsock(ua);
   ua->send_msg(_("Device \"%s\" has %hd slots.\n"), store->dev_name(), slots);

   return slots;
}

/**
 * We get the number of drives in the changer from the SD
 */
drive_number_t native_get_num_drives(UaContext *ua, StoreResource *store)
{
   char dev_name[MAX_NAME_LENGTH];
   BareosSocket *sd;
   drive_number_t drives = 0;

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      return 0;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);

   /*
    * Ask for autochanger number of drives
    */
   sd->fsend(changerdrivescmd, dev_name);
   while (sd->recv() >= 0) {
      if (sscanf(sd->msg, changerdrivesresponse, &drives) == 1) {
         break;
      } else {
         ua->send_msg("%s", sd->msg);
      }
   }
   close_sd_bsock(ua);

   return drives;
}

/**
 * Cancel a running job on a storage daemon. Used by the interactive cancel
 * command to cancel a JobId on a Storage Daemon this can be used when the
 * Director already removed the Job and thinks it finished but the Storage
 * Daemon still thinks its active.
 */
bool cancel_storage_daemon_job(UaContext *ua, StoreResource *store, char *JobId)
{
   BareosSocket *sd;
   JobControlRecord *control_jcr;

   control_jcr = new_control_jcr("*JobCancel*", JT_SYSTEM);

   control_jcr->res.wstore = store;
   if (!connect_to_storage_daemon(control_jcr, 10, me->SDConnectTimeout, true)) {
      ua->error_msg(_("Failed to connect to Storage daemon.\n"));
   }

   Dmsg0(200, "Connected to storage daemon\n");
   sd = control_jcr->store_bsock;
   sd->fsend("cancel Job=%s\n", JobId);
   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }
   sd->signal(BNET_TERMINATE);
   sd->close();
   delete control_jcr->store_bsock;
   control_jcr->store_bsock = NULL;

   free_jcr(control_jcr);

   return true;
}

/**
 * Cancel a running job on a storage daemon. The interactive flag sets
 * if we are interactive or not e.g. when doing an interactive cancel
 * or a system invoked one.
 */
bool cancel_storage_daemon_job(UaContext *ua, JobControlRecord *jcr, bool interactive)
{
   BareosSocket *sd;
   UnifiedStoreResource store;

   if (!ua->jcr->res.wstorage) {
      if (jcr->res.rstorage) {
         copy_wstorage(ua->jcr, jcr->res.rstorage, _("Job resource"));
      } else {
         copy_wstorage(ua->jcr, jcr->res.wstorage, _("Job resource"));
      }
   } else {
      if (jcr->res.rstorage) {
         store.store = jcr->res.rstore;
      } else {
         store.store = jcr->res.wstore;
      }
      set_wstorage(ua->jcr, &store);
   }

   if (!connect_to_storage_daemon(ua->jcr, 10, me->SDConnectTimeout, true)) {
      if (interactive) {
         ua->error_msg(_("Failed to connect to Storage daemon.\n"));
      }
      return false;
   }
   Dmsg0(200, "Connected to storage daemon\n");
   sd = ua->jcr->store_bsock;
   sd->fsend(canceljobcmd, jcr->Job);
   while (sd->recv() >= 0) {
      if (interactive) {
         ua->send_msg("%s", sd->msg);
      }
   }
   sd->signal(BNET_TERMINATE);
   sd->close();
   delete ua->jcr->store_bsock;
   ua->jcr->store_bsock = NULL;
   if (!interactive) {
      jcr->sd_canceled = true;
   }
   jcr->store_bsock->set_timed_out();
   jcr->store_bsock->set_terminated();
   sd_msg_thread_send_signal(jcr, TIMEOUT_SIGNAL);

   /*
    * An interactive cancel means we need to send a signal to the actual
    * controlling JobControlRecord of the Job to let it know it got canceled.
    */
   if (interactive) {
      jcr->my_thread_send_signal(TIMEOUT_SIGNAL);
   }

   return true;
}

/**
 * Cancel a running job on a storage daemon. System invoked
 * non interactive version this builds a ua context and calls
 * the interactive one with the interactive flag set to false.
 */
void cancel_storage_daemon_job(JobControlRecord *jcr)
{
   UaContext *ua;
   JobControlRecord *control_jcr;

   if (jcr->sd_canceled) {
      return;                   /* cancel only once */
   }

   ua = new_ua_context(jcr);
   control_jcr = new_control_jcr("*JobCancel*", JT_SYSTEM);

   ua->jcr = control_jcr;
   if (jcr->store_bsock) {
      if (!cancel_storage_daemon_job(ua, jcr, false)) {
         goto bail_out;
      }
   }

bail_out:
   free_jcr(control_jcr);
   free_ua_context(ua);
}

/**
 * Get the status of a remote storage daemon.
 */
void do_native_storage_status(UaContext *ua, StoreResource *store, char *cmd)
{
   BareosSocket *sd;
   UnifiedStoreResource lstore;

   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(ua->jcr, &lstore);

   /*
    * Try connecting for up to 15 seconds
    */
   if (!ua->api) {
      ua->send_msg(_("Connecting to Storage daemon %s at %s:%d\n"),
                   store->name(), store->address, store->SDport);
   }

   if (!connect_to_storage_daemon(ua->jcr, 10, me->SDConnectTimeout, false)) {
      ua->send_msg(_("\nFailed to connect to Storage daemon %s.\n====\n"),
                   store->name());
      if (ua->jcr->store_bsock) {
         ua->jcr->store_bsock->close();
         delete ua->jcr->store_bsock;
         ua->jcr->store_bsock = NULL;
      }
      return;
   }

   Dmsg0(20, _("Connected to storage daemon\n"));
   sd = ua->jcr->store_bsock;
   if (cmd) {
      sd->fsend(dotstatuscmd, cmd);

   } else {
      int cnt = 0;
      DeviceResource *device;
      PoolMem devicenames;

      /*
       * Build a list of devicenames that belong to this storage defintion.
       */
      foreach_alist(device, store->device) {
         if (cnt == 0) {
            pm_strcpy(devicenames, device->name());
         } else {
            pm_strcat(devicenames, ",");
            pm_strcat(devicenames, device->name());
         }
         cnt++;
      }

      bash_spaces(devicenames);
      sd->fsend(statuscmd, devicenames.c_str());
   }

   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
   }

   sd->signal(BNET_TERMINATE);
   sd->close();
   delete ua->jcr->store_bsock;
   ua->jcr->store_bsock = NULL;

   return;
}

/**
 * Ask the autochanger to move a volume from one slot to another.
 * You have to update the database slots yourself afterwards.
 */
bool native_transfer_volume(UaContext *ua, StoreResource *store,
                            slot_number_t src_slot, slot_number_t dst_slot)
{
   BareosSocket *sd = NULL;
   bool retval = true;
   char dev_name[MAX_NAME_LENGTH];

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      return false;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);

   /*
    * Ask for autochanger transfer of volumes
    */
   sd->fsend(changertransfercmd, dev_name, src_slot, dst_slot);
   while (bnet_recv(sd) >= 0) {
      strip_trailing_junk(sd->msg);

      /*
       * Check for returned SD messages
       */
      if (sd->msg[0] == '3' && B_ISDIGIT(sd->msg[1]) &&
          B_ISDIGIT(sd->msg[2]) && B_ISDIGIT(sd->msg[3]) &&
          sd->msg[4] == ' ') {
         /*
          * See if this is a failure msg.
          */
         if (sd->msg[1] == '9')
            retval = false;

         ua->send_msg("%s\n", sd->msg);   /* pass them on to user */
         continue;
      }

      ua->send_msg("%s\n", sd->msg);   /* pass them on to user */
   }
   close_sd_bsock(ua);

   return retval;
}

/**
 * Ask the autochanger to perform a mount, umount or release operation.
 */
bool native_autochanger_volume_operation(UaContext *ua, StoreResource *store,
                                         const char *operation, drive_number_t drive, slot_number_t slot)
{
   BareosSocket *sd = NULL;
   bool retval = true;
   char dev_name[MAX_NAME_LENGTH];

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      return false;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);

   if (slot > 0) {
      sd->fsend(changervolopslotcmd, operation, dev_name, drive, slot);
   } else {
      sd->fsend(changervolopcmd, operation, dev_name, drive);
   }

   /*
    * We use bget_dirmsg here and not bnet_recv because as part of
    * the mount request the stored can request catalog information for
    * any plugin who listens to the bsdEventLabelVerified event.
    * As we don't want to loose any non protocol data e.g. errors
    * without a 3xxx prefix we set the allow_any_message of
    * bget_dirmsg to true and as such is behaves like a normal
    * bnet_recv for any non protocol messages.
    */
   while (bget_dirmsg(sd, true) >= 0) {
      ua->send_msg("%s", sd->msg);
   }

   close_sd_bsock(ua);

   return retval;
}

/**
 * sends request to report secure erase command
 * and receives the command or "*None*" of not set
 */
bool send_secure_erase_req_to_sd(JobControlRecord *jcr) {
   int32_t n;
   BareosSocket *sd = jcr->store_bsock;

   if (!jcr->SDSecureEraseCmd) {
      jcr->SDSecureEraseCmd = get_pool_memory(PM_NAME);
   }

   sd->fsend(getSecureEraseCmd);
   while ((n = bget_dirmsg(sd)) >= 0) {
      jcr->SDSecureEraseCmd = check_pool_memory_size(jcr->SDSecureEraseCmd, sd->msglen);
      if (sscanf(sd->msg, OKSecureEraseCmd, jcr->SDSecureEraseCmd) == 1) {
         Dmsg1(421, "Got SD Secure Erase Cmd: %s\n", jcr->SDSecureEraseCmd);
         break;
      } else {
         Jmsg(jcr, M_WARNING, 0, _("Unexpected SD Secure Erase Cmd: %s\n"), sd->msg);
         pm_strcpy(jcr->SDSecureEraseCmd, "*None*");
         return false;
      }
   }

   return true;
}

bool send_bwlimit_to_sd(JobControlRecord *jcr, const char *Job)
{
   BareosSocket *sd = jcr->store_bsock;

   sd->fsend(bandwidthcmd, jcr->max_bandwidth, Job);
   if (!response(jcr, sd, OKBandwidth, "Bandwidth", DISPLAY_ERROR)) {
      jcr->max_bandwidth = 0;      /* can't set bandwidth limit */
      return false;
   }

   return true;
}

/**
 * resolve a host on a storage daemon
 */
bool do_storage_resolve(UaContext *ua, StoreResource *store)
{
   BareosSocket *sd;
   UnifiedStoreResource lstore;

   lstore.store = store;
   pm_strcpy(lstore.store_source, _("unknown source"));
   set_wstorage(ua->jcr, &lstore);

   ua->jcr->res.wstore = store;
   if (!(sd = open_sd_bsock(ua))) {
      return false;
   }

   for (int i = 1; i < ua->argc; i++) {
      if (!*ua->argk[i]) {
          continue;
       }

       sd->fsend("resolve %s", ua->argk[i]);
       while (sd->recv() >= 0) {
          ua->send_msg("%s", sd->msg);
       }
   }

   close_sd_bsock(ua);

   return true;
}

/**
 * send Job specific plugin options to a storage daemon
 */
bool do_storage_plugin_options(JobControlRecord *jcr)
{
   int i;
   PoolMem cur_plugin_options(PM_MESSAGE);
   const char *plugin_options;
   BareosSocket *sd = jcr->store_bsock;

   if (jcr->res.job &&
       jcr->res.job->SdPluginOptions &&
       jcr->res.job->SdPluginOptions->size()) {
      foreach_alist_index(i, plugin_options, jcr->res.job->SdPluginOptions) {
         pm_strcpy(cur_plugin_options, plugin_options);
         bash_spaces(cur_plugin_options.c_str());

         sd->fsend(pluginoptionscmd, cur_plugin_options.c_str());
         if (!response(jcr, sd, OKpluginoptions, "PluginOptions", DISPLAY_ERROR)) {
            return false;
         }
      }
   }

   return true;
}
