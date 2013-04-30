/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.

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
/*
 *
 *   Bacula Director -- Tape labeling commands
 *
 *     Kern Sibbald, April MMIII
 *
 */

#include "bacula.h"
#include "dird.h"

/* Slot list definition */
typedef struct s_vol_list {
   struct s_vol_list *next;
   char *VolName;
   int Slot;
} vol_list_t;


/* Forward referenced functions */
static int do_label(UAContext *ua, const char *cmd, int relabel);
static void label_from_barcodes(UAContext *ua, int drive);
static bool send_label_request(UAContext *ua, MEDIA_DBR *mr, MEDIA_DBR *omr,
               POOL_DBR *pr, int relabel, bool media_record_exits, int drive);
static vol_list_t *get_vol_list_from_SD(UAContext *ua, bool scan);
static void free_vol_list(vol_list_t *vol_list);
static bool is_cleaning_tape(UAContext *ua, MEDIA_DBR *mr, POOL_DBR *pr);
static BSOCK *open_sd_bsock(UAContext *ua);
static void close_sd_bsock(UAContext *ua);
static char *get_volume_name_from_SD(UAContext *ua, int Slot, int drive);
static int get_num_slots_from_SD(UAContext *ua);


/*
 * Label a tape
 *
 *   label storage=xxx volume=vvv
 */
int label_cmd(UAContext *ua, const char *cmd)
{
   return do_label(ua, cmd, 0);       /* standard label */
}

int relabel_cmd(UAContext *ua, const char *cmd)
{
   return do_label(ua, cmd, 1);      /* relabel tape */
}

static bool get_user_slot_list(UAContext *ua, char *slot_list, int num_slots)
{
   int i;
   const char *msg;

   /* slots are numbered 1 to num_slots */
   for (int i=0; i <= num_slots; i++) {
      slot_list[i] = 0;
   }
   i = find_arg_with_value(ua, "slots");
   if (i == -1) {  /* not found */
      i = find_arg_with_value(ua, "slot");
   }
   if (i > 0) {
      /* scan slot list in ua->argv[i] */
      char *p, *e, *h;
      int beg, end;

      strip_trailing_junk(ua->argv[i]);
      for (p=ua->argv[i]; p && *p; p=e) {
         /* Check for list */
         e = strchr(p, ',');
         if (e) {
            *e++ = 0;
         }
         /* Check for range */
         h = strchr(p, '-');             /* range? */
         if (h == p) {
            msg = _("Negative numbers not permitted\n");
            goto bail_out;
         }
         if (h) {
            *h++ = 0;
            if (!is_an_integer(h)) {
               msg = _("Range end is not integer.\n");
               goto bail_out;
            }
            skip_spaces(&p);
            if (!is_an_integer(p)) {
               msg = _("Range start is not an integer.\n");
               goto bail_out;
            }
            beg = atoi(p);
            end = atoi(h);
            if (end < beg) {
               msg = _("Range end not bigger than start.\n");
               goto bail_out;
            }
         } else {
            skip_spaces(&p);
            if (!is_an_integer(p)) {
               msg = _("Input value is not an integer.\n");
               goto bail_out;
            }
            beg = end = atoi(p);
         }
         if (beg <= 0 || end <= 0) {
            msg = _("Values must be be greater than zero.\n");
            goto bail_out;
         }
         if (end > num_slots) {
            msg = _("Slot too large.\n");
            goto bail_out;
         }
         for (i=beg; i<=end; i++) {
            slot_list[i] = 1;         /* Turn on specified range */
         }
      }
   } else {
      /* Turn everything on */
      for (i=1; i <= num_slots; i++) {
         slot_list[i] = 1;
      }
   }
   if (debug_level >= 100) {
      Dmsg0(100, "Slots turned on:\n");
      for (i=1; i <= num_slots; i++) {
         if (slot_list[i]) {
            Dmsg1(100, "%d\n", i);
         }
      }
   }
   return true;

bail_out:
   Dmsg1(100, "Problem with user selection ERR=%s\n", msg);
   return false;
}

/*
 * Update Slots corresponding to Volumes in autochanger
 */
