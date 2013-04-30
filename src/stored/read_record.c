/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.

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
 *  This routine provides a routine that will handle all
 *    the gory little details of reading a record from a Bacula
 *    archive. It uses a callback to pass you each record in turn,
 *    as well as a callback for mounting the next tape.  It takes
 *    care of reading blocks, applying the bsr, ...
 *    Note, this routine is really the heart of the restore routines,
 *    and we are *really* bit pushing here so be careful about making
 *    any modifications.
 *
 *    Kern E. Sibbald, August MMII
 *
 */

#include "bacula.h"
#include "stored.h"

/* Forward referenced functions */
static void handle_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec);
static BSR *position_to_first_file(JCR *jcr, DCR *dcr);
static bool try_repositioning(JCR *jcr, DEV_RECORD *rec, DCR *dcr);
#ifdef DEBUG
static char *rec_state_bits_to_str(DEV_RECORD *rec);
#endif

static const int dbglvl = 500;
static const int no_FileIndex = -999999;

/*
 * This subroutine reads all the records and passes them back to your
 *  callback routine (also mount routine at EOM).
 * You must not change any values in the DEV_RECORD packet
 */
bool read_records(DCR *dcr,
       bool record_cb(DCR *dcr, DEV_RECORD *rec),
       bool mount_cb(DCR *dcr))
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   DEV_BLOCK *block = dcr->block;
   DEV_RECORD *rec = NULL;
   uint32_t record;
   int32_t lastFileIndex;
   bool ok = true;
   bool done = false;
   SESSION_LABEL sessrec;
   dlist *recs;                         /* linked list of rec packets open */

   recs = New(dlist(rec, &rec->link));
   position_to_first_file(jcr, dcr);
   jcr->mount_next_volume = false;

   for ( ; ok && !done; ) {
      if (job_canceled(jcr)) {
         ok = false;
         break;
      }
      if (!dcr->read_block_from_device(CHECK_BLOCK_NUMBERS)) {
         if (dev->at_eot()) {
            DEV_RECORD *trec = new_record();
            Jmsg(jcr, M_INFO, 0, _("End of Volume at file %u on device %s, Volume \"%s\"\n"),
                 dev->file, dev->print_name(), dcr->VolumeName);
            volume_unused(dcr);       /* mark volume unused */
            if (!mount_cb(dcr)) {
               Jmsg(jcr, M_INFO, 0, _("End of all volumes.\n"));
               ok = false;            /* Stop everything */
               /*
                * Create EOT Label so that Media record may
                *  be properly updated because this is the last
                *  tape.
                */
               trec->FileIndex = EOT_LABEL;
               trec->File = dev->file;
               ok = record_cb(dcr, trec);
               free_record(trec);
               if (jcr->mount_next_volume) {
                  jcr->mount_next_volume = false;
                  dev->clear_eot();
               }
               break;
            }
            jcr->mount_next_volume = false;
            /*  
             * The Device can change at the end of a tape, so refresh it
             *   and the block from the dcr.
             */
            dev = dcr->dev;
            block = dcr->block;
            /*
             * We just have a new tape up, now read the label (first record)
             *  and pass it off to the callback routine, then continue
             *  most likely reading the previous record.
             */
            dcr->read_block_from_device(NO_BLOCK_NUMBER_CHECK);
            read_record_from_block(dcr, trec);
            handle_session_record(dev, trec, &sessrec);
            ok = record_cb(dcr, trec);
            free_record(trec);
            position_to_first_file(jcr, dcr);
            /* After reading label, we must read first data block */
            continue;

         } else if (dev->at_eof()) {
#ifdef neeeded_xxx
            if (verbose) {
               char *fp;
               uint32_t fp_num;
               if (dev->is_dvd()) {
                  fp = _("part");
                  fp_num = dev->part;
               } else {
                  fp = _("file");
                  fp_num = dev->file;
               }
               Jmsg(jcr, M_INFO, 0, _("End of %s %u on device %s, Volume \"%s\"\n"),
                    fp, fp_num, dev->print_name(), dcr->VolumeName);
            }
#endif
            Dmsg3(200, "End of file %u  on device %s, Volume \"%s\"\n",
                  dev->file, dev->print_name(), dcr->VolumeName);
            continue;
         } else if (dev->is_short_block()) {
            Jmsg1(jcr, M_ERROR, 0, "%s", dev->errmsg);
            continue;
         } else {
            /* I/O error or strange end of tape */
            display_tape_error_status(jcr, dev);
            if (forge_on || jcr->ignore_label_errors) {
               dev->fsr(1);       /* try skipping bad record */
               Pmsg0(000, _("Did fsr in attemp to skip bad record.\n"));
               continue;              /* try to continue */
            }
            ok = false;               /* stop everything */
            break;
         }
      }
      Dmsg2(dbglvl, "Read new block at pos=%u:%u\n", dev->file, dev->block_num);
#ifdef if_and_when_FAST_BLOCK_REJECTION_is_working
      /* this does not stop when file/block are too big */
      if (!match_bsr_block(jcr->bsr, block)) {
         if (try_repositioning(jcr, rec, dcr)) {
            break;                    /* get next volume */
         }
         continue;                    /* skip this record */
      }
#endif

      /*
       * Get a new record for each Job as defined by
       *   VolSessionId and VolSessionTime
       */
      bool found = false;
      foreach_dlist(rec, recs) {
         if (rec->VolSessionId == block->VolSessionId &&
             rec->VolSessionTime == block->VolSessionTime) {
            found = true;
            break;
          }
      }
      if (!found) {
         rec = new_record();
         recs->prepend(rec);
         Dmsg3(dbglvl, "New record for state=%s SI=%d ST=%d\n",
             rec_state_bits_to_str(rec),
             block->VolSessionId, block->VolSessionTime);
      }
      Dmsg3(dbglvl, "Before read rec loop. stat=%s blk=%d rem=%d\n", rec_state_bits_to_str(rec),
            block->BlockNumber, rec->remainder);
      record = 0;
      rec->state_bits = 0;
      lastFileIndex = no_FileIndex;
      Dmsg1(dbglvl, "Block %s empty\n", is_block_empty(rec)?"is":"NOT");
      for (rec->state_bits=0; ok && !is_block_empty(rec); ) {
         if (!read_record_from_block(dcr, rec)) {
            Dmsg3(400, "!read-break. state_bits=%s blk=%d rem=%d\n", rec_state_bits_to_str(rec),
                  block->BlockNumber, rec->remainder);
            break;
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
         record++;
         Dmsg6(dbglvl, "recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n", record,
            rec_state_bits_to_str(rec), block->BlockNumber,
            rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);

         if (rec->FileIndex == EOM_LABEL) { /* end of tape? */
            Dmsg0(40, "Get EOM LABEL\n");
            break;                         /* yes, get out */
         }

         /* Some sort of label? */
         if (rec->FileIndex < 0) {
            handle_session_record(dev, rec, &sessrec);
            if (jcr->bsr) {
               /* We just check block FI and FT not FileIndex */
               rec->match_stat = match_bsr_block(jcr->bsr, block);
            } else {
               rec->match_stat = 0;
            }
            /*
             * Note, we pass *all* labels to the callback routine. If
             *  he wants to know if they matched the bsr, then he must
             *  check the match_stat in the record */
            ok = record_cb(dcr, rec);
#ifdef xxx
            /*
             * If this is the end of the Session (EOS) for this record
             *  we can remove the record.  Note, there is a separate
             *  record to read each session. If a new session is seen
             *  a new record will be created at approx line 157 above.
             * However, it seg faults in the for line at lineno 196.
             */
            if (rec->FileIndex == EOS_LABEL) {
               Dmsg2(dbglvl, "Remove EOS rec. SI=%d ST=%d\n", rec->VolSessionId,
                  rec->VolSessionTime);
               recs->remove(rec);
               free_record(rec);
            }
#endif
            continue;
         } /* end if label record */

         /*
          * Apply BSR filter
          */
         if (jcr->bsr) {
            rec->match_stat = match_bsr(jcr->bsr, rec, &dev->VolHdr, &sessrec, jcr);
            if (rec->match_stat == -1) { /* no more possible matches */
               done = true;   /* all items found, stop */
               Dmsg2(dbglvl, "All done=(file:block) %u:%u\n", dev->file, dev->block_num);
               break;
            } else if (rec->match_stat == 0) {  /* no match */
               Dmsg4(dbglvl, "BSR no match: clear rem=%d FI=%d before set_eof pos %u:%u\n",
                  rec->remainder, rec->FileIndex, dev->file, dev->block_num);
               rec->remainder = 0;
               rec->state_bits &= ~REC_PARTIAL_RECORD;
               if (try_repositioning(jcr, rec, dcr)) {
                  break;
               }
               continue;              /* we don't want record, read next one */
            }
         }
         dcr->VolLastIndex = rec->FileIndex;  /* let caller know where we are */
         if (is_partial_record(rec)) {
            Dmsg6(dbglvl, "Partial, break. recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n", record,
               rec_state_bits_to_str(rec), block->BlockNumber,
               rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);
            break;                    /* read second part of record */
         }

         Dmsg6(dbglvl, "OK callback. recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n", record,
               rec_state_bits_to_str(rec), block->BlockNumber,
               rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);
         if (lastFileIndex != no_FileIndex && lastFileIndex != rec->FileIndex) {
            if (is_this_bsr_done(jcr->bsr, rec) && try_repositioning(jcr, rec, dcr)) {
               Dmsg2(dbglvl, "This bsr done, break pos %u:%u\n",
                     dev->file, dev->block_num);
               break;
            }
            Dmsg2(dbglvl, "==== inside LastIndex=%d FileIndex=%d\n", lastFileIndex, rec->FileIndex);
         }
         Dmsg2(dbglvl, "==== LastIndex=%d FileIndex=%d\n", lastFileIndex, rec->FileIndex);
         lastFileIndex = rec->FileIndex;
         ok = record_cb(dcr, rec);
#if 0
         /*
          * If we have a digest stream, we check to see if we have 
          *  finished the current bsr, and if so, repositioning will
          *  be turned on.
          */
         if (crypto_digest_stream_type(rec->Stream) != CRYPTO_DIGEST_NONE) {
            Dmsg3(dbglvl, "=== Have digest FI=%u before bsr check pos %u:%u\n", rec->FileIndex,
                  dev->file, dev->block_num);
            if (is_this_bsr_done(jcr->bsr, rec) && try_repositioning(jcr, rec, dcr)) {
               Dmsg1(dbglvl, "==== BSR done at FI=%d\n", rec->FileIndex);
               Dmsg2(dbglvl, "This bsr done, break pos %u:%u\n",
                     dev->file, dev->block_num);
               break;
            }
            Dmsg2(900, "After is_bsr_done pos %u:%u\n", dev->file, dev->block_num);
         }
#endif
      } /* end for loop over records */
      Dmsg2(dbglvl, "After end recs in block. pos=%u:%u\n", dev->file, dev->block_num);
   } /* end for loop over blocks */
// Dmsg2(dbglvl, "Position=(file:block) %u:%u\n", dev->file, dev->block_num);

   /* Walk down list and free all remaining allocated recs */
   while (!recs->empty()) {
      rec = (DEV_RECORD *)recs->first();
      recs->remove(rec);
      free_record(rec);
   }
   delete recs;
   print_block_read_errors(jcr, block);
   return ok;
}

