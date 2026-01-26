/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern E. Sibbald, August MMII
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

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/butil.h"
#include "stored/device.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/label.h"
#include "stored/match_bsr.h"
#include "stored/read_ctx.h"
#include "include/jcr.h"

namespace storagedaemon {

static const int debuglevel = 500;

static void HandleSessionRecord(Device* dev,
                                DeviceRecord* rec,
                                Session_Label* sessrec)
{
  const char* rtype;
  char buf[100];
  static const Session_Label empty_SESSION_LABEL{};
  *sessrec = empty_SESSION_LABEL;
  switch (rec->FileIndex) {
    case PRE_LABEL:
      rtype = T_("Fresh Volume Label");
      break;
    case VOL_LABEL:
      rtype = T_("Volume Label");
      UnserVolumeLabel(dev, rec);
      break;
    case SOS_LABEL:
      rtype = T_("Begin Session");
      UnserSessionLabel(sessrec, rec);
      break;
    case EOS_LABEL:
      rtype = T_("End Session");
      break;
    case EOM_LABEL:
      rtype = T_("End of Media");
      break;
    default:
      Bsnprintf(buf, sizeof(buf), T_("Unknown code %d\n"), rec->FileIndex);
      rtype = buf;
      break;
  }
  Dmsg5(
      debuglevel,
      T_("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n"),
      rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream,
      rec->data_len);
}

static char* rec_state_bits_to_str(DeviceRecord* rec)
{
  static char buf[200];

  buf[0] = 0;
  if (BitIsSet(REC_NO_HEADER, rec->state_bits)) {
    bstrncat(buf, T_("Nohdr,"), sizeof(buf));
  }

  if (IsPartialRecord(rec)) { bstrncat(buf, T_("partial,"), sizeof(buf)); }

  if (BitIsSet(REC_BLOCK_EMPTY, rec->state_bits)) {
    bstrncat(buf, T_("empty,"), sizeof(buf));
  }

  if (BitIsSet(REC_NO_MATCH, rec->state_bits)) {
    bstrncat(buf, T_("Nomatch,"), sizeof(buf));
  }

  if (BitIsSet(REC_CONTINUATION, rec->state_bits)) {
    bstrncat(buf, T_("cont,"), sizeof(buf));
  }

  if (buf[0]) { buf[strlen(buf) - 1] = 0; }

  return buf;
}

/**
 * Allocate a new read context which will contains accumulated data from a read
 * session.
 */
READ_CTX* new_read_context(void)
{
  READ_CTX* rctx;

  rctx = (READ_CTX*)malloc(sizeof(READ_CTX));
  READ_CTX empty_READ_CTX;
  *rctx = empty_READ_CTX;

  rctx->recs = new dlist<DeviceRecord>();
  return rctx;
}

// Free a read context which contains accumulated data from a read session.
void FreeReadContext(READ_CTX* rctx)
{
  DeviceRecord* rec;

  // Walk down list and free all remaining allocated recs
  while (!rctx->recs->empty()) {
    rec = (DeviceRecord*)rctx->recs->first();
    rctx->recs->remove(rec);
    FreeRecord(rec);
  }
  delete rctx->recs;

  free(rctx);
}

/**
 * Setup the record pointer in the Read Context.
 * Reuse an already existing record when available in the linked
 * list or allocate a fresh one and prepend it in the linked list.
 */
void ReadContextSetRecord(DeviceControlRecord* dcr, READ_CTX* rctx)
{
  DeviceRecord* rec;
  bool found = false;

  foreach_dlist (rec, rctx->recs) {
    if (rec->VolSessionId == dcr->block->VolSessionId
        && rec->VolSessionTime == dcr->block->VolSessionTime) {
      found = true;
      break;
    }
  }

  if (!found) {
    rec = new_record();
    rctx->recs->prepend(rec);
    Dmsg3(debuglevel, "New record for state=%s SI=%d ST=%d\n",
          rec_state_bits_to_str(rec), dcr->block->VolSessionId,
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
bool ReadNextBlockFromDevice(DeviceControlRecord* dcr,
                             READ_CTX* ctx,
                             Session_Label* sessrec,
                             bool RecordCb(DeviceControlRecord* dcr,
                                           DeviceRecord* rec,
                                           void* user_data),
                             bool mount_cb(DeviceControlRecord* dcr),
                             void* user_data,
                             bool* status)
{
  JobControlRecord* jcr = dcr->jcr;
  DeviceRecord* trec;

  while (1) {
    switch (dcr->ReadBlockFromDevice(CHECK_BLOCK_NUMBERS)) {
      case DeviceControlRecord::ReadStatus::Ok:
        // no handling required if read was successful
        break;
      case DeviceControlRecord::ReadStatus::EndOfTape:
        Jmsg(jcr, M_INFO, 0,
             T_("End of Volume at file %u on device %s, Volume \"%s\"\n"),
             dcr->dev->file, dcr->dev->print_name(), dcr->VolumeName);

        VolumeUnused(dcr); /* mark volume unused */
        if (!mount_cb(dcr)) {
          Jmsg(jcr, M_INFO, 0, T_("End of all volumes.\n"));
          if (RecordCb) {
            /* Create EOT Label so that Media record may
             *  be properly updated because this is the last
             *  tape. */
            trec = new_record();
            trec->FileIndex = EOT_LABEL;
            trec->File = dcr->dev->file;
            *status = RecordCb(dcr, trec, user_data);
            if (jcr->sd_impl->read_session.mount_next_volume) {
              jcr->sd_impl->read_session.mount_next_volume = false;
              dcr->dev->ClearEot();
            }
            FreeRecord(trec);
          }
          return false;
        }
        jcr->sd_impl->read_session.mount_next_volume = false;

        /* We just have a new tape up, now read the label (first record)
         *  and pass it off to the callback routine, then continue
         *  most likely reading the previous record. */
        dcr->ReadBlockFromDevice(NO_BLOCK_NUMBER_CHECK);
        trec = new_record();
        ReadRecordFromBlock(dcr, trec);
        HandleSessionRecord(dcr->dev, trec, sessrec);
        if (RecordCb) { RecordCb(dcr, trec, user_data); }

        FreeRecord(trec);
        PositionDeviceToFirstFile(jcr, &ctx->current, dcr);

        // After reading label, we must read first data block
        continue;
      case DeviceControlRecord::ReadStatus::EndOfFile:
        Dmsg3(200, "End of file %u on device %s, Volume \"%s\"\n",
              dcr->dev->file, dcr->dev->print_name(), dcr->VolumeName);
        continue;
      default:
        if (dcr->dev->IsShortBlock()) {
          Jmsg1(jcr, M_ERROR, 0, "%s", dcr->dev->errmsg);
          continue;
        } else {
          // I/O error or strange end of tape
          DisplayTapeErrorStatus(jcr, dcr->dev);
          if (forge_on || jcr->sd_impl->ignore_label_errors) {
            dcr->dev->fsr(1); /* try skipping bad record */
            Pmsg0(000, T_("Did fsr in attempt to skip bad record.\n"));
            continue;
          }
          *status = false;
          return false;
        }
    }

    Dmsg2(debuglevel, "Read new block at pos=%u:%u\n", dcr->dev->file,
          dcr->dev->block_num);
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
bool ReadNextRecordFromBlock(DeviceControlRecord* dcr,
                             READ_CTX* rctx,
                             bool* done)
{
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = dcr->dev;
  DeviceBlock* block = dcr->block;
  DeviceRecord* rec = rctx->rec;

  while (1) {
    if (!ReadRecordFromBlock(dcr, rec)) {
      Dmsg3(400, "!read-break. state_bits=%s blk=%d rem=%d\n",
            rec_state_bits_to_str(rec), block->BlockNumber, rec->remainder);
      return false;
    }

    Dmsg5(debuglevel, "read-OK. state_bits=%s blk=%d rem=%d file:block=%u:%u\n",
          rec_state_bits_to_str(rec), block->BlockNumber, rec->remainder,
          dev->file, dev->block_num);

    /* At this point, we have at least a record header.
     *  Now decide if we want this record or not, but remember
     *  before accessing the record, we may need to read again to
     *  get all the data. */
    rctx->records_processed++;
    Dmsg6(debuglevel, "recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n",
          rctx->records_processed, rec_state_bits_to_str(rec),
          block->BlockNumber, rec->VolSessionId, rec->VolSessionTime,
          rec->FileIndex);

    if (rec->FileIndex == EOM_LABEL) { /* end of tape? */
      Dmsg0(40, "Get EOM LABEL\n");
      return false;
    }

    // Some sort of label?
    if (rec->FileIndex < 0) {
      HandleSessionRecord(dcr->dev, rec, &rctx->sessrec);
      if (jcr->sd_impl->read_session.bsr) {
        // We just check block FI and FT not FileIndex
        rec->match_stat = MatchBsrBlock(rctx->current, dcr->block);
      } else {
        rec->match_stat = 0;
      }

      return true;
    }

    // Apply BootStrapRecord filter
    if (jcr->sd_impl->read_session.bsr) {
      rec->match_stat
          = MatchBsr(rctx->current, rec, &dev->VolHdr, &rctx->sessrec, jcr);
      if (rec->match_stat == -1) { /* no more possible matches */
        *done = true;              /* all items found, stop */
        Dmsg2(debuglevel, "All done=(file:block) %u:%u\n", dev->file,
              dev->block_num);
        return false;
      } else if (rec->match_stat == 0) { /* no match */
        Dmsg4(debuglevel,
              "BootStrapRecord no match: clear rem=%d FI=%d before SetEof pos "
              "%u:%u\n",
              rec->remainder, rec->FileIndex, dev->file, dev->block_num);
        rec->remainder = 0;
        ClearBit(REC_PARTIAL_RECORD, rec->state_bits);
        if (TryDeviceRepositioning(jcr, &rctx->current, rec, dcr)) {
          return false;
        }
        continue; /* we don't want record, read next one */
      }
    }

    dcr->VolLastIndex = rec->FileIndex; /* let caller know where we are */

    if (IsPartialRecord(rec)) {
      Dmsg6(debuglevel,
            "Partial, break. recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n",
            rctx->records_processed, rec_state_bits_to_str(rec),
            block->BlockNumber, rec->VolSessionId, rec->VolSessionTime,
            rec->FileIndex);
      return false; /* read second part of record */
    }

    if (rctx->lastFileIndex != READ_NO_FILEINDEX
        && rctx->lastFileIndex != rec->FileIndex) {
      if (IsThisBsrDone(jcr->sd_impl->read_session.bsr, rec)
          && TryDeviceRepositioning(jcr, &rctx->current, rec, dcr)) {
        Dmsg2(debuglevel, "This bsr done, break pos %u:%u\n", dev->file,
              dev->block_num);
        return false;
      }
      Dmsg2(debuglevel, "==== inside LastIndex=%d FileIndex=%d\n",
            rctx->lastFileIndex, rec->FileIndex);
    }

    Dmsg2(debuglevel, "==== LastIndex=%d FileIndex=%d\n", rctx->lastFileIndex,
          rec->FileIndex);
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
bool ReadRecords(DeviceControlRecord* dcr,
                 bool RecordCb(DeviceControlRecord* dcr,
                               DeviceRecord* rec,
                               void* user_data),
                 bool mount_cb(DeviceControlRecord* dcr),
                 void* user_data)
{
  JobControlRecord* jcr = dcr->jcr;
  READ_CTX* rctx;
  bool ok = true;
  bool done = false;

  rctx = new_read_context();
  PositionDeviceToFirstFile(jcr, &rctx->current, dcr);
  jcr->sd_impl->read_session.mount_next_volume = false;

  while (ok && !done) {
    if (jcr->IsJobCanceled()) {
      ok = false;
      break;
    }

    // Read the next block into our buffers.
    if (!ReadNextBlockFromDevice(dcr, rctx, &rctx->sessrec, RecordCb, mount_cb,
                                 user_data, &ok)) {
      break;
    }

    /* Get a new record for each Job as defined by VolSessionId and
     * VolSessionTime */
    if (!rctx->rec || rctx->rec->VolSessionId != dcr->block->VolSessionId
        || rctx->rec->VolSessionTime != dcr->block->VolSessionTime) {
      ReadContextSetRecord(dcr, rctx);
    }

    Dmsg3(debuglevel, "Before read rec loop. stat=%s blk=%d rem=%d\n",
          rec_state_bits_to_str(rctx->rec), dcr->block->BlockNumber,
          rctx->rec->remainder);

    rctx->records_processed = 0;
    ClearAllBits(REC_STATE_MAX, rctx->rec->state_bits);
    rctx->lastFileIndex = READ_NO_FILEINDEX;
    Dmsg1(debuglevel, "Block %s empty\n",
          IsBlockEmpty(rctx->rec) ? "is" : "NOT");

    /* Process the block and read all records in the block and send
     * them to the defined callback. */
    while (ok && !IsBlockEmpty(rctx->rec)) {
      if (!ReadNextRecordFromBlock(dcr, rctx, &done)) { break; }

      if (rctx->rec->FileIndex < 0) {
        /* Note, we pass *all* labels to the callback routine. If
         *  he wants to know if they matched the bsr, then he must
         *  check the match_stat in the record */
        ok = RecordCb(dcr, rctx->rec, user_data);
      } else {
        Dmsg6(debuglevel,
              "OK callback. recno=%d state_bits=%s blk=%d SI=%d ST=%d FI=%d\n",
              rctx->records_processed, rec_state_bits_to_str(rctx->rec),
              dcr->block->BlockNumber, rctx->rec->VolSessionId,
              rctx->rec->VolSessionTime, rctx->rec->FileIndex);

        // Perform record translations.
        dcr->before_rec = rctx->rec;
        dcr->after_rec = NULL;

        /*
         * We want the plugins to be called in reverse order so we give the
         * GeneratePluginEvent() the reverse argument so it knows that we want
         * the plugins to be called in that order. */
        if (GeneratePluginEvent(jcr, bSdEventReadRecordTranslation, dcr, true)
            != bRC_OK) {
          ok = false;
          continue;
        }

        /* The record got translated when we got an after_rec pointer after
         * calling the bSdEventReadRecordTranslation plugin event. If no
         * translation has taken place we just point the rec pointer to same
         * DeviceRecord as in the before_rec pointer. */
        // callbacks happily overwrite the dcr->*_rec pointers, so we need to
        // make sure that we actually free the correct thing.
        auto* brec = dcr->before_rec;
        auto* arec = dcr->after_rec;
        if (arec) {
          ok = RecordCb(dcr, arec, user_data);
          FreeRecord(arec);
          arec = nullptr;
        } else {
          ok = RecordCb(dcr, dcr->before_rec, user_data);
        }
        dcr->after_rec = arec;
        dcr->before_rec = brec;
      }
    }
    Dmsg2(debuglevel, "After end recs in block. pos=%u:%u\n", dcr->dev->file,
          dcr->dev->block_num);
  }
  // Dmsg2(debuglevel, "Position=(file:block) %u:%u\n", dcr->dev->file,
  // dcr->dev->block_num);

  FreeReadContext(rctx);
  PrintBlockReadErrors(jcr, dcr->block);

  return ok;
}

static bool NoUserData(DeviceControlRecord* dcr,
                       DeviceRecord* rec,
                       void* user_data)
{
  auto* RecordCb
      = reinterpret_cast<bool (*)(DeviceControlRecord* dcr, DeviceRecord* rec)>(
          user_data);

  return RecordCb(dcr, rec);
}

bool ReadRecords(DeviceControlRecord* dcr,
                 bool RecordCb(DeviceControlRecord* dcr, DeviceRecord* rec),
                 bool mount_cb(DeviceControlRecord* dcr))
{
  return ReadRecords(dcr, &NoUserData, mount_cb,
                     reinterpret_cast<void*>(RecordCb));
}

} /* namespace storagedaemon */
