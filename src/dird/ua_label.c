/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
 * BAREOS Director -- Tape labeling commands
 *
 * Kern Sibbald, April MMIII
 */

#include "bareos.h"
#include "dird.h"

/* Forward referenced functions */

/* External functions */

static inline bool update_database(UAContext *ua,
                                   MEDIA_DBR *mr,
                                   POOL_DBR *pr,
                                   bool media_record_exists)
{
   bool retval = true;

   if (media_record_exists) {
      /*
       * Update existing media record.
       */
      mr->InChanger = mr->Slot > 0;  /* If slot give assume in changer */
      set_storageid_in_mr(ua->jcr->res.wstore, mr);
      if (!db_update_media_record(ua->jcr, ua->db, mr)) {
          ua->error_msg("%s", db_strerror(ua->db));
          retval = false;
      }
   } else {
      /*
       * Create the media record
       */
      set_pool_dbr_defaults_in_media_dbr(mr, pr);
      mr->InChanger = mr->Slot > 0;  /* If slot give assume in changer */
      mr->Enabled = 1;
      set_storageid_in_mr(ua->jcr->res.wstore, mr);

      if (db_create_media_record(ua->jcr, ua->db, mr)) {
         ua->info_msg(_("Catalog record for Volume \"%s\", Slot %hd successfully created.\n"),
         mr->VolumeName, mr->Slot);

         /*
          * Update number of volumes in pool
          */
         pr->NumVols++;
         if (!db_update_pool_record(ua->jcr, ua->db, pr)) {
            ua->error_msg("%s", db_strerror(ua->db));
         }
      } else {
         ua->error_msg("%s", db_strerror(ua->db));
         retval = false;
      }
   }

   return retval;
}

/*
 * NOTE! This routine opens the SD socket but leaves it open
 */
static inline bool native_send_label_request(UAContext *ua,
                                             MEDIA_DBR *mr,
                                             MEDIA_DBR *omr,
                                             POOL_DBR *pr,
                                             bool relabel,
                                             drive_number_t drive)
{
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];
   bool retval = false;
   uint64_t VolBytes = 0;

   if (!(sd = open_sd_bsock(ua))) {
      return false;
   }

   bstrncpy(dev_name, ua->jcr->res.wstore->dev_name(), sizeof(dev_name));
   bash_spaces(dev_name);
   bash_spaces(mr->VolumeName);
   bash_spaces(mr->MediaType);
   bash_spaces(pr->Name);

   if (relabel) {
      bash_spaces(omr->VolumeName);
      sd->fsend("relabel %s OldName=%s NewName=%s PoolName=%s "
                "MediaType=%s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d",
                dev_name, omr->VolumeName, mr->VolumeName, pr->Name,
                mr->MediaType, mr->Slot, drive,
                 /*
                  * if relabeling, keep blocksize settings
                  */
                omr->MinBlocksize, omr->MaxBlocksize);
      ua->send_msg(_("Sending relabel command from \"%s\" to \"%s\" ...\n"),
                   omr->VolumeName, mr->VolumeName);
   } else {
      sd->fsend("label %s VolumeName=%s PoolName=%s MediaType=%s "
                "Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d",
                dev_name, mr->VolumeName, pr->Name, mr->MediaType,
                mr->Slot, drive,
                 /*
                  * If labeling, use blocksize defined in pool
                  */
                pr->MinBlocksize, pr->MaxBlocksize);
      ua->send_msg(_("Sending label command for Volume \"%s\" Slot %hd ...\n"),
                   mr->VolumeName, mr->Slot);
      Dmsg8(100, "label %s VolumeName=%s PoolName=%s MediaType=%s "
                 "Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d\n",
            dev_name, mr->VolumeName, pr->Name, mr->MediaType, mr->Slot, drive,
            pr->MinBlocksize, pr->MaxBlocksize);
   }

   /*
    * We use bget_dirmsg here and not bnet_recv because as part of
    * the label request the stored can request catalog information for
    * any plugin who listens to the bsdEventLabelVerified event.
    * As we don't want to loose any non protocol data e.g. errors
    * without a 3xxx prefix we set the allow_any_message of
    * bget_dirmsg to true and as such is behaves like a normal
    * bnet_recv for any non protocol messages.
    */
   while (bget_dirmsg(sd, true) >= 0) {
      ua->send_msg("%s", sd->msg);
      if (sscanf(sd->msg, "3000 OK label. VolBytes=%llu ", &VolBytes) == 1) {
         retval = true;
      }
   }

   unbash_spaces(mr->VolumeName);
   unbash_spaces(mr->MediaType);
   unbash_spaces(pr->Name);
   mr->LabelDate = time(NULL);
   mr->set_label_date = true;
   mr->VolBytes = VolBytes;

   return retval;
}