void update_slots(UAContext *ua)
{
   USTORE store;
   vol_list_t *vl, *vol_list = NULL;
   MEDIA_DBR mr;
   char *slot_list;
   bool scan;
   int max_slots;
   int drive;
   int Enabled = 1;
   bool have_enabled;
   int i;


   if (!open_client_db(ua)) {
      return;
   }
   store.store = get_storage_resource(ua, true/*arg is storage*/);
   if (!store.store) {
      return;
   }
   pm_strcpy(store.store_source, _("command line"));
   set_wstorage(ua->jcr, &store);
   drive = get_storage_drive(ua, store.store);

   scan = find_arg(ua, NT_("scan")) >= 0;
   if ((i=find_arg_with_value(ua, NT_("Enabled"))) >= 0) {
      Enabled = get_enabled(ua, ua->argv[i]);
      if (Enabled < 0) {
         return;
      }
      have_enabled = true;
   } else {
      have_enabled = false;
   }

   max_slots = get_num_slots_from_SD(ua);
   Dmsg1(100, "max_slots=%d\n", max_slots);
   if (max_slots <= 0) {
      ua->warning_msg(_("No slots in changer to scan.\n"));
      return;
   }
   slot_list = (char *)malloc(max_slots+1);
   if (!get_user_slot_list(ua, slot_list, max_slots)) {
      free(slot_list);
      return;
   }

   vol_list = get_vol_list_from_SD(ua, scan);

   if (!vol_list) {
      ua->warning_msg(_("No Volumes found to label, or no barcodes.\n"));
      goto bail_out;
   }

   /* First zap out any InChanger with StorageId=0 */
   db_sql_query(ua->db, "UPDATE Media SET InChanger=0 WHERE StorageId=0", NULL, NULL);

   /* Walk through the list updating the media records */
   for (vl=vol_list; vl; vl=vl->next) {
      if (vl->Slot > max_slots) {
         ua->warning_msg(_("Slot %d greater than max %d ignored.\n"),
            vl->Slot, max_slots);
         continue;
      }
      /* Check if user wants us to look at this slot */
      if (!slot_list[vl->Slot]) {
         Dmsg1(100, "Skipping slot=%d\n", vl->Slot);
         continue;
      }
      /* If scanning, we read the label rather than the barcode */
      if (scan) {
         if (vl->VolName) {
            free(vl->VolName);
            vl->VolName = NULL;
         }
         vl->VolName = get_volume_name_from_SD(ua, vl->Slot, drive);
         Dmsg2(100, "Got Vol=%s from SD for Slot=%d\n", vl->VolName, vl->Slot);
      }
      slot_list[vl->Slot] = 0;        /* clear Slot */
      mr.Slot = vl->Slot;
      mr.InChanger = 1;
      mr.MediaId = 0;                 /* Force using VolumeName */
      if (vl->VolName) {
         bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
      } else {
         mr.VolumeName[0] = 0;
      }
      set_storageid_in_mr(store.store, &mr);
      Dmsg4(100, "Before make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
            mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
      db_lock(ua->db);
      /* Set InChanger to zero for this Slot */
      db_make_inchanger_unique(ua->jcr, ua->db, &mr);
      db_unlock(ua->db);
      Dmsg4(100, "After make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
            mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
      if (!vl->VolName) {
         Dmsg1(100, "No VolName for Slot=%d setting InChanger to zero.\n", vl->Slot);
         ua->info_msg(_("No VolName for Slot=%d InChanger set to zero.\n"), vl->Slot);
         continue;
      }
      db_lock(ua->db);
      Dmsg4(100, "Before get MR: Vol=%s slot=%d inchanger=%d sid=%d\n",
            mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
         Dmsg4(100, "After get MR: Vol=%s slot=%d inchanger=%d sid=%d\n",
            mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
         /* If Slot, Inchanger, and StorageId have changed, update the Media record */
         if (mr.Slot != vl->Slot || !mr.InChanger || mr.StorageId != store.store->StorageId) {
            mr.Slot = vl->Slot;
            mr.InChanger = 1;
            if (have_enabled) {
               mr.Enabled = Enabled;
            }
            set_storageid_in_mr(store.store, &mr);
            if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
               ua->error_msg("%s", db_strerror(ua->db));
            } else {
               ua->info_msg(_(
                 "Catalog record for Volume \"%s\" updated to reference slot %d.\n"),
                 mr.VolumeName, mr.Slot);
            }
         } else {
            ua->info_msg(_("Catalog record for Volume \"%s\" is up to date.\n"),
               mr.VolumeName);
         }
         db_unlock(ua->db);
         continue;
      } else {
         ua->warning_msg(_("Volume \"%s\" not found in catalog. Slot=%d InChanger set to zero.\n"),
             mr.VolumeName, vl->Slot);
      }
      db_unlock(ua->db);
   }
   mr.clear();
   mr.InChanger = 1;
   set_storageid_in_mr(store.store, &mr);
   db_lock(ua->db);
   for (int i=1; i <= max_slots; i++) {
      if (slot_list[i]) {
         mr.Slot = i;
         /* Set InChanger to zero for this Slot */
         db_make_inchanger_unique(ua->jcr, ua->db, &mr);
      }
   }
   db_unlock(ua->db);

bail_out:

   free_vol_list(vol_list);
   free(slot_list);
   close_sd_bsock(ua);

   return;
}


/*
 * Common routine for both label and relabel
 */
static int do_label(UAContext *ua, const char *cmd, int relabel)
{
   USTORE store;
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];
   MEDIA_DBR mr, omr;
   POOL_DBR pr;
   bool print_reminder = true;
   bool label_barcodes = false;
   int ok = FALSE;
   int i, j;
   int drive;
   bool media_record_exists = false;
   static const char *barcode_keyword[] = {
      "barcode",
      "barcodes",
      NULL};


   memset(&pr, 0, sizeof(pr));
   if (!open_client_db(ua)) {
      return 1;
   }

   /* Look for one of the barcode keywords */
   if (!relabel && (i=find_arg_keyword(ua, barcode_keyword)) >= 0) {
      /* Now find the keyword in the list */
      if ((j = find_arg(ua, barcode_keyword[i])) > 0) {
         *ua->argk[j] = 0;      /* zap barcode keyword */
      }
      label_barcodes = true;
   }

   store.store = get_storage_resource(ua, true/*use default*/);
   if (!store.store) {
      return 1;
   }
   pm_strcpy(store.store_source, _("command line"));
   set_wstorage(ua->jcr, &store);
   drive = get_storage_drive(ua, store.store);

   if (label_barcodes) {
      label_from_barcodes(ua, drive);
      return 1;
   }

   /* If relabel get name of Volume to relabel */
   if (relabel) {
      /* Check for oldvolume=name */
      i = find_arg_with_value(ua, "oldvolume");
      if (i >= 0) {
         bstrncpy(omr.VolumeName, ua->argv[i], sizeof(omr.VolumeName));
         if (db_get_media_record(ua->jcr, ua->db, &omr)) {
            goto checkVol;
         }
         ua->error_msg("%s", db_strerror(ua->db));
      }
      /* No keyword or Vol not found, ask user to select */
      if (!select_media_dbr(ua, &omr)) {
         return 1;
      }

      /* Require Volume to be Purged or Recycled */
checkVol:
      if (strcmp(omr.VolStatus, "Purged") != 0 && strcmp(omr.VolStatus, "Recycle") != 0) {
         ua->error_msg(_("Volume \"%s\" has VolStatus %s. It must be Purged or Recycled before relabeling.\n"),
            omr.VolumeName, omr.VolStatus);
         return 1;
      }
   }

   /* Check for volume=NewVolume */
   i = find_arg_with_value(ua, "volume");
   if (i >= 0) {
      pm_strcpy(ua->cmd, ua->argv[i]);
      goto checkName;
   }

   /* Get a new Volume name */
   for ( ;; ) {
      media_record_exists = false;
      if (!get_cmd(ua, _("Enter new Volume name: "))) {
         return 1;
      }
checkName:
      if (!is_volume_name_legal(ua, ua->cmd)) {
         continue;
      }

      /* Search by Media name so set VolumeName and clear MediaId. */
      mr.MediaId = 0;
      bstrncpy(mr.VolumeName, ua->cmd, sizeof(mr.VolumeName));

      /* If VolBytes are zero the Volume is not labeled */
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
         if (mr.VolBytes != 0) {
             ua->error_msg(_("Media record for new Volume \"%s\" already exists.\n"),
                mr.VolumeName);
             continue;
          }
          media_record_exists = true;
      }
      break;                          /* Got it */
   }

   /* If autochanger, request slot */
   i = find_arg_with_value(ua, "slot");
   if (i >= 0) {
      mr.Slot = atoi(ua->argv[i]);
      if (mr.Slot < 0) {
         mr.Slot = 0;
      }
      mr.InChanger = mr.Slot > 0;  /* if slot give assume in changer */
   } else if (store.store->autochanger) {
      if (!get_pint(ua, _("Enter slot (0 or Enter for none): "))) {
         return 1;
      }
      mr.Slot = ua->pint32_val;
      if (mr.Slot < 0) {
         mr.Slot = 0;
      }
      mr.InChanger = mr.Slot > 0;  /* if slot give assume in changer */
   }
   set_storageid_in_mr(store.store, &mr);

   bstrncpy(mr.MediaType, store.store->media_type, sizeof(mr.MediaType));

   /* Must select Pool if not already done */
   if (pr.PoolId == 0) {
      memset(&pr, 0, sizeof(pr));
      if (!select_pool_dbr(ua, &pr)) {
         return 1;
      }
   }

   ok = send_label_request(ua, &mr, &omr, &pr, relabel, media_record_exists, drive);

   if (ok) {
      sd = ua->jcr->store_bsock;
      if (relabel) {
         /* Delete the old media record */
         if (!db_delete_media_record(ua->jcr, ua->db, &omr)) {
            ua->error_msg(_("Delete of Volume \"%s\" failed. ERR=%s"),
               omr.VolumeName, db_strerror(ua->db));
         } else {
            ua->info_msg(_("Old volume \"%s\" deleted from catalog.\n"),
               omr.VolumeName);
            /* Update the number of Volumes in the pool */
            pr.NumVols--;
            if (!db_update_pool_record(ua->jcr, ua->db, &pr)) {
               ua->error_msg("%s", db_strerror(ua->db));
            }
         }
      }
      if (ua->automount) {
         bstrncpy(dev_name, store.store->dev_name(), sizeof(dev_name));
         ua->info_msg(_("Requesting to mount %s ...\n"), dev_name);
         bash_spaces(dev_name);
         bnet_fsend(sd, "mount %s drive=%d", dev_name, drive);
         unbash_spaces(dev_name);
         while (bnet_recv(sd) >= 0) {
            ua->send_msg("%s", sd->msg);
            /* Here we can get
             *  3001 OK mount. Device=xxx      or
             *  3001 Mounted Volume vvvv
             *  3002 Device "DVD-Writer" (/dev/hdc) is mounted.
             *  3906 is cannot mount non-tape
             * So for those, no need to print a reminder
             */
            if (strncmp(sd->msg, "3001 ", 5) == 0 ||
                strncmp(sd->msg, "3002 ", 5) == 0 ||
                strncmp(sd->msg, "3906 ", 5) == 0) {
               print_reminder = false;
            }
         }
      }
   }
   if (print_reminder) {
      ua->info_msg(_("Do not forget to mount the drive!!!\n"));
   }
   close_sd_bsock(ua);

   return 1;
}

