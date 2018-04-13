/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Kern E. Sibbald, August MMII
 */
/**
 * This file provides routines that will handle all
 * the gory little details of reading a record from a Bareos
 * archive. It uses a callback to pass you each record in turn,
 * as well as a callback for mounting the next tape.  It takes
 * care of reading blocks, applying the bsr, ...
 *
 * Note, this routine is really the heart of the restore routines,
 * and we are *really* bit pushing here so be careful about making
 * any modifications.
 */

#include "bareos.h"
#include "stored.h"

/* Forward referenced functions */

static const int dbglvl = 500;

static void handle_session_record(Device *dev, DeviceRecord *rec, SESSION_LABEL *sessrec)
{
   const char *rtype;
   char buf[100];

   memset(sessrec, 0, sizeof(SESSION_LABEL));
   switch (rec->FileIndex) {
   case PRE_LABEL:
      rtype = _("Fresh Volume Label");
      break;
   case VOL_LABEL:
      rtype = _("Volume Label");
      unser_volume_label(dev, rec);
      break;
   case SOS_LABEL:
      rtype = _("Begin Session");
      unser_session_label(sessrec, rec);
      break;
   case EOS_LABEL:
      rtype = _("End Session");
      break;
   case EOM_LABEL:
      rtype = _("End of Media");
      break;
   default:
      bsnprintf(buf, sizeof(buf), _("Unknown code %d\n"), rec->FileIndex);
      rtype = buf;
      break;
   }
   Dmsg5(dbglvl, _("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n"),
         rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
}

static char *rec_state_bits_to_str(DeviceRecord *rec)
{
   static char buf[200];

   buf[0] = 0;
   if (bit_is_set(REC_NO_HEADER, rec->state_bits)) {
      bstrncat(buf, _("Nohdr,"), sizeof(buf));
   }

   if (is_partial_record(rec)) {
      bstrncat(buf, _("partial,"), sizeof(buf));
   }

   if (bit_is_set(REC_BLOCK_EMPTY, rec->state_bits)) {
      bstrncat(buf, _("empty,"), sizeof(buf));
   }

   if (bit_is_set(REC_NO_MATCH, rec->state_bits)) {
      bstrncat(buf, _("Nomatch,"), sizeof(buf));
   }

   if (bit_is_set(REC_CONTINUATION, rec->state_bits)) {
      bstrncat(buf, _("cont,"), sizeof(buf));
   }

   if (buf[0]) {
      buf[strlen(buf)-1] = 0;
   }

   return buf;
}

/**
 * Allocate a new read context which will contains accumulated data from a read session.
 */
READ_CTX *new_read_context(void)
{
   DeviceRecord *rec = NULL;
   READ_CTX *rctx;

   rctx = (READ_CTX *)malloc(sizeof(READ_CTX));
   memset(rctx, 0, sizeof(READ_CTX));

   rctx->recs = New(dlist(rec, &rec->link));
   return rctx;
}

/**
 * Free a read context which contains accumulated data from a read session.
 */
void free_read_context(READ_CTX *rctx)
{
   DeviceRecord *rec;

   /*
    * Walk down list and free all remaining allocated recs
    */
   while (!rctx->recs->empty()) {
      rec = (DeviceRecord *)rctx->recs->first();
      rctx->recs->remove(rec);
      free_record(rec);
   }
   delete rctx->recs;

   free(rctx);
}

/**
 * Setup the record pointer in the Read Context.
 * Reuse an already existing record when available in the linked
 * list or allocate a fresh one and prepend it in the linked list.
 */
void read_context_set_record(DeviceControlRecord *dcr, READ_CTX *rctx)
{
   DeviceRecord *rec;
   bool found = false;

   foreach_dlist(rec, rctx->recs) {
      if (rec->VolSessionId == dcr->block->VolSessionId &&
          rec->VolSessionTime == dcr->block->VolSessionTime) {
         found = true;
         break;
       }
   }

   if (!found) {
      rec = new_record();
      rctx->recs->prepend(rec);
      Dmsg3(dbglvl, "New record for state=%s SI=%d ST=%d\n",
            rec_state_bits_to_str(rec),
            dcr->block->VolSessionId,
            dcr->block->VolSessionTime);
   }

   rctx->rec = rec;
}

/**
 * Read the next block from the device and handle any volume
 * switches etc.
 *
 * Returns:  true on success
 *           false on error
 *
 * Any fatal error sets the status bool to false.
 */
bool read_next_block_from_device(DeviceControlRecord *dcr,
                                 SESSION_LABEL *sessrec,
                                 bool record_cb(DeviceControlRecord *dcr, DeviceRecord *rec),
                                 bool mount_cb(DeviceControlRecord *dcr),
                                 bool *status)
{
   JobControlRecord *jcr = dcr->jcr;
   DeviceRecord *trec;

   while (1) {
      if (!dcr->read_block_from_device(CHECK_BLOCK_NUMBERS)) {
         if (dcr->dev->at_eot()) {
            Jmsg(jcr, M_INFO, 0, _("End of Volume at file %u on device %s, Volume \"%s\"\n"),
                 dcr->dev->file, dcr->dev->print_name(), dcr->VolumeName);

            volume_unused(dcr);       /* mark volume unused */
            if (!mount_cb(dcr)) {
               Jmsg(jcr, M_INFO, 0, _("End of all volumes.\n"));
               if (record_cb) {
                  /*
                   * Create EOT Label so that Media record may
                   *  be properly updated because this is the last
                   *  tape.
                   */
                  trec = new_record();
                  trec->FileIndex = EOT_LABEL;
                  trec->File = dcr->dev->file;
                  *status = record_cb(dcr, trec);
                  if (jcr->mount_next_volume) {
                     jcr->mount_next_volume = false;
                     dcr->dev->clear_eot();
                  }
                  free_record(trec);
               }
               return false;
            }
            jcr->mount_next_volume = false;

            /*
             * We just have a new tape up, now read the label (first record)
             *  and pass it off to the callback routine, then continue
             *  most likely reading the previous record.
             */
            dcr->read_block_from_device(NO_BLOCK_NUMBER_CHECK);
            trec = new_record();
            read_record_from_block(dcr, trec);
            handle_session_record(dcr->dev, trec, sessrec);
            if (record_cb) {
               record_cb(dcr, trec);
            }

            free_record(trec);
            position_device_to_first_file(jcr, dcr);

            /*
             * After reading label, we must read first data block
             */
            continue;
         } else if (dcr->dev->at_eof()) {
            Dmsg3(200, "End of file %u on device %s, Volume \"%s\"\n",
                  dcr->dev->file, dcr->dev->print_name(), dcr->VolumeName);
            continue;
         } else if (dcr->dev->is_short_block()) {
            Jmsg1(jcr, M_ERROR, 0, "%s", dcr->dev->errmsg);
            continue;
         } else {
            /*
             * I/O error or strange end of tape
             */
            display_tape_error_status(jcr, dcr->dev);
            if (forge_on || jcr->ignore_label_errors) {
               dcr->dev->fsr(1);           /* try skipping bad record */
               Pmsg0(000, _("Did fsr in attemp to skip bad record.\n"));
               continue;
            }
            *status = false;
            return false;
         }
      }

      Dmsg2(dbglvl, "Read new block at pos=%u:%u\n", dcr->dev->file, dcr->dev->block_num);
      return true;
   }
}

/**
 * Read the next record from a block.
 *
 * Returns:  true on continue processing.
 *           false on error or when done with this block.
 *
 * When we are done processing all records the done bool is set to true.
 */
bool read_next_record_from_block(DeviceControlRecord *dcr, READ_CTX *rctx, bool *done)
{
   JobControlRecord *jcr = dcr->jcr;
   Device *dev = dcr->dev;
   DeviceBlock *block = dcr->block;
   DeviceRecord *rec = rctx->rec;

   while (1) {
      if (!read_record_from_block(dcr, rec)) {
         Dmsg3(400, "!read-break. state_bits=%s blk=%d rem=%d\n",
               rec_state_bits_to_str(rec), block->BlockNumber, rec->remainder);
         return false;
      }

      Dmsg5(dbglvl, "read-OK. state_bits=%s blk=%d rem=%d file:block=%u:%u\n",
            rec_state_bits_to_str(rec), block->BlockNumber, rec->remainder,
            dev->file, dev->block_num);

      /*
       * At this point, we have at least a record header.
       *  Now decide if we want this record or not, but remember
       *  before accessing the record, we may need to read again to
       *  get all the data.
       */
      rctx->records_processed++;
      Dmsg6(dbglvl, "recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n",
            rctx->records_processed, rec_state_bits_to_str(rec), block->BlockNumber,
            rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);

      if (rec->FileIndex == EOM_LABEL) {     /* end of tape? */
         Dmsg0(40, "Get EOM LABEL\n");
         return false;
      }

      /*
       * Some sort of label?
       */
      if (rec->FileIndex < 0) {
         handle_session_record(dcr->dev, rec, &rctx->sessrec);
         if (jcr->bsr) {
            /*
             * We just check block FI and FT not FileIndex
             */
            rec->match_stat = match_bsr_block(jcr->bsr, dcr->block);
         } else {
            rec->match_stat = 0;
         }

         return true;
      }

      /*
       * Apply BootStrapRecord filter
       */
      if (jcr->bsr) {
         rec->match_stat = match_bsr(jcr->bsr, rec, &dev->VolHdr, &rctx->sessrec, jcr);
         if (rec->match_stat == -1) {        /* no more possible matches */
            *done = true;                    /* all items found, stop */
            Dmsg2(dbglvl, "All done=(file:block) %u:%u\n", dev->file, dev->block_num);
            return false;
         } else if (rec->match_stat == 0) {  /* no match */
            Dmsg4(dbglvl, "BootStrapRecord no match: clear rem=%d FI=%d before set_eof pos %u:%u\n",
               rec->remainder, rec->FileIndex, dev->file, dev->block_num);
            rec->remainder = 0;
            clear_bit(REC_PARTIAL_RECORD, rec->state_bits);
            if (try_device_repositioning(jcr, rec, dcr)) {
               return false;
            }
            continue;                        /* we don't want record, read next one */
         }
      }

      dcr->VolLastIndex = rec->FileIndex;    /* let caller know where we are */

      if (is_partial_record(rec)) {
         Dmsg6(dbglvl, "Partial, break. recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n",
               rctx->records_processed, rec_state_bits_to_str(rec), block->BlockNumber, rec->VolSessionId,
               rec->VolSessionTime, rec->FileIndex);
         return false;                       /* read second part of record */
      }

      if (rctx->lastFileIndex != READ_NO_FILEINDEX && rctx->lastFileIndex != rec->FileIndex) {
         if (is_this_bsr_done(jcr->bsr, rec) && try_device_repositioning(jcr, rec, dcr)) {
            Dmsg2(dbglvl, "This bsr done, break pos %u:%u\n", dev->file, dev->block_num);
            return false;
         }
         Dmsg2(dbglvl, "==== inside LastIndex=%d FileIndex=%d\n", rctx->lastFileIndex, rec->FileIndex);
      }

      Dmsg2(dbglvl, "==== LastIndex=%d FileIndex=%d\n", rctx->lastFileIndex, rec->FileIndex);
      rctx->lastFileIndex = rec->FileIndex;

      return true;
   }
}

/**
 * This subroutine reads all the records and passes them back to your
 * callback routine (also mount routine at EOM).
 *
 * You must not change any values in the DeviceRecord packet
 */
bool read_records(DeviceControlRecord *dcr,
                  bool record_cb(DeviceControlRecord *dcr, DeviceRecord *rec),
                  bool mount_cb(DeviceControlRecord *dcr))
{
   JobControlRecord *jcr = dcr->jcr;
   READ_CTX *rctx;
   bool ok = true;
   bool done = false;

   rctx = new_read_context();
   position_device_to_first_file(jcr, dcr);
   jcr->mount_next_volume = false;

   while (ok && !done) {
      if (job_canceled(jcr)) {
         ok = false;
         break;
      }

      /*
       * Read the next block into our buffers.
       */
      if (!read_next_block_from_device(dcr, &rctx->sessrec, record_cb, mount_cb, &ok)) {
         break;
      }

#ifdef if_and_when_FAST_BLOCK_REJECTION_is_working
      /*
       * This does not stop when file/block are too big
       */
      if (!match_bsr_block(jcr->bsr, block)) {
         if (try_device_repositioning(jcr, rctx->rec, dcr)) {
            break;                    /* get next volume */
         }
         continue;                    /* skip this record */
      }
#endif

      /*
       * Get a new record for each Job as defined by VolSessionId and VolSessionTime
       */
      if (!rctx->rec ||
          rctx->rec->VolSessionId != dcr->block->VolSessionId ||
          rctx->rec->VolSessionTime != dcr->block->VolSessionTime) {
         read_context_set_record(dcr, rctx);
      }

      Dmsg3(dbglvl, "Before read rec loop. stat=%s blk=%d rem=%d\n",
            rec_state_bits_to_str(rctx->rec), dcr->block->BlockNumber, rctx->rec->remainder);

      rctx->records_processed = 0;
      clear_all_bits(REC_STATE_MAX, rctx->rec->state_bits);
      rctx->lastFileIndex = READ_NO_FILEINDEX;
      Dmsg1(dbglvl, "Block %s empty\n", is_block_empty(rctx->rec) ? "is" : "NOT");

      /*
       * Process the block and read all records in the block and send
       * them to the defined callback.
       */
      while (ok && !is_block_empty(rctx->rec)) {
         if (!read_next_record_from_block(dcr, rctx, &done)) {
            break;
         }

         if (rctx->rec->FileIndex < 0) {
            /*
             * Note, we pass *all* labels to the callback routine. If
             *  he wants to know if they matched the bsr, then he must
             *  check the match_stat in the record */
            ok = record_cb(dcr, rctx->rec);
         } else {
            DeviceRecord *rec;

            Dmsg6(dbglvl, "OK callback. recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n",
                  rctx->records_processed, rec_state_bits_to_str(rctx->rec), dcr->block->BlockNumber,
                  rctx->rec->VolSessionId, rctx->rec->VolSessionTime, rctx->rec->FileIndex);

            /*
             * Perform record translations.
             */
            dcr->before_rec = rctx->rec;
            dcr->after_rec = NULL;

            /*
             * We want the plugins to be called in reverse order so we give the generate_plugin_event()
             * the reverse argument so it knows that we want the plugins to be called in that order.
             */
            if (generate_plugin_event(jcr, bsdEventReadRecordTranslation, dcr, true) != bRC_OK) {
               ok = false;
               continue;
            }

            /*
             * The record got translated when we got an after_rec pointer after calling the
             * bsdEventReadRecordTranslation plugin event. If no translation has taken place
             * we just point the rec pointer to same DeviceRecord as in the before_rec pointer.
             */
            rec = (dcr->after_rec) ? dcr->after_rec : dcr->before_rec;
            ok = record_cb(dcr, rec);

#if 0
            /*
             * If we have a digest stream, we check to see if we have
             *  finished the current bsr, and if so, repositioning will
             *  be turned on.
             */
            if (crypto_digest_stream_type(rec->Stream) != CRYPTO_DIGEST_NONE) {
               Dmsg3(dbglvl, "=== Have digest FI=%u before bsr check pos %u:%u\n",
                     rec->FileIndex, dev->file, dev->block_num);
               if (is_this_bsr_done(jcr->bsr, rec) && try_repositioning(jcr, rec, dcr)) {
                  Dmsg1(dbglvl, "==== BootStrapRecord done at FI=%d\n", rec->FileIndex);
                  Dmsg2(dbglvl, "This bsr done, break pos %u:%u\n",
                        dev->file, dev->block_num);
                  break;
               }
               Dmsg2(900, "After is_bsr_done pos %u:%u\n", dev->file, dev->block_num);
            }
#endif

            /*
             * We can just release the translated record here as the record may not be
             * changed by the record callback so any changes made don't need to be copied
             * back to the original DeviceRecord.
             */
            if (dcr->after_rec) {
               free_record(dcr->after_rec);
               dcr->after_rec = NULL;
            }
         }
      }
      Dmsg2(dbglvl, "After end recs in block. pos=%u:%u\n", dcr->dev->file, dcr->dev->block_num);
   }
// Dmsg2(dbglvl, "Position=(file:block) %u:%u\n", dcr->dev->file, dcr->dev->block_num);

   free_read_context(rctx);
   print_block_read_errors(jcr, dcr->block);

   return ok;
}