/*
 * See if we can reposition.
 *   Returns:  true  if at end of volume
 *             false otherwise
 */
static bool try_repositioning(JCR *jcr, DEV_RECORD *rec, DCR *dcr)
{
   BSR *bsr;
   DEVICE *dev = dcr->dev;

   bsr = find_next_bsr(jcr->bsr, dev);
   if (bsr == NULL && jcr->bsr->mount_next_volume) {
      Dmsg0(dbglvl, "Would mount next volume here\n");
      Dmsg2(dbglvl, "Current postion (file:block) %u:%u\n",
         dev->file, dev->block_num);
      jcr->bsr->mount_next_volume = false;
      if (!dev->at_eot()) {
         /* Set EOT flag to force mount of next Volume */
         jcr->mount_next_volume = true;
         dev->set_eot();
      }
      rec->Block = 0;
      return true;
   }
   if (bsr) {
      /*
       * ***FIXME*** gross kludge to make disk seeking work.  Remove
       *   when find_next_bsr() is fixed not to return a bsr already
       *   completed.
       */
      uint32_t block, file;
      /* TODO: use dev->file_addr ? */
      uint64_t dev_addr = (((uint64_t) dev->file)<<32) | dev->block_num;
      uint64_t bsr_addr = get_bsr_start_addr(bsr, &file, &block);

      if (dev_addr > bsr_addr) {
         return false;
      }
      Dmsg4(dbglvl, "Try_Reposition from (file:block) %u:%u to %u:%u\n",
            dev->file, dev->block_num, file, block);
      dev->reposition(dcr, file, block);
      rec->Block = 0;
   }
   return false;
}