/*
 * Request SD to send us the slot:barcodes, then wiffle
 *  through them all labeling them.
 */
static void label_from_barcodes(UAContext *ua, int drive)
{
   STORE *store = ua->jcr->wstore;
   POOL_DBR pr;
   MEDIA_DBR mr, omr;
   vol_list_t *vl, *vol_list = NULL;
   bool media_record_exists;
   char *slot_list;
   int max_slots;

  
   max_slots = get_num_slots_from_SD(ua);
   if (max_slots <= 0) {
      ua->warning_msg(_("No slots in changer to scan.\n"));
      return;
   }
   slot_list = (char *)malloc(max_slots+1);
   if (!get_user_slot_list(ua, slot_list, max_slots)) {
      goto bail_out;
   }

   vol_list = get_vol_list_from_SD(ua, false /*no scan*/);

   if (!vol_list) {
      ua->warning_msg(_("No Volumes found to label, or no barcodes.\n"));
      goto bail_out;
   }

   /* Display list of Volumes and ask if he really wants to proceed */
   ua->send_msg(_("The following Volumes will be labeled:\n"
                  "Slot  Volume\n"
                  "==============\n"));
   for (vl=vol_list; vl; vl=vl->next) {
      if (!vl->VolName || !slot_list[vl->Slot]) {
         continue;
      }
      ua->send_msg("%4d  %s\n", vl->Slot, vl->VolName);
   }
   if (!get_yesno(ua, _("Do you want to label these Volumes? (yes|no): ")) ||
       (ua->pint32_val == 0)) {
      goto bail_out;
   }
   /* Select a pool */
   memset(&pr, 0, sizeof(pr));
   if (!select_pool_dbr(ua, &pr)) {
      goto bail_out;
   }

   /* Fire off the label requests */
   for (vl=vol_list; vl; vl=vl->next) {
      if (!vl->VolName || !slot_list[vl->Slot]) {
         continue;
      }
      mr.clear();
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
      media_record_exists = false;
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
          if (mr.VolBytes != 0) {
             ua->warning_msg(_("Media record for Slot %d Volume \"%s\" already exists.\n"),
                vl->Slot, mr.VolumeName);
             mr.Slot = vl->Slot;
             mr.InChanger = mr.Slot > 0;  /* if slot give assume in changer */
             set_storageid_in_mr(store, &mr);
             if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
                ua->error_msg(_("Error setting InChanger: ERR=%s"), db_strerror(ua->db));
             }
             continue;
          }
          media_record_exists = true;
      }
      mr.InChanger = mr.Slot > 0;  /* if slot give assume in changer */
      set_storageid_in_mr(store, &mr);
      /*
       * Deal with creating cleaning tape here. Normal tapes created in
       *  send_label_request() below
       */
      if (is_cleaning_tape(ua, &mr, &pr)) {
         if (media_record_exists) {      /* we update it */
            mr.VolBytes = 1;             /* any bytes to indicate it exists */
            bstrncpy(mr.VolStatus, "Cleaning", sizeof(mr.VolStatus));
            mr.MediaType[0] = 0;
            set_storageid_in_mr(store, &mr);
            if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
                ua->error_msg("%s", db_strerror(ua->db));
            }
         } else {                        /* create the media record */
            if (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
               ua->error_msg(_("Maximum pool Volumes=%d reached.\n"), pr.MaxVols);
               goto bail_out;
            }
            set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
            bstrncpy(mr.VolStatus, "Cleaning", sizeof(mr.VolStatus));
            mr.MediaType[0] = 0;
            set_storageid_in_mr(store, &mr);
            if (db_create_media_record(ua->jcr, ua->db, &mr)) {
               ua->send_msg(_("Catalog record for cleaning tape \"%s\" successfully created.\n"),
                  mr.VolumeName);
               pr.NumVols++;          /* this is a bit suspect */
               if (!db_update_pool_record(ua->jcr, ua->db, &pr)) {
                  ua->error_msg("%s", db_strerror(ua->db));
               }
            } else {
               ua->error_msg(_("Catalog error on cleaning tape: %s"), db_strerror(ua->db));
            }
         }
         continue;                    /* done, go handle next volume */
      }
      bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));

      mr.Slot = vl->Slot;
      send_label_request(ua, &mr, &omr, &pr, 0, media_record_exists, drive);
   }


