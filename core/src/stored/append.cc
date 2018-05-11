/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MM
 */
/**
 * @file
 * Append code for Storage daemon
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/acquire.h"
#include "stored/append.h"
#include "stored/fd_cmds.h"
#include "stored/label.h"
#include "stored/spool.h"
#include "lib/bget_msg.h"
#include "lib/edit.h"
#include "include/jcr.h"

/* Responses sent to the daemon */
static char OK_data[] =
   "3000 OK data\n";
static char OK_append[] =
   "3000 OK append data\n";
static char OK_replicate[] =
   "3000 OK replicate data\n";

/* Forward referenced functions */

void PossibleIncompleteJob(JobControlRecord *jcr, int32_t last_file_index)
{
}

/**
 * Append Data sent from File daemon
 */
bool DoAppendData(JobControlRecord *jcr, BareosSocket *bs, const char *what)
{
   int32_t n, file_index, stream, last_file_index, job_elapsed;
   bool ok = true;
   char buf1[100];
   DeviceControlRecord *dcr = jcr->dcr;
   Device *dev;
   POOLMEM *rec_data;
   char ec[50];

   if (!dcr) {
      Jmsg0(jcr, M_FATAL, 0, _("DeviceControlRecord is NULL!!!\n"));
      return false;
   }
   dev = dcr->dev;
   if (!dev) {
      Jmsg0(jcr, M_FATAL, 0, _("Device is NULL!!!\n"));
      return false;
   }

   Dmsg1(100, "Start append data. res=%d\n", dev->NumReserved());

   if (!bs->SetBufferSize(dcr->device->max_network_buffer_size, BNET_SETBUF_WRITE)) {
      Jmsg0(jcr, M_FATAL, 0, _("Unable to set network buffer size.\n"));
      goto bail_out;
   }

   if (!acquire_device_for_append(dcr)) {
      goto bail_out;
   }

   if (GeneratePluginEvent(jcr, bsdEventSetupRecordTranslation, dcr) != bRC_OK) {
      goto bail_out;
   }

   jcr->sendJobStatus(JS_Running);

   if (dev->VolCatInfo.VolCatName[0] == 0) {
      Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
   }
   Dmsg1(50, "Begin append device=%s\n", dev->print_name());

   if (!BeginDataSpool(dcr) ) {
      goto bail_out;
   }

   if (!BeginAttributeSpool(jcr)) {
      DiscardDataSpool(dcr);
      goto bail_out;
   }

   Dmsg0(100, "Just after acquire_device_for_append\n");
   if (dev->VolCatInfo.VolCatName[0] == 0) {
      Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
   }

   /*
    * Write Begin Session Record
    */
   if (!WriteSessionLabel(dcr, SOS_LABEL)) {
      Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
         dev->bstrerror());
      jcr->setJobStatus(JS_ErrorTerminated);
      ok = false;
   }
   if (dev->VolCatInfo.VolCatName[0] == 0) {
      Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
   }

   /*
    * Tell daemon to send data
    */
   if (!bs->fsend(OK_data)) {
      BErrNo be;
      Jmsg2(jcr, M_FATAL, 0, _("Network send error to %s. ERR=%s\n"),
            what, be.bstrerror(bs->b_errno));
      ok = false;
   }

   /*
    * Get Data from daemon, write to device.  To clarify what is
    * going on here.  We expect:
    * - A stream header
    * - Multiple records of data
    * - EOD record
    *
    * The Stream header is just used to sychronize things, and
    * none of the stream header is written to tape.
    * The Multiple records of data, contain first the Attributes,
    * then after another stream header, the file data, then
    * after another stream header, the MD5 data if any.
    *
    * So we get the (stream header, data, EOD) three time for each
    * file. 1. for the Attributes, 2. for the file data if any,
    * and 3. for the MD5 if any.
    */
   dcr->VolFirstIndex = dcr->VolLastIndex = 0;
   jcr->run_time = time(NULL);              /* start counting time for rates */
   for (last_file_index = 0; ok && !jcr->IsJobCanceled(); ) {
      /*
       * Read Stream header from the daemon.
       *
       * The stream header consists of the following:
       * - file_index (sequential Bareos file index, base 1)
       * - stream     (Bareos number to distinguish parts of data)
       * - info       (Info for Storage daemon -- compressed, encrypted, ...)
       *               info is not currently used, so is read, but ignored!
       */
      if ((n = BgetMsg(bs)) <= 0) {
         if (n == BNET_SIGNAL && bs->message_length == BNET_EOD) {
            break;                    /* end of data */
         }
         Jmsg2(jcr, M_FATAL, 0, _("Error reading data header from %s. ERR=%s\n"),
               what, bs->bstrerror());
         PossibleIncompleteJob(jcr, last_file_index);
         ok = false;
         break;
      }

      if (sscanf(bs->msg, "%ld %ld", &file_index, &stream) != 2) {
         Jmsg2(jcr, M_FATAL, 0, _("Malformed data header from %s: %s\n"),
               what, bs->msg);
         ok = false;
         PossibleIncompleteJob(jcr, last_file_index);
         break;
      }

      Dmsg2(890, "<filed: Header FilInx=%d stream=%d\n", file_index, stream);

      /*
       * We make sure the file_index is advancing sequentially.
       * An incomplete job can start the file_index at any number.
       * otherwise, it must start at 1.
       */
      if (jcr->rerunning && file_index > 0 && last_file_index == 0) {
         goto fi_checked;
      }
      if (file_index > 0 && (file_index == last_file_index ||
          file_index == last_file_index + 1)) {
         goto fi_checked;
      }
      Jmsg3(jcr, M_FATAL, 0, _("FI=%d from %s not positive or sequential=%d\n"),
            file_index, what, last_file_index);
      PossibleIncompleteJob(jcr, last_file_index);
      ok = false;
      break;

fi_checked:
      if (file_index != last_file_index) {
         jcr->JobFiles = file_index;
         last_file_index = file_index;
      }

      /*
       * Read data stream from the daemon. The data stream is just raw bytes.
       * We save the original data pointer from the record so we can restore
       * that after the loop ends.
       */
      rec_data = dcr->rec->data;
      while ((n = BgetMsg(bs)) > 0 && !jcr->IsJobCanceled()) {
         dcr->rec->VolSessionId = jcr->VolSessionId;
         dcr->rec->VolSessionTime = jcr->VolSessionTime;
         dcr->rec->FileIndex = file_index;
         dcr->rec->Stream = stream;
         dcr->rec->maskedStream = stream & STREAMMASK_TYPE; /* strip high bits */
         dcr->rec->data_len = bs->message_length;
         dcr->rec->data = bs->msg; /* use message buffer */

         Dmsg4(850, "before writ_rec FI=%d SessId=%d Strm=%s len=%d\n",
               dcr->rec->FileIndex, dcr->rec->VolSessionId,
               stream_to_ascii(buf1, dcr->rec->Stream,
               dcr->rec->FileIndex), dcr->rec->data_len);

         ok = dcr->WriteRecord();
         if (!ok) {
            Dmsg2(90, "Got WriteBlockToDev error on device %s. %s\n",
                  dcr->dev->print_name(), dcr->dev->bstrerror());
            break;
         }

         SendAttrsToDir(jcr, dcr->rec);
         Dmsg0(650, "Enter bnet_get\n");
      }
      Dmsg2(650, "End read loop with %s. Stat=%d\n", what, n);

      /*
       * Restore the original data pointer.
       */
      dcr->rec->data = rec_data;

      if (bs->IsError()) {
         if (!jcr->IsJobCanceled()) {
            Dmsg2(350, "Network read error from %s. ERR=%s\n",
                  what, bs->bstrerror());
            Jmsg2(jcr, M_FATAL, 0, _("Network error reading from %s. ERR=%s\n"),
                  what, bs->bstrerror());
            PossibleIncompleteJob(jcr, last_file_index);
         }
         ok = false;
         break;
      }
   }

   /*
    * Create Job status for end of session label
    */
   jcr->setJobStatus(ok ? JS_Terminated : JS_ErrorTerminated);

   if (ok && bs == jcr->file_bsock) {
      /*
       * Terminate connection with FD
       */
      bs->fsend(OK_append);
      DoFdCommands(jcr);               /* finish dialog with FD */
   } else if (bs == jcr->store_bsock) {
      bs->fsend(OK_replicate);
   } else {
      bs->fsend("3999 Failed append\n");
   }

   Dmsg1(200, "Write EOS label JobStatus=%c\n", jcr->JobStatus);

   /*
    * Check if we can still write. This may not be the case
    * if we are at the end of the tape or we got a fatal I/O error.
    */
   if (ok || dev->CanWrite()) {
      if (!WriteSessionLabel(dcr, EOS_LABEL)) {
         /*
          * Print only if ok and not cancelled to avoid spurious messages
          */
         if (ok && !jcr->IsJobCanceled()) {
            Jmsg1(jcr, M_FATAL, 0, _("Error writing end session label. ERR=%s\n"),
                  dev->bstrerror());
            PossibleIncompleteJob(jcr, last_file_index);
         }
         jcr->setJobStatus(JS_ErrorTerminated);
         ok = false;
      }
      Dmsg0(90, "back from write_end_session_label()\n");

      /*
       * Flush out final partial block of this session
       */
      if (!dcr->WriteBlockToDevice()) {
         /*
          * Print only if ok and not cancelled to avoid spurious messages
          */
         if (ok && !jcr->IsJobCanceled()) {
            Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
                  dev->print_name(), dev->bstrerror());
            Dmsg0(100, _("Set ok=FALSE after WriteBlockToDevice.\n"));
            PossibleIncompleteJob(jcr, last_file_index);
         }
         jcr->setJobStatus(JS_ErrorTerminated);
         ok = false;
      }
   }

   if (!ok && !jcr->is_JobStatus(JS_Incomplete)) {
      DiscardDataSpool(dcr);
   } else {
      /*
       * Note: if commit is OK, the device will remain blocked
       */
      CommitDataSpool(dcr);
   }

   /*
    * Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
    * and the subsequent Jmsg() editing will break
    */
   job_elapsed = time(NULL) - jcr->run_time;
   if (job_elapsed <= 0) {
      job_elapsed = 1;
   }

   Jmsg(dcr->jcr, M_INFO, 0, _("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
        job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
        edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec));

   /*
    * Release the device -- and send final Vol info to DIR and unlock it.
    */
   ReleaseDevice(dcr);

   if ((!ok || jcr->IsJobCanceled()) && !jcr->is_JobStatus(JS_Incomplete)) {
      DiscardAttributeSpool(jcr);
   } else {
      CommitAttributeSpool(jcr);
   }

   jcr->sendJobStatus();          /* update director */

   Dmsg1(100, "return from DoAppendData() ok=%d\n", ok);
   return ok;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);
   return false;
}

/**
 * Send attributes and digest to Director for Catalog
 */
bool SendAttrsToDir(JobControlRecord *jcr, DeviceRecord *rec)
{
   if (rec->maskedStream == STREAM_UNIX_ATTRIBUTES    ||
       rec->maskedStream == STREAM_UNIX_ATTRIBUTES_EX ||
       rec->maskedStream == STREAM_RESTORE_OBJECT     ||
       CryptoDigestStreamType(rec->maskedStream) != CRYPTO_DIGEST_NONE) {
      if (!jcr->no_attributes) {
         BareosSocket *dir = jcr->dir_bsock;
         if (AreAttributesSpooled(jcr)) {
            dir->SetSpooling();
         }
         Dmsg0(850, "Send attributes to dir.\n");
         if (!jcr->dcr->DirUpdateFileAttributes(rec)) {
            Jmsg(jcr, M_FATAL, 0, _("Error updating file attributes. ERR=%s\n"),
               dir->bstrerror());
            dir->ClearSpooling();
            return false;
         }
         dir->ClearSpooling();
      }
   }
   return true;
}