/*
 * Position to the first file on this volume
 */
static BSR *position_to_first_file(JCR *jcr, DCR *dcr)
{
   BSR *bsr = NULL;
   DEVICE *dev = dcr->dev;
   uint32_t file, block;
   /*
    * Now find and position to first file and block
    *   on this tape.
    */
   if (jcr->bsr) {
      jcr->bsr->reposition = true;    /* force repositioning */
      bsr = find_next_bsr(jcr->bsr, dev);
      
      if (get_bsr_start_addr(bsr, &file, &block) > 0) {
         Jmsg(jcr, M_INFO, 0, _("Forward spacing Volume \"%s\" to file:block %u:%u.\n"),
              dev->VolHdr.VolumeName, file, block);
         dev->reposition(dcr, file, block);
      }
   }
   return bsr;
}


static void handle_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec)
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

#ifdef DEBUG
static char *rec_state_bits_to_str(DEV_RECORD *rec)
{
   static char buf[200];
   buf[0] = 0;
   if (rec->state_bits & REC_NO_HEADER) {
      bstrncat(buf, "Nohdr,", sizeof(buf));
   }
   if (is_partial_record(rec)) {
      bstrncat(buf, "partial,", sizeof(buf));
   }
   if (rec->state_bits & REC_BLOCK_EMPTY) {
      bstrncat(buf, "empty,", sizeof(buf));
   }
   if (rec->state_bits & REC_NO_MATCH) {
      bstrncat(buf, "Nomatch,", sizeof(buf));
   }
   if (rec->state_bits & REC_CONTINUATION) {
      bstrncat(buf, "cont,", sizeof(buf));
   }
   if (buf[0]) {
      buf[strlen(buf)-1] = 0;
   }
   return buf;
}
#endif