bail_out:
   free(slot_list);
   free_vol_list(vol_list);
   close_sd_bsock(ua);

   return;
}

/*
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
bool is_volume_name_legal(UAContext *ua, const char *name)
{
   int len;
   const char *p;
   const char *accept = ":.-_";

   /* Restrict the characters permitted in the Volume name */
   for (p=name; *p; p++) {
      if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
         continue;
      }
      if (ua) {
         ua->error_msg(_("Illegal character \"%c\" in a volume name.\n"), *p);
      }
      return 0;
   }
   len = strlen(name);
   if (len >= MAX_NAME_LENGTH) {
      if (ua) {
         ua->error_msg(_("Volume name too long.\n"));
      }
      return 0;
   }
   if (len == 0) {
      if (ua) {
         ua->error_msg(_("Volume name must be at least one character long.\n"));
      }
      return 0;
   }
   return 1;
}

/*
 * NOTE! This routine opens the SD socket but leaves it open
 */
static bool send_label_request(UAContext *ua, MEDIA_DBR *mr, MEDIA_DBR *omr,
                               POOL_DBR *pr, int relabel, bool media_record_exists,
                               int drive)
{
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];
   bool ok = false;
   bool is_dvd = false;
   uint64_t VolBytes = 0;

   if (!(sd=open_sd_bsock(ua))) {
      return false;
   }
   bstrncpy(dev_name, ua->jcr->wstore->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   bash_spaces(mr->VolumeName);
   bash_spaces(mr->MediaType);
   bash_spaces(pr->Name);
   if (relabel) {
      bash_spaces(omr->VolumeName);
      sd->fsend("relabel %s OldName=%s NewName=%s PoolName=%s "
                     "MediaType=%s Slot=%d drive=%d",
                 dev_name, omr->VolumeName, mr->VolumeName, pr->Name, 
                 mr->MediaType, mr->Slot, drive);
      ua->send_msg(_("Sending relabel command from \"%s\" to \"%s\" ...\n"),
         omr->VolumeName, mr->VolumeName);
   } else {
      sd->fsend("label %s VolumeName=%s PoolName=%s MediaType=%s "
                     "Slot=%d drive=%d",
                 dev_name, mr->VolumeName, pr->Name, mr->MediaType, 
                 mr->Slot, drive);
      ua->send_msg(_("Sending label command for Volume \"%s\" Slot %d ...\n"),
         mr->VolumeName, mr->Slot);
      Dmsg6(100, "label %s VolumeName=%s PoolName=%s MediaType=%s Slot=%d drive=%d\n",
         dev_name, mr->VolumeName, pr->Name, mr->MediaType, mr->Slot, drive);
   }

   while (sd->recv() >= 0) {
      int dvd;
      ua->send_msg("%s", sd->msg);
      if (sscanf(sd->msg, "3000 OK label. VolBytes=%llu DVD=%d ", &VolBytes,
                 &dvd) == 2) {
         is_dvd = dvd;
         ok = true;
      }
   }
   unbash_spaces(mr->VolumeName);
   unbash_spaces(mr->MediaType);
   unbash_spaces(pr->Name);
   mr->LabelDate = time(NULL);
   mr->set_label_date = true;
   if (is_dvd) {
      /* We know that a freshly labelled DVD has 1 VolParts */
      /* This does not apply to auto-labelled DVDs. */
      mr->VolParts = 1;
   }
   if (ok) {
      if (media_record_exists) {      /* we update it */
         mr->VolBytes = VolBytes;
         mr->InChanger = mr->Slot > 0;  /* if slot give assume in changer */
         set_storageid_in_mr(ua->jcr->wstore, mr);
         if (!db_update_media_record(ua->jcr, ua->db, mr)) {
             ua->error_msg("%s", db_strerror(ua->db));
             ok = false;
         }
      } else {                        /* create the media record */
         set_pool_dbr_defaults_in_media_dbr(mr, pr);
         mr->VolBytes = VolBytes;
         mr->InChanger = mr->Slot > 0;  /* if slot give assume in changer */
         mr->Enabled = 1;
         set_storageid_in_mr(ua->jcr->wstore, mr);
         if (db_create_media_record(ua->jcr, ua->db, mr)) {
            ua->info_msg(_("Catalog record for Volume \"%s\", Slot %d  successfully created.\n"),
            mr->VolumeName, mr->Slot);
            /* Update number of volumes in pool */
            pr->NumVols++;
            if (!db_update_pool_record(ua->jcr, ua->db, pr)) {
               ua->error_msg("%s", db_strerror(ua->db));
            }
         } else {
            ua->error_msg("%s", db_strerror(ua->db));
            ok = false;
         }
      }
   } else {
      ua->error_msg(_("Label command failed for Volume %s.\n"), mr->VolumeName);
   }
   return ok;
}