/*
 * Generate a new encryption key for use in volume encryption.
 * We don't ask the user for a encryption key but generate a semi
 * random passphrase of the wanted length which is way stronger.
 * When the config has a wrap key we use that to wrap the newly
 * created passphrase using RFC3394 aes wrap and always convert
 * the passphrase into a base64 encoded string. This key is
 * stored in the database and is passed to the storage daemon
 * when needed. The storage daemon has the same wrap key per
 * director so it can unwrap the passphrase for use.
 *
 * Changing the wrap key will render any previously created
 * crypto keys useless so only change the wrap key after initial
 * setting when you know what you are doing and always store
 * the old key somewhere save so you can use bscrypto to
 * convert them for the new wrap key.
 */
static bool generate_new_encryption_key(UAContext *ua, MEDIA_DBR *mr)
{
   int length;
   char *passphrase;

   passphrase = generate_crypto_passphrase(DEFAULT_PASSPHRASE_LENGTH);
   if (!passphrase) {
      ua->error_msg(_("Failed to generate new encryption passphrase for Volume %s.\n"), mr->VolumeName);
      return false;
   }

   /*
    * See if we need to wrap the passphrase.
    */
   if (me->keyencrkey.value) {
      char *wrapped_passphrase;

      length = DEFAULT_PASSPHRASE_LENGTH + 8;
      wrapped_passphrase = (char *)malloc(length);
      memset(wrapped_passphrase, 0, length);
      aes_wrap((unsigned char *)me->keyencrkey.value,
               DEFAULT_PASSPHRASE_LENGTH / 8,
               (unsigned char *)passphrase,
               (unsigned char *)wrapped_passphrase);

      free(passphrase);
      passphrase = wrapped_passphrase;
   } else {
      length = DEFAULT_PASSPHRASE_LENGTH;
   }

   /*
    * The passphrase is always base64 encoded.
    */
   bin_to_base64(mr->EncrKey, sizeof(mr->EncrKey), passphrase, length, true);

   free(passphrase);
   return true;
}

static bool send_label_request(UAContext *ua,
                               STORERES *store,
                               MEDIA_DBR *mr,
                               MEDIA_DBR *omr,
                               POOL_DBR *pr,
                               bool media_record_exists,
                               bool relabel,
                               drive_number_t drive,
                               slot_number_t slot)
{
   bool retval;

   switch (store->Protocol) {
   case APT_NATIVE:
      retval = native_send_label_request(ua, mr, omr, pr, relabel, drive);
      break;
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
   default:
      retval = false;
      break;
   }

   if (retval) {
      retval = update_database(ua, mr, pr, media_record_exists);
   } else {
      ua->error_msg(_("Label command failed for Volume %s.\n"), mr->VolumeName);
   }

   return retval;
}

/*
 * Check if this is a cleaning tape by comparing the Volume name
 *  with the Cleaning Prefix. If they match, this is a cleaning
 *  tape.
 */
static inline bool is_cleaning_tape(UAContext *ua, MEDIA_DBR *mr, POOL_DBR *pr)
{
   bool retval;

   /*
    * Find Pool resource
    */
   ua->jcr->res.pool = (POOLRES *)GetResWithName(R_POOL, pr->Name);
   if (!ua->jcr->res.pool) {
      ua->error_msg(_("Pool \"%s\" resource not found for volume \"%s\"!\n"),
                    pr->Name, mr->VolumeName);
      return false;
   }

   retval = bstrncmp(mr->VolumeName,
                     ua->jcr->res.pool->cleaning_prefix,
                     strlen(ua->jcr->res.pool->cleaning_prefix));

   Dmsg4(100, "CLNprefix=%s: Vol=%s: len=%d bstrncmp=%s\n",
         ua->jcr->res.pool->cleaning_prefix, mr->VolumeName,
         strlen(ua->jcr->res.pool->cleaning_prefix),
         retval ? "true" : "false");

   return retval;
}


/*
 * Request Storage to send us the slot:barcodes, then wiffle through them all labeling them.
 */