static BSOCK *open_sd_bsock(UAContext *ua)
{
   STORE *store = ua->jcr->wstore;

   if (!ua->jcr->store_bsock) {
      ua->send_msg(_("Connecting to Storage daemon %s at %s:%d ...\n"),
         store->name(), store->address, store->SDport);
      if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
         ua->error_msg(_("Failed to connect to Storage daemon.\n"));
         return NULL;
      }
   }
   return ua->jcr->store_bsock;
}

static void close_sd_bsock(UAContext *ua)
{
   if (ua->jcr->store_bsock) {
      bnet_sig(ua->jcr->store_bsock, BNET_TERMINATE);
      bnet_close(ua->jcr->store_bsock);
      ua->jcr->store_bsock = NULL;
   }
}

static char *get_volume_name_from_SD(UAContext *ua, int Slot, int drive)
{
   STORE *store = ua->jcr->wstore;
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];
   char *VolName = NULL;
   int rtn_slot;

   if (!(sd=open_sd_bsock(ua))) {
      ua->error_msg(_("Could not open SD socket.\n"));
      return NULL;
   }
   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   /* Ask for autochanger list of volumes */
   sd->fsend(NT_("readlabel %s Slot=%d drive=%d\n"), dev_name, Slot, drive);
   Dmsg1(100, "Sent: %s", sd->msg);

   /* Get Volume name in this Slot */
   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
      Dmsg1(100, "Got: %s", sd->msg);
      if (strncmp(sd->msg, NT_("3001 Volume="), 12) == 0) {
         VolName = (char *)malloc(sd->msglen);
         if (sscanf(sd->msg, NT_("3001 Volume=%s Slot=%d"), VolName, &rtn_slot) == 2) {
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

/*
 * We get the slot list from the Storage daemon.
 *  If scan is set, we return all slots found,
 *  otherwise, we return only slots with valid barcodes (Volume names)
 */
static vol_list_t *get_vol_list_from_SD(UAContext *ua, bool scan)
{
   STORE *store = ua->jcr->wstore;
   char dev_name[MAX_NAME_LENGTH];
   BSOCK *sd;
   vol_list_t *vl;
   vol_list_t *vol_list = NULL;


   if (!(sd=open_sd_bsock(ua))) {
      return NULL;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   /* Ask for autochanger list of volumes */
   bnet_fsend(sd, NT_("autochanger list %s \n"), dev_name);

   /* Read and organize list of Volumes */
   while (bnet_recv(sd) >= 0) {
      char *p;
      int Slot;
      strip_trailing_junk(sd->msg);

      /* Check for returned SD messages */
      if (sd->msg[0] == '3'     && B_ISDIGIT(sd->msg[1]) &&
          B_ISDIGIT(sd->msg[2]) && B_ISDIGIT(sd->msg[3]) &&
          sd->msg[4] == ' ') {
         ua->send_msg("%s\n", sd->msg);   /* pass them on to user */
         continue;
      }

      /* Validate Slot: if scanning, otherwise  Slot:Barcode */
      p = strchr(sd->msg, ':');
      if (scan && p) {
         /* Scanning -- require only valid slot */
         Slot = atoi(sd->msg);
         if (Slot <= 0) {
            p--;
            *p = ':';
            ua->error_msg(_("Invalid Slot number: %s\n"), sd->msg);
            continue;
         }
      } else {
         /* Not scanning */
         if (p && strlen(p) > 1) {
            *p++ = 0;
            if (!is_an_integer(sd->msg) || (Slot=atoi(sd->msg)) <= 0) {
               p--;
               *p = ':';
               ua->error_msg(_("Invalid Slot number: %s\n"), sd->msg);
               continue;
            }
         } else {
            continue;
         }
         if (!is_volume_name_legal(ua, p)) {
            p--;
            *p = ':';
            ua->error_msg(_("Invalid Volume name: %s\n"), sd->msg);
            continue;
         }
      }

      /* Add Slot and VolumeName to list */
      vl = (vol_list_t *)malloc(sizeof(vol_list_t));
      vl->Slot = Slot;
      if (p) {
         if (*p == ':') {
            p++;                      /* skip separator */
         }
         vl->VolName = bstrdup(p);
      } else {
         vl->VolName = NULL;
      }
      Dmsg2(100, "Add slot=%d Vol=%s to SD list.\n", vl->Slot, NPRT(vl->VolName));
      if (!vol_list) {
         vl->next = vol_list;
         vol_list = vl;
      } else {
         vol_list_t *prev=vol_list;
         /* Add new entry to the right place in the list */
         for (vol_list_t *tvl=vol_list; tvl; tvl=tvl->next) {
            if (tvl->Slot > vl->Slot) {
               /* no previous item, update vol_list directly */
               if (prev == vol_list) {  
                  vl->next = vol_list;
                  vol_list = vl;

               } else {     /* replace the previous pointer */
                  prev->next = vl;
                  vl->next = tvl;
               }
               break;
            }
            /* we are at the end */
            if (!tvl->next) {
               tvl->next = vl;
               vl->next = NULL;
               break;
            }
            prev = tvl;
         }
      }
   }
   close_sd_bsock(ua);
   return vol_list;
}

static void free_vol_list(vol_list_t *vol_list)
{
   vol_list_t *vl;

   /* Free list */
   for (vl=vol_list; vl; ) {
      vol_list_t *ovl;
      if (vl->VolName) {
         free(vl->VolName);
      }
      ovl = vl;
      vl = vl->next;
      free(ovl);
   }
}

/*
 * We get the number of slots in the changer from the SD
 */
static int get_num_slots_from_SD(UAContext *ua)
{
   STORE *store = ua->jcr->wstore;
   char dev_name[MAX_NAME_LENGTH];
   BSOCK *sd;
   int slots = 0;


   if (!(sd=open_sd_bsock(ua))) {
      return 0;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   /* Ask for autochanger number of slots */
   sd->fsend(NT_("autochanger slots %s\n"), dev_name);

   while (sd->recv() >= 0) {
      if (sscanf(sd->msg, "slots=%d\n", &slots) == 1) {
         break;
      } else {
         ua->send_msg("%s", sd->msg);
      }
   }
   close_sd_bsock(ua);
   ua->send_msg(_("Device \"%s\" has %d slots.\n"), store->dev_name(), slots);
   return slots;
}

/*
 * We get the number of drives in the changer from the SD
 */
int get_num_drives_from_SD(UAContext *ua)
{
   STORE *store = ua->jcr->wstore;
   char dev_name[MAX_NAME_LENGTH];
   BSOCK *sd;
   int drives = 0;


   if (!(sd=open_sd_bsock(ua))) {
      return 0;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   /* Ask for autochanger number of slots */
   sd->fsend(NT_("autochanger drives %s\n"), dev_name);

   while (sd->recv() >= 0) {
      if (sscanf(sd->msg, NT_("drives=%d\n"), &drives) == 1) {
         break;
      } else {
         ua->send_msg("%s", sd->msg);
      }
   }
   close_sd_bsock(ua);
//   bsendmsg(ua, _("Device \"%s\" has %d drives.\n"), store->dev_name(), drives);
   return drives;
}

/*
 * Check if this is a cleaning tape by comparing the Volume name
 *  with the Cleaning Prefix. If they match, this is a cleaning
 *  tape.
 */
static bool is_cleaning_tape(UAContext *ua, MEDIA_DBR *mr, POOL_DBR *pr)
{
   /* Find Pool resource */
   ua->jcr->pool = (POOL *)GetResWithName(R_POOL, pr->Name);
   if (!ua->jcr->pool) {
      ua->error_msg(_("Pool \"%s\" resource not found for volume \"%s\"!\n"),
         pr->Name, mr->VolumeName);
      return false;
   }
   if (ua->jcr->pool->cleaning_prefix == NULL) {
      return false;
   }
   Dmsg4(100, "CLNprefix=%s: Vol=%s: len=%d strncmp=%d\n",
      ua->jcr->pool->cleaning_prefix, mr->VolumeName,
      strlen(ua->jcr->pool->cleaning_prefix),
      strncmp(mr->VolumeName, ua->jcr->pool->cleaning_prefix,
                  strlen(ua->jcr->pool->cleaning_prefix)));
   return strncmp(mr->VolumeName, ua->jcr->pool->cleaning_prefix,
                  strlen(ua->jcr->pool->cleaning_prefix)) == 0;
}

static void content_send_info(UAContext *ua, char type, int Slot, char *vol_name)
{
   char ed1[50], ed2[50], ed3[50];
   POOL_DBR pr;
   MEDIA_DBR mr;
   /* Type|Slot|RealSlot|Volume|Bytes|Status|MediaType|Pool|LastW|Expire */
   const char *slot_api_full_format="%c|%i|%i|%s|%s|%s|%s|%s|%s|%s\n";
   const char *slot_api_empty_format="%c|%i||||||||\n";

   if (is_volume_name_legal(NULL, vol_name)) {
      bstrncpy(mr.VolumeName, vol_name, sizeof(mr.VolumeName));
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
         memset(&pr, 0, sizeof(POOL_DBR));
         pr.PoolId = mr.PoolId;
         if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
            strcpy(pr.Name, "?");
         }
         ua->send_msg(slot_api_full_format, type,
                      Slot, mr.Slot, mr.VolumeName, 
                      edit_uint64(mr.VolBytes, ed1), 
                      mr.VolStatus, mr.MediaType, pr.Name, 
                      edit_uint64(mr.LastWritten, ed2),
                      edit_uint64(mr.LastWritten+mr.VolRetention, ed3));
         
      } else {                  /* Media unknown */
         ua->send_msg(slot_api_full_format,
                      type, Slot, 0, mr.VolumeName, "?", "?", "?", "?", 
                      "0", "0");
         
      }
   } else {
      ua->send_msg(slot_api_empty_format, type, Slot);
   }
}         

/* 
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
 * 
 * Output:
 *
 * Drive list:       D|Drive num|Slot loaded|Volume Name
 * D|0|45|vol45
 * D|1|42|vol42
 * D|3||
 *
 * Slot list: Type|Slot|RealSlot|Volume|Bytes|Status|MediaType|Pool|LastW|Expire
 *
 * S|1|1|vol1|31417344|Full|LTO1-ANSI|Inc|1250858902|1282394902
 * S|2||||||||
 * S|3|3|vol4|15869952|Append|LTO1-ANSI|Inc|1250858907|1282394907
 *
 * TODO: need to merge with status_slots()
 */
void status_content(UAContext *ua, STORE *store)
{
   int Slot, Drive;
   char type;
   char dev_name[MAX_NAME_LENGTH];
   char vol_name[MAX_NAME_LENGTH];
   BSOCK *sd;
   vol_list_t *vl=NULL, *vol_list = NULL;

   if (!(sd=open_sd_bsock(ua))) {
      return;
   }

   if (!open_client_db(ua)) {
      return;
   }

   bstrncpy(dev_name, store->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   /* Ask for autochanger list of volumes */
   bnet_fsend(sd, NT_("autochanger listall %s \n"), dev_name);

   /* Read and organize list of Drive, Slots and I/O Slots */
   while (bnet_recv(sd) >= 0) {
      strip_trailing_junk(sd->msg);

      /* Check for returned SD messages */
      if (sd->msg[0] == '3'     && B_ISDIGIT(sd->msg[1]) &&
          B_ISDIGIT(sd->msg[2]) && B_ISDIGIT(sd->msg[3]) &&
          sd->msg[4] == ' ') {
         ua->send_msg("%s\n", sd->msg);   /* pass them on to user */
         continue;
      }

      Drive = Slot = -1;
      *vol_name = 0;

      if (sscanf(sd->msg, "D:%d:F:%d:%127s", &Drive, &Slot, vol_name) == 3) {
         ua->send_msg("D|%d|%d|%s\n", Drive, Slot, vol_name);

         /* we print information on the slot if we have a volume name */
         if (*vol_name) {
            /* Add Slot and VolumeName to list */
            vl = (vol_list_t *)malloc(sizeof(vol_list_t));
            vl->Slot = Slot;
            vl->VolName = bstrdup(vol_name);
            vl->next = vol_list;
            vol_list = vl;
         }

      } else if (sscanf(sd->msg, "D:%d:E", &Drive) == 1) {
         ua->send_msg("D|%d||\n", Drive);

      } else if (sscanf(sd->msg, "%c:%d:F:%127s", &type, &Slot, vol_name)== 3) {
         content_send_info(ua, type, Slot, vol_name);

      } else if (sscanf(sd->msg, "%c:%d:E", &type, &Slot) == 2) {
         /* type can be S (slot) or I (Import/Export slot) */
         vol_list_t *prev=NULL;
         for (vl = vol_list; vl; vl = vl->next) {
            if (vl->Slot == Slot) {
               bstrncpy(vol_name, vl->VolName, MAX_NAME_LENGTH);

               /* remove the node */
               if (prev) {
                  prev->next = vl->next;
               } else {
                  vol_list = vl->next;
               }
               free(vl->VolName);
               free(vl);
               break;
            }
            prev = vl;
         }
         content_send_info(ua, type, Slot, vol_name);

      } else {
         Dmsg1(10, "Discarding msg=%s\n", sd->msg);
      }
   }
   close_sd_bsock(ua);
}

/*
 * Print slots from AutoChanger
 */
void status_slots(UAContext *ua, STORE *store_r)
{
   USTORE store;
   POOL_DBR pr;
   vol_list_t *vl, *vol_list = NULL;
   MEDIA_DBR mr;
   char *slot_list;
   int max_slots;
   int i=1;
   /* Slot | Volume | Status | MediaType | Pool */
   const char *slot_hformat=" %4i%c| %16s | %9s | %20s | %18s |\n";

   if (ua->api) {
      status_content(ua, store_r);
      return;
   }

   if (!open_client_db(ua)) {
      return;
   }
   store.store = store_r;

   pm_strcpy(store.store_source, _("command line"));
   set_wstorage(ua->jcr, &store);
   get_storage_drive(ua, store.store);

   max_slots = get_num_slots_from_SD(ua);

   if (max_slots <= 0) {
      ua->warning_msg(_("No slots in changer to scan.\n"));
      return;
   }
   slot_list = (char *)malloc(max_slots+1);
   if (!get_user_slot_list(ua, slot_list, max_slots)) {
      free(slot_list);
      return;
   }

   vol_list = get_vol_list_from_SD(ua, true /* want to see all slots */);

   if (!vol_list) {
      ua->warning_msg(_("No Volumes found, or no barcodes.\n"));
      goto bail_out;
   }
   ua->send_msg(_(" Slot |   Volume Name    |   Status  |     Media Type       |      Pool          |\n"));
   ua->send_msg(_("------+------------------+-----------+----------------------+--------------------|\n"));

   /* Walk through the list getting the media records */
   for (vl=vol_list; vl; vl=vl->next) {
      if (vl->Slot > max_slots) {
         ua->warning_msg(_("Slot %d greater than max %d ignored.\n"),
            vl->Slot, max_slots);
         continue;
      }
      /* Check if user wants us to look at this slot */
      if (!slot_list[vl->Slot]) {
         Dmsg1(100, "Skipping slot=%d\n", vl->Slot);
         continue;
      }

      slot_list[vl->Slot] = 0;        /* clear Slot */

      if (!vl->VolName) {
         Dmsg1(100, "No VolName for Slot=%d.\n", vl->Slot);
         ua->send_msg(slot_hformat,
                      vl->Slot, '*',
                      "?", "?", "?", "?");
         continue;
      }

      /* Hope that slots are ordered */
      for (; i < vl->Slot; i++) {
         if (slot_list[i]) {
            ua->send_msg(slot_hformat,
                         i, ' ', "", "", "", "");
            slot_list[i]=0;
         }
      }

      memset(&mr, 0, sizeof(MEDIA_DBR));
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));

      if (mr.VolumeName[0] && db_get_media_record(ua->jcr, ua->db, &mr)) {
         memset(&pr, 0, sizeof(POOL_DBR));
         pr.PoolId = mr.PoolId;
         if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
            strcpy(pr.Name, "?");
         }

         /* Print information */
         ua->send_msg(slot_hformat,
                      vl->Slot, ((vl->Slot==mr.Slot)?' ':'*'),
                      mr.VolumeName, mr.VolStatus, mr.MediaType, pr.Name);

      } else {                  /* TODO: get information from catalog  */
         ua->send_msg(slot_hformat,
                      vl->Slot, '*',
                      mr.VolumeName, "?", "?", "?");
      }
   }

   /* Display the rest of the autochanger
    */
   for (; i <= max_slots; i++) {
      if (slot_list[i]) {
         ua->send_msg(slot_hformat,
                      i, ' ', "", "", "", "");
         slot_list[i]=0;
      }
   }

bail_out:

   free_vol_list(vol_list);
   free(slot_list);
   close_sd_bsock(ua);

   return;
}