static void label_from_barcodes(UAContext *ua, drive_number_t drive,
                                bool label_encrypt, bool yes)
{
   STORERES *store = ua->jcr->res.wstore;
   POOL_DBR pr;
   MEDIA_DBR mr;
   vol_list_t *vl;
   changer_vol_list_t *vol_list = NULL;
   bool media_record_exists;
   char *slot_list;
   int max_slots;

   memset(&mr, 0, sizeof(mr));

   max_slots = get_num_slots(ua, ua->jcr->res.wstore);
   if (max_slots <= 0) {
      ua->warning_msg(_("No slots in changer to scan.\n"));
      return;
   }

   slot_list = (char *)malloc(nbytes_for_bits(max_slots));
   clear_all_bits(max_slots, slot_list);
   if (!get_user_slot_list(ua, slot_list, "slots", max_slots)) {
      goto bail_out;
   }

   vol_list = get_vol_list_from_storage(ua, store, false /* no listall */ , false /* no scan */);
   if (!vol_list) {
      ua->warning_msg(_("No Volumes found to label, or no barcodes.\n"));
      goto bail_out;
   }

   /*
    * Display list of Volumes and ask if he really wants to proceed
    */
   ua->send_msg(_("The following Volumes will be labeled:\n"
                  "Slot  Volume\n"
                  "==============\n"));
   foreach_dlist(vl, vol_list->contents) {
      if (!vl->VolName || !bit_is_set(vl->Slot - 1, slot_list)) {
         continue;
      }
      ua->send_msg("%4d  %s\n", vl->Slot, vl->VolName);
   }

   if (!yes &&
      (!get_yesno(ua, _("Do you want to label these Volumes? (yes|no): ")) || !ua->pint32_val)) {
      goto bail_out;
   }

   /*
    * Select a pool
    */
   memset(&pr, 0, sizeof(pr));
   if (!select_pool_dbr(ua, &pr)) {
      goto bail_out;
   }

   /*
    * Fire off the label requests
    */
   foreach_dlist(vl, vol_list->contents) {
      if (!vl->VolName || !bit_is_set(vl->Slot - 1, slot_list)) {
         continue;
      }
      memset(&mr, 0, sizeof(mr));
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
      media_record_exists = false;
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
         if (mr.VolBytes != 0) {
            ua->warning_msg(_("Media record for Slot %hd Volume \"%s\" already exists.\n"),
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
       * Deal with creating cleaning tape here.
       * Normal tapes created in send_label_request() below
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

      /*
       * See if we need to generate a new passphrase for hardware encryption.
       */
      if (label_encrypt) {
         if (!generate_new_encryption_key(ua, &mr)) {
            continue;
         }
      }

      mr.Slot = vl->Slot;
      send_label_request(ua, store, &mr, NULL, &pr, media_record_exists, false, drive, vl->Slot);
   }

bail_out:
   free(slot_list);
   if (vol_list) {
      storage_release_vol_list(store, vol_list);
   }
   close_sd_bsock(ua);

   return;
}

/*
 * Common routine for both label and relabel
 */
static int do_label(UAContext *ua, const char *cmd, bool relabel)
{
   int i, j;
   BSOCK *sd;
   MEDIA_DBR mr, omr;
   POOL_DBR pr;
   USTORERES store;
   bool ok = false;
   bool yes = false;                 /* Was "yes" given on cmdline */
   drive_number_t drive;
   bool print_reminder = true;
   bool label_barcodes = false;
   bool label_encrypt = false;
   bool media_record_exists = false;
   char dev_name[MAX_NAME_LENGTH];
   static const char *barcode_keywords[] = {
      "barcode",
      "barcodes",
      NULL
   };

   if (!open_client_db(ua)) {
      return 1;
   }

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));
   memset(&omr, 0, sizeof(omr));

   if (ua->batch || find_arg(ua, NT_("yes")) > 0) {
      yes = true;
   }

   /*
    * Look for one of the barcode keywords
    */
   if (!relabel && (i = find_arg_keyword(ua, barcode_keywords)) >= 0) {
      /*
       * Now find the keyword in the list
       */
      if ((j = find_arg(ua, barcode_keywords[i])) > 0) {
         *ua->argk[j] = 0;      /* zap barcode keyword */
      }
      label_barcodes = true;
   }

   /*
    * Look for the encrypt keyword
    */
   if ((i = find_arg(ua, "encrypt")) > 0) {
      *ua->argk[i] = 0;         /* zap encrypt keyword */
      label_encrypt = true;
   }

   store.store = get_storage_resource(ua, true, label_barcodes);
   if (!store.store) {
      return 1;
   }

   switch (store.store->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      /*
       * See if the user selected a NDMP storage device but its
       * handled by a native Bareos storage daemon e.g. we have
       * a paired_storage pointer.
       */
      if (store.store->paired_storage) {
         store.store = store.store->paired_storage;
      }
      break;
   default:
      break;
   }

   pm_strcpy(store.store_source, _("command line"));
   set_wstorage(ua->jcr, &store);
   drive = get_storage_drive(ua, store.store);

   if (label_barcodes) {
      label_from_barcodes(ua, drive, label_encrypt, yes);
      return 1;
   }

   /*
    * If relabel get name of Volume to relabel
    */
   if (relabel) {
      /*
       * Check for oldvolume=name
       */
      i = find_arg_with_value(ua, "oldvolume");
      if (i >= 0) {
         bstrncpy(omr.VolumeName, ua->argv[i], sizeof(omr.VolumeName));
         if (db_get_media_record(ua->jcr, ua->db, &omr)) {
            goto checkVol;
         }
         ua->error_msg("%s", db_strerror(ua->db));
      }
      /*
       * No keyword or Vol not found, ask user to select
       */
      if (!select_media_dbr(ua, &omr)) {
         return 1;
      }

      /*
       * Require Volume to be Purged or Recycled
       */
checkVol:
      if (!bstrcmp(omr.VolStatus, "Purged") && !bstrcmp(omr.VolStatus, "Recycle")) {
         ua->error_msg(_("Volume \"%s\" has VolStatus %s. It must be Purged or Recycled before relabeling.\n"),
            omr.VolumeName, omr.VolStatus);
         return 1;
      }
   }

   /*
    * Check for volume=NewVolume
    */
   i = find_arg_with_value(ua, "volume");
   if (i >= 0) {
      pm_strcpy(ua->cmd, ua->argv[i]);
      goto checkName;
   }

   /*
    * Get a new Volume name
    */
   for ( ;; ) {
      media_record_exists = false;
      if (!get_cmd(ua, _("Enter new Volume name: "))) {
         return 1;
      }
checkName:
      if (!is_volume_name_legal(ua, ua->cmd)) {
         continue;
      }

      /*
       * Search by Media name so set VolumeName and clear MediaId.
       */
      mr.MediaId = 0;
      bstrncpy(mr.VolumeName, ua->cmd, sizeof(mr.VolumeName));

      /*
       * If VolBytes are zero the Volume is not labeled
       */
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

   /*
    * If autochanger, request slot
    */
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
      mr.Slot = (slot_number_t)ua->pint32_val;
      if (mr.Slot < 0) {
         mr.Slot = 0;
      }
      mr.InChanger = mr.Slot > 0;  /* if slot give assume in changer */
   }
   set_storageid_in_mr(store.store, &mr);

   bstrncpy(mr.MediaType, store.store->media_type, sizeof(mr.MediaType));

   /*
    * Must select Pool if not already done
    */
   if (pr.PoolId == 0) {
      memset(&pr, 0, sizeof(pr));
      if (!select_pool_dbr(ua, &pr)) {
         return 1;
      }
   }

   /*
    * See if we need to generate a new passphrase for hardware encryption.
    */
   if (label_encrypt) {
      ua->info_msg(_("Generating new hardware encryption key\n"));
      if (!generate_new_encryption_key(ua, &mr)) {
         return 1;
      }
   }

   ok = send_label_request(ua, store.store, &mr, &omr, &pr, media_record_exists, relabel, drive, mr.Slot);
   if (ok) {
      sd = ua->jcr->store_bsock;
      if (relabel) {
         /*
          * Delete the old media record
          */
         if (!db_delete_media_record(ua->jcr, ua->db, &omr)) {
            ua->error_msg(_("Delete of Volume \"%s\" failed. ERR=%s"),
               omr.VolumeName, db_strerror(ua->db));
         } else {
            ua->info_msg(_("Old volume \"%s\" deleted from catalog.\n"),
               omr.VolumeName);
            /*
             * Update the number of Volumes in the pool
             */
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
         sd->fsend("mount %s drive=%hd", dev_name, drive);
         unbash_spaces(dev_name);

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

            /*
             * Here we can get
             *  3001 OK mount. Device=xxx      or
             *  3001 Mounted Volume vvvv
             *  3002 Device "DVD-Writer" (/dev/hdc) is mounted.
             *  3906 is cannot mount non-tape
             * So for those, no need to print a reminder
             */
            if (bstrncmp(sd->msg, "3001 ", 5) ||
                bstrncmp(sd->msg, "3002 ", 5) ||
                bstrncmp(sd->msg, "3906 ", 5)) {
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
 * Label a tape
 *
 *   label storage=xxx volume=vvv
 */
bool label_cmd(UAContext *ua, const char *cmd)
{
   return do_label(ua, cmd, false);   /* standard label */
}

bool relabel_cmd(UAContext *ua, const char *cmd)
{
   return do_label(ua, cmd, true);    /* relabel tape */
}

/*
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
bool is_volume_name_legal(UAContext *ua, const char *name)
{
   int len;
   const char *p;
   const char *accept = ":.-_/";

   if (name[0] == '/') {
      if (ua) {
         ua->error_msg(_("Volume name can not start with \"/\".\n"));
      }
      return 0;
   }

   /*
    * Restrict the characters permitted in the Volume name
    */
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
